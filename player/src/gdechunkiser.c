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
unsigned long 		videoInputIBits = 0;
unsigned long 		videoInputPBits = 0;
unsigned long 		videoInputBBits = 0;
unsigned long 		audioFrameCnt = 0;
unsigned long 		audioInputBits = 0;
unsigned long 		videoEncFrameCnt = 0;
unsigned long 		audioEncFrameCnt = 0;

unsigned long 		networkInputBits = 0;

unsigned int		prevVideoFrameCnt = 0;
unsigned int		prevAudioFrameCnt = 0;

unsigned long		videoSendQueue = 0;
unsigned long		audioSendQueue = 0;

extern unsigned int			playback_delay_ms;

extern unsigned int		 	chunk_miss_ratio;
extern unsigned int	 		max_chunk_id; 

extern unsigned int			videoFrameDrops;
extern unsigned int			audioFrameDrops;

extern unsigned int	videoFrameErrors;
extern unsigned int	audioFrameErrors;

extern int				numBFrames;
extern int 			videoLayer;

extern int				video_scalability;

unsigned int 		prevAudioChunk=0;

int				prevChunkID = -1;
int				gcount = 0;

int recvBufferUDP(int fd, unsigned char * buffer, int buffer_size, unsigned char * tmpBuffer, pGDeChunkiserSettings_t pSettings, PayloadHeader_t * pHdr)
{
	int 					res, ret, len, addrlen;
  	struct sockaddr_in 	raddr;
	int 					msg_type;
	uint16_t 				transid;
	Chunk 				grapesChunk;
	PayloadHeader_t 		payloadHeader;
	int					start_byte = 1;
	packet_t				inPacketBuf;
	unsigned long			inSize = 0;
	
	//printf("RECV++\n");
	ret = 0;
	do 
	{
		if (buffer_size > MAX_UDP_PACKET_SIZE) 
		{
			len = MAX_UDP_PACKET_SIZE;
		} 
		else 
		{
			len = buffer_size;
		}

		while(1)
		{
			//printf("recv len: %d\n", len);
			//res = recv(fd, tmpBuffer, len, MSG_DONTWAIT);
			if((pSettings->in_Q[LIVE_QUEUE] == NULL) && (pSettings->in_Q[IDLE_QUEUE] == NULL))
			{
	 			addrlen = sizeof(struct sockaddr_in);
				res = recvfrom(fd, tmpBuffer, len, MSG_DONTWAIT, &raddr, &addrlen);
			}
			else
			{
				int rval = receiveQueue(pSettings->in_Q[LIVE_QUEUE], &inPacketBuf, sizeof(packet_t), &inSize, -1);
				if(rval)
				{
					if(rval != TIMEOUT_ERROR)
					{
						fprintf(stderr, "GDeChunkiserThread: Receive error !!!\n");
						exit(-1);
					}
				}

				if(inPacketBuf.lastPacket == 1)
				{
					fprintf(stderr, "GDeChunkiserThread: Last Packet !!!\n");
					inPacketBuf.lastPacket = 0;
					sendQueue(pSettings->in_Q[IDLE_QUEUE], &inPacketBuf, inSize);		

					return 0;
				}

				res		= inPacketBuf.data[0]->packet_size;
				memcpy(tmpBuffer, inPacketBuf.data[0]->packet, res);

				inPacketBuf.lastPacket = 0;
				sendQueue(pSettings->in_Q[IDLE_QUEUE], &inPacketBuf, inSize);
			}

			//if(res != -1)
			//fprintf(stderr, "recv res: %d\n", res);

			if(res == -1)
			{
				if(pSettings->exit != FALSE_STATE) 
					return 0;
				if(pSettings->pause != FALSE_STATE) 
					return 0;

				usleep(1);
			}
			else
			{
				//Decode MSG Header
				decodeMSGHeader(&msg_type, &transid, tmpBuffer);
				if(msg_type != MSG_TYPE_CHUNK)
				{
					fprintf(stderr, "GDeChunkiserThread: chunk MSG type 0x%x != MSG_TYPE_CHUNK\n", msg_type);
					//exit(-1);
					oSleep(1);
					continue;
				}

				//printf("\n[MSG_HDR]: type: %d\n", msg_type);
				//Decode GRAPES Header
				decodeChunkHeader(&grapesChunk, tmpBuffer+MSG_HEADER_SIZE, pSettings->max_chunk_size);
				if(grapesChunk.size != (res-(MSG_HEADER_SIZE + GRAPES_CHUNK_HEADER_SIZE)))
				{
					fprintf(stderr, "GDeChunkiserThread: incorrect grapes chunk size [%d,%d] received !!!\n", grapesChunk.size, res);
					//exit(-1);
					continue;
				}

				res = grapesChunk.size+MSG_HEADER_SIZE + GRAPES_CHUNK_HEADER_SIZE;

				//printf("[GRAPES_HDR]: id: %d size: %d\n", grapesChunk.id, grapesChunk.size);
				//Decode PayloadHeader Header
				decodePayloadHeader(&payloadHeader, tmpBuffer+MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE);

				int dupPacket = 0;

				if(payloadHeader.codec_type == AVMEDIA_TYPE_AUDIO)
				{
					if(prevAudioChunk != 0)
					{
						if(payloadHeader.frame_count == prevAudioChunk)
							dupPacket = 1;
#if 0
						else if((payloadHeader.frame_count - prevAudioChunk) > 1)
							printf("G Drop chunks %d -> %d\n", prevAudioChunk, payloadHeader.frame_count);
#endif
					}

					prevAudioChunk = payloadHeader.frame_count;
				}

				if(dupPacket == 1)
					continue;

#if 0
				if(prevChunkID != -1)
				{
					if((prevChunkID+1) != grapesChunk.id)
					{
						gcount += (grapesChunk.id - prevChunkID);
						printf("Packet drop detected !!! %d -> %d : %d\n", prevChunkID, grapesChunk.id, gcount);
						//exit(-1);
					}
				}
				prevChunkID = grapesChunk.id;
#else
				struct timeval 		now;
				gettimeofday(&now, NULL);
				uint64_t currTimestamp = now.tv_sec * 1000000ULL + now.tv_usec;
				//printf("Playback delay: %u ms\n", (currTimestamp - chunkStreamer->timestamp)/1000);

				playback_delay_ms =  (unsigned int)(currTimestamp - grapesChunk.timestamp)/1000;
				
				if(prevChunkID != -1)
				{
					if((prevChunkID+1) != grapesChunk.id)
					{
						gcount += (grapesChunk.id - prevChunkID);
						//printf("Packet drop detected !!! %d -> %d : %d\n", prevChunkID, grapesChunk.id, gcount);

						chunk_miss_ratio += (grapesChunk.id - prevChunkID - 1);											
					}
				}
				max_chunk_id = grapesChunk.id;
				prevChunkID = grapesChunk.id;
#endif

				//printf("[PAYLOAD_HDR]: codec_id: %d codec_type: %d frame_count: %d flags: %d marker: %d meta[0]: %d meta[1]: %d\n", payloadHeader.codec_id, payloadHeader.codec_type, payloadHeader.frame_count, payloadHeader.flags, payloadHeader.marker, payloadHeader.meta_data[0], payloadHeader.meta_data[1]);
				//gCodecType = payloadHeader.codec_type;
				
				break;
			}
		}

		//printf("\t tmpBuffer[0]: %d - (%d) %d\n", tmpBuffer[0], res, ret);
		//datasize = grapesChunk.size - (PAYLOAD_HEADER_SIZE);
		//printf("marker: %d\n", payloadHeader.marker);

		if(start_byte)
		{
			if( ((payloadHeader.marker>>1)&0x1) == 0x1 ) 
			{
				// Start of frame 
			}
			else // Continuation packet
			{
				continue; // start byte remains 1 as seeking for first packet
			}

			start_byte = 0;
		}
		else
		{
			if( ((payloadHeader.marker>>1) & 0x1) == 0x1 ) // Start packet again received, so complete prev frame
				break;    		
		}

		memcpy(buffer, tmpBuffer + (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE), res-(MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE));
		buffer_size -= (res-(MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE));
		buffer += (res - (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE));
		ret += (res - (MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+FEC_HEADER_SIZE+PAYLOAD_HEADER_SIZE));
		//datasize -= res;
		//printf("[DATA]: len: %d\n", res-(MSG_HEADER_SIZE+GRAPES_CHUNK_HEADER_SIZE+PAYLOAD_HEADER_SIZE));

		if( (payloadHeader.marker == 0x3) || (payloadHeader.marker == 0x1) ) // First packet Start+End 
		{
			break; // Frame complete	
		}
		

	} while (buffer_size > 0);

	pHdr->codec_id			= payloadHeader.codec_id;
	pHdr->codec_type		= payloadHeader.codec_type;
	pHdr->flags				= payloadHeader.flags;
	pHdr->timestamp_90KHz	= payloadHeader.timestamp_90KHz;
	pHdr->meta_data[0]		= payloadHeader.meta_data[0];
	pHdr->meta_data[1]		= payloadHeader.meta_data[1];
	pHdr->frame_count		= payloadHeader.frame_count;
	
	return ret;
}

