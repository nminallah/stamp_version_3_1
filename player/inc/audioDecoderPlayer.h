#ifndef ADECODERPLAYER_H
#define ADECODERPLAYER_H

#include <stdlib.h>
#include "error.h"
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

typedef struct AudioDecoderPlayerSettings
{
	int				maintain_input_frame_rate;
	
	unsigned long		in_Q[2];

	float		 		AVSync_lag_msec;

	/* internal variables */
	int				threadStatus;
	
}AudioDecoderPlayerSettings_t, *pAudioDecoderPlayerSettings_t;

int AudioDecoderPlayerThread(void * arg);

#endif

