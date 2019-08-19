#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "root.h"

/**************************************************************************
	STRUCTURES
**************************************************************************/
typedef struct                       				/**** BMP file header structure ****/
{
	unsigned int   		bfSize;           		/* Size of file */
	unsigned short 	bfReserved1;     		/* Reserved */
	unsigned short 	bfReserved2;     		/* ... */
	unsigned int   		bfOffBits;       		/* Offset to bitmap data */
} BITMAPFILEHEADER;

typedef struct                       				/**** BMP file info structure ****/
{
	unsigned int   		biSize;           		/* Size of info header */
	int            		biWidth;          		/* Width of image */
	int            		biHeight;         		/* Height of image */
	unsigned short 	biPlanes;        		/* Number of color planes */
	unsigned short 	biBitCount;      		/* Number of bits per pixel */
	unsigned int   		biCompression;   	/* Type of compression to use */
	unsigned int   		biSizeImage;      		/* Size of image data */
	int            		biXPelsPerMeter;  	/* X pixels per meter */
	int            		biYPelsPerMeter;  	/* Y pixels per meter */
	unsigned int   		biClrUsed;        		/* Number of colors used */
	unsigned int   		biClrImportant; 		/* Number of important colors */
} BITMAPINFOHEADER;

typedef struct
{
	struct SwsContext *	imgCtx;
	AVFrame *			YUVframe;
	AVFrame *			RGBframe;
	
	char * 				doublebuff[2];
	int					doublebuff_size[2];

	int					doublebuff_index;

	unsigned long 			sema4;
} YUV2BMPStruct;

/**************************************************************************
	FUNCTION PROTOTYPES
**************************************************************************/
static int bitmap(char * bitmapbuff, char * rgbbuff, int width, int height, int stride);

/**************************************************************************
	FUNCTION DEFINITIONS
**************************************************************************/
static int bitmap(char * bitmapbuff, char * rgbbuff, int width, int height, int stride)
{
	BITMAPFILEHEADER bfh;
	BITMAPINFOHEADER bih;
	int w, h, len=0;

	/* Magic number for file. It does not fit in the header structure due to alignment requirements, so put it outside */
	unsigned short bfType	= 0x4d42;   
	
	bfh.bfReserved1 		= 0;
	bfh.bfReserved2 		= 0;
	bfh.bfSize 			= 2+sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)+width*height*3;
	bfh.bfOffBits 			= 0x36;

	bih.biSize 			= sizeof(BITMAPINFOHEADER);
	bih.biWidth 			= width;
	bih.biHeight 			= height;
	bih.biPlanes 			= 1;
	bih.biBitCount 		= 24;
	bih.biCompression 		= 0;
	bih.biSizeImage 		= 0;
	bih.biXPelsPerMeter 	= 5000;
	bih.biYPelsPerMeter 	= 5000;
	bih.biClrUsed 			= 0;
	bih.biClrImportant 		= 0;

	/*Write headers*/
	memcpy(bitmapbuff + len, &bfType, sizeof(bfType));
	len += sizeof(bfType);
	memcpy(bitmapbuff + len, &bfh, sizeof(bfh));
	len += sizeof(bfh);
	memcpy(bitmapbuff + len, &bih, sizeof(bih));
	len += sizeof(bih);

	bitmapbuff += len;
	for(h=0; h < height; h++)
	{
		memcpy(bitmapbuff +(h*stride), rgbbuff + ((height-1-h)*stride), stride);
		len += stride;
	}

	return len;
}

unsigned int init_YUV2BMP(int width, int height)
{
	YUV2BMPStruct *	pHandle = NULL;

	pHandle= (YUV2BMPStruct *)malloc(sizeof(YUV2BMPStruct)); 
	if(pHandle == NULL)
	{
		fprintf(stderr, "yuv2bmp.c: initYUV2BMP: Error YUV2BMPStruct handle malloc failed !!!\n");
		exit(-1);
	}

	pHandle->YUVframe = av_frame_alloc();
	if(pHandle->YUVframe == NULL) 
	{
		fprintf(stderr, "yuv2bmp: initYUV2BMP: Error av_frame_allov (YUVframe) malloc failed !!!\n");
		exit(1);
	}

	pHandle->RGBframe = av_frame_alloc();
	if(pHandle->RGBframe == NULL) 
	{
		fprintf(stderr, "yuv2bmp: initYUV2BMP: Error av_frame_allov (RGBframe) malloc failed !!!\n");
		exit(-1);
	}

	pHandle->imgCtx 				= NULL;
	
	// Output RGB frame memory allocaton
	pHandle->RGBframe->width 	= width;
	pHandle->RGBframe->height 	= height;

	pHandle->RGBframe->format	= AV_PIX_FMT_BGR24;
	av_image_alloc(pHandle->RGBframe->data, pHandle->RGBframe->linesize, pHandle->RGBframe->width, pHandle->RGBframe->height, pHandle->RGBframe->format, 1);	

	// Bitmap double buffers allocation
	pHandle->doublebuff[0] = malloc(2+sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)+width*height*3+1024*10);
	if(pHandle->doublebuff[0] == NULL) 
	{
		fprintf(stderr, "yuv2bmp.c: initYUV2BMP: Error doublebuff[0] malloc failed !!!\n");
		exit(-1);
	}
	pHandle->doublebuff_size[0] = 0;

	pHandle->doublebuff[1] = malloc(2+sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)+width*height*3+1024*10);
	if(pHandle->doublebuff[1] == NULL) 
	{
		fprintf(stderr, "yuv2bmp.c: initYUV2BMP: Error doublebuff[1] malloc failed !!!\n");		
		exit(-1);
	}
	pHandle->doublebuff_size[1] = 0;

	pHandle->doublebuff_index = -1;

	createSemaphore(&pHandle->sema4, 1);

	return (unsigned int)pHandle;
}

