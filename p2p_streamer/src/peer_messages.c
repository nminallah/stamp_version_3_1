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

#define PEERSAMPLER_TIMEOUT_SEC	20
#define MAX_NODES_IN_PEERSET		20

#define BUFFSIZE MAX_RAW_VIDEO_FRAME//60*1024

unsigned int 				chunkInputCnt = 0;
unsigned int 				chunkOutputCnt = 0;

unsigned int				rec_min_id = 0;

unsigned int				lastChunkID = 0;

extern int 				my_metadata;
extern metaDataStruct_t	my_metaDataParam;
extern unsigned long 		psmplSema4Handle;
extern char				local_ip[256];
extern int					local_port;

extern int					loopback_disable;

extern unsigned int			start_up_latency_ms;
//extern unsigned int			playback_delay_ms;
extern unsigned int			network_delay_ms;

//extern unsigned int		 	chunk_miss_ratio;
//extern unsigned int	 		max_chunk_id; 

extern int					debug_level;

/******************************************************************************/
static struct nodeID *		rttlocalID = NULL;
unsigned long				rttSignallingTimeStart = 0;
unsigned long				rttSignallingTimeStop = 0;

unsigned int 				network_rtt_ms = 0;

unsigned long				select_peer_time = 0;
char						saved_peer_addr[256] = {0};

#define MAX_RTT_MONITOR_ARRAY	1024
#define MAX_AVG_RTT_ARRAY 			32
#define MAX_AVG_DELAY_ARRAY 		32
typedef struct RTTStruct
{
	char  		address[32];
	unsigned int 	id;
	unsigned long 	time;
}RTTStruct_t, *pRTTStruct_t;

RTTStruct_t 	rttMonitorParams[MAX_RTT_MONITOR_ARRAY];
int			rttArray[MAX_AVG_RTT_ARRAY] = {-1};
int			rttIndex = 0;

int			delayArray[MAX_AVG_DELAY_ARRAY] = {-1};
int			delayIndex = 0;

int RTTInit(struct nodeID *myID)
{
	int i;
	rttlocalID = myID; 

	for(i = 0; i < MAX_RTT_MONITOR_ARRAY; i++)
	{
		memset(rttMonitorParams[i].address, 0, 32);
		rttMonitorParams[i].id = -1;
		rttMonitorParams[i].time = 0;
	}

	rttIndex = 0;
	for(i = 0; i < MAX_AVG_RTT_ARRAY; i++)
	{
		rttArray[i] = -1;
	}

	delayIndex = 0;
	for(i = 0; i < MAX_AVG_DELAY_ARRAY; i++)
	{
		delayArray[i] = -1;
	}
	return 1; 
}

int sendRTTSignalling(struct nodeID *to, int type)
{
	unsigned char * buff = NULL;
	int	len = 0;

	buff = malloc(64);
	if (!buff) {
		fprintf(stderr, "Error allocating buffer\n");
		return -1;
	}
	memset(buff, 0, 64);
	
	buff[0] = MSG_TYPE_RTT;
	buff[1] = type;

	len = 2;

	send_to_peer(rttlocalID, to, buff, len);
	
	free(buff);

	return 0;
}

