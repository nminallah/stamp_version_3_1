#ifndef DEMUX_H
#define DEMUX_H

#include <stdlib.h>
#include <libavformat/avformat.h>

#include "error.h"

typedef struct DemuxSettings
{
	char 			av_filename[1024];	
	int				loop;

	unsigned long		video_out_Q[2];
	unsigned long		audio_out_Q[2];

	/* internal variables */
	int				threadStatus;
	int				pause;
}DemuxSettings_t, *pDemuxSettings_t;

int DemuxThread(void * arg);

#endif

