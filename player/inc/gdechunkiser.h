#ifndef DECHUNKISER_H
#define DECHUNKISER_H

#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <chunk.h>
#include <payload_headers.h>
#include "error.h"

typedef struct GDeChunkiserSettings
{
	unsigned int		max_chunk_size;
	
	unsigned int 		player_listen_port;

	unsigned long		in_Q[2];

	unsigned long		out_Video_Q[2];
	unsigned long		out_Audio_Q[2];
	int				headless;
	
	/* internal variables */
	int				threadStatus;
	int				exit;
	int				pause;

}GDeChunkiserSettings_t, *pGDeChunkiserSettings_t;

int GDeChunkiserThread(void * arg);

#endif