int AddToRTTStruct(int id, char * address)
{
	int i, foundempty = -1,foundsameID = -1;

	unsigned long 	mintimestamp = 0xFFFFFFFF;
	int 			mintimeindex = -1;

	for(i = 0; i < MAX_RTT_MONITOR_ARRAY; i++)
	{
		if(foundempty == -1)
		{
			if(rttMonitorParams[i].id == -1)
			{
				foundempty = i;			
			}
		}
		
		if(foundsameID == -1)
		{
			if((rttMonitorParams[i].id == id))// && (strcmp(address, rttMonitorParams[i].address) == 0))
			{
				foundsameID = i;	
			}
		}

		if(rttMonitorParams[i].id != -1)
		{
			if(rttMonitorParams[i].time < mintimestamp)
			{
				mintimestamp = rttMonitorParams[i].time;
				mintimeindex = i;
			}
		}
	}

	//printf("sameID: %d empty: %d min: %d [id: %d addr: %s]\n", foundsameID, foundempty, mintimeindex, id, address);
/*
	if(foundsameID != -1)
	{
		// same slot found, modify entry
		rttMonitorParams[foundsameID].id = id;
		sprintf(rttMonitorParams[foundsameID].address, "%s", address);
		rttMonitorParams[foundsameID].time = timer_msec();		
		//printf("AddToRTTStruct: same index: [%d, %s, %u]\n",rttMonitorParams[foundsameID].id, rttMonitorParams[foundsameID].address, rttMonitorParams[foundsameID].time);
	}
	else
*/
	if(foundsameID == -1)
	{
		if(foundempty != -1)
		{
			// emty slot found, add entry
			rttMonitorParams[foundempty].id = id;
			sprintf(rttMonitorParams[foundempty].address, "%s", address);
			rttMonitorParams[foundempty].time = timer_msec();		
			//printf("AddToRTTStruct: empty index: [%d, %s, %u]\n",rttMonitorParams[foundempty].id, rttMonitorParams[foundempty].address, rttMonitorParams[foundempty].time);
		}
		else if(mintimeindex != -1)
		{
			// no emty slot found, override min time entry
			rttMonitorParams[mintimeindex].id = id;
			sprintf(rttMonitorParams[mintimeindex].address, "%s", address);
			rttMonitorParams[mintimeindex].time = timer_msec();
			//printf("AddToRTTStruct: mintime index: [%d, %s, %u]\n",rttMonitorParams[mintimeindex].id, rttMonitorParams[mintimeindex].address, rttMonitorParams[mintimeindex].time);
		}
	}
	
	return 0;
}

int GetFromRTTStruct(int id, char * address)
{
	int i, rttvalue = -1;
	int avgrttvalue = -1;

	unsigned long 	maxtimestamp = 0;
	int 			maxtimeindex = -1;

	unsigned long	currTimeStamp = timer_msec();

	for(i = 0; i < MAX_RTT_MONITOR_ARRAY; i++)
	{
		//if((rttMonitorParams[i].time > maxtimestamp) && (strcmp(address, rttMonitorParams[i].address) == 0))
		if((rttMonitorParams[i].id == id) && (strcmp(address, rttMonitorParams[i].address) == 0))
		{
			maxtimestamp = rttMonitorParams[i].time;
			maxtimeindex = i;
			break;
		}
	}

	if(maxtimeindex != -1)
	{
		rttvalue = (currTimeStamp - maxtimestamp);

		//printf("GetFromRTTStruct: index: %u [%d, %s, %u]\n", currTimeStamp, rttMonitorParams[maxtimeindex].id, rttMonitorParams[maxtimeindex].address, rttMonitorParams[maxtimeindex].time);

		memset(rttMonitorParams[maxtimeindex].address, 0, 32);
		rttMonitorParams[maxtimeindex].id = -1;
		rttMonitorParams[maxtimeindex].time = 0;
	}

	//printf("\t GetFromRTTStruct: RTT: %d id: %d address: %s\n", rttvalue, id, address);

#if 1	
	if(rttvalue != -1)
	{
		rttArray[rttIndex] = rttvalue;
		rttIndex++;
		if(rttIndex > MAX_AVG_RTT_ARRAY)
			rttIndex = 0;
	}

	int avg = 0;
	int avgCnt = 0;
	
	for(i =0; i < MAX_AVG_RTT_ARRAY; i++)
	{
		if(rttArray[i] != -1)
		{
			avg += rttArray[i];
			avgCnt++;
		}
	}

	if(avgCnt != 0)
	{
		avgrttvalue = avg/avgCnt;
	}
#else
	avgrttvalue = rttvalue;
#endif

	return avgrttvalue;
}

int CheckFromRTTStruct(int id, char * address)
{
	int i;

	unsigned long	currTimeStamp = timer_msec();

	for(i = 0; i < MAX_RTT_MONITOR_ARRAY; i++)
	{
		//if((rttMonitorParams[i].time > maxtimestamp) && (strcmp(address, rttMonitorParams[i].address) == 0))
		if(rttMonitorParams[i].id == id)
		{
			if((currTimeStamp - rttMonitorParams[i].time) > 100)//((network_rtt_ms/2)+100))
			{
				memset(rttMonitorParams[i].address, 0, 32);
				rttMonitorParams[i].id = -1;
				rttMonitorParams[i].time = 0;

				return 0;
			}

			return 1;
		}
	}

	return 0;
}

