#include <stdio.h>
#include <stdarg.h>
#include <root.h>
#include <peer_sampling.h>

#define PEERSAMPLER_TIMEOUT_SEC	20
#define MAX_NODES_IN_PEERSET		20

extern unsigned long		rttSignallingTimeStart;
extern int 				my_metadata;
extern metaDataStruct_t	my_metaDataParam;
extern unsigned long 		psmplSema4Handle;

int send_buffermap(struct nodeID * remote, struct chunk_buffer * chunkbuf)
{
	struct chunk *		chunks;
	struct chunkID_set *	cb_bmap;
	char 				addr[256];
	int 					size, i;

	node_addr(remote, addr, 256);

	chunks = cb_get_chunks(chunkbuf, &size);
	//fprintf(stderr, "chunks size: %d\n", size);

	if(chunks > 0)
	{
		cb_bmap = chunkID_set_init("type=bitmap");

		//fprintf(stderr,"[%u] %s:%d -> %s: PeerMessasgesThread: peerSampling: sendBufferMap (transid: %u)\n", timer_msec(), local_ip, local_port, addr, 0);	
		for(i=0; i<size; i++) 
		{
			//fprintf(stderr,"\t\t PeerMessasgesThread: peerSampling: sendBufferMap:  chunks[%d]: id %d\n", i, chunks[i].id);	
			chunkID_set_add_chunk(cb_bmap, chunks[i].id);
		}
/*
		{
			char addr[256];
			node_addr(remote, addr, 256);
			printf("\t\tSend Chunk %d: %s\n", i, addr);
		}
*/
		//fprintf(stderr, "sending buffermap: %s\n", addr);
		sendBufferMap(remote, NULL, cb_bmap, size, 0);

		chunkID_set_free(cb_bmap);
	}

	return 0;
}

