#ifndef CHUNK_SENDDING_H
#define CHUNK_SENDING_H

#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <chunk.h>
#include <chunkbuffer.h>
#include <net_helper.h>
#include <payload_headers.h>

typedef struct ChunkSendingSettings
{
  	struct chunk_buffer *		chunkbuf;

	unsigned int				max_chunk_size;
	unsigned int				min_chunk_delay_ms;
	
	unsigned int 				player_port;

	/* internal variables */
	int					threadStatus;

}ChunkSendingSettings_t, *pChunkSendingSettings_t;

int ChunkSendingThread(void * arg);

#endif //CHUNK_TRADING_H