/******************************************************************************/

int checkPeerMetadata(struct nodeID * remote)
{
	const struct nodeID **neighbours;
	struct peer ** peers;
	char addr[256];
       int i, n;
	pmetaDataStruct_t pmetaDataParam;
	const uint8_t *mdata;
	int msize;
	int	found_index = -1;

	//3Parse topology list to check if remote nodeID exists
       neighbours = topoGetNeighbourhood(&n);
	mdata = topoGetMetadata(&msize);
       for (i = 0; i < n; i++) 
	{
		//fprintf(stderr, "i: %d\n", i);
		if(nodeid_cmp(remote, neighbours[i]) == 0)
		{
			found_index = i;
			break;
		}
       }

	if(found_index != -1)
	{
		const int *d;
		d = (const int*)(mdata+found_index*msize);
		pmetaDataParam = (pmetaDataStruct_t)d;

		//3Entry already exists in peerset, check if it has matching metadata, update creation timestamp. If metadata mismatch, remove peer from peerset
		if(pmetaDataParam->id == my_metaDataParam.id)
		{
			return 1;
		}
	}

	return 0;
}

int sendBufferUDP(int fd, unsigned char * buffer, int buffer_size, unsigned char * tmpBuffer, struct sockaddr_in    * servaddr)
{
	int ret, len;

	//printf("SEND++\n");
	ret = 0;
	//do 
	{
	/*
		if (buffer_size > (MAX_UDP_PACKET_SIZE)) 
		{
			len = (MAX_UDP_PACKET_SIZE);
			//tmpBuffer[0] = 0;
		} 
		else
	*/	
		{
			len = buffer_size;
			//tmpBuffer[0] = 1;
		}

		memcpy(tmpBuffer, buffer, len);
		buffer_size -= len;
		buffer += len;
		
		//printf("\t tmpBuffer[0]: %d - (%d) %d\n", tmpBuffer[0], len+1, ret);
		//ret += send(fd, tmpBuffer, len + 1, 0);
	       ret += sendto(fd, tmpBuffer, len, 0, servaddr, sizeof(struct sockaddr_in));

		//usleep(1);

	} //while (buffer_size > 0);

	return ret;
}

