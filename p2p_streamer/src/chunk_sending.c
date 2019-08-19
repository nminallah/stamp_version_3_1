#include <stdio.h>
#include <stdarg.h>
#include <root.h>
#include <peer_messages.h>
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

extern unsigned long 		psmplSema4Handle;
extern int					loopback_disable;
extern unsigned int			lastChunkID;
extern unsigned int			start_up_latency_ms;

int ChunkSendingThread(void * arg)
{
	int						rval;
	pChunkSendingSettings_t 	pSettings = (pChunkSendingSettings_t ) arg;

	unsigned char *			encodeBuffer = NULL; 
	unsigned int				encodeBufferDataSize = 0;

	unsigned char *			tmpBuffer = NULL;

	int	 					fd = -1;
	struct sockaddr_in 		address;

	unsigned int				prevAudioChunk = 0;
	int						sndBufSize;

	fprintf(stderr, "+++ ChunkSendingThread +++\n");

	pSettings->threadStatus = IDLE_THREAD;


	//Open UDP socket
	fd = socket(AF_INET, SOCK_DGRAM, 0);

#if 1
	char laddress[256] = {0};

	if(loopback_disable == 0)
		strcpy(laddress, "127.0.0.1");
	else
		strcpy(laddress, get_external_ip());

	memset(&address, 0, sizeof(address));
	 
	// Filling server information
	address.sin_family 	= AF_INET;
	address.sin_port 	= htons(pSettings->player_port);
	if(inet_pton(AF_INET, laddress, &address.sin_addr)<=0)
	{
		printf("\n inet_pton error occured\n");
		return 1;
	}
#else
	address.sin_family 	= AF_INET; 
#ifdef WIN32
	address.sin_addr.S_un.S_addr 	= inet_addr("127.0.0.1");
#else
	address.sin_addr.s_addr 		= htonl(INADDR_ANY);
#endif
	address.sin_port 	= htons(pSettings->player_port);
	 
	result = connect(fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in));
	if(result == -1){
		fprintf(stderr, "ChunkSendingThread: could not UDP connect to the player: ip: %s port: %d\n", "127.0.0.1", pSettings->player_port);
		exit(-1);
	}
