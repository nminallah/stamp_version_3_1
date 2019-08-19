#ifndef ROOT_H
#define ROOT_H

#include "error.h"
#include "os.h"
#include "packet.h"
#include "fileWrite.h"
#include "recvfromsrc.h"
#include "peer_messages.h"
#include "peer_sampling.h"
#include "chunk_trading.h"
#include "chunk_sending.h"
#include "net_helper.h"
#include "topology.h"
#include "tman.h"
#include "trade_sig_la.h"
#include "trade_sig_ha.h"
#include "peer.h"
#include "peerset.h"
#include "chunkidset.h"

#define IDLE_THREAD				-1
#define RUNNING_THREAD			1
#define STOPPED_THREAD			0

#define MAX_VIDEO_BITRATE		120*1024*1024	// 120 Mbits peak
#define MAX_AUDIO_BITRATE		1024*1024		// 256 Kbits

#define MAX_RAW_VIDEO_FRAME	1920*1080	// 1080p HD Resolution
#define MAX_RAW_AUDIO_FRAME	1024*1024	// 

#define MAX_UDP_PACKET_SIZE	1200

#define INVALID_NETWORK_DELAY	3000

#ifdef WIN32
# define timeradd(a, b, result)                                               	\
{                                                                        				\
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;                    	\
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;            	\
    if ((result)->tv_usec >= 1000000)                                         	\
    {                                                                       				\
        ++(result)->tv_sec;                                                   		\
        (result)->tv_usec -= 1000000;                                         	\
    }                                                                       				\
}

# define timersub(a, b, result)                                               	\
{                                                                        				\
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                      	\
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                  	\
    if ((result)->tv_usec < 0) {                                              	\
      --(result)->tv_sec;                                                     		\
      (result)->tv_usec += 1000000;                                           	\
    }                                                                         			\
}

#endif

typedef struct metaDataStruct
{
	int					id;
	int 					is_source;
	int 					number_of_BFrames;	
	int 					video_layer_number;	
	char 				ip[256];
	int					port;
}metaDataStruct_t, *pmetaDataStruct_t;

#endif