int sig_receive(struct nodeID * remote, uint8_t * buff, int buff_len,  struct chunk_buffer * chunkbuf, struct peerset * peerinfo, int source)
{
	struct chunkID_set *	cset;
	struct nodeID *		owner;
	enum signaling_type 	sig_type;
	int 					ret;
	int 					max_deliver = 0;
	uint16_t 				trans_id = 0;
	char 				addr[256];

	node_addr(remote, addr, 256);

	ret = parseSignaling(buff + 1, buff_len - 1, &owner, &cset, &max_deliver, &trans_id, &sig_type);
	//fprintf(stdout,"Parsed %d bytes: trans_id %d, Max_deliver %d\n", ret, trans_id, max_deliver);
	switch (sig_type) {
		case sig_offer:
			//fprintf(stdout, "Offer of %d chunks from %s\n", chunkID_set_size(cset), addr);
			if(source == 0)
			{
				struct peer ** 		peers = peerset_get_peers(peerinfo);
				struct peer *			p = peerset_get_peer(peerinfo, remote);
				struct chunkID_set *	cb_bmap;
				struct chunk *		chunks;
				int 					chunk_size, j;
				int 					chunkid, min_id;
				int 					i, size;
				struct chunkID_set *	chunk_req;
				char 				selected_peer_addr[256];
				int					selected_peer_indx;
				
				//fprintf(stderr,"[%u] %s -> %s:%d: PeerMessasgesThread: signal_receive (offer): %d chunks offered (transid: %u) +++\n", timer_msec(), addr, local_ip, local_port, chunkID_set_size(cset),trans_id);	
				
				if(p == NULL)
				{
					return;
					//peerset_add_peer(peerinfo, remote);
					//p = peerset_get_peer(peerinfo, remote);
				}
				
				if(checkPeerMetadata(remote) == 0)
				{
					break;
				}

				chunkID_set_clear(p->bmap,0);
				chunkID_set_union(p->bmap, cset);
				gettimeofday(&p->bmap_timestamp, NULL);

#if 0
				{
					int i;
					for (i = 0; i < chunkID_set_size(p->bmap); i++) 
					{
						fprintf(stderr,"\t\t PeerMessasgesThread: signal_receive (offer): offer chunk[%d]: %d\n", i, chunkID_set_get_chunk(p->bmap, i));	
					}
					}
#endif
#if 1
				if(select_peer_time == 0)
				{
					int retVal = chunk_random_peer(peerinfo, selected_peer_addr);
					if(retVal != -1)
						sprintf(saved_peer_addr, "%s", selected_peer_addr);

					select_peer_time = timer_msec();
				}
				else
				{
					if((timer_msec() - select_peer_time) > 100)
					{
						int retVal = chunk_random_peer(peerinfo, selected_peer_addr);
						if(retVal != -1)
							sprintf(saved_peer_addr, "%s", selected_peer_addr);

						//printf("saved_peer_addr: %s\n", saved_peer_addr);
						select_peer_time = timer_msec();
					}
				}

				if(strcmp(saved_peer_addr, addr) != 0)
				{
					//printf("saved_peer_addr: %s addr: %s\n", saved_peer_addr, addr);
					break;
				}
#endif

				//fprintf(stderr,"\n");
				// local chunkbuffer to buffer map
				cb_bmap	= chunkID_set_init("type=bitmap");
				chunks 	= cb_get_chunks(chunkbuf, &chunk_size);
				for(j=0; j < chunk_size; j++) 
				{
					//fprintf(stderr,"\t\t PeerMessasgesThread: signal_receive (offer): chunkbuffer chunk[%d]: %d\n", j, chunks[j].id);	
					chunkID_set_add_chunk(cb_bmap, chunks[j].id);
				}

				if(chunk_size > 0)
				{
					min_id = chunks[0].id;
					//max_id = chunks[chunk_size-1].id;
				}
	
				//fprintf(stderr,"\n");
				// Received chunkbuffer in offer
				chunk_req	= chunkID_set_init("size=0");
				size 			= chunkID_set_size(cset);
				//fprintf(stderr, "offer received chunk from %s size %d [%u -> %u] transid: %d \n", addr, size, chunkID_set_get_chunk(cset, 0), chunkID_set_get_chunk(cset, size-1), trans_id);

				if(rec_min_id == 0)
					rec_min_id = chunkID_set_get_chunk(cset, 0);

				int max_allowed_offer = 0;
				for (i = (size-1); i >= 0; i--) 
				{
					chunkid = chunkID_set_get_chunk(cset, i);
					//fprintf(stderr, "\t %d. chunk id %d\n", i, chunkid);

					//Check if chunkid in received buffermap is present in local chunkbuffer
					#if 0
					if (chunkID_set_check(cb_bmap, chunkid) < 0) 
					{
						if(chunkid > min_id)
						{
							//fprintf(stderr,"\t\t PeerMessasgesThread: signal_receive (offer): request for missing chunk: %d\n", chunkid);	
							chunkID_set_add_chunk(chunk_req, chunkid);							
						}
					}

					#else
					if (chunkID_set_check(cb_bmap, chunkid) < 0) 
					{
						if(chunkid > lastChunkID)
						{
							if(chunkid > rec_min_id)
							{
								/*
								if(chunk_size >= min_chunk_delay)
								{
									if(chunkid > min_id)
									{
										//fprintf(stderr,"\t\t PeerMessasgesThread: signal_receive (offer): request for missing chunk: %d\n", chunkid);	
										chunkID_set_add_chunk(chunk_req, chunkid);							
										max_allowed_offer++;
									}
								}
								else
								*/
								{
									//fprintf(stderr, "chunk_req: %d Check: %d addr: %s lastChunkID: %d\n", chunkid, CheckFromRTTStruct(chunkid, addr), addr, lastChunkID);
									if(CheckFromRTTStruct(chunkid, addr) == 0)
									{
										//fprintf(stderr,"\t\t PeerMessasgesThread: signal_receive (offer): request for missing chunk: %d\n", chunkid);	
										chunkID_set_add_chunk(chunk_req, chunkid);
										max_allowed_offer++;

										AddToRTTStruct(chunkid, addr);
										
										//fprintf(stderr, "\t\t chunk_req: %d\n", chunkid);
									}
								}
							}
						}

						if(max_allowed_offer > 30)
							break;
					}
					#endif
				}

				//fprintf(stderr, "chunk accept size %d from %s\n", chunkID_set_size(chunk_req), addr);
				if(chunkID_set_size(chunk_req) > 0)
				{
					//fprintf(stderr,"[%u] %s:%d -> %s: PeerMessasgesThread: signal_receive (acceptChunks): total missing chunks %d requested (transid: %u)\n", timer_msec(), local_ip, local_port, addr, chunkID_set_size(chunk_req), trans_id);
					acceptChunks(remote, chunk_req, trans_id);
				}

				chunkID_set_free(cb_bmap);
				chunkID_set_free(chunk_req);

				//fprintf(stderr,"\t PeerMessasgesThread: signal_receive: offer: receive offer of %d chunks from %s (transid: %u) ---\n", chunkID_set_size(cset), addr, trans_id);	
			}
		break;
		case sig_accept:
			//fprintf(stdout, "Accept of %d chunks\n", chunkID_set_size(cset));
			{
				struct peer *			p = peerset_get_peer(peerinfo, remote);
				struct chunk *		c;
				int 					chunk_size, j;
				int 					chunkid;
				int 					i, size;

				//fprintf(stderr,"[%u] %s -> %s:%d: PeerMessasgesThread: signal_receive (accept): %d chunks received (transid: %u) +++\n", timer_msec(), addr, local_ip, local_port, chunkID_set_size(cset), trans_id);	
				
				if(p == NULL)
				{
					return;
					//peerset_add_peer(peerinfo, remote);
					//p = peerset_get_peer(peerinfo, remote);
				}

				if(checkPeerMetadata(remote) == 0)
				{
					break;
				}

				gettimeofday(&p->bmap_timestamp, NULL);
				size = chunkID_set_size(cset);

				for (i = 0; i < size; i++) 
				{
					chunkid 	= chunkID_set_get_chunk(cset, i);
					c 		= cb_get_chunk(chunkbuf, chunkid);
					if (c != NULL) 
					{
						//fprintf(stderr,"\t\t PeerMessasgesThread: signal_receive (accept): sendChunk id %d (transid: %u)\n", c->id);	
						sendChunk(remote, c, 0);

						//logUploadStats(addr, 20 + sizeof(uint16_t) + c->size + c->attributes_size);

						chunkOutputCnt++;
#if 0
						if(i < (size-1))
						{
							uint64_t 	curr_timestamp;
							uint64_t 	next_timestamp;
						
							curr_timestamp = c->timestamp;

							chunkid 	= chunkID_set_get_chunk(cset, i+1);
							c 		= cb_get_chunk(chunkbuf, chunkid);
							next_timestamp = c->timestamp;

							oSleep((next_timestamp-curr_timestamp)/1000);
						}
#endif
					}
				}
			}
		break;
		case sig_deliver:
			//fprintf(stdout, "Deliver of %d chunks\n", chunkID_set_size(cset));
		break;
		case sig_send_buffermap:
			//printf(stdout, "I received a buffer of %d chunk ids from %s\n", chunkID_set_size(cset), addr);
			{
				struct peer *p = peerset_get_peer(peerinfo, remote);

				//fprintf(stderr,"[%u] %s -> %s:%d: PeerMessasgesThread: signal_receive (buffermap): send buffermap (transid: %u) +++\n", timer_msec(), addr, local_ip, local_port, trans_id);	

				if(p == NULL)
				{
					return;
					//peerset_add_peer(peerinfo, remote);
					//p = peerset_get_peer(peerinfo, remote);
				}

				if(checkPeerMetadata(remote) == 0)
				{
					break;
				}

				chunkID_set_clear(p->bmap,0);
				chunkID_set_union(p->bmap, cset);
				gettimeofday(&p->bmap_timestamp, NULL);

#if 0
				{
					int i;
					for (i = 0; i < chunkID_set_size(p->bmap); i++) 
					{
						fprintf(stderr,"\t\t PeerMessasgesThread: signal_receive (buffermap): chunk[%d]: %d (transid: %u)\n", i, chunkID_set_get_chunk(p->bmap, i), trans_id);	
					}
				}				
#endif
			}
		break;
	    	case sig_ack:
			//fprintf(stdout, "Ack of %d chunks\n", chunkID_set_size(cset));
			{
				struct peer *			p = peerset_get_peer(peerinfo, remote);
				struct chunkID_set *	cb_bmap;
				struct chunk *		chunks;
				int 					chunk_size, j;
				int 					chunkid;
				int 					i, size;
				struct chunkID_set *	chunk_req;
				
				//fprintf(stderr,"[%u] %s -> %s:%d: PeerMessasgesThread: signal_receive (ack): %d chunks acked (transid: %u) +++\n", timer_msec(), addr, local_ip, local_port, chunkID_set_size(cset),trans_id);	
				
				if(p == NULL)
				{
					return;
					//peerset_add_peer(peerinfo, remote);
					//p = peerset_get_peer(peerinfo, remote);
				}
				
				if(checkPeerMetadata(remote) == 0)
				{
					break;
				}
				
				chunkID_set_clear(p->bmap,0);
				chunkID_set_union(p->bmap, cset);
				gettimeofday(&p->bmap_timestamp, NULL);

			}
		break;

	}

	if (cset) chunkID_set_free(cset);
	if(owner) nodeid_free(owner);

	return ret;
}

