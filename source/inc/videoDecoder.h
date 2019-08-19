#ifndef VIDECODER_H
#define VIDECODER_H

#include <stdlib.h>
#include "error.h"
#include <libavcodec/avcodec.h>

typedef struct VideoDecoderSettings
{
	int				maintain_input_frame_rate;
	unsigned long		in_Q[2];
	unsigned long		out_Q[2];

	unsigned int 		view_handle;

	/* internal variables */
	int				threadStatus;
	
}VideoDecoderSettings_t, *pVideoDecoderSettings_t;

int VideoDecoderThread(void * arg);

#endif

