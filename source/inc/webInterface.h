#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdlib.h>
#include "error.h"

typedef struct WebServerSettings
{
	char				httpPort[16];
	unsigned int 		view_handle;
	
	/* internal variables */
	int				threadStatus;
	int				pause;

}WebServerSettings_t, *pWebServerSettings_t;


int WebServerThread(void * arg);

#endif


