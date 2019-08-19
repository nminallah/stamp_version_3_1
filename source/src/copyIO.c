#include <stdio.h>
#include <stdarg.h>
#include <root.h>

int CopyIOThread(void * arg)
{
	int					rval;
	pCopyIOSettings_t 	pSettings = (pCopyIOSettings_t ) arg;
	
	packet_t				InpacketBuf;
	packet_t				OutpacketBuf;
	int					InSize = 0;
	int					OutSize = 0;

	int					i;

	fprintf(stderr, "+++ CopyIOThread +++\n");

	pSettings->threadStatus = IDLE_THREAD;
	
	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;

		/* Recieve packet from Live Queue */
		
		rval = receiveQueue(pSettings->in_Q[LIVE_QUEUE], &InpacketBuf, sizeof(packet_t), &InSize, -1);
		if(rval)
		{
			if(rval != TIMEOUT_ERROR)
			{
				fprintf(stderr, "CopyIOThread: Receive error !!!\n");
				exit(-1);
			}
		}

		/* Recieve packet from Idle Queue */
		
		rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &OutpacketBuf, sizeof(packet_t), &OutSize, -1);
		if(rval)
		{
			if(rval != TIMEOUT_ERROR)
			{
				fprintf(stderr, "CopyIOThread: Receive error !!!\n");
				exit(-1);
			}
		}

		/* Copy packet from In to Out */
		
		for(i=0; i < InpacketBuf.num_buffers; i++)
		{
			if(InpacketBuf.data[i]->packet_size > OutpacketBuf.data[i]->buf_size)
			{
				fprintf(stderr, "Error ... CopyIO data (%d) exceeds maximum buffer limit (%d) \n", InpacketBuf.data[i]->packet_size, OutpacketBuf.data[i]->buf_size);
				exit(1);
			}
			
			OutpacketBuf.timestamp = InpacketBuf.timestamp;
			OutpacketBuf.codecID = InpacketBuf.codecID;
			OutpacketBuf.pix_fmt = InpacketBuf.pix_fmt;
			OutpacketBuf.sampleRate = InpacketBuf.sampleRate;
			OutpacketBuf.channels = InpacketBuf.channels;
			memcpy(OutpacketBuf.data[i]->packet, InpacketBuf.data[i]->packet, InpacketBuf.data[i]->packet_size);
			OutpacketBuf.data[i]->packet_size = InpacketBuf.data[i]->packet_size;
		}

		OutpacketBuf.lastPacket = InpacketBuf.lastPacket;

		/* Send packet to Live Queue*/
		
		sendQueue(pSettings->out_Q[LIVE_QUEUE], &OutpacketBuf, OutSize);

		/* Send packet to Idle Queue*/
		
		sendQueue(pSettings->in_Q[IDLE_QUEUE], &InpacketBuf, InSize);
    	}

	printf("CopyIOThread Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;
}