int peerSampling(struct nodeID * remote, uint8_t * buff, int len, int source, struct nodeID *	snd_ssock, struct chunk_buffer * chunkbuf, struct peerset * peerinfo)
{
	const struct nodeID **neighbours;
	struct peer ** peers;
	char addr[256];
       int i, n;
	pmetaDataStruct_t pmetaDataParam;
	const uint8_t *mdata;
	int msize;

	topoParseData(buff, len);

	//3Always try pooling for source
	if((source == 0))// && (peerset_size(peerinfo) == 0))
	{
		topoAddNeighbour(snd_ssock ,NULL,0);
	}

	if(buff != NULL)
	{
		int	found_index = -1;
		
		node_addr(remote, addr, 256);

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

		//fprintf(stderr, "found_index: %d\n", found_index);

		//3Remote nodeID exists in topology list
		if(found_index != -1)
		{
			//3Check if remote nodeID exists in peerset
			if(peerset_get_peer(peerinfo, remote) == NULL)
			{
				const int *d;
				d = (const int*)(mdata+found_index*msize);
				pmetaDataParam = (pmetaDataStruct_t)d;
				
				//3New entry in peerset, check if it has matching metadata, add to peerset and update creation timestamp
				if(pmetaDataParam->id == my_metaDataParam.id)
				{
					//fprintf(stderr, "source: %d time: %u\n", pmetaDataParam->is_source, pmetaDataParam->time_ms);

					if(pmetaDataParam->is_source == 1)
					{
						//fprintf(stderr, "RTT QUERY: source: %d time: %u\n", pmetaDataParam->is_source, pmetaDataParam->time_ms);

						rttSignallingTimeStart = timer_msec();
						sendRTTSignalling(remote, RTT_QUERY);
					}
					
					//fprintf(stderr, "Adding node: %s\n", addr);
					if(peerset_size(peerinfo) < MAX_NODES_IN_PEERSET)
					{
						peerset_add_peer(peerinfo, remote);
						//peerset_get_peer(peerinfo, remote)->unique_id = *d;
						gettimeofday(&peerset_get_peer(peerinfo, remote)->creation_timestamp, NULL);
					}
				}
			}
			else
			{
				const int *d;
				d = (const int*)(mdata+found_index*msize);
				pmetaDataParam = (pmetaDataStruct_t)d;

				//3Entry already exists in peerset, check if it has matching metadata, update creation timestamp. If metadata mismatch, remove peer from peerset
				if(pmetaDataParam->id == my_metaDataParam.id)
				{
					//fprintf(stderr, "source: %d time: %u\n", pmetaDataParam->is_source, pmetaDataParam->time_ms);

					if(pmetaDataParam->is_source == 1)
					{
						//fprintf(stderr, "RTT QUERY: source: %d time: %u\n", pmetaDataParam->is_source, pmetaDataParam->time_ms);

						rttSignallingTimeStart = timer_msec();
						sendRTTSignalling(remote, RTT_QUERY);
					}

					//fprintf(stderr, "Modifying creation timestamp node: %s\n", addr);
					gettimeofday(&peerset_get_peer(peerinfo, remote)->creation_timestamp, NULL);
				}
				else
				{
					//fprintf(stderr, "Removing node: %s metadata mismatch: %d %d\n", addr, *d, my_metadata);
					topoRemoveNeighbour(remote);
					peerset_remove_peer(peerinfo, remote);
				}
			}
		}
	}
	else
	{
		struct timeval 	curr_timestamp, res;	
		struct peer **	peers;
		unsigned int		difftimestamp;

		gettimeofday(&curr_timestamp, NULL);

		//fprintf(stderr, "curr_timestamp: %u %u %u\n", curr_timestamp.tv_sec, curr_timestamp.tv_usec, peerset_size(peerinfo));

		//3Parse peerset, compare last time difference b/w current & creation time and remove peer from peerset if timeout occurs
		peers= peerset_get_peers(peerinfo);
		for (i = 0; i < peerset_size(peerinfo); i++) 
		{
			//fprintf(stderr, "%d. bmap_timestamp: %u %u creation: %u %u\n", i, peers[i]->bmap_timestamp.tv_sec, peers[i]->bmap_timestamp.tv_usec,  peers[i]->creation_timestamp.tv_sec, peers[i]->creation_timestamp.tv_usec);
			//if(timerisset(&peers[i]->creation_timestamp))
			{
				node_addr(peers[i]->id, addr, 256);
				timersub(&curr_timestamp, &peers[i]->creation_timestamp, &res);
				difftimestamp = (unsigned int)((res.tv_usec + res.tv_sec * 1000000ull)/(1000ull));
				//fprintf(stderr, "%d. difftimestamp2: %u %s\n", i, difftimestamp, addr);

				if(difftimestamp > (PEERSAMPLER_TIMEOUT_SEC*1000))
				{
					//fprintf(stderr, "Removing2 node: %s\n", addr);

					topoRemoveNeighbour(peers[i]->id);
					int rind = peerset_remove_peer(peerinfo, peers[i]->id);

					//fprintf(stderr, "Peerset rind: %d new size: %d\n", rind, peerset_size(peerinfo));
				}
			}
		}

	}

	if(buff != NULL)
		send_buffermap(remote, chunkbuf);

	return 0;
}

int PeerSamplingThread(void * arg)
{
	int						rval;
	pPeerSamplingSettings_t 	pSettings = (pPeerSamplingSettings_t ) arg;


	fprintf(stderr, "+++ PeerSamplingThread +++\n");

	pSettings->threadStatus 	= IDLE_THREAD;

	acquireSemaphore(psmplSema4Handle, -1);
	peerSampling(NULL, NULL, 0, pSettings->source, pSettings->snd_ssock, NULL, pSettings->peerinfo);
	releaseSemaphore(psmplSema4Handle);

	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;

		//fprintf(stderr,"PeerMessasgesThread: timeout +++\n");	
		acquireSemaphore(psmplSema4Handle, -1);
		peerSampling(NULL, NULL, 0, pSettings->source, pSettings->snd_ssock, NULL, pSettings->peerinfo);
		releaseSemaphore(psmplSema4Handle);			
		//fprintf(stderr,"PeerMessasgesThread: timeout ---\n");	
		
		//oSleep(pSettings->period_msec);
		usleep(pSettings->period_msec*1000);
	}

	fprintf(stderr,"PeerSamplingThread Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;

}

