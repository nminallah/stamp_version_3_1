#ifndef PEER_SAMPLING_H
#define PEER_SAMPLING_H

#include <stdlib.h>
#include <net_helper.h>
#include <payload_headers.h>

typedef struct PeerSamplingSettings
{
	struct nodeID *			snd_ssock;
	int						source;
	struct peerset *			peerinfo;
	int						period_msec;

	/* internal variables */
	int					threadStatus;

}PeerSamplingSettings_t, *pPeerSamplingSettings_t;

int PeerSamplingThread(void * arg);

#endif //PEER_SAMPLING_H