int GDeChunkiserThread(void * arg)
{
	int					rval;
	pGDeChunkiserSettings_t 	pSettings = (pGDeChunkiserSettings_t ) arg;

	packet_t				outPacketBuf;
	unsigned long			outSize = 0;

	unsigned char *		chunkBuffer = NULL; 
	unsigned int			chunkBufferDataSize = 0;

	unsigned char *		tmpBuffer = NULL; 

	int 					msg_type;
	uint16_t 				transid;
	Chunk 				grapesChunk;
	PayloadHeader_t 		payloadHeader;
//	FrameHeader_t 		frameHeader;

	int64_t				prevTimestamp = 0;
	unsigned long			realtimeclock = 0;

	int	 				fd = -1;
	int					result, i;
	int					len;

	int					prevvideoLayer = -1;

	fprintf(stderr, "+++ GDeChunkiserThread +++\n");

	pSettings->threadStatus = IDLE_THREAD;

	//Reset headers
	chunkBufferDataSize = 0;
	prevChunkID = -1;

	prevVideoFrameCnt = 0;
	prevAudioFrameCnt = 0;

	if((pSettings->in_Q[LIVE_QUEUE] == NULL) && (pSettings->in_Q[IDLE_QUEUE] == NULL))
	{
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
	}

	//Allocate Chunk Buffer
	chunkBuffer = malloc(pSettings->max_chunk_size);
	if(chunkBuffer == NULL)
	{
		fprintf(stderr, "GDeChunkiserThread: could not allocate chunk buffer size: %d\n", pSettings->max_chunk_size);
		exit(-1);
	}

	tmpBuffer = malloc(pSettings->max_chunk_size);
	if(tmpBuffer == NULL)
	{
		fprintf(stderr, "GDeChunkiserThread: could not allocate tmpBuffer chunk buffer size: %d\n", pSettings->max_chunk_size);
		exit(-1);
	}

	if(pSettings->headless == 1)
	{
#if 0
		FILE * fptr = fopen("FLost.txt", "wt");
		if(fptr == NULL)
		{
			fprintf(stderr, "GDeChunkiserThread: Flost.txt open error !!!\n");
			exit(-1);							
		}
		fclose(fptr);
#endif

		initMuxPlayer();
	}

	unsigned long prevTS = 0;
	
	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;
		
		len = recvBufferUDP(fd, chunkBuffer, pSettings->max_chunk_size, tmpBuffer, pSettings, &payloadHeader);
		
		if((pSettings->in_Q[LIVE_QUEUE] == NULL) && (pSettings->in_Q[IDLE_QUEUE] == NULL))
		{
			if(len == 0)
			{
				oSleep(1);

				if((pSettings->exit == FALSE_STATE) && (pSettings->pause == FALSE_STATE))
					continue;
			}

			if(pSettings->exit != FALSE_STATE)
			{
				fprintf(stderr, "GDeChunkiserThread: Exited ...\n");
				
				if(pSettings->headless == 0)
				{
					rval = receiveQueue(pSettings->out_Video_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
					if(rval == 0)
					{
						outPacketBuf.lastPacket = 1;
						sendQueue(pSettings->out_Video_Q[LIVE_QUEUE], &outPacketBuf, outSize);
					}

					rval = receiveQueue(pSettings->out_Audio_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
					if(rval == 0)
					{
						outPacketBuf.lastPacket = 1;
						sendQueue(pSettings->out_Audio_Q[LIVE_QUEUE], &outPacketBuf, outSize);
					}
				}

				pSettings->exit = DONE_STATE;

				break;
			}

			if(pSettings->pause != FALSE_STATE)
			{
				fprintf(stderr, "GDeChunkiserThread: Paused %d ...\n", pSettings->pause);
				
				if(pSettings->headless == 0)
				{
					if(pSettings->pause == TRUE_STATE)
					{
						rval = receiveQueue(pSettings->out_Video_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
						if(rval == 0)
						{
							outPacketBuf.lastPacket = 1;
							sendQueue(pSettings->out_Video_Q[LIVE_QUEUE], &outPacketBuf, outSize);
						}

						rval = receiveQueue(pSettings->out_Audio_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
						if(rval == 0)
						{
							outPacketBuf.lastPacket = 1;
							sendQueue(pSettings->out_Audio_Q[LIVE_QUEUE], &outPacketBuf, outSize);
						}

					}
				}
				else
				{
					closeMuxPlayer();
					oSleep(1);
					initMuxPlayer();
				}
				
				if(pSettings->pause == TRUE_STATE)
					pSettings->pause = DONE_STATE;

				prevVideoFrameCnt = 0;
				prevAudioFrameCnt = 0;

				videoSendQueue = 0;
				audioSendQueue = 0;
				
				oSleep(1);
				continue;
			}
		}
		else
		{
			if(len == 0)
			{
				rval = receiveQueue(pSettings->out_Video_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
				if(rval == 0)
				{
					outPacketBuf.lastPacket = 1;
					sendQueue(pSettings->out_Video_Q[LIVE_QUEUE], &outPacketBuf, outSize);
				}

				rval = receiveQueue(pSettings->out_Audio_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
				if(rval == 0)
				{
					outPacketBuf.lastPacket = 1;
					sendQueue(pSettings->out_Audio_Q[LIVE_QUEUE], &outPacketBuf, outSize);
				}
				
				break;
			}
		}

		//pauseStart = 0;

		if(len == 0)
			continue;

		//if(playback_delay_ms > 1300)
			//continue;

#if 1
		if((payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO_US) || (payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO) ||  (payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO_1) ||  (payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO_2) ||  (payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO_3))
		{
			int currLayer = -1;
			int changeOverLayer = 0;

			if(payloadHeader.codec_type != AVMEDIA_TYPE_VIDEO_US) 
			{
				video_scalability = 1;
				
				if(payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO)
					currLayer = 0;
				else if(payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO_1)
					currLayer = 1;
				else if(payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO_2)
					currLayer = 2;
				else if(payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO_3)
					currLayer = 3;
		
				if(prevvideoLayer != -1)
				{
					if((prevvideoLayer != currLayer) && (payloadHeader.flags == 1))
					{
						prevvideoLayer = currLayer;
						changeOverLayer = 1;
					}
				}
				else
				{
					prevvideoLayer = currLayer;
				}
				
				if(prevvideoLayer != currLayer)
				{
					continue;
				}		
			}
			else
			{
				video_scalability = 0; 
			}
			

			if(payloadHeader.frame_count <= prevVideoFrameCnt)
				continue;

			if(pSettings->headless == 0)
			{
				rval = receiveQueue(pSettings->out_Video_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
				if(rval)
				{
					if(rval != TIMEOUT_ERROR)
					{
						fprintf(stderr, "GDeChunkiserThread: Receive error !!!\n");
						exit(-1);
					}
				}
			}

			videoFrameCnt++;

			if(pSettings->headless == 0)
			{
				if(len > outPacketBuf.data[0]->buf_size)
				{
					fprintf(stderr, "GDeCunkiserThread: Error. Video frame size %d > buffer limit %d\n", len, outPacketBuf.data[0]->buf_size);

					videoFrameErrors++;

					outPacketBuf.data[0]->packet_size = 0;
					sendQueue(pSettings->out_Video_Q[LIVE_QUEUE], &outPacketBuf, outSize);

					continue;
					//exit(-1);
				}
			}

		       videoInputBits+=(len*8);

			if(payloadHeader.flags == 1)
				videoInputIBits += (len*8);
			else if(payloadHeader.flags == 0)
				videoInputPBits += (len*8);
			else
				videoInputBBits += (len*8);

			if((prevVideoFrameCnt != 0)&&(changeOverLayer == 0))
			{
				//printf("Fr: %d PrFr: %d Diff: %d videoFrameErrors: %d\n", payloadHeader.frame_count, prevVideoFrameCnt, payloadHeader.frame_count - prevVideoFrameCnt, videoFrameErrors);
				if((payloadHeader.frame_count - prevVideoFrameCnt) > ((2-numBFrames)+1))
				{
					videoFrameDrops += (payloadHeader.frame_count - prevVideoFrameCnt - 1);
					videoFrameErrors += (payloadHeader.frame_count - prevVideoFrameCnt - 1);
				}
			}
			prevVideoFrameCnt = payloadHeader.frame_count;

			if(pSettings->headless == 0)
			{
				outPacketBuf.codecType = payloadHeader.codec_type;
				outPacketBuf.codecID 	= payloadHeader.codec_id;
				outPacketBuf.timestamp = payloadHeader.timestamp_90KHz;
				outPacketBuf.flags		= payloadHeader.flags;
				outPacketBuf.data[0]->width = (unsigned int)payloadHeader.meta_data[0];
				outPacketBuf.data[0]->height = (unsigned int)payloadHeader.meta_data[1];
				
				memcpy(outPacketBuf.data[0]->packet, chunkBuffer+chunkBufferDataSize, len);
				outPacketBuf.data[0]->packet_size = len;
				/*
								struct timeval 		now;
								gettimeofday(&now, NULL);
								uint64_t currTimestamp = now.tv_sec * 1000000ULL + now.tv_usec;
								printf("V Pdlay: %u\n", currTimestamp - grapesChunk.timestamp);
				 */
			 	//printf("GdeChunkiser: W: %d H: %d Fr: %d changeOverLayer: %d\n", outPacketBuf.data[0]->width, outPacketBuf.data[0]->height,payloadHeader.frame_count, changeOverLayer);
				sendQueue(pSettings->out_Video_Q[LIVE_QUEUE], &outPacketBuf, outSize);

				videoSendQueue++;
			}
			else
			{
#if 0
				if(prevVideoFrameCnt != 0)
				{
					if((payloadHeader.frame_count - prevVideoFrameCnt) > 1)
					{
						FILE * fptr = fopen("FLost.txt", "at");
						if(fptr == NULL)
						{
							fprintf(stderr, "GDeChunkiserThread: Flost.txt open error !!!\n");
							exit(-1);							
						}

						for(i=1; i < (payloadHeader.frame_count - prevVideoFrameCnt); i++)
						{	
							fprintf(fptr, "%d\n",prevVideoFrameCnt+i); 
						}
						fclose(fptr);
					}
				}
				prevVideoFrameCnt = payloadHeader.frame_count;
#endif

				muxVideoPlayer(chunkBuffer+chunkBufferDataSize, len, payloadHeader.timestamp_90KHz, payloadHeader.codec_id, payloadHeader.meta_data[0], payloadHeader.meta_data[1]);
			}
		}
		else if(payloadHeader.codec_type == AVMEDIA_TYPE_AUDIO)
		{
			if(payloadHeader.frame_count <= prevAudioFrameCnt)
				continue;

			if(pSettings->headless == 0)
			{
				rval = receiveQueue(pSettings->out_Audio_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
				if(rval)
				{
					if(rval != TIMEOUT_ERROR)
					{
						fprintf(stderr, "GDeChunkiserThread: Receive error !!!\n");
						exit(-1);
					}
				}
			}
			
			audioFrameCnt++;

			if(pSettings->headless == 0)
			{
				if(len > outPacketBuf.data[0]->buf_size)
				{
					fprintf(stderr, "GDeCunkiserThread: Error. Audio frame size %d > buffer limit %d\n", len, outPacketBuf.data[0]->buf_size);
					//exit(-1);

					audioFrameErrors++;
					
					outPacketBuf.data[0]->packet_size = 0;
					sendQueue(pSettings->out_Audio_Q[LIVE_QUEUE], &outPacketBuf, outSize);

					continue;

				}
			}

		       audioInputBits+=(len*8);

			if(prevAudioFrameCnt != 0)
			{
				if((payloadHeader.frame_count - prevAudioFrameCnt) > 1)
				{
					audioFrameDrops += (payloadHeader.frame_count - prevAudioFrameCnt - 1);
					audioFrameErrors += (payloadHeader.frame_count - prevAudioFrameCnt - 1);
				}
			}
			prevAudioFrameCnt = payloadHeader.frame_count;

			if(pSettings->headless == 0)
			{
				outPacketBuf.codecType = payloadHeader.codec_type;
				outPacketBuf.codecID 	= payloadHeader.codec_id;
				outPacketBuf.timestamp = payloadHeader.timestamp_90KHz;
				outPacketBuf.channels	= (unsigned int)payloadHeader.meta_data[0];
				outPacketBuf.sampleRate = (unsigned int)payloadHeader.meta_data[1];

				memcpy(outPacketBuf.data[0]->packet, chunkBuffer+chunkBufferDataSize, len);
				outPacketBuf.data[0]->packet_size = len;
				/*
								struct timeval 		now;
								gettimeofday(&now, NULL);
								uint64_t currTimestamp = now.tv_sec * 1000000ULL + now.tv_usec;
								printf("A Pdlay: %u\n", currTimestamp - grapesChunk.timestamp);
				 */
 			 	//printf("GdeChunkiser: channels: %d sampleRate: %d\n", outPacketBuf.channels, outPacketBuf.sampleRate);

				sendQueue(pSettings->out_Audio_Q[LIVE_QUEUE], &outPacketBuf, outSize);

				audioSendQueue++;				
			}
			else
			{
#if 0
				if(prevAudioFrameCnt != 0)
				{
					if((payloadHeader.frame_count - prevAudioFrameCnt) > 1)
					{
					}
				}
				prevAudioFrameCnt = payloadHeader.frame_count;
#endif

				muxAudioPlayer(chunkBuffer+chunkBufferDataSize, len, payloadHeader.timestamp_90KHz, payloadHeader.codec_id, payloadHeader.meta_data[1]);
			}

		}

#endif

    	}

	if(fd != -1)
		close(fd);

	free(tmpBuffer);

	free(chunkBuffer);

	printf("GDeChunkiser Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;

}

