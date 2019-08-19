#include <stdio.h>
#include <stdarg.h>
#include <root.h>
#ifdef WIN32
#include <process.h>
#include <io.h>
#include <ws2tcpip.h>
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

unsigned long 		networkOutputBits = 0;
unsigned long 		chunkOutputCnt = 0;

unsigned int		chunkID = 1;

static int			curr_drop_packet = 0;

extern int			fec_enable;
extern int *		drop_packet_array;
extern int			drop_packet_size;

extern unsigned long sema4;

unsigned long			prevTS = 0;

int sendFrameBufferUDP(pGChunkiserSettings_t 	pSettings, packet_t * packet, unsigned char * chunkBuffer, unsigned int max_chunk_size)
{
	int ret;

	Chunk 				grapesChunk;
	FECHeader_t			fecHeader;
	PayloadHeader_t		payloadHeader;
	packet_t				vpacketBuf;
	unsigned long			vSize = 0;

	unsigned int			total_pkts;
	unsigned int			frame_count;
	unsigned int			header_size;
	unsigned int			frame_size;
	unsigned char *		frame_ptr = NULL;
	unsigned int			frame_len;
	unsigned int			pkt_data_len;
	unsigned int			pkt_len;
	unsigned int			pkt_cnt;
	
	struct timeval 		now;
	int					len;

	header_size		= MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE;
	frame_size		= packet->data[0]->packet_size;
	frame_ptr		= packet->data[0]->packet;
	frame_len		= 0;
	pkt_data_len		= 0;
	pkt_len			= 0;

	frame_count		= 0;
	total_pkts 		= (frame_size /(MAX_UDP_PACKET_SIZE-header_size)) + ((frame_size % (MAX_UDP_PACKET_SIZE-header_size)) != 0);
	pkt_cnt			= 0;
	
	//printf("SEND: ChunkId: %d frame_size: %d\n", chunkID, frame_size);
	do 
	{
		if (frame_size > (MAX_UDP_PACKET_SIZE-header_size)) 
		{
			pkt_data_len 	= (MAX_UDP_PACKET_SIZE-header_size);
			pkt_len 		= (MAX_UDP_PACKET_SIZE);
		} 
		else 
		{
			pkt_data_len 	= frame_size;
			pkt_len 		= frame_size+header_size;
		}

		//printf("\t pkt_len: %d pkt_data_len: %d\n", pkt_len, pkt_data_len);

		// header copy
		gettimeofday(&now, NULL);
		
		/* set MSG Type */
		encodeMSGHeader(MSG_TYPE_CHUNK, 0, chunkBuffer);
		//printf("\n[MSG_HDR]: type: %d\n", MSG_TYPE_CHUNK);

		/* encode the GRAPES chunk into network bytes */
		//fill grapes		
		grapesChunk.id 					= chunkID;
		grapesChunk.timestamp 			= now.tv_sec * 1000000ULL + now.tv_usec;
		grapesChunk.size 					= pkt_len - (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE);
		grapesChunk.attributes 				= NULL;
		grapesChunk.attributes_size 			= 0;
		encodeChunkHeader(&grapesChunk, chunkBuffer + MSG_HEADER_SIZE, max_chunk_size);
		//printf("[GRAPES_HDR]: id: %d size: %d\n", grapesChunk.id, grapesChunk.size);
		
		/* encode the payload header into network bytes */
		payloadHeader.codec_type 			= packet->codecType;
		payloadHeader.codec_id 			= packet->codecID;
		payloadHeader.flags 				= packet->flags;			
		payloadHeader.frame_count			= packet->frame_count;
		if(total_pkts == 1)
			payloadHeader.marker 			= 0x3; //Start-Stop
		else if(frame_count == (total_pkts-1))
			payloadHeader.marker 			= 0x1; //Stop
		else if(frame_count == 0)
			payloadHeader.marker 			= 0x2; //Start
		else
			payloadHeader.marker 			= 0x0; //Continuation
		
		payloadHeader.timestamp_90KHz		= packet->timestamp;
		if(packet->codecType == AVMEDIA_TYPE_AUDIO)
		{
			payloadHeader.meta_data[0]	= (unsigned short)(packet->channels);
			payloadHeader.meta_data[1]	= (unsigned short)(packet->sampleRate);
		}
		else
		{
			payloadHeader.meta_data[0]	= (unsigned short)(packet->data[0]->width);
			payloadHeader.meta_data[1]	= (unsigned short)(packet->data[0]->height);
		}			
		encodePayloadHeader(&payloadHeader, chunkBuffer + (MSG_HEADER_SIZE + GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE));	
		//printf("[PAYLOAD_HDR]: codec_id: %d codec_type: %d flags: %d marker: %d meta[0]: %d meta[1]: %d\n", payloadHeader.codec_id, payloadHeader.codec_type, payloadHeader.flags, payloadHeader.marker, payloadHeader.meta_data[0], payloadHeader.meta_data[1]);

		// data copy
		memcpy(chunkBuffer+header_size, frame_ptr, pkt_data_len);
		frame_size 	-= pkt_data_len;
		frame_ptr 	+= pkt_data_len;
		
		//printf("\t frame: %d size: %d marker: %d\n", frame_count, pkt_len, payloadHeader.marker);

	      	networkOutputBits	+= (pkt_len*8);
		chunkOutputCnt++;

		if((pSettings->out_Q[IDLE_QUEUE] != NULL) && (pSettings->out_Q[LIVE_QUEUE] != NULL))
		{
			receiveQueue(pSettings->out_Q[IDLE_QUEUE], &vpacketBuf, sizeof(packet_t), &vSize, -1);

			memcpy(vpacketBuf.data[0]->packet, chunkBuffer, pkt_len);
			vpacketBuf.data[0]->packet_size = pkt_len;
			
			sendQueue(pSettings->out_Q[LIVE_QUEUE], &vpacketBuf, vSize);
		}			
		else
		{
			int pass_packet = 1;
			
			if(fec_enable == 0)
			{
				if(drop_packet_array[curr_drop_packet] == 0)
						pass_packet = 0;
			}

			//printf("curr_drop_packet: %d drop_packet_array: %d pass_packet: %d\n", curr_drop_packet, drop_packet_array[curr_drop_packet], pass_packet);

			if(pass_packet == 1)
			{
				ret = sendto(pSettings->socketHandle, chunkBuffer, pkt_len, 0, pSettings->servaddr, sizeof(struct sockaddr_in));
				if(payloadHeader.codec_type == AVMEDIA_TYPE_AUDIO)
				{					
					// Duplicate audio packets for local host connection with p2p streamer
					ret = sendto(pSettings->socketHandle, chunkBuffer, pkt_len, 0, pSettings->servaddr, sizeof(struct sockaddr_in));
				}
			}

			curr_drop_packet++;
			if(curr_drop_packet >= drop_packet_size)
				curr_drop_packet = 0;
			
			//printf("[DATA]: len: %d + %d -> %d MARKER: %d\n", header_size, pkt_data_len, pkt_len, payloadHeader.marker);

			//usleep(1);
		}

		frame_count++;

		chunkID++;

	} while (frame_size > 0);

}