#endif

	sndBufSize 	= 4*1024*1024;
	setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&sndBufSize, sizeof(sndBufSize));		

	//Allocate Chunk Buffer
	encodeBuffer = malloc(pSettings->max_chunk_size);
	if(encodeBuffer == NULL)
	{
		fprintf(stderr, "ChunkSendingThread: could not allocate chunk buffer size: %d\n", pSettings->max_chunk_size);
		exit(-1);
	}

	tmpBuffer = malloc(pSettings->max_chunk_size);
	if(tmpBuffer == NULL)
	{
		fprintf(stderr, "ChunkSendingThread: could not allocate tmpBuffer chunk buffer size: %d\n", pSettings->max_chunk_size);
		exit(-1);
	}

	/* read frames from the file */
	while (1) 
	{
		struct chunkID_set *	cb_bmap;
		struct chunk *		chunks;
		int 					chunk_size, j;
		int 					expel_chunk = 0;
		int 					new_chunk_size = 0;

		PayloadHeader_t  		payloadHeader;

		acquireSemaphore(psmplSema4Handle, -1);

		struct timeval 		now;
		gettimeofday(&now, NULL);

		uint64_t 				currTimestamp = now.tv_sec * 1000000ULL + now.tv_usec;

		// local chunkbuffer to buffer map
		cb_bmap	= chunkID_set_init("type=bitmap");
		chunks 	= cb_get_chunks(pSettings->chunkbuf, &chunk_size);
		for(j=0; j < chunk_size; j++) 
		{
			chunkID_set_add_chunk(cb_bmap, chunks[j].id);
		}							

		//printf("chunk_size: %d\n", chunk_size);
		
		if(chunk_size > 0)
		{
			uint64_t initial_timestamp = 0;
			
			for(j=0; j < chunk_size; j++) 
			{
				decodePayloadHeader(&payloadHeader, chunks[j].data + FEC_HEADER_SIZE);

				if(initial_timestamp == 0)
				{
					if(payloadHeader.codec_type == AVMEDIA_TYPE_AUDIO)
					{
						initial_timestamp = chunks[j].timestamp;
					}
				}
				else
				{
					if(payloadHeader.codec_type == AVMEDIA_TYPE_AUDIO)
					{
						unsigned int diffTS = (unsigned int)(chunks[j].timestamp - initial_timestamp);
						//printf("\t %d. ts: %u ms\n", j, diffTS);

						if(diffTS > (pSettings->min_chunk_delay_ms*1000))
						{
							expel_chunk 		= 1;
							new_chunk_size 	= j;
							
							break;
						}
					}
				}
			}
		}

		//if(expel_chunk == 1)
		//	printf("expel: %d. chunk_size: %d\n", expel_chunk, new_chunk_size);

		if(expel_chunk)
		{
			while(1)
			{
				struct chunk * 	chunkStreamer 	= cb_get_chunk(pSettings->chunkbuf, chunkID_set_get_earliest(cb_bmap));
				int 				audioPacket 	= 0;

				memset(encodeBuffer, 0, MSG_HEADER_SIZE+ GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE+MAX_UDP_PACKET_SIZE);
	/*			
				if((MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+ck.size) > pSettings->max_chunk_size)
				{
					fprintf(stderr, "Error ... ChunkSendingThread: packet data (%d) exceeds maximum buffer limit (%d) \n", (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+ck.size), pSettings->max_chunk_size);
					exit(1);
				}
	*/
				/* set MSG Type */
				encodeMSGHeader(MSG_TYPE_CHUNK, 0, encodeBuffer);
				encodeBufferDataSize = MSG_HEADER_SIZE;

				/* encode the GRAPES chunk into network bytes */
				encodeChunkHeader(chunkStreamer, encodeBuffer + encodeBufferDataSize, pSettings->max_chunk_size);
				encodeBufferDataSize += GRAPES_CHUNK_HEADER_SIZE;

				memcpy(encodeBuffer + encodeBufferDataSize, chunkStreamer->data, chunkStreamer->size);
				encodeBufferDataSize += chunkStreamer->size;

				FECHeader_t 			fecHeader;
				decodeFECHeader(&fecHeader, encodeBuffer+(MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));

				//printf("P2P: FEC: %d grapesChunk.id: %d\n", fecHeader.sequence_number, fecHeader.grapes_id);

#if 1
				if(start_up_latency_ms == 0)
					start_up_latency_ms =  (chunks[new_chunk_size-1].timestamp - chunks[0].timestamp)/1000;
#else
				struct timeval 		now;
				gettimeofday(&now, NULL);
				uint64_t currTimestamp = now.tv_sec * 1000000ULL + now.tv_usec;
				//printf("Playback delay: %u ms\n", (currTimestamp - chunkStreamer->timestamp)/1000);

				if(start_up_latency_ms == 0)
					start_up_latency_ms =  (chunks[pSettings->min_chunk_delay-1].timestamp - chunks[0].timestamp)/1000;

				playback_delay_ms =  (currTimestamp - chunkStreamer->timestamp)/1000;
				
				if(prevChunkID != -1)
				{
					if((prevChunkID+1) != chunkStreamer->id)
					{
						chunk_miss_ratio += (chunkStreamer->id - prevChunkID - 1);											
					}
				}
				max_chunk_id = chunkStreamer->id;
				prevChunkID = chunkStreamer->id;
#endif

				audioPacket = 0;
				decodePayloadHeader(&payloadHeader, chunkStreamer->data + FEC_HEADER_SIZE);
				if(payloadHeader.codec_type == AVMEDIA_TYPE_AUDIO)
					audioPacket = 1;
				
				//sendBufferUDP(pSettings->socketHandle, encodeBuffer, encodeBufferDataSize, tmpBuffer, pSettings->servaddr);
				//printf("\t Send id: %u\n", chunkStreamer->id);
				sendBufferUDP(fd, encodeBuffer, encodeBufferDataSize, tmpBuffer, &address);
				if(audioPacket)
				{
					// Duplicate audio packets for local host connection with p2p streamer
					sendBufferUDP(fd, encodeBuffer, encodeBufferDataSize, tmpBuffer, &address);
				}

				lastChunkID = chunkStreamer->id;

				//fprintf(stderr,"ChunkSendingThread: MSG_TYPE_CHUNK: remove oldest chunk %d chunksize %d\n", chunkStreamer->id, chunk_size);
				cb_remove_chunk(pSettings->chunkbuf, 0);		

				if(audioPacket)
				{
#if 0
					if(prevAudioChunk != 0)
					{
						if((expelPL.frame_count - prevAudioChunk) > 1)
							printf("Drop chunks %d -> %d\n", prevAudioChunk, expelPL.frame_count);
					}

					prevAudioChunk = expelPL.frame_count;
#endif
					//printf("\t EXPEL (%d) ...\n", expelPL.frame_count);
					
					break;
				}
					
				chunkID_set_free(cb_bmap);

				cb_bmap	= chunkID_set_init("type=bitmap");
				chunks 	= cb_get_chunks(pSettings->chunkbuf, &chunk_size);
				//fprintf(stderr,"\t\t ChunkSendingThread: chunk_size %d chunkbuf: 0x%x\n", chunk_size, pSettings->chunkbuf);
				for(j=0; j < chunk_size; j++) 
				{
					//fprintf(stderr,"\t\t ChunkSendingThread: MSG_TYPE_CHUNK: chunkbuffer chunk[%d]: %d (transid: %u)\n", j, chunks[j].id, transid);	
					chunkID_set_add_chunk(cb_bmap, chunks[j].id);
				}							

			}
			
		}

		chunkID_set_free(cb_bmap);

		releaseSemaphore(psmplSema4Handle);

		usleep(1000); // 1 msec
		
	}

	free(encodeBuffer);

	free(tmpBuffer);

	close(fd);
	
	printf("ChunkSendingThread Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;

}
