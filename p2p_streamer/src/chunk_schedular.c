
#include <stdio.h>
#include <stdarg.h>
#include <root.h>
#include <peer_messages.h>

#define PEERSAMPLER_TIMEOUT_SEC	20
#define MAX_NODES_IN_PEERSET		20
#define delay_msec 					10//250

static int selectedPeerIndx			= -1;
static int selectedPeerSize 		= -1;

extern metaDataStruct_t	my_metaDataParam;

int chunk_random_schedular(struct peerset * peerinfo)
{
	struct peer ** 	peers = peerset_get_peers(peerinfo);

       struct timeval 	curr_timestamp;
	struct timeval  	res;
       unsigned int 		bmap_timestamp;
	unsigned int		difftimestamp;

	int 				indx;

	//if(selectedPeerIndx == -1)
	{		
		selectedPeerSize = peerset_size(peerinfo);
		selectedPeerIndx++;
		if(selectedPeerIndx >= selectedPeerSize)
		{
			selectedPeerIndx = 0;
		}
	}

	if(peers[selectedPeerIndx] == NULL)
	{
		selectedPeerSize = peerset_size(peerinfo);
		selectedPeerIndx++;
		if(selectedPeerIndx >= selectedPeerSize)
		{
			selectedPeerIndx = 0;
		}
	}

	if(peers[selectedPeerIndx] == NULL)
	{
		return -1;
	}
	

#if 0
	bmap_timestamp = (unsigned int)((peers[selectedPeerIndx]->bmap_timestamp.tv_usec + peers[selectedPeerIndx]->bmap_timestamp.tv_sec * 1000000ull)/(1000ull));
	if(bmap_timestamp != 0)
	{
		gettimeofday(&curr_timestamp, NULL);
		timersub(&curr_timestamp, &peers[selectedPeerIndx]->bmap_timestamp, &res);
		difftimestamp = (unsigned int)((res.tv_usec + res.tv_sec * 1000000ull)/(1000ull));
		if(difftimestamp > (delay_msec))
		{
			int indx;
			//fprintf(stderr, "chunk_random_schedular: difftimestamp: %u\n", difftimestamp);
			
			selectedPeerSize = peerset_size(peerinfo);
			selectedPeerIndx++;
			if(selectedPeerIndx >= selectedPeerSize)
			{
				selectedPeerIndx = 0;
			}
		}
	}
#endif	

	return selectedPeerIndx;

}

int chunk_random_peer(struct peerset * peerinfo, char * addr)
{
	//struct peer ** 		peers = peerset_get_peers(peerinfo);
	//int 					size = -1;

	const struct nodeID **	neighbours;
       int 					i, n;
	pmetaDataStruct_t 	pmetaDataParam;
	const uint8_t *		mdata;
	int 					msize;
	int					cnt = 0;
	int					selectedIndex[1024];
	int 				indx = -1;

       neighbours 	= topoGetNeighbourhood(&n);
	mdata 		= topoGetMetadata(&msize);

       for (i = 0; i < n; i++) 
	{
		const int *d 		= (const int*)(mdata+i*msize);
		pmetaDataParam 	= (pmetaDataStruct_t)d;
		if((pmetaDataParam->is_source == 1) || (pmetaDataParam->video_layer_number == my_metaDataParam.video_layer_number))
	{
			selectedIndex[cnt] = i;
			cnt++;
		}
	}

	if(cnt > 0)
	{
		//size = peerset_size(peerinfo);
		indx = rand()%cnt;
		node_addr(neighbours[selectedIndex[indx]], addr, 256);

	//printf("chunk_random_peer: %d -> %s\n", indx, addr);

	return 0;
	}

	return -1;
}


