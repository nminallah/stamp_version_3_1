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

unsigned long prevTSF = 0;

extern unsigned int crc_error;
extern unsigned int fec_correction;

extern int numBFrames;

int recvPacketUDP(int fd, unsigned char * buffer, int buffer_size, pFECDecSettings_t pSettings)
{
	int 					res, len, addrlen;
  	struct sockaddr_in 	raddr;
	
	len = buffer_size;

	while(1)
	{
		addrlen = sizeof(struct sockaddr_in);
		res = recvfrom(fd, buffer, len, MSG_DONTWAIT, &raddr, &addrlen);

		if(res == -1)
		{
			if(pSettings->exit != FALSE_STATE) 
				return 0;
			if(pSettings->pause != FALSE_STATE) 
				return 0;

			usleep(1);

			continue;
		}
		
		break;
	}

	return res;
}


int FECDecThread(void * arg)
{
	int					rval;
	pFECDecSettings_t 	pSettings = (pFECDecSettings_t) arg;

	packet_t				outPacketBuf;
	unsigned long			outSize = 0;

	unsigned char *		chunkBuffer = NULL; 
	unsigned int			chunkBufferSize = 0;

	int	 				fd = -1;
	int					result;
	int					len;
	
	int 					n = pSettings->n;
	int 					k = pSettings->k;
       void * 				deccode = fec_new(k, n);
	   
	char **				dst = NULL;
	int 					dst_pkt_index = 0;
	int 					i = 0;
	int *					ixs = NULL ;

	int *					ixs_monitor = NULL ;

	uint32_t 				crc = 0, gotCrc = 0;	
	FECHeader_t 			fecHeader;
	Chunk 				grapesChunk;

	fprintf(stderr, "+++ FECDecThread +++\n");

	pSettings->threadStatus = IDLE_THREAD;

	//Reset headers
	chunkBufferSize = 0;

	//Open UDP socket
	fd = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in address;

	fprintf(stderr,"player_listen_port: %d\n", pSettings->player_listen_port);
	memset((char *) &address, 0, sizeof(address));
	address.sin_family 	= AF_INET; 
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port 	= htons(pSettings->player_listen_port);
	 
	result = bind(fd, (struct sockaddr *)&address, sizeof(address));
	if(result == -1){
		fprintf(stderr, "GDeChunkiserThread: could not UDP bind to the streamer listen port: %d\n", pSettings->player_listen_port);
		exit(-1);
	}

#ifdef WIN32
	DWORD timeout = 10;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));	
#endif

	int rcvBufSize = 4*1024*1024;
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&rcvBufSize, sizeof(rcvBufSize));

	//Allocate Chunk Buffer
	chunkBuffer = malloc(pSettings->max_chunk_size);
	if(chunkBuffer == NULL)
	{
		fprintf(stderr, "GDeChunkiserThread: could not allocate chunk buffer size: %d\n", pSettings->max_chunk_size);
		exit(-1);
	}

	dst = ( char ** ) malloc ( n * sizeof ( char* ));
	for(i=0; i < n; i++){
		dst[i] = ( char* ) malloc( (MAX_UDP_PACKET_SIZE+MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE)* sizeof ( char ) );
	}

	ixs = malloc(n* sizeof(int));
	memset(ixs, 0, n* sizeof(int));

	ixs_monitor = malloc(n* sizeof(int));
	memset(ixs_monitor, 0, n* sizeof(int));
	
	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;

		memset(chunkBuffer, 0, MSG_HEADER_SIZE+ GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE+MAX_UDP_PACKET_SIZE);

		len = recvPacketUDP(fd, chunkBuffer, pSettings->max_chunk_size, pSettings);
		if(len == 0)
		{
			oSleep(1);

			if((pSettings->exit == FALSE_STATE) && (pSettings->pause == FALSE_STATE))
				continue;
		}

		if(pSettings->exit != FALSE_STATE)
		{
			fprintf(stderr, "FECDecThread: Exited ...\n");
			
			rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
			if(rval == 0)
			{
				outPacketBuf.lastPacket = 1;
				sendQueue(pSettings->out_Q[LIVE_QUEUE], &outPacketBuf, outSize);
			}

			pSettings->exit = DONE_STATE;

			break;
		}

		if(pSettings->pause != FALSE_STATE)
		{
			fprintf(stderr, "FECDecThread: Paused %d ...\n", pSettings->pause);
			
			if(pSettings->pause == TRUE_STATE)
			{
				rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
				if(rval == 0)
				{
					outPacketBuf.lastPacket = 1;
					sendQueue(pSettings->out_Q[LIVE_QUEUE], &outPacketBuf, outSize);
				}
			}
			
			if(pSettings->pause == TRUE_STATE)
				pSettings->pause = DONE_STATE;
			
			memset(ixs, 0, n* sizeof(int));

			for(i=0; i < n; i++)
			{
				memset(dst[i], 0, MAX_UDP_PACKET_SIZE);
			}

			dst_pkt_index = 0;
			
			oSleep(1);

			continue;
		}

		if(len == 0)
			continue;