int GChunkiserThread(void * arg)
{
	int					rval;
	pGChunkiserSettings_t 	pSettings = (pGChunkiserSettings_t ) arg;

	FILE *				fptr;
	FILE *				fptr2;
	FILE *				fptr3;
	FILE *				fptr4;
	
	packet_t				inPacketBuf;
	unsigned long			inSize = 0;

	unsigned char *		chunkBuffer = NULL; 
	//unsigned int			chunkBufferDataSize = 0;

	//unsigned char *		tmpBuffer = NULL; 

	Chunk 				grapesChunk;
	FECHeader_t			fecHeader;
	PayloadHeader_t		payloadHeader;

	unsigned int			total_pkts;
	unsigned int			frame_count;
	unsigned int			header_size;
	unsigned int			frame_size;
	unsigned char *		frame_ptr = NULL;
	unsigned int			frame_len;
	unsigned int			pkt_data_len;
	unsigned int			pkt_len;
	unsigned int			pkt_cnt;
	
	struct timeval 		now;
	int					len;

	//int	 				fd = -1;
	//int					result, i;

	fprintf(stderr, "+++ GChunkiserThread +++\n");

	/*
	//Open UDP socket
	fd = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in address;
	address.sin_family 	= AF_INET; 
	address.sin_addr.s_addr = inet_addr(pSettings->streamer_ip);
	address.sin_port 	= htons(pSettings->streamer_port);
	 
	result = connect(fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in));
	if(result == -1){
		fprintf(stderr, "GChunkiserThread: could not UDP connect to the streamer: ip: %s port: %d\n", pSettings->streamer_ip, pSettings->streamer_port);
		exit(-1);
	}
	*/

	//Allocate Chunk Buffer
	chunkBuffer = malloc(pSettings->max_chunk_size);
	if(chunkBuffer == NULL)
	{
		fprintf(stderr, "GChunkiserThread: could not allocate chunk buffer size: %d\n", pSettings->max_chunk_size);
		exit(-1);
	}

#if 0
	tmpBuffer = malloc(pSettings->max_chunk_size);
	if(tmpBuffer == NULL)
	{
		fprintf(stderr, "GChunkiserThread: could not allocate tmpBuffer chunk buffer size: %d\n", pSettings->max_chunk_size);
		exit(-1);
	}
#endif

	pSettings->threadStatus = STOPPED_THREAD;

	/*
	fptr 		= fopen("1.h264","wb");
	fptr2 	= fopen("2.h264","wb");
	fptr3 	= fopen("3.h264","wb");
	fptr4 	= fopen("4.h264","wb");
	*/
	
	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;
		
		rval = receiveQueue(pSettings->in_Q[LIVE_QUEUE], &inPacketBuf, sizeof(packet_t), &inSize, -1);
		if(rval)
		{
			if(rval != TIMEOUT_ERROR)
			{
				fprintf(stderr, "GChunkiserThread: Receive error !!!\n");
				exit(-1);
			}
		}

		acquireSemaphore(sema4, -1);

		//if(pSettings->type == AVMEDIA_TYPE_VIDEO)
			//fwrite(inPacketBuf.data[0]->packet, inPacketBuf.data[0]->packet_size, 1, fptr);

		//printf("GCHUNK: IN PACKET: CodecType: %d size: %d\n", inPacketBuf.codecType, inPacketBuf.data[0]->packet_size);

		if(inPacketBuf.lastPacket == 1)
		{
#if 0
			if(chunkBufferDataSize != 0)
			{
				gettimeofday(&now, NULL);
				grapesChunk.timestamp = now.tv_sec * 1000000ULL + now.tv_usec;
			
				/* encode the GRAPES chunk into network bytes */
				encodeChunkHeader(&grapesChunk, chunkBuffer, pSettings->max_chunk_size);

				/* send chunk data on the network */
				sendBufferUDP(pSettings->socketHandle, chunkBuffer, chunkBufferDataSize, tmpBuffer, pSettings->socketHandle);

				chunkBufferDataSize = 0;
			}
#endif
		
			inPacketBuf.lastPacket = 0;
			sendQueue(pSettings->in_Q[IDLE_QUEUE], &inPacketBuf, inSize);

			releaseSemaphore(sema4);
			
			break;
		}

		/* send chunk data on the network */
		sendFrameBufferUDP(pSettings, &inPacketBuf, chunkBuffer, pSettings->max_chunk_size);
	
		releaseSemaphore(sema4);

		sendQueue(pSettings->in_Q[IDLE_QUEUE], &inPacketBuf, inSize);

    	}

	//close(fd);
	if((pSettings->out_Q[IDLE_QUEUE] != NULL) && (pSettings->out_Q[LIVE_QUEUE] != NULL))
	{
		packet_t				vpacketBuf;
		unsigned long			vSize = 0;

		rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &vpacketBuf, sizeof(packet_t), &vSize, -1);
		if(rval == 0)
		{
			vpacketBuf.lastPacket = 1;
			sendQueue(pSettings->out_Q[LIVE_QUEUE], &vpacketBuf, vSize);
		}
	}

	//free(tmpBuffer);
	free(chunkBuffer);

	printf("GChunkiser Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;

}

