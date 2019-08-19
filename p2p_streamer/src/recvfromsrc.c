#include <stdio.h>
#include <stdarg.h>
#include <root.h>
#ifdef WIN32
#include <process.h>
#include <io.h>
#include <ws2tcpip.h>

#define MSG_DONTWAIT 0
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

unsigned long 		videoFrameCnt = 0;
unsigned long 		videoInputBits = 0;
unsigned long 		networkInputBits = 0;

extern unsigned long 	psmplSema4Handle;
extern unsigned long	gtransid;
extern char			local_ip[256];
extern int				local_port;
extern unsigned int 	chunkOutputCnt;

int recvBufferUDP(int fd, unsigned char * buffer, int buffer_size, unsigned char * tmpBuffer)
{
	int res, ret, len, addrlen;
  	struct sockaddr_in raddr;

	//printf("RECV++\n");
	ret = 0;
	//do 
	{
	/*
		if (buffer_size > MAX_UDP_PACKET_SIZE) 
		{
			len = MAX_UDP_PACKET_SIZE;
		} 
		else 
	*/
		{
			len = buffer_size;
		}

		while(1)
		{
			//printf("recv len: %d\n", len);
			//res = recv(fd, tmpBuffer, len, MSG_DONTWAIT);
	 		addrlen = sizeof(struct sockaddr_in);
			res = recvfrom(fd, tmpBuffer, len, MSG_DONTWAIT, &raddr, &addrlen);

			if(res == -1)
			{
				usleep(1);
			}
			else
			{
				break;
			}
		}

		//printf("\t tmpBuffer[0]: 0x%x - (%d) %d\n", tmpBuffer[0], res, ret);

		memcpy(buffer, tmpBuffer, res );
		buffer_size -= (res);
		buffer += (res );
		ret += (res );
	}// while ((tmpBuffer[0] == 0) && (buffer_size > 0));

       networkInputBits+=(ret*8);
	
	return ret;
}

int RecvFromSrcThread(void * arg)
{
	int						rval;
	pRecvFromSrcSettings_t 	pSettings = (pRecvFromSrcSettings_t ) arg;

	packet_t				outPacketBuf;
	unsigned long			outSize = 0;

	unsigned char *		dataBuffer = NULL; 
	unsigned int			dataBufferSize = 0;
	unsigned int			size;

	unsigned char *		tmpBuffer = NULL;

	int 					msg_type;
	uint16_t 				transid;
	Chunk 				grapesChunk;
	PayloadHeader_t 		payloadHeader;
//	FrameHeader_t 		frameHeader;

	int64_t				prevTimestamp = 0;
	unsigned long			realtimeclock = 0;

	unsigned int			prevAudioChunk = 0;
	
	int	 				fd = -1;
	int					result, i;
	int					ccnt = 0;

	fprintf(stderr, "+++ RecvFromSrcThread +++\n");

	pSettings->threadStatus = IDLE_THREAD;

	//Reset headers
	dataBufferSize = 0;

	//Open UDP socket
	fd = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in address;
	memset((char *) &address, 0, sizeof(address));
	address.sin_family 	= AF_INET; 
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port 	= htons(pSettings->recv_source_listen_port);
	 
	result = bind(fd, (struct sockaddr *)&address, sizeof(address));
	if(result == -1){
		fprintf(stderr, "RecvFromSrcThread: could not UDP bind to the source receive listen port: %d\n", pSettings->recv_source_listen_port);
		exit(-1);
	}

#ifdef WIN32
	DWORD timeout = 10;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
#endif

	int rcvBufSize = 4*1024*1024;
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&rcvBufSize, sizeof(rcvBufSize));
	
	//Allocate Chunk Buffer
	dataBuffer = malloc(pSettings->max_chunk_size);
	if(dataBuffer == NULL)
	{
		fprintf(stderr, "RecvFromSrcThread: could not allocate source chunk buffer size: %d\n", pSettings->max_chunk_size);
		exit(-1);
	}

	tmpBuffer = malloc(pSettings->max_chunk_size);
	if(tmpBuffer == NULL)
	{
		fprintf(stderr, "RecvFromSrcThread: could not allocate tmpBuffer chunk buffer size: %d\n", pSettings->max_chunk_size);
		exit(-1);
	}

	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;
		
		size = recvBufferUDP(fd, dataBuffer, pSettings->max_chunk_size, tmpBuffer);
		if(size == 0)
		{
			oSleep(1);
			continue;
		}

		//fprintf(stderr,"recvBufferUDP: len: %d\n", size);
		
		//Decode MSG Header
		decodeMSGHeader(&msg_type, &transid, dataBuffer);
		dataBufferSize = MSG_HEADER_SIZE;
		
		if(msg_type != MSG_TYPE_CHUNK)
		{
			fprintf(stderr, "RecvFromSrcThread: chunk MSG type 0x%x != MSG_TYPE_CHUNK\n", msg_type);
			//exit(-1);
			oSleep(1);
			continue;
		}

		//Decode GRAPES Header
		decodeChunkHeader(&grapesChunk, dataBuffer+dataBufferSize, pSettings->max_chunk_size);
		dataBufferSize += GRAPES_CHUNK_HEADER_SIZE;
		//printf("[RECV GRAPES_HDR]: id: %d size: %d\n", grapesChunk.id, grapesChunk.size);

		FECHeader_t 			fecHeader;
		decodeFECHeader(&fecHeader, dataBuffer+(MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));
		//size = grapesChunk.size+MSG_HEADER_SIZE + GRAPES_CHUNK_HEADER_SIZE;

		if(grapesChunk.size > (size-(MSG_HEADER_SIZE + GRAPES_CHUNK_HEADER_SIZE)))
		{
			fprintf(stderr, "RecvFromSrcThread: incorrect grapes chunk id: %d ts: %u size [%d,%d] received !!!\n", grapesChunk.id, grapesChunk.timestamp, grapesChunk.size, size);
			//exit(-1);
			continue;
		}

		videoFrameCnt++;
	       videoInputBits+=(size*8);
		   
		acquireSemaphore(psmplSema4Handle, -1);
		
		grapesChunk.data = malloc(size-(MSG_HEADER_SIZE + GRAPES_CHUNK_HEADER_SIZE));
		memcpy(grapesChunk.data, dataBuffer+dataBufferSize, grapesChunk.size);

		grapesChunk.attributes = NULL;

		PayloadHeader_t  chunkPayload;
		decodePayloadHeader(&chunkPayload, grapesChunk.data + FEC_HEADER_SIZE);	
		//printf("codec_type: %d\n", currPL.codec_type);

		int dupPacket = 0;
		if(chunkPayload.codec_type == AVMEDIA_TYPE_AUDIO)
		{
			if(prevAudioChunk != 0)
			{
				if(chunkPayload.frame_count == prevAudioChunk)
					dupPacket = 1;
#if 0
				else if((chunkPayload.frame_count - prevAudioChunk) > 1)
					printf("Drop chunks %d -> %d\n", prevAudioChunk, currPL.frame_count);
#endif
			}

			prevAudioChunk = chunkPayload.frame_count;
		}

		if(dupPacket == 0)
		{
			//Add chunk to chunk buffer
			cb_add_chunk(pSettings->chunkbuf, &grapesChunk);
		}

		releaseSemaphore(psmplSema4Handle);
    	}

	close(fd);

	free(tmpBuffer);
	
	free(dataBuffer);

	printf("RecvFromSrcThread Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;

}

