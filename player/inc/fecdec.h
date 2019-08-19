#ifndef FECDEC_H
#define FECDEC_H

#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <chunk.h>
#include <payload_headers.h>
#include "error.h"

typedef struct FECDecSettings
{
	unsigned int		max_chunk_size;
	unsigned int		initial_crc;
	int				k;
	int				n;
	
	unsigned int 		player_listen_port;

	unsigned long		out_Q[2];
	
	/* internal variables */
	int				threadStatus;
	int				exit;
	int				pause;
	
}FECDecSettings_t, *pFECDecSettings_t;

int FECDecThread(void * arg);

#endif