int PeerMessagesThread(void * arg)
{
	int						rval;
	pPeerMessagesSettings_t 	pSettings = (pPeerMessagesSettings_t ) arg;

	struct nodeID *			remote;
	static uint8_t 				buff[BUFFSIZE];
	 
	int 						len;

	int 						news;
	struct timeval			tout = {0, 20000}; //0.02 seonds
	struct timeval 			t1;

	struct chunk 				ck;
	uint16_t 					transid;
	char 					addr[256];
	
	fprintf(stderr, "+++ PeerMessasgesThread +++\n");

	pSettings->threadStatus 	= IDLE_THREAD;

	tout.tv_usec     			= pSettings->period_msec*1000;

	select_peer_time = 0;
	memset(saved_peer_addr, 0, 256);

	RTTInit(pSettings->rcv_msock);

	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;

		t1 = tout;

		news = wait4data(pSettings->rcv_msock, &t1,NULL);
		if (news > 0) 
		{		
			acquireSemaphore(psmplSema4Handle, -1);
			
			len = recv_from_peer(pSettings->rcv_msock, &remote, buff, BUFFSIZE);
			if (len < 0) {
				//fprintf(stderr,"PeerMessasgesThread: Error receiving message. Maybe larger than %d bytes\n", BUFFSIZE);
				nodeid_free(remote);

				releaseSemaphore(psmplSema4Handle);

				oSleep(1);
				continue;
			}

			node_addr(remote, addr,256);
			if(debug_level > 0)
				fprintf(stderr, "Message from %s\n", addr);

//Players behind same WAN
#if 1
			{
				const struct nodeID **neighbours;
				pmetaDataStruct_t pmetaDataParam;	   
				const uint8_t *mdata;
			       int i, n;
				int msize;
				int found_index = -1;
				
				neighbours = topoGetNeighbourhood(&n);
				mdata = topoGetMetadata(&msize);
				for (i = 0; i < n; i++) 
				{
					//fprintf(stderr, "i: %d\n", i);
					char maddr[256];
					const int *d;
					
					d = (const int*)(mdata+i*msize);
					pmetaDataParam = (pmetaDataStruct_t)d;

					if(strlen(pmetaDataParam->ip) > 0)
					{
						sprintf(maddr, "%s:%d", pmetaDataParam->ip, pmetaDataParam->port);

						if(strcmp(addr, maddr) == 0)
						{
							char naddr[256];
							int nport;
							
							node_ip(neighbours[i], naddr, 256);
							nport = node_port(neighbours[i]);

							node_set_ip(remote, naddr);
							node_set_port(remote, nport);
							//printf("override recv IP: %s:%d\n", naddr, nport);

							break;
						}
					}
					
				}
			}
#endif

			node_addr(remote, addr, 256);
			//fprintf(stderr, "Message from %s\n", addr);

			switch (buff[0]) 
			{
				case MSG_TYPE_TMAN:
				case MSG_TYPE_TOPOLOGY:
					//fprintf(stderr,"PeerMessasgesThread: recv_from_peer len %d from %s: MSG_TYPE_TOPOLGY +++\n", len, addr);	
					peerSampling(remote, buff, len, pSettings->source, pSettings->snd_ssock, pSettings->chunkbuf, pSettings->peerinfo);
					//fprintf(stderr,"PeerMessasgesThread: recv_from_peer len %d from %s: MSG_TYPE_TOPOLGY ---\n", len, addr);	
				break;
				case MSG_TYPE_RTT:
					if(buff[1] == RTT_QUERY)
					{
						sendRTTSignalling(remote, RTT_REPLY);
					}
					else
					{
						rttSignallingTimeStop = timer_msec();
						//printf("RTT REPLY :: %u msec --\n", rttMeasuringTimeStop - rttMeasuringTimeStart);
					}
				break;					
				case MSG_TYPE_CHUNK:
					if(pSettings->source == 0)
					{
						struct peer *	p = peerset_get_peer(pSettings->peerinfo, remote);
					
						rval = parseChunkMsg(buff + 1, len - 1, &ck, &transid);
						//printf("PeerMessasgesThread: recv_from_peer len %d from %s: MSG_TYPE_CHUNK (transid: %u) +++\n", len, addr, transid);	
						//fprintf(stderr,"[%u] %s -> %s:%d: PeerMessasgesThread: signal_receive (MSG_TYPE_CHUNK): receive chunk %d (transid: %u) +++\n", timer_msec(), addr, local_ip, local_port, ck.id, transid);	
					
						if(p == NULL)
						{
							break;
							//peerset_add_peer(pSettings->peerinfo, remote);
							//p = peerset_get_peer(pSettings->peerinfo, remote);
						}

						if(checkPeerMetadata(remote) == 0)
						{
							break;
						}
						
						if(rval == 1)
						{
							struct chunkID_set *	cb_bmap;
							struct chunk *		chunks;
							int 					chunk_size, j;

							if(ck.attributes_size == 0)
								ck.attributes = NULL;

							struct timeval 		now;
							gettimeofday(&now, NULL);

							uint64_t currTimestamp = now.tv_sec * 1000000ULL + now.tv_usec;
							uint64_t ckTimestamp = ck.timestamp;
							
#if 1
							int rval = GetFromRTTStruct(ck.id, addr);
							if(rval != -1)
							{
								network_rtt_ms = (unsigned int)(rval);
							}
#endif

							//printf("RTT: %d\n", rttval);

							ck.timestamp = currTimestamp;

							//PayloadHeader_t  currPL;
							//decodePayloadHeader(&currPL, ck.data + FEC_HEADER_SIZE);	
							//printf("ID: ... %d - %d**** \n", ck.id, currPL.codec_type);

							// local chunkbuffer to buffer map
							cb_bmap	= chunkID_set_init("type=bitmap");
							chunks 	= cb_get_chunks(pSettings->chunkbuf, &chunk_size);
							//fprintf(stderr,"\t\t PeerMessasgesThread: chunk_size %d chunkbuf: 0x%x\n", chunk_size, pSettings->chunkbuf);
							for(j=0; j < chunk_size; j++) 
							{
								//fprintf(stderr,"\t\t PeerMessasgesThread: MSG_TYPE_CHUNK: chunkbuffer chunk[%d]: %d (transid: %u)\n", j, chunks[j].id, transid);	
								chunkID_set_add_chunk(cb_bmap, chunks[j].id);
							}							

							//fprintf(stderr,"PeerMessasgesThread: MSG_TYPE_CHUNK:\n");
							//printf("\t P2P : check %d\n", chunkID_set_check(cb_bmap, ck.id));

							if (chunkID_set_check(cb_bmap, ck.id) < 0) 
							{
#if 1
								PayloadHeader_t  payloadHeader;
								decodePayloadHeader(&payloadHeader, ck.data + FEC_HEADER_SIZE);	

								int layer;
								if(payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO)
									layer = 0;
								else if(payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO_1)
									layer = 1;
								else if(payloadHeader.codec_type == AVMEDIA_TYPE_VIDEO_2)
									layer = 2;
								else
									layer = 3; // AVMEDIA_TYPE_VIDEO, AVMEDIA_TYPE_VIDEO_US 

								//printf("mdParam: %d layer: %d\n", my_metaDataParam.video_layer_number, layer);
								if(my_metaDataParam.video_layer_number == layer)
								{
									unsigned int delay_ms;
									if(currTimestamp >= ckTimestamp)
										delay_ms = (unsigned int)(currTimestamp - ckTimestamp)/1000;
									else
										delay_ms = (unsigned int)(ckTimestamp - currTimestamp)/1000;

									delayArray[rttIndex] = delay_ms;
									delayIndex++;
									if(delayIndex > MAX_AVG_DELAY_ARRAY)
										delayIndex = 0;

									int avg = 0;
									int avgCnt = 0;
									int avgrttvalue = -1;
									int i;
									
									for(i =0; i < MAX_AVG_DELAY_ARRAY; i++)
									{
										if(delayArray[i] != -1)
										{
											avg += delayArray[i];
											avgCnt++;
										}
									}

									if(avgCnt != 0)
									{
										avgrttvalue = avg/avgCnt;
									}

									if(avgrttvalue != -1)
									{
										network_delay_ms = (unsigned int)(avgrttvalue);
									}
								}

#endif

								rval = cb_add_chunk(pSettings->chunkbuf, &ck);	

								//logDownloadStats(addr, len);
								//chunks = cb_get_chunks(pSettings->chunkbuf, &chunk_size);
								//fprintf(stderr,"PeerSignallingThread: MSG_TYPE_CHUNK: add chunk %d to chunkbuffer from %s [%d: %d]\n", ck.id, addr, chunks[0].id, chunks[chunk_size-1].id);
								chunkInputCnt++;
							}
  							chunkID_set_free(cb_bmap);

							// send Ack local chunkbuffer to buffer map
							cb_bmap	= chunkID_set_init("type=bitmap");
							chunks 	= cb_get_chunks(pSettings->chunkbuf, &chunk_size);
							for(j=0; j < chunk_size; j++) 
							{
								chunkID_set_add_chunk(cb_bmap, chunks[j].id);
							}

							sendAck(remote, cb_bmap,transid);
							chunkID_set_free(cb_bmap);
							
						}
						else
						{
							fprintf(stderr,"PeerSignallingThread: Error %d decoding chunk %s !!!\n", rval, addr);

							network_delay_ms = INVALID_NETWORK_DELAY; //3000;
							
							//exit(-1);
						}

						//dbgprint("PeerMessasgesThread: recv_from_peer len %d from %s: MSG_TYPE_CHUNK ---\n", len, addr);	
					}
					else
					{
						//fprintf(stderr,"PeerMessasgesThread: Warning !!! source does not need chunks as it is the provider\n");						
					}
				break;
				case MSG_TYPE_SIGNALLING:
					//printf("PeerMessasgesThread: recv_from_peer len %d from %s: MSG_TYPE_SIGNALLING +++\n", len, addr);	
					sig_receive(remote, buff, len, pSettings->chunkbuf, pSettings->peerinfo, pSettings->source);
					//printf("PeerMessasgesThread: recv_from_peer len %d from %s: MSG_TYPE_SIGNALLING ---\n", len, addr);	
				break;
				default:
					fprintf(stderr, "PeerMessasgesThread: Unknown Message Type %x\n", buff[0]);
			}
			
			nodeid_free(remote);	

			releaseSemaphore(psmplSema4Handle);
		}
		
	}

	fprintf(stderr,"PeerMessagesThread Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;

}

