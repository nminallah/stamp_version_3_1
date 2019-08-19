#ifndef CHUNK_TRADING_H
#define CHUNK_TRADING_H

#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <chunk.h>
#include <chunkbuffer.h>
#include <net_helper.h>
#include <payload_headers.h>

typedef struct ChunkTradingSettings
{
  	struct chunk_buffer *		chunkbuf;
	struct peerset *			peerinfo;
	int						period_msec;

	/* internal variables */
	int					threadStatus;

}ChunkTradingSettings_t, *pChunkTradingSettings_t;

int ChunkTradingThread(void * arg);

#endif //CHUNK_TRADING_H


