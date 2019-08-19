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

static int			curr_drop_packet = 0;

extern int			fec_enable;
extern int *		drop_packet_array;
extern int			drop_packet_size;

unsigned int		fecChunkID = 1;

int FECEncThread(void * arg)
{
	int					rval;
	pFECEncSettings_t 		pSettings = (pFECEncSettings_t) arg;

	packet_t				inPacketBuf;
	unsigned long			inSize = 0;

	unsigned char *		chunkBuffer = NULL; 
	unsigned int			chunkBufferDataSize = 0;

	struct timeval 		now;
	int					len;

	int 					n = pSettings->n;//16;
	int 					k = pSettings->k;;//8;

       void * 				enccode = fec_new(k, n);

	char **				src = NULL;
	char **				pkt = NULL;  
	int 					src_index = 0;
	int 					i = 0;

	int					lastPacketCnt = 0;

	unsigned long			startTime = 0;
	
	fprintf(stderr, "+++ FECEncThread +++\n");

	pSettings->threadStatus = STOPPED_THREAD;

	//Allocate Chunk Buffer
	chunkBuffer = malloc(pSettings->max_chunk_size);
	if(chunkBuffer == NULL)
	{
		fprintf(stderr, "FECEncThread: could not allocate chunk buffer size: %d\n", pSettings->max_chunk_size);
		exit(-1);
	}

	src = ( char ** ) malloc ( n * sizeof ( char* ));
	pkt = ( char ** ) malloc ( n * sizeof ( char* ));
	for(i=0; i < n; i++){
		src[i] = ( char* ) malloc( (MAX_UDP_PACKET_SIZE+MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE)* sizeof ( char ) );
		pkt[i] = ( char* ) malloc( (MAX_UDP_PACKET_SIZE+MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE)* sizeof ( char ) ); 
	}

	/* read frames from the file */
	while (1) 
	{
		uint32_t 				crc = 0;	
		FECHeader_t 			fecHeader;
		Chunk 				grapesHeader;
		PayloadHeader_t		payloadHeader;
	
		pSettings->threadStatus = RUNNING_THREAD;
		
		//printf("FECEnc: receiveQueue++\n");
		rval = receiveQueue(pSettings->in_Q[LIVE_QUEUE], &inPacketBuf, sizeof(packet_t), &inSize, -1);
		if(rval)
		{
			if(rval != TIMEOUT_ERROR)
			{
				fprintf(stderr, "FECEncThread: Receive error !!!\n");
				exit(-1);
			}
		}
		//printf("FECEnc: src_index: %d receiveQueue--\n", src_index);
		if(inPacketBuf.lastPacket == 1)
		{		
			printf("FEC last Packet\n");
			lastPacketCnt++;
			inPacketBuf.lastPacket = 0;
			sendQueue(pSettings->in_Q[IDLE_QUEUE], &inPacketBuf, inSize);

			if(lastPacketCnt < 2)
				continue;
			else
				break;
		}

		//if(src_index == 0)
		//	startTime = timer_msec();
		
		memset(chunkBuffer, 0, MAX_UDP_PACKET_SIZE);
		
		memcpy(chunkBuffer, inPacketBuf.data[0]->packet, inPacketBuf.data[0]->packet_size);
		chunkBufferDataSize = MAX_UDP_PACKET_SIZE;//inPacketBuf.data[0]->packet_size;

		// Computer CRC
		decodeChunkHeader(&grapesHeader, chunkBuffer + MSG_HEADER_SIZE, MAX_UDP_PACKET_SIZE);

		gettimeofday(&now, NULL);
		unsigned int 			grapesChunkid = grapesHeader.id;
		
		grapesHeader.id 					= fecChunkID;
		grapesHeader.timestamp 			= now.tv_sec * 1000000ULL + now.tv_usec;
		encodeChunkHeader(&grapesHeader, chunkBuffer+ MSG_HEADER_SIZE, MAX_UDP_PACKET_SIZE);
		
		fecHeader.grapes_id				= grapesChunkid;
		fecHeader.sequence_number 		= src_index;
		fecHeader.reserved1				= 0;
		fecHeader.crc_checksum 			= 0;
		encodeFECHeader(&fecHeader, chunkBuffer + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));

		// Fill CRC
		crc = pSettings->initial_crc;
		crc32_fast(chunkBuffer, chunkBufferDataSize, &crc);
		
		fecHeader.crc_checksum 			= crc;
		encodeFECHeader(&fecHeader, chunkBuffer + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));
		
		memcpy(src[src_index], chunkBuffer, chunkBufferDataSize);
		src_index++;

		//printf("FEC: %d grapesChunk.id: %d\n", fecHeader.sequence_number, fecHeader.grapes_id);

		//printf("FEC_Enc: src_index: %d outSize: %d inSize: %d grapes: %d\n", src_index, chunkBufferDataSize, inPacketBuf.data[0]->packet_size, grapesHeader.size);

		fecChunkID++;

		int pass_packet = 1;
		
		if(fec_enable == 1)
		{
			if(drop_packet_array[curr_drop_packet] == 0)
					pass_packet = 0;
		}

		//printf("curr_drop_packet: %d drop_packet_array: %d pass_packet: %d drop_packet_size: %d\n", curr_drop_packet, drop_packet_array[curr_drop_packet], pass_packet, drop_packet_size);

		if(pass_packet == 1)
		{
			sendto(pSettings->socketHandle, chunkBuffer, chunkBufferDataSize, 0, pSettings->servaddr, sizeof(struct sockaddr_in));
		}

		curr_drop_packet++;
		if(curr_drop_packet >= drop_packet_size)
			curr_drop_packet = 0;


		sendQueue(pSettings->in_Q[IDLE_QUEUE], &inPacketBuf, inSize);

		//fprintf(stderr,"src_index: %d\n", src_index);
		//2 FEC Data
		if(src_index >= k)
		{
			//fprintf(stderr,"fec_encoder: %d\n", n);
			for(i=0; i < n; i++)
			{	
				//fprintf(stderr,"   fec_encoder: i %d\n", i);
				fec_encode(enccode, src, pkt[i], i, MAX_UDP_PACKET_SIZE) ;
			}

			for(i=k; i < n; i++)
			{
				//fprintf(stderr,"   fec_encoder: Queue %d\n", i);			
				memset(chunkBuffer, 0, MSG_HEADER_SIZE+ GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE+MAX_UDP_PACKET_SIZE);

				char * 	buffPtr = chunkBuffer;
				Chunk 	grapesChunk;
				
				// header copy
				gettimeofday(&now, NULL);
				
				/* set MSG Type */
				encodeMSGHeader(MSG_TYPE_CHUNK, 0, buffPtr);
				//printf("\n[MSG_HDR]: type: %d\n", MSG_TYPE_CHUNK);

				/* encode the GRAPES chunk into network bytes */
				//fill grapes		
				grapesChunk.id 					= fecChunkID;
				grapesChunk.timestamp 			= now.tv_sec * 1000000ULL + now.tv_usec;
				grapesChunk.size 					= MAX_UDP_PACKET_SIZE + FEC_HEADER_SIZE + PAYLOAD_HEADER_SIZE;
				grapesChunk.attributes 				= NULL;
				grapesChunk.attributes_size 			= 0;
				encodeChunkHeader(&grapesChunk, buffPtr + MSG_HEADER_SIZE, MSG_HEADER_SIZE+ GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE+MAX_UDP_PACKET_SIZE);
				//printf("[GRAPES_HDR]: id: %d size: %d\n", grapesChunk.id, grapesChunk.size);

				fecHeader.grapes_id				= 0;
				fecHeader.sequence_number 		= i;
				fecHeader.reserved1		 		= 0;
				fecHeader.crc_checksum 			= 0;
				encodeFECHeader(&fecHeader, buffPtr + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));
				//printf("[FEC_HDR]: id: %d\n", fecHeader.sequence_number);

				payloadHeader.codec_type 			= 0;
				payloadHeader.codec_id 			= 0;
				payloadHeader.flags 				= 0;			
				payloadHeader.frame_count			= 0;
				payloadHeader.marker 				= 0;
				payloadHeader.timestamp_90KHz		= 0;
				payloadHeader.meta_data[0]		= 0;
				payloadHeader.meta_data[1]		= 0;
				encodePayloadHeader(&payloadHeader, buffPtr + (MSG_HEADER_SIZE + GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE));	

				memcpy(buffPtr + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE),pkt[i], MAX_UDP_PACKET_SIZE);

				// Fill CRC
				crc = pSettings->initial_crc;
				chunkBufferDataSize= MSG_HEADER_SIZE+ GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE+MAX_UDP_PACKET_SIZE;

				crc32_fast(chunkBuffer, chunkBufferDataSize, &crc);
				
				//fprintf(stderr, "CRC: %u\n", crc);
				fecHeader.crc_checksum 			= crc;
				encodeFECHeader(&fecHeader, buffPtr + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));


				fecChunkID++;
				//printf("\t FEC_Enc: seq: %d outSize: %d grapes: %d\n", i, outPacketBuf.data[0]->packet_size, grapesChunk.size);
				//printf("FEC: %d grapesChunk.id: %d\n", fecHeader.sequence_number, fecHeader.grapes_id);

				int pass_packet = 1;
				
				if(fec_enable == 1)
				{
					if(drop_packet_array[curr_drop_packet] == 0)
							pass_packet = 0;
				}

				//printf("curr_drop_packet: %d drop_packet_array: %d pass_packet: %d drop_packet_size: %d\n", curr_drop_packet, drop_packet_array[curr_drop_packet], pass_packet, drop_packet_size);

				if(pass_packet == 1)
				{
					sendto(pSettings->socketHandle, chunkBuffer, chunkBufferDataSize, 0, pSettings->servaddr, sizeof(struct sockaddr_in));
				}

				curr_drop_packet++;
				if(curr_drop_packet >= drop_packet_size)
					curr_drop_packet = 0;

				//usleep(1);
			}
			
			for(i=0; i < n; i++)
			{
				memset(src[i], 0, MAX_UDP_PACKET_SIZE);
				memset(pkt[i], 0, MAX_UDP_PACKET_SIZE);
			}
			
			src_index = 0;

			//printf("FECEnc: time: %d ms\n",  timer_msec() - startTime);

		}

   	}

	//close(fd);

	//free(tmpBuffer);

	for(i=0; i < n; i++){
		free(src[i]);
		free(pkt[i]); 
	}
	free(src);
	free(pkt);

	free(chunkBuffer);

	fec_free(enccode);

	printf("FECEncThread Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;

}

