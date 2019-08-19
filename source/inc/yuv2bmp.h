#ifndef YUV2BMP_H
#define YUV2BMP_H

#include <stdlib.h>

#include "os.h"
#include "error.h"

unsigned int init_YUV2BMP(int width, int height);
int get_YUV2BMP_data(unsigned int handle, char * buff);
int set_YUV2BMP_data(unsigned int handle, unsigned char ** data, int width, int height, int * stride, enum AVPixelFormat pix_fmt);

#endif

