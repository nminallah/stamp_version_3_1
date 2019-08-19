#ifndef AENCODER_H
#define AENCODER_H

#include <stdlib.h>
#include "error.h"
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

typedef struct AudioEncoderSettings
{
	unsigned long	in_Q[2];
	unsigned long	out_Q[2];

	int			codec_id;
	int 			bit_rate;

	/* internal variables */
	int				threadStatus;
	
}AudioEncoderSettings_t, *pAudioEncoderSettings_t;

int AudioEncoderThread(void * arg);

#endif

