#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include <stdlib.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "error.h"

typedef struct VideoEncoderSettings
{
	int		codec_id;
	int		bit_rate;
	int		width;
	int		height;

	int		bit_rate_2;
	int		width_2;
	int		height_2;

	int		bit_rate_3;
	int		width_3;
	int		height_3;

	int		bit_rate_4;
	int		width_4;
	int		height_4;

	unsigned long		in_Q[2];
	unsigned long		out_Q[2];
	
	int				scalability;
	
	/* internal variables */
	int				threadStatus;

}VideoEncoderSettings_t, *pVideoEncoderSettings_t;

int VideoEncoderThread(void * arg);

#endif


