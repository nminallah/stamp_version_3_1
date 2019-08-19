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

#define BUFFSIZE MAX_RAW_VIDEO_FRAME//60*1024

extern unsigned long 	psmplSema4Handle;
extern unsigned long	gtransid;
extern char			local_ip[256];
extern int				local_port;

int ChunkTradingThread(void * arg)
{
	int						rval;
	pChunkTradingSettings_t 	pSettings = (pChunkTradingSettings_t ) arg;

	struct peer **			peers;
	char 					addr[256];
	int 						i;
	int						size;
	int						transid = 0;
	
	fprintf(stderr, "+++ ChunkTradingThread +++\n");

	pSettings->threadStatus = IDLE_THREAD;

	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;

		acquireSemaphore(psmplSema4Handle, -1);

		peers 		= peerset_get_peers(pSettings->peerinfo);

		if(peerset_size(pSettings->peerinfo) > 0)
		{
			struct chunk *	chunks  				= cb_get_chunks(pSettings->chunkbuf, &size);
			int 				selected_peer_indx 	= chunk_random_schedular(pSettings->peerinfo);
			int 				j;

			if((size > 0) && (selected_peer_indx != -1))
			{
				struct chunkID_set *	cb_bmap 		= chunkID_set_init("type=bitmap");
				
				node_addr(peers[selected_peer_indx]->id, addr, 256);
				//fprintf(stderr, "chunk_trading: send offer to peer %s\n", addr);
				//fprintf(stderr,"ChunkTradingThread: chunks to offer to %s (transid: %u)+++\n", addr, gtransid);
				//fprintf(stderr,"[%u] %s:%d -> %s: ChunkTradingThread: chunks to offer (transid: %u) +++\n", timer_msec(), local_ip, local_port, addr, transid);	

				int BFrames 		= getBFrameCount(peers[selected_peer_indx]->id);
				int dropflag1 	= -1;
				int dropflag2 	= -1;
				//printf("BFrames: %d\n", BFrames);
				
				if(BFrames==1)
				{
					dropflag2 = 3;
				}

				if(BFrames==0)
				{
					dropflag1 = 2;
					dropflag2 = 3;
				}

				int VLayer 		= getVideoLayer(peers[selected_peer_indx]->id);
				int droplayer0 	= -1;
				int droplayer1 	= -1;
				int droplayer2	= -1;
				int droplayer3 	= -1;
				//printf("VLayer: %d addr: %s\n", VLayer, addr);
				
				if(VLayer==0)
				{
					droplayer1 = AVMEDIA_TYPE_VIDEO_1;
					droplayer2 = AVMEDIA_TYPE_VIDEO_2;
					droplayer3 = AVMEDIA_TYPE_VIDEO_3;
				}

				if(VLayer==1)
				{
					droplayer0 = AVMEDIA_TYPE_VIDEO;
					droplayer2 = AVMEDIA_TYPE_VIDEO_2;
					droplayer3 = AVMEDIA_TYPE_VIDEO_3;
				}				

				if(VLayer==2)
				{
					droplayer0 = AVMEDIA_TYPE_VIDEO;
					droplayer1 = AVMEDIA_TYPE_VIDEO_1;
					droplayer3 = AVMEDIA_TYPE_VIDEO_3;
				}				

				if(VLayer==3)
				{
					droplayer0 = AVMEDIA_TYPE_VIDEO;
					droplayer1 = AVMEDIA_TYPE_VIDEO_1;
					droplayer2 = AVMEDIA_TYPE_VIDEO_2;
				}				

				for(j = 0; j < size; j++) 
				{
					//fprintf(stderr,"\t\t ChunkTradingThread: offer chunk %d\n", chunks[j].id);

					if (chunkID_set_check(peers[selected_peer_indx]->bmap, chunks[j].id) < 0) 
					{
						PayloadHeader_t 		payloadHeader;
						decodePayloadHeader(&payloadHeader, chunks[j].data+FEC_HEADER_SIZE);
						//fprintf(stderr,"\t\t ChunkTradingThread: flags %d\n", payloadHeader.flags);

						if(dropflag1 == payloadHeader.flags)
						{
							continue;
						}
						if(dropflag2 == payloadHeader.flags)
						{
							continue;
						}

						if(payloadHeader.codec_type != AVMEDIA_TYPE_VIDEO_US)
						{
							if(droplayer0 == payloadHeader.codec_type)
							{
								continue;
							}
							if(droplayer1 == payloadHeader.codec_type)
							{
								continue;
							}
							if(droplayer2 == payloadHeader.codec_type)
							{
								continue;
							}
							if(droplayer3 == payloadHeader.codec_type)
							{
								continue;
							}
						}

						//printf("\t %d: ID: codec_type: %d fr: %d W: %d, H: %d\n", chunks[j].id, payloadHeader.codec_type, payloadHeader.frame_count, payloadHeader.meta_data[0], payloadHeader.meta_data[1]);
						
						chunkID_set_add_chunk(cb_bmap, chunks[j].id);
					}

				}

				offerChunks(peers[selected_peer_indx]->id, cb_bmap, size, transid);
				//fprintf(stderr,"ChunkTradingThread: chunks to offer to %s (%u -> %u) transid: %u ---\n", addr, chunkID_set_get_chunk(cb_bmap, 0), chunkID_set_get_chunk(cb_bmap, size-1), transid);

			       chunkID_set_free(cb_bmap);
			}
		}		

		releaseSemaphore(psmplSema4Handle);

		usleep(pSettings->period_msec*1000);
    	}

	printf("ChunkTradingThread Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;

}

