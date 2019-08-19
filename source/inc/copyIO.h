#ifndef COPYIO_H
#define COPYIO_H

#include <stdlib.h>
#include "error.h"

typedef struct CopyIOSettings
{
	unsigned long		in_Q[2];
	unsigned long		out_Q[2];

	/* internal variables */
	int				threadStatus;
	
}CopyIOSettings_t, *pCopyIOSettings_t;

int CopyIOThread(void * arg);

#endif

