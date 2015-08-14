/*
 * Copyright 2002-2010 Guillaume Cottenceau.
 *
 * This software may be freely redistributed under the terms
 * of the X11 license.
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define PNG_DEBUG 3
#include "png.h"

void abort_(const char * s, ...)
{
  va_list args;
  va_start(args, s);
  vfprintf(stderr, s, args);
  fprintf(stderr, "\n");
  va_end(args);
  abort();
}

int x, y;

int width, height;
png_byte color_type;
png_byte bit_depth;

png_structp png_ptr;
png_infop info_ptr;
int number_of_passes;
png_bytep * row_pointers = NULL;

// BGR or BGRA data will be read into this buffer of pixels

uint32_t *pixels = NULL;

const int debugPrintPixelsReadAndWritten = 0;

void allocate_row_pointers()
{
  if (row_pointers != NULL) {
    abort_("[allocate_row_pointers] row_pointers already allocated");
  }
  
  row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
  
  if (!row_pointers) {
    abort_("[allocate_row_pointers] could not allocate %d bytes to store row data", (sizeof(png_bytep) * height));
  }
  
  for (y=0; y<height; y++) {
    row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr,info_ptr));
    
    if (!row_pointers[y]) {
      abort_("[allocate_row_pointers] could not allocate %d bytes to store row data", png_get_rowbytes(png_ptr,info_ptr));
    }
  }
}

void free_row_pointers()
{
  if (row_pointers == NULL) {
    return;
  }
  
  for (y=0; y<height; y++)
    free(row_pointers[y]);
  free(row_pointers);
  row_pointers = NULL;
}

void read_png_file(char* file_name)
{
  char header[8];    // 8 is the maximum size that can be checked
  
  /* open file and test for it being a png */
  FILE *fp = fopen(file_name, "rb");
  if (!fp)
    abort_("[read_png_file] File %s could not be opened for reading", file_name);
  fread(header, 1, 8, fp);
  if (png_sig_cmp((png_const_bytep)header, 0, 8))
    abort_("[read_png_file] File %s is not recognized as a PNG file", file_name);
  
  /* initialize stuff */
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  
  if (!png_ptr)
    abort_("[read_png_file] png_create_read_struct failed");
  
  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    abort_("[read_png_file] png_create_info_struct failed");
  
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[read_png_file] Error during init_io");
  
  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);
  
  png_read_info(png_ptr, info_ptr);
  
  width = png_get_image_width(png_ptr, info_ptr);
  height = png_get_image_height(png_ptr, info_ptr);
  color_type = png_get_color_type(png_ptr, info_ptr);
  bit_depth = png_get_bit_depth(png_ptr, info_ptr);
  
  number_of_passes = png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
  
  if (bit_depth != 8) {
    abort_("[read_png_file] only 8 bit pixel depth PNG is supported");
  }
  
  /* read file */
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[read_png_file] Error during read_image");
  
  allocate_row_pointers();
  
  png_read_image(png_ptr, row_pointers);
  
  /* allocate pixels data and read into array of pixels */
  
  pixels = (uint32_t*) malloc(width * height * sizeof(uint32_t));
  
  if (!pixels) {
    abort_("[read_png_file] could not allocate %d bytes to store pixel data", (width * height * sizeof(uint32_t)));
  }
  
  int pixeli = 0;
  
  int isBGRA = 0;
  int isGrayscale = 0;
  
  if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGBA) {
    isBGRA = 1;
  } else if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB) {
    isBGRA = 0;
  } else if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY) {
    isGrayscale = 1;
  } else {
    abort_("[read_png_file] unsupported input format type");
  }
  
  for (y=0; y<height; y++) {
    png_byte* row = row_pointers[y];
    
    if (isGrayscale) {
      for (x=0; x<width; x++) {
        png_byte* ptr = &(row[x]);
        
        uint32_t gray = ptr[0];
        uint32_t pixel = (0xFF << 24) | (gray << 16) | (gray << 8) | gray;
        
        if (debugPrintPixelsReadAndWritten) {
          fprintf(stdout, "Read pixel 0x%08X at (x,y) (%d, %d)\n", pixel, x, y);
        }
        
        pixels[pixeli] = pixel;
        
        pixeli++;
      }
    } else if (isBGRA) {
      for (x=0; x<width; x++) {
        png_byte* ptr = &(row[x*4]);
        
        uint32_t B = ptr[2];
        uint32_t G = ptr[1];
        uint32_t R = ptr[0];
        uint32_t A = ptr[3];
        
        uint32_t pixel = (A << 24) | (R << 16) | (G << 8) | B;
        
        if (debugPrintPixelsReadAndWritten) {
        fprintf(stdout, "Read pixel 0x%08X at (x,y) (%d, %d)\n", pixel, x, y);
        }
        
        pixels[pixeli] = pixel;
        
        pixeli++;
      }
    } else {
      // BGR with no alpha channel
      for (x=0; x<width; x++) {
        png_byte* ptr = &(row[x*3]);
        
        uint32_t B = ptr[2];
        uint32_t G = ptr[1];
        uint32_t R = ptr[0];
        
        uint32_t pixel = (0xFF << 24) | (R << 16) | (G << 8) | B;
        
        if (debugPrintPixelsReadAndWritten) {
        fprintf(stdout, "Read pixel 0x%08X at (x,y) (%d, %d)\n", pixel, x, y);
        }
        
        pixels[pixeli] = pixel;
        
        pixeli++;
      }
    }
  }
  
  fclose(fp);
  
  free_row_pointers();
}


