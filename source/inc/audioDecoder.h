#ifndef ADECODER_H
#define ADECODER_H

#include <stdlib.h>
#include "error.h"
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

typedef struct AudioDecoderSettings
{
	int				maintain_input_frame_rate;
	
	unsigned long		in_Q[2];
	unsigned long		out_Q[2];

	/* internal variables */
	int				threadStatus;
	
}AudioDecoderSettings_t, *pAudioDecoderSettings_t;

int AudioDecoderThread(void * arg);

#endif

