#ifndef FECENC_H
#define FECENC_H

#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <chunk.h>
#include <payload_headers.h>
#include "error.h"

typedef struct FECEncettings
{
	unsigned int		max_chunk_size;
	unsigned int		initial_crc;
	int				k;
	int				n;
	
	unsigned long		in_Q[2];
	int				socketHandle;

	struct sockaddr_in    * servaddr;
	
	/* internal variables */
	int				threadStatus;

}FECEncSettings_t, *pFECEncSettings_t;

int FECEncThread(void * arg);

#endif


