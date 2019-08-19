#ifndef VIDECODERPLAYER_H
#define VIDECODERPLAYER_H

#include <stdlib.h>
#include "error.h"
#include <libavcodec/avcodec.h>

#include <SDL.h>
#include <SDL_thread.h>
#include <libswscale/swscale.h>
#include "params.h"

typedef struct VideoDecoderPlayerSettings
{
	int				maintain_input_frame_rate;
	pSDLParams_t	pgui_vdec_struct;

	unsigned long		in_Q[2];

	float		 		AVSync_lag_msec;
	
	/* internal variables */
	int				threadStatus;
	
}VideoDecoderPlayerSettings_t, *pVideoDecoderPlayerSettings_t;

int VideoDecoderPlayerThread(void * arg);

#endif

