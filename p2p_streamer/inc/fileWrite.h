#ifndef FILEWRITE_H
#define FILEWRITE_H

#include <stdlib.h>
#include "error.h"

typedef struct FileWriteSettings
{
	char 			filename[1024];	

	unsigned long		in_Q[2];
	
	/* internal variables */
	int				threadStatus;

}FileWriteSettings_t, *pFileWriteSettings_t;

int FileWriteThread(void * arg);

#endif