void write_png_file(char* file_name)
{
  /* create file */
  FILE *fp = fopen(file_name, "wb");
  if (!fp)
    abort_("[write_png_file] File %s could not be opened for writing", file_name);
  
  
  /* initialize stuff */
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  
  if (!png_ptr)
    abort_("[write_png_file] png_create_write_struct failed");
  
  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    abort_("[write_png_file] png_create_info_struct failed");
  
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during init_io");
  
  png_init_io(png_ptr, fp);
  
  
  /* write header */
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during writing header");
  
  png_set_IHDR(png_ptr, info_ptr, width, height,
               bit_depth, color_type, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  
  png_write_info(png_ptr, info_ptr);
  
  /* write pixels back to row_pointers */
  
  allocate_row_pointers();
  
  int isBGRA = 0;
  int isGrayscale = 0;
  
  if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGBA) {
    isBGRA = 1;
  } else if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB) {
    isBGRA = 0;
  } else if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY) {
    isGrayscale = 1;
  } else {
    abort_("[write_png_file] unsupported input format type");
  }
  
  int pixeli = 0;
  
  for (y=0; y<height; y++) {
    png_byte* row = row_pointers[y];
    
    if (isGrayscale) {
      for (x=0; x<width; x++) {
        png_byte* ptr = &(row[x]);
        
        uint32_t pixel = pixels[pixeli];
        
        uint32_t gray = pixel & 0xFF;
        
        ptr[0] = gray;
        
        if (debugPrintPixelsReadAndWritten) {
          fprintf(stdout, "Wrote pixel 0x%08X at (x,y) (%d, %d)\n", pixel, x, y);
        }
        
        pixeli++;
      }
    } else if (isBGRA) {
      for (x=0; x<width; x++) {
        png_byte* ptr = &(row[x*4]);
        
        uint32_t pixel = pixels[pixeli];
        
        uint32_t B = pixel & 0xFF;
        uint32_t G = (pixel >> 8) & 0xFF;
        uint32_t R = (pixel >> 16) & 0xFF;
        uint32_t A = (pixel >> 24) & 0xFF;
        
        ptr[0] = R;
        ptr[1] = G;
        ptr[2] = B;
        ptr[3] = A;
        
        if (debugPrintPixelsReadAndWritten) {
        fprintf(stdout, "Wrote pixel 0x%08X at (x,y) (%d, %d)\n", pixel, x, y);
        }
        
        pixeli++;
      }
    } else {
      for (x=0; x<width; x++) {
        png_byte* ptr = &(row[x*3]);
        
        uint32_t pixel = pixels[pixeli];
        
        uint32_t B = pixel & 0xFF;
        uint32_t G = (pixel >> 8) & 0xFF;
        uint32_t R = (pixel >> 16) & 0xFF;
        
        ptr[0] = R;
        ptr[1] = G;
        ptr[2] = B;
        
        if (debugPrintPixelsReadAndWritten) {
        fprintf(stdout, "Wrote pixel 0x%08X at (x,y) (%d, %d)\n", pixel, x, y);
        }
        
        pixeli++;
      }
    }
  }
  
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during writing bytes");
  
  png_write_image(png_ptr, row_pointers);
  
  
  /* end write */
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during end of write");
  
  png_write_end(png_ptr, NULL);
  
  fclose(fp);
}

void process_file(void)
{
  // Swap the B and R channels in the BGRA format pixels
  
  int numPixels = width * height;
  
  for ( int i = 0; i < numPixels; i++) {
    uint32_t pixel = pixels[i];
    uint32_t B = pixel & 0xFF;
    uint32_t G = (pixel >> 8) & 0xFF;
    uint32_t R = (pixel >> 16) & 0xFF;
    uint32_t A = (pixel >> 24) & 0xFF;
    
    // Swap B and R channels, a grayscale image will not be modified
    
    if ((1)) {
      uint32_t tmp = B;
      B = R;
      R = tmp;
    }
   
    uint32_t outPixel = (A << 24) | (R << 16) | (G << 8) | B;
    
    pixels[i] = outPixel;
  }
}

/* deallocate memory */

void cleanup()
{
  free_row_pointers();
  
  free(pixels);
}

int main(int argc, char **argv)
{
  if (argc != 3) {
    abort_("Usage: %s <in_png> <out_png>", argv[0]);
  }
  
  read_png_file(argv[1]);
  process_file();
  write_png_file(argv[2]);
  
  cleanup();
  
  printf("success processing %d pixels from image of dimensions %d x %d\n", width*height, width, height);
  
  return 0;
}