/**/
		decodeChunkHeader(&grapesChunk, chunkBuffer + MSG_HEADER_SIZE, MAX_UDP_PACKET_SIZE);

		decodeFECHeader(&fecHeader, chunkBuffer + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));

		if(fecHeader.sequence_number < 8)
			chunkBufferSize  = MAX_UDP_PACKET_SIZE;
		else
			chunkBufferSize  = MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE+MAX_UDP_PACKET_SIZE;

		//printf("FEC: %d grapesChunk.id: %d dst_pkt_index: %d crc: %u\n", fecHeader.sequence_number, fecHeader.grapes_id, dst_pkt_index, fecHeader.crc_checksum);

		if(ixs[dst_pkt_index-1] >  fecHeader.sequence_number)
		{
			int x;
			//printf("FECDec: ixs[%d-1] : %d SQNUM: %d\n", dst_pkt_index, ixs[dst_pkt_index-1] , fecHeader.sequence_number);

#if  0
			int k;
			for(k=0; k < dst_pkt_index; k++)
			{
				FILE * ffptr = fopen("fecdec.txt", "ab");
				fwrite(dst[k], 1, MAX_UDP_PACKET_SIZE, ffptr);
				fclose(ffptr);
			}
#endif
			for(x=0; x < n; x++)
				ixs_monitor[x] = -1;

			for(x=0; x < dst_pkt_index; x++)
				ixs_monitor[ixs[x]]=1;


			// fec decode
			fec_decode(deccode, dst, ixs, MAX_UDP_PACKET_SIZE);

#if  0
			int k;
			for(k=0; k < n; k++)
			{
				FILE * ffptr = fopen("fecdec.txt", "ab");
				fwrite(dst[k], 1, MAX_UDP_PACKET_SIZE, ffptr);
				fclose(ffptr);
			}
#endif

			for(i=0; i < k; i++)
			{
				FECHeader_t 			fecHeader2;
				PayloadHeader_t 		payloadHeader;
				Chunk 				grapesChunk2;

				int 					msg_type;
				uint16_t 				transid;
				uint32_t 				crc2 = 0, gotCrc2 = 0;	

				decodeMSGHeader(&msg_type, &transid, dst[i]);
				//printf("FEC: [MSG_HDR]: type: %d\n", msg_type);
				if(msg_type != MSG_TYPE_CHUNK)
				{
					fprintf(stderr, "FECDec: chunk MSG type 0x%x != MSG_TYPE_CHUNK\n", msg_type);
					continue;
				}

				decodeChunkHeader(&grapesChunk2, dst[i] + MSG_HEADER_SIZE, MAX_UDP_PACKET_SIZE);
				//printf("[GRAPES_HDR]: id: %d size: %d\n", grapesChunk.id, grapesChunk.size);
				
				decodeFECHeader(&fecHeader2, dst[i] + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));
				//printf("[FEC_HDR]: id: %d\n", fecHeader.sequence_number);

				decodePayloadHeader(&payloadHeader, dst[i]+(MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE));

				gotCrc2 					= fecHeader2.crc_checksum;
				fecHeader2.crc_checksum 	= 0;
				encodeFECHeader(&fecHeader2, dst[i] + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));

				// Fill CRC
				crc2 = pSettings->initial_crc;
				crc32_fast(dst[i], MAX_UDP_PACKET_SIZE, &crc2);

				grapesChunk2.id 			= fecHeader2.grapes_id;
				encodeChunkHeader(&grapesChunk2, dst[i] + MSG_HEADER_SIZE, MAX_UDP_PACKET_SIZE);

				fecHeader2.crc_checksum 	= crc;
				encodeFECHeader(&fecHeader2, dst[i] + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));

				if(gotCrc2 != crc2)
				{
					crc_error++;
					
					printf("FECDec: CRC corruption found :: Got CRC %u Compute CRC %u seq: %d size: %d\n",gotCrc2,crc2,fecHeader2.sequence_number, MAX_UDP_PACKET_SIZE);
					//exit(-1);
				}
				else
				{
					if(ixs_monitor[i] == -1)
						fec_correction++;

					//printf("BFrames: %d\n", BFrames);
					int BFrames = numBFrames;
					int dropflag1 = -1;
					int dropflag2 = -1;
					int noOut = 0;
									
					if(BFrames==1)
					{
						dropflag2 = 3;
					}

					if(BFrames==0)
					{
						dropflag1 = 2;
						dropflag2 = 3;
					}

					if(dropflag1 == payloadHeader.flags)
					{
						noOut = 1;
					}
					if(dropflag2 == payloadHeader.flags)
					{
						noOut = 1;
					}

					if(noOut == 0)
					{
						receiveQueue(pSettings->out_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
						
						outPacketBuf.data[0]->packet_size = grapesChunk2.size+MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE;
						memcpy(outPacketBuf.data[0]->packet, dst[i], outPacketBuf.data[0]->packet_size);	

						sendQueue(pSettings->out_Q[LIVE_QUEUE], &outPacketBuf, outSize);
					}
				}
			}

			memset(ixs, 0, n* sizeof(int));

			for(i=0; i < n; i++)
			{
				memset(dst[i], 0, MAX_UDP_PACKET_SIZE);
			}
			dst_pkt_index = 0;
		}

		gotCrc 					= fecHeader.crc_checksum;
		fecHeader.crc_checksum 	= 0;
		encodeFECHeader(&fecHeader, chunkBuffer + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));

		// Fill CRC
		crc = pSettings->initial_crc;
		crc32_fast(chunkBuffer, chunkBufferSize, &crc);

		fecHeader.crc_checksum = crc;
		encodeFECHeader(&fecHeader, chunkBuffer + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE));

		if(gotCrc != crc)
		{
			printf("CRC corruption found :: Got CRC %u Compute CRC %u seq: %d size: %d\n",gotCrc,crc,fecHeader.sequence_number, chunkBufferSize);			
		}

		if(gotCrc == crc)
		{
			//if((fecHeader.sequence_number <= 2) || (fecHeader.sequence_number >= 6))
			//fprintf(stderr, "dst_pkt_index: %d\n", dst_pkt_index);
			if(dst_pkt_index < n)
			{
				if(fecHeader.sequence_number < k)
					memcpy(dst[dst_pkt_index], chunkBuffer, MAX_UDP_PACKET_SIZE);
				else
					memcpy(dst[dst_pkt_index], chunkBuffer + MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE, MAX_UDP_PACKET_SIZE);
				ixs[dst_pkt_index] = fecHeader.sequence_number;

				dst_pkt_index++;
			}
		}
/**/

	}

	close(fd);

	//free(tmpBuffer);
	free(chunkBuffer);

	fec_free(deccode);

	printf("FECDecThread Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;
	
}

