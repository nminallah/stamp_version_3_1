#ifndef RECVFROMSRC_H
#define RECVFROMSRC_H

#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <chunk.h>
#include <chunkbuffer.h>
#include <net_helper.h>
#include <payload_headers.h>

typedef struct RecvFromSrcSettings
{
	unsigned int			max_chunk_size;

  	struct chunk_buffer *	chunkbuf;
	struct peerset *		peerinfo;
	
	unsigned int 			recv_source_listen_port;
	
	/* internal variables */
	int					threadStatus;

}RecvFromSrcSettings_t, *pRecvFromSrcSettings_t;

int RecvFromSrcThread(void * arg);

#endif


