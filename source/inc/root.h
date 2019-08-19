#ifndef ROOT_H
#define ROOT_H

#include "error.h"
#include "os.h"
#include "packet.h"
#include "demux.h"
#include "fileWrite.h"
#include "copyIO.h"
#include "videoDecoder.h"
#include "videoEncoder.h"
#include "audioDecoder.h"
#include "audioEncoder.h"
#include "gchunkiser.h"
#include "fec.h"
#include "fecenc.h"
#include "payload_headers.h"
#include "webInterface.h"
#include "crc32_fast.h"
#include "statsServer.h"


#define IDLE_THREAD				-1
#define RUNNING_THREAD			1
#define STOPPED_THREAD			0

#define FALSE_STATE				0
#define TRUE_STATE				1
#define DONE_STATE				3

#define MAX_VIDEO_BITRATE		120*1024*1024	// 120 Mbits peak
#define MAX_AUDIO_BITRATE		1024*1024		// 256 Kbits

#define MAX_RAW_VIDEO_FRAME	1920*1080	// 1080p HD Resolution
#define MAX_RAW_AUDIO_FRAME	1024*1024	// 

#define MAX_UDP_PACKET_SIZE	1200

#define LOG_BUFFER_SIZE 1024
#define LOG_BUFFER_SIZE2 4096

#endif
