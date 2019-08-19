#include <stdio.h>
#include <stdarg.h>
#include <root.h>

int FileWriteThread(void * arg)
{
	int					rval;
	pFileWriteSettings_t 	pSettings = (pFileWriteSettings_t ) arg;

	FILE *				fptr;
	
	packet_t				packetBuf;
	int					Size = 0;

	int					i;

	fprintf(stderr, "+++ FileWriteThread +++\n");

	pSettings->threadStatus = STOPPED_THREAD;

	fptr = fopen(pSettings->filename,"wb");
	if(fptr == NULL)
	{
		fprintf(stderr, "FileWriteThread: Video file %s writing open error\n", pSettings->filename);
		exit(-1);
	}		

	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;
		
		rval = receiveQueue(pSettings->in_Q[LIVE_QUEUE], &packetBuf, sizeof(packet_t), &Size, -1);
		if(rval)
		{
			if(rval != TIMEOUT_ERROR)
			{
				fprintf(stderr, "FileWriteThread: Receive error !!!\n");
				exit(-1);
			}
		}

		if(packetBuf.lastPacket == 1)
		{
			sendQueue(pSettings->in_Q[IDLE_QUEUE], &packetBuf, Size);
			
			break;
		}

		for(i=0; i < packetBuf.num_buffers; i++)
		{
			//printf("Write packet (%d) size %d\n", i, packetBuf.data[i]->packet_size); 
			fwrite(packetBuf.data[i]->packet, packetBuf.data[i]->packet_size, 1, fptr);
		}

		sendQueue(pSettings->in_Q[IDLE_QUEUE], &packetBuf, Size);
    	}

	fclose(fptr);

	printf("File write Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;

}