int get_YUV2BMP_data(unsigned int handle, char * buff)
{
	YUV2BMPStruct *	pHandle = (YUV2BMPStruct *)(handle);
	int index, len;

	if(pHandle == NULL)
		return 0;
	
	acquireSemaphore(pHandle->sema4, -1);
	
	if(pHandle->doublebuff_index == -1)
	{
		releaseSemaphore(pHandle->sema4);		
		return 0;
	}

	if(pHandle->doublebuff_index== 0)
		index = 1;
	else
		index = 0;

	len = pHandle->doublebuff_size[index];
	memcpy(buff, pHandle->doublebuff[index], len);

	releaseSemaphore(pHandle->sema4);

	return len;
}

int set_YUV2BMP_data(unsigned int handle, unsigned char ** data, int width, int height, int * stride, enum AVPixelFormat pix_fmt)
{
	YUV2BMPStruct *	pHandle = (YUV2BMPStruct *)(handle);

	if(pHandle == NULL)
		return -1;

	acquireSemaphore(pHandle->sema4, -1);

	if(pHandle->doublebuff_index== -1)
		pHandle->doublebuff_index = 0;

	// Input frame parameters
	pHandle->YUVframe->data[0] 		= data[0];
	pHandle->YUVframe->data[1] 		= data[1];
	pHandle->YUVframe->data[2] 		= data[2];

	if(pHandle->imgCtx == NULL)
	{
		pHandle->YUVframe->width			= width;
		pHandle->YUVframe->height		= height;
		
		pHandle->YUVframe->linesize[0] 	= stride[0];
		pHandle->YUVframe->linesize[1] 	= stride[1];
		pHandle->YUVframe->linesize[2] 	= stride[2];

		pHandle->YUVframe->format		= pix_fmt;
		
		pHandle->imgCtx = sws_getCachedContext(pHandle->imgCtx, pHandle->YUVframe->width, pHandle->YUVframe->height, pHandle->YUVframe->format, pHandle->RGBframe->width, pHandle->RGBframe->height, pHandle->RGBframe->format, SWS_BICUBIC, NULL, NULL, NULL);
		if(pHandle->imgCtx == NULL) 
		{
			fprintf(stderr, "yuv2bmp.c: set_YUV2BMP_data: Cannot initialize the conversion context!\n");
			exit(1);
		}
	}
	else
	{
		if((pHandle->YUVframe->width != width) || (pHandle->YUVframe->height != height) || 
			(pHandle->YUVframe->linesize[0] != stride[0]) || (pHandle->YUVframe->linesize[1] != stride[1]) || (pHandle->YUVframe->linesize[2] != stride[2]) ||
			(pHandle->YUVframe->format != pix_fmt) )
		{
			sws_freeContext(pHandle->imgCtx);
			pHandle->imgCtx = NULL;
			
			pHandle->YUVframe->width			= width;
			pHandle->YUVframe->height		= height;
			
			pHandle->YUVframe->linesize[0] 	= stride[0];
			pHandle->YUVframe->linesize[1] 	= stride[1];
			pHandle->YUVframe->linesize[2] 	= stride[2];

			pHandle->YUVframe->format		= pix_fmt;
			
			pHandle->imgCtx = sws_getCachedContext(pHandle->imgCtx, pHandle->YUVframe->width, pHandle->YUVframe->height, pHandle->YUVframe->format, pHandle->RGBframe->width, pHandle->RGBframe->height, pHandle->RGBframe->format, SWS_BICUBIC, NULL, NULL, NULL);
			if(pHandle->imgCtx == NULL) 
			{
				fprintf(stderr, "yuv2bmp.c: set_YUV2BMP_data: Cannot initialize the conversion context!\n");
				exit(1);
			}
		}
	}

	/* Scale Image */
	sws_scale(pHandle->imgCtx, pHandle->YUVframe->data, pHandle->YUVframe->linesize, 0, pHandle->YUVframe->height, pHandle->RGBframe->data, pHandle->RGBframe->linesize);

	pHandle->doublebuff_size[pHandle->doublebuff_index]  = bitmap(pHandle->doublebuff[pHandle->doublebuff_index], pHandle->RGBframe->data[0], pHandle->RGBframe->width, pHandle->RGBframe->height, pHandle->RGBframe->linesize[0]);

	if(pHandle->doublebuff_index== 0)
		pHandle->doublebuff_index= 1;
	else
		pHandle->doublebuff_index= 0;

	releaseSemaphore(pHandle->sema4);

	return 0;
}

