#ifndef PEER_MESSAGES_H
#define PEER_MESSAGES_H

#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <chunk.h>
#include <chunkbuffer.h>
#include <net_helper.h>
#include <payload_headers.h>

#define MSG_TYPE_RTT   	0x16
#define RTT_QUERY 		0x01
#define RTT_REPLY 		0x02

typedef struct PeerMessagesSettings
{
	struct nodeID *			snd_ssock;
	struct nodeID *			rcv_msock;
	int						source;
  	struct chunk_buffer *		chunkbuf;
	struct peerset *			peerinfo;
	int						period_msec;

	/* internal variables */
	int					threadStatus;

}PeerMessagesSettings_t, *pPeerMessagesSettings_t;

int PeerMessagesThread(void * arg);

#endif //PEER_MESSAGES_H


