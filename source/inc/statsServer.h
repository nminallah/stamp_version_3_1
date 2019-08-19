#ifndef STATS_SERVER_H
#define STATS_SERVER_H

#include <stdlib.h>
#include <libavformat/avformat.h>

#include "error.h"

typedef struct StatsServerSettings
{
	char 			statsServerLocalIP[1024];	
	int				statsServerListenPort;

	/* internal variables */
	int				threadStatus;
}StatsServerSettings_t, *pStatsServerSettings_t;

int StatsServerThread(void * arg);

#endif

