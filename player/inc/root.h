#ifndef ROOT_H
#define ROOT_H

#include "error.h"
#include "os.h"
#include "packet.h"
#include "fileWrite.h"
#include "gdechunkiser.h"
#include "fec.h"
#include "fecdec.h"
#include "gui.h"
#include "videoDecoderPlayer.h"
#include "audioDecoderPlayer.h"
#include "params.h"
#include "MuxPlayer.h"
#include "crc32_fast.h"

#define IDLE_THREAD				-1
#define RUNNING_THREAD			1
#define STOPPED_THREAD			0

#define FALSE_STATE				0
#define TRUE_STATE				1
#define DONE_STATE				3

#define MAX_VIDEO_BITRATE		120*1024*1024	// 120 Mbits peak
#define MAX_AUDIO_BITRATE		1024*1024		// 512 Kbits

#define MAX_RAW_VIDEO_FRAME	1920*1080	// 1080p HD Resolution
#define MAX_RAW_AUDIO_FRAME	1024*1024	// 

#define MAX_UDP_PACKET_SIZE	1200
#endif
