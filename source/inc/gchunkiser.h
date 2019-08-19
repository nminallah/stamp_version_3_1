#ifndef CHUNKISER_H
#define CHUNKISER_H

#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <chunk.h>
#include <payload_headers.h>
#include "error.h"

typedef struct GChunkiserSettings
{
	enum AVMediaType	type;
	int				num_frames_in_payload;

	unsigned int		max_chunk_size;
	
	unsigned long		in_Q[2];
	unsigned long		out_Q[2];
	int				socketHandle;

	struct sockaddr_in    * servaddr;
	
	/* internal variables */
	int				threadStatus;

}GChunkiserSettings_t, *pGChunkiserSettings_t;

int GChunkiserThread(void * arg);

#endif


