#include <stdio.h>
#include <stdarg.h>
#include "root.h"
#include <getopt.h>
#ifdef WIN32
#include <process.h>
#include <io.h>
#include <ws2tcpip.h>
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/shm.h>
#endif

unsigned long cumVideoInputBits;
unsigned long cumNetworkInputBits;
unsigned long	total_secs_count;

int					src_server_port = 0;
char					srv_ip[256] = {0};
int					srv_port = 0;
char					local_ip[256] = {0};
int					local_port = 0;
int					player_client_port = 0;

int					loopback_disable = 0;

int					wan_emulation_enable = 0;
int					upload_bitrate_limit = 1000000;
int					download_bitrate_limit = 1000000;

int					debug_level = 0;

int 					my_metadata = 100;
metaDataStruct_t		my_metaDataParam;
tmanRankingFunction 	funct = NULL;

unsigned long 			psmplSema4Handle;

extern unsigned long 	videoFrameCnt;
extern unsigned long 	videoInputBits;
extern unsigned long 	networkInputBits;
extern unsigned int  	chunkInputCnt;
extern unsigned int 	chunkOutputCnt;

unsigned long 		pdbgSema4Handle;

char * 			logUploadAddr[10] ={0};
unsigned int		logUploadDataSize[10] = {0};

char * 			logDownloadAddr[10] ={0};
unsigned int		logDownloadDataSize[10] = {0};

unsigned int		upload_data_size = 0;
unsigned int		download_data_size = 0;

unsigned int		maxUploadDataSize = 0;
unsigned int		maxDownloadDataSize = 0;

unsigned int		nw_upload_data_size = 0;
unsigned int		nw_download_data_size = 0;

unsigned int		start_up_latency_ms = 0;
//unsigned int		playback_delay_ms = 0;
unsigned int		network_delay_ms = 0;

//unsigned int	 	chunk_miss_ratio = 0;
//unsigned int	 	max_chunk_id = 0; 

#ifdef WIN32
HANDLE			msg_sent_from_p2p_mutex;
HANDLE			msg_sent_from_p2p_id;
#else
int 				msg_sent_from_p2p_id; 
#endif
char* 			shared_memory_sent_from_p2p = NULL;

#ifdef WIN32
HANDLE			cntlr_msg_recv_from_player_mutex; 
HANDLE			cntlr_msg_recv_from_player_id; 
#else
int 				cntlr_msg_recv_from_player_id; 
#endif
char* 			shared_cntlr_memory_recv_from_player = NULL;

int 				fd_status = -1;

static char			external_ip[64];
static int 			external_port = 0;

char * get_external_ip(void)
{
	return external_ip;
}

void set_external_ip(char * ip)
{
	sprintf(external_ip, "%s", ip);
}

int get_external_port(void)
{
	return external_port;
}

void set_external_port(int port)
{
	external_port = port;
}

int logUploadStats(char * addr, unsigned int size)
{
	int i;
	int emptyIndx = -1;
	
	for(i=0; i < 10; i++)
	{
		if((emptyIndx == -1)&&(strlen(logUploadAddr[i]) == 0))
		{
			emptyIndx = i;
		}
		
		if(strcmp(addr, logUploadAddr[i]) == 0)
		{
			logUploadDataSize[i] += ((size*8)/1024);
			return 0;
		}
	}

	if(emptyIndx != -1)
	{
		strcpy(logUploadAddr[emptyIndx], addr);
		logUploadDataSize[emptyIndx] += ((size*8)/1024);
	}
}

int logDownloadStats(char * addr, unsigned int size)
{
	int i;
	int emptyIndx = -1;
	
	for(i=0; i < 10; i++)
	{
		if((emptyIndx == -1)&&(strlen(logDownloadAddr[i]) == 0))
		{
			emptyIndx = i;
		}
		
		if(strcmp(addr, logDownloadAddr[i]) == 0)
		{
			logDownloadDataSize[i] += ((size*8)/1024);
			return 0;
		}
	}

	if(emptyIndx != -1)
	{
		strcpy(logDownloadAddr[emptyIndx], addr);
		logDownloadDataSize[emptyIndx] += ((size*8)/1024);
	}
}

unsigned long timer_msec(void){
	struct timeval tdecA;
	unsigned long msec, usec;

	gettimeofday(&tdecA, NULL);

	usec = (tdecA.tv_sec*1000000)+tdecA.tv_usec;

	msec = usec/1000;

	return msec;
}

int is_peer_source(void)
{
	return (srv_port == 0);
}

static void app_defaults(void)
{
	int i;
	
	src_server_port = 2222;

	strcpy(local_ip, "127.0.0.1");
	local_port = 6666;
	set_external_ip("127.0.0.1");

	memset(srv_ip, 0, sizeof(srv_ip));
	srv_port = 0;

	player_client_port = 2224;

	loopback_disable = 0;
	
	for(i=0; i<10; i++)
	{
		logUploadAddr[i] 	= malloc(32);
		memset(logUploadAddr[i], 0, 32);
		logUploadDataSize[i] 	= 0;
	}
	
	for(i=0; i<10; i++)
	{
		logDownloadAddr[i] 		= malloc(32);
		memset(logDownloadAddr[i], 0, 32);
		logDownloadDataSize[i] 	= 0;
	}

}

static void cmdline_parse(int argc, char *argv[])
{
	int o;

	while ((o = getopt(argc, argv, "p:i:P:I:X:x:s:c:m:l:e:u:d:z:w:g:")) != -1) {
		switch(o) {
			case 'p':
				srv_port =  atoi(optarg);
			break;
			case 'i':
				sprintf(srv_ip, "%s", optarg);
			break;
		      case 'P':
				local_port =  atoi(optarg);
			break;
		      case 'I':
				sprintf(local_ip, "%s", optarg);
			break;
		      case 'X':
				set_external_ip(optarg);
			break;
		      case 'x':
				set_external_port(atoi(optarg));
			break;
		      case 's':
				src_server_port = atoi(optarg);
			break;
		      case 'c':
				player_client_port = atoi(optarg);
			break;
		      case 'm':
				my_metadata = atoi(optarg);
			break;
			case 'l':
				loopback_disable = 1;
			break;
			case 'e':
				wan_emulation_enable = atoi(optarg);
			break;
			case 'u':
				upload_bitrate_limit = atoi(optarg);
			break;
			case 'd':
				download_bitrate_limit = atoi(optarg);
			break;
#ifndef WIN32
			case 'z':
				msg_sent_from_p2p_id = atoi(optarg);
				shared_memory_sent_from_p2p = (char*) shmat (msg_sent_from_p2p_id, (void*) 0x00, 0); 
			break;
			case 'w':
				cntlr_msg_recv_from_player_id = atoi(optarg);
				shared_cntlr_memory_recv_from_player = (char*) shmat (cntlr_msg_recv_from_player_id, (void*) 0x00, 0); 
			break;
#endif
			case 'g':
				debug_level = atoi(optarg);
			break;
			default:
				fprintf(stderr, "Error: unknown option %c\n", o);

			exit(-1);
		}
	}

	//set_external_port(local_port);
	
	printf("I: %s\n", local_ip);
	printf("X: %s\n", get_external_ip());
	printf("P: %d\n", local_port);
	printf("x: %d\n", get_external_port());
	printf("i: %s\n", srv_ip);
	printf("p: %d\n", srv_port);
	printf("s: %d\n", src_server_port);
	printf("c: %d\n", player_client_port);
	printf("m: %d\n", my_metadata);
	printf("e: %d\n", wan_emulation_enable);
	printf("u: %d\n", upload_bitrate_limit);
	printf("d: %d\n", download_bitrate_limit);
	printf("l: %d\n", loopback_disable);
#ifndef WIN32
	printf("z: %u\n", msg_sent_from_p2p_id);
	printf("w: %u\n", cntlr_msg_recv_from_player_id);
#endif
	printf("g: %d\n", debug_level);
}

void cb_print(const struct chunk_buffer *cb)
{
	struct chunk *buff;
	int i, size;

	buff = cb_get_chunks(cb, &size);
	for (i = 0; i < size; i++) {
		printf("C[%d]: %s %d\n", i, buff[i].data, buff[i].id);
	}
}

int getBFrameCount(struct nodeID * remote)
{
	const struct nodeID **neighbours;
	pmetaDataStruct_t pmetaDataParam;
	int i, n, msize;
	const uint8_t *mdata;
	int	found_index = -1;
	int BFrames = -1;

       neighbours = topoGetNeighbourhood(&n);
	mdata = topoGetMetadata(&msize);
       for (i = 0; i < n; i++) 
	{
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

		BFrames = pmetaDataParam->number_of_BFrames;
	}

	return BFrames;
}

int setBFrameCount(int BFrames)
{
	
	my_metaDataParam.number_of_BFrames = BFrames;
	
	topoChangeMetadata(&my_metaDataParam, sizeof(my_metaDataParam));
	tmanChangeMetadata(&my_metaDataParam, sizeof(my_metaDataParam));

	return 0;
}

int getVideoLayer(struct nodeID * remote)
{
	const struct nodeID **neighbours;
	pmetaDataStruct_t pmetaDataParam;
	int i, n, msize;
	const uint8_t *mdata;
	int	found_index = -1;
	int layer = -1;

       neighbours = topoGetNeighbourhood(&n);
	mdata = topoGetMetadata(&msize);
       for (i = 0; i < n; i++) 
	{
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

		layer = pmetaDataParam->video_layer_number;
	}

	return layer;
}

int setVideoLayer(int layer)
{
	
	my_metaDataParam.video_layer_number = layer;
	
	topoChangeMetadata(&my_metaDataParam, sizeof(my_metaDataParam));
	tmanChangeMetadata(&my_metaDataParam, sizeof(my_metaDataParam));

	return 0;
}

int testRanker (const void *tin, const void *p1in, const void *p2in) {
	const int *tt, *pp1, *pp2;

	tt = ((const int *)(tin));
	pp1 = ((const int *)(p1in));
	pp2 = ((const int *)(p2in));

	return (abs(*tt-*pp1) == abs(*tt-*pp2))?0:(abs(*tt-*pp1) < abs(*tt-*pp2))?1:2;
}

struct nodeID * networkInitialize(char * my_addr, int port)
{
	struct nodeID *myID;

#ifdef WIN32
	WSADATA      		wsaData;
	short              	wVersionRequested = 0x101;

	// Initialize Winsock version 2.2
	if (WSAStartup( wVersionRequested, &wsaData ) == -1) 
	{
		printf("Root: WSAStartup failed with error %ld\n", WSAGetLastError());
		return -1;
	}
#endif

	funct = testRanker;

	myID = net_helper_init(my_addr, port, "");
	if (myID == NULL) {
		fprintf(stderr, "networkInitialize: Error creating my socket (%s:%d)!\n", my_addr, port);
		return NULL;
	}

	my_metaDataParam.id = my_metadata;
	my_metaDataParam.is_source = is_peer_source();
	my_metaDataParam.number_of_BFrames = 2;
	my_metaDataParam.video_layer_number = 3;	
	sprintf(my_metaDataParam.ip, "%s", my_addr);
	my_metaDataParam.port = port;
	
//	topoInit(myID, &my_metaDataParam, sizeof(my_metaDataParam), "period=1000000,bootstrap_period=1000000");
	topoInit(myID, &my_metaDataParam, sizeof(my_metaDataParam), "period=50000,bootstrap_period=50000");
	tmanInit(myID,&my_metaDataParam, sizeof(my_metaDataParam),funct,"period=0.05");

	return myID;
}

void sendStatus(unsigned char * buff, unsigned char * msg, int port)
{				
	//Open UDP socket
	int result, ret;
	char laddress[256] = {0};

	if(loopback_disable == 0)
		strcpy(laddress, "127.0.0.1");
	else
		strcpy(laddress, get_external_ip());

	if(fd_status == -1)
	{
		fd_status= socket(AF_INET, SOCK_STREAM, 0);

		struct sockaddr_in address;
		memset((char *) &address, 0, sizeof(address));
		address.sin_family 	= AF_INET; 
#ifdef WIN32
		address.sin_addr.S_un.S_addr 	= inet_addr("127.0.0.1");
#else
//		address.sin_addr.s_addr 		= htonl(INADDR_ANY);
#endif

		if(inet_pton(AF_INET, laddress, &address.sin_addr)<=0)
		{
			fprintf(stderr, "P2P_Streamer: Error inet_pton set IP address %s for TCP socket\n", laddress);
			close(fd_status);
			fd_status = -1;

			return;
		} 

		address.sin_port 	= htons(port);

		result = connect(fd_status, (struct sockaddr *)&address, sizeof(address));
		if(result == -1){
			fprintf(stderr, "P2P_Streamer: could not TCP connect to the port: %d\n", port);
			close(fd_status);
			fd_status = -1;
			exit(-1);
			
			return;
		}
	}

	sprintf(buff, 
		"POST /P2PStatus HTTP/1.0\r\n"
		"Host: 127.0.0.1:%d\r\n"
		"Connection: keep-alive\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		"%s",
		port,
		strlen(msg),
		msg);

	ret = send(fd_status, buff, strlen(buff), 0);
	if(ret < 0)
	{
		close(fd_status);

		fd_status= socket(AF_INET, SOCK_STREAM, 0);

		struct sockaddr_in address;
		memset((char *) &address, 0, sizeof(address));
		address.sin_family 	= AF_INET; 
#ifdef WIN32
		address.sin_addr.S_un.S_addr 	= inet_addr("127.0.0.1");
#else
//		address.sin_addr.s_addr 		= htonl(INADDR_ANY);
#endif

		if(inet_pton(AF_INET, laddress, &address.sin_addr)<=0)
		{
			fprintf(stderr, "P2P_Streamer: Error inet_pton set IP address %s for TCP socket\n", laddress);
			close(fd_status);
			fd_status = -1;

			return;
		} 

		address.sin_port 	= htons(port);

		result = connect(fd_status, (struct sockaddr *)&address, sizeof(address));
		if(result == -1){
			fprintf(stderr, "P2P_Streamer: could not TCP connect to the port: %d\n", port);
			close(fd_status);
			fd_status = -1;

			return;
		}

	}
	
}


int main(int argc, char *argv[])
{
	int 						ret = 0;

	unsigned long 				recvFromSrcHandle;
	RecvFromSrcSettings_t		recvFromSrcSettings;

	unsigned long				peerMessagesHandle;
	PeerMessagesSettings_t		peerMessagesSettings;

	unsigned long				peerSamplingHandle;
	PeerSamplingSettings_t		peerSamplingSettings;

	unsigned long				chunkTradingHandle;
	ChunkTradingSettings_t		chunkTradingSettings;

	unsigned long				chunkSendingHandle;
	ChunkSendingSettings_t	chunkSendingSettings;

	unsigned long 				totalpackets;
	int						bufSize;

  	struct chunk_buffer *		chunkbuf = NULL;
	unsigned int				max_chunk_buffer_size = 800; // packets
	char 					config[32];

	struct peerset *			peerinfo;
	
	struct nodeID *			rcv_msg_sock;
	struct nodeID *			snd_srv_sock;

 	unsigned long				onesecondclock;
 	unsigned long				onesecondclock2;
 	unsigned long				twosecondclock;
 	unsigned long				twosecondclock2;
	
//	char 					statFilename[1024] = {0};
	
#ifdef WIN32
	msg_sent_from_p2p_mutex 		= CreateMutex(NULL, FALSE, "SRC_P2P_SHARED_MUTEX");
	msg_sent_from_p2p_id 			= CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, "SRC_P2P_SHARED_MEM");
	shared_memory_sent_from_p2p 	= (char*) MapViewOfFile(msg_sent_from_p2p_id, FILE_MAP_ALL_ACCESS, 0, 0, 1024);

	cntlr_msg_recv_from_player_mutex 		= CreateMutex(NULL, FALSE, "PLR_P2P_CNTLR_SHARED_MUTEX");
	cntlr_msg_recv_from_player_id 			= CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, "PLR_P2P_CNTLR_SHARED_MEM");
	shared_cntlr_memory_recv_from_player 	= (char*) MapViewOfFile(cntlr_msg_recv_from_player_id, FILE_MAP_ALL_ACCESS, 0, 0, 1024);

	memset(shared_memory_sent_from_p2p, 0, 1024);
	memset(shared_cntlr_memory_recv_from_player, 0, 1024);
#endif
	
	fprintf(stderr,"--- P2P Streamer Application ---\n");

	//2Defaults
	app_defaults();
		
	cmdline_parse(argc, argv);

/*
	if(is_peer_source() == 0)
 		sprintf(statFilename, "StatsPlayer.txt");
 	else
 		sprintf(statFilename, "StatsSource.txt");

	FILE * fptr = fopen(statFilename, "wt");
	fclose(fptr);
*/

	//2 Application initialization

	//Initialize network
	rcv_msg_sock = networkInitialize(local_ip, local_port);

	//Initialize chunk buffer
	sprintf(config, "size=%d", max_chunk_buffer_size); // 2 seconds
	chunkbuf = cb_init(config);

	//Initializing chunk delivery
	chunkDeliveryInit(rcv_msg_sock);

	//Initializing chunk signaling
	chunkSignalingInit(rcv_msg_sock);

	//Initialize peerset (Peer info)
	peerinfo = peerset_init(0);

	// Create semaphore to make peer sampling updates atomic
	createSemaphore(&psmplSema4Handle, 1);

	// If peer is not source, then connect with the peer and add it to sampling list 	
	if(is_peer_source() == 0)
	{
		printf("srv_ip: %s srv_port: %d\n", srv_ip, srv_port);
		
		snd_srv_sock = create_node(srv_ip, srv_port);
		if (snd_srv_sock == NULL) 
		{
			fprintf(stderr, "Error creating server socket (%s:%d)!\n", srv_ip, srv_port);
			return -1;
		}

		//topoAddNeighbour(snd_srv_sock ,NULL,0);
	}

	//2Setting Components

	// Peer Sampling settings
	peerSamplingSettings.snd_ssock					= snd_srv_sock;
	peerSamplingSettings.source						= is_peer_source();
	peerSamplingSettings.peerinfo					= peerinfo; 
	peerSamplingSettings.period_msec				= 10;//250;

	// Peer Message settings
	peerMessagesSettings.snd_ssock					= snd_srv_sock;
	peerMessagesSettings.rcv_msock					= rcv_msg_sock;
	peerMessagesSettings.source					= is_peer_source();
	peerMessagesSettings.chunkbuf					= chunkbuf; 
	peerMessagesSettings.peerinfo					= peerinfo; 
	peerMessagesSettings.period_msec				= 15;//250;

	// Chunk Trading settings
	chunkTradingSettings.chunkbuf				= chunkbuf; 
	chunkTradingSettings.peerinfo				= peerinfo; 
	chunkTradingSettings.period_msec			= 25;//250;

	//2Start Components
	if(is_peer_source() == 0)
	{
		chunkSendingSettings.chunkbuf				= chunkbuf;
		chunkSendingSettings.max_chunk_size		= (MAX_VIDEO_BITRATE*2)/(8*30);
		chunkSendingSettings.min_chunk_delay_ms	= 500;	// msec	
		chunkSendingSettings.player_port			= player_client_port;
		
		createThread(ChunkSendingThread, 
					&chunkSendingSettings,
					10,
					(32*1024*1024), // stack size
					&chunkSendingHandle);	
	}
	
	createThread(PeerSamplingThread, 
				&peerSamplingSettings,
				12,
				(32*1024*1024), // stack size
				&peerSamplingHandle);	

	createThread(PeerMessagesThread, 
				&peerMessagesSettings,
				12,
				(32*1024*1024), // stack size
				&peerSamplingHandle);	

	createThread(ChunkTradingThread, 
				&chunkTradingSettings,
				12,
				(32*1024*1024), // stack size
				&chunkTradingHandle);

	if(is_peer_source())
	{
		// Receive chunks from source settings
		recvFromSrcSettings.max_chunk_size 			= (MAX_VIDEO_BITRATE*4)/(8*30);
		recvFromSrcSettings.recv_source_listen_port	= src_server_port;
		recvFromSrcSettings.chunkbuf				= chunkbuf; 
		recvFromSrcSettings.peerinfo				= peerinfo; 

		createThread(RecvFromSrcThread, 
					&recvFromSrcSettings,
					10,
					(32*1024*1024), // stack size
					&recvFromSrcHandle);
	}


	cumVideoInputBits = 0;
	cumNetworkInputBits = 0;
	total_secs_count = 1;

	oSleep(100);
	
	onesecondclock = timer_msec();
	twosecondclock = timer_msec();
	
	while( (recvFromSrcSettings.threadStatus != STOPPED_THREAD) || (peerSamplingSettings.threadStatus != STOPPED_THREAD) || (peerMessagesSettings.threadStatus != STOPPED_THREAD) )
	{

#if 0
		if((timer_msec() - twosecondclock2) > 2000)
		{
			struct peer **peers;
			char addr[256];
			int i;
			
			acquireSemaphore(psmplSema4Handle, -1);
			
			peers = peerset_get_peers(peerinfo);
			
			printf("+++ peerset +++\n");
			for (i = 0; i < peerset_size(peerinfo); i++) 
			{
				node_addr(peers[i]->id, addr, 256);
				printf("\t%d: %s\n", i, addr);
			}
			printf("--- peerset ---\n");
			
			releaseSemaphore(psmplSema4Handle);

			twosecondclock2 = timer_msec();
		}
#endif

#if 0
		if((timer_msec() - twosecondclock) > 2000)
		{
			const struct nodeID **neighbours;
			char addr[256];
			int n, i;
			struct peer ** peers;
			const uint8_t *mdata;
			int msize;
			pmetaDataStruct_t pmetaDataParam;	   

			static int count = 0;

			acquireSemaphore(psmplSema4Handle, -1);
			neighbours = topoGetNeighbourhood(&n);
			mdata = topoGetMetadata(&msize);
			printf("\t-- I have %d neighbours - %d:\n",n, is_peer_source());
			for (i = 0; i < n; i++) {
				const int *d;
				d = (const int*)((mdata+i*msize));
				pmetaDataParam = (pmetaDataStruct_t)d;

				node_addr(neighbours[i], addr, 256);
				printf("\t\t%d:: %d: %s : [%d,%d,%s,%d]\n", count, i, addr, 
					pmetaDataParam->number_of_BFrames, pmetaDataParam->video_layer_number, pmetaDataParam->ip, pmetaDataParam->port);

				//if(count == 30)
			//	{
				//	printf("Removing node !!!\n");
				//	topoRemoveNeighbour(neighbours[i]);
			//	}
				count++;
			}
			releaseSemaphore(psmplSema4Handle);

			twosecondclock = timer_msec();
		}
#endif

		unsigned char print_buffer[4096];

		if((timer_msec() - onesecondclock2) > 100)
		{
			if(shared_memory_sent_from_p2p != NULL)
			{
				int size;

				acquireSemaphore(psmplSema4Handle, -1);
				cb_get_chunks(chunkbuf, &size);
				releaseSemaphore(psmplSema4Handle);
			
#ifdef WIN32
				WaitForSingleObject(msg_sent_from_p2p_mutex, INFINITE);
#endif				
				sprintf(shared_memory_sent_from_p2p, "%s"
									"$%03u"
									"$%03u"
									"$%u"
									"$%u"
									"$%03u"
									"$%03u"
									"$%u"
									"$",
									print_buffer,
									maxUploadDataSize, 
									maxDownloadDataSize, 
									size,//smax_chunk_buffer_size,
									start_up_latency_ms,
									(nw_upload_data_size*8)/1024,
									(nw_download_data_size*8)/1024,
									network_delay_ms);				

#ifdef WIN32
				ReleaseMutex(msg_sent_from_p2p_mutex);
#endif
			}


			//printf("[V:%d fps] Enc:[V:%07.2f kbps] NW:[%07.2f kbps] CumBR:[V:%07.2f kbps, NW:%07.2f kbps]\n", videoFrameCnt, (float)(videoInputBits)/1024.0, (float)(networkInputBits/1024.0), (float)(cumVideoInputBits)/(total_secs_count*1024.0), (float)(cumNetworkInputBits)/(total_secs_count*1024.0));

			if(is_peer_source() == 0)
			{ 
				if(shared_cntlr_memory_recv_from_player != NULL)
				{
					char temp[128] = {0};
					char temp2[128] = {0};
					char *cptr1, *cptr2;
#ifdef WIN32
					WaitForSingleObject(cntlr_msg_recv_from_player_mutex, INFINITE);
#endif

					memcpy(temp, shared_cntlr_memory_recv_from_player, strlen(shared_cntlr_memory_recv_from_player));

#ifdef WIN32
					ReleaseMutex(cntlr_msg_recv_from_player_mutex);
#endif
					cptr1 = temp;
					cptr2 = strstr(temp,"$");
					if(cptr2 != NULL)
					{
						memset(temp2, 0, 128);
						memcpy(temp2, cptr1, (cptr2-cptr1));
						
						int BFrame = atoi(temp2+3);
						temp2[2] = '\0';
						int VLayer = atoi(temp2);

						if(my_metaDataParam.number_of_BFrames != BFrame)
						{
							setBFrameCount(BFrame);
							//fprintf(stderr, "B FRAME: %s\n", shared_cntlr_memory_recv_from_player);
							//strcpy(shared_cntlr_memory, "test");
						}

						if(my_metaDataParam.video_layer_number != VLayer)
						{
							setVideoLayer(VLayer);
							//fprintf(stderr, "B FRAME: %s\n", shared_cntlr_memory_recv_from_player);
							//strcpy(shared_cntlr_memory, "test");
						}			
					}
				}
			}

			onesecondclock2 = timer_msec();
		}

		if((timer_msec() - onesecondclock) > 1000)
		{
			int i, firstPrint;
			unsigned char send_buffer[4096];

			//printf("chunkInputCnt: %d\n", chunkInputCnt);
			//if(((float)(videoInputBits)) == 0.000)
			if((chunkInputCnt == 0) && (chunkOutputCnt == 0))
			{
				if(is_peer_source())
				{	
					sendStatus(send_buffer, "", src_server_port-2);
				}

				oSleep(100);
				continue;
			}

#if 1
			printf("ChunkRate: [Out: %02d/s In: %02d/s] Upload: [", chunkOutputCnt, chunkInputCnt);
			sprintf(print_buffer, "ChunkRate: [Out: %02d/s In: %02d/s]\nUpload: [", chunkOutputCnt, chunkInputCnt);
/*
			firstPrint = 0;
			for(i=0; i<10; i++)
			{
				if(logUploadDataSize[i] != 0)
				{
					if(strlen(logUploadAddr[i]) != 0)
						firstPrint += logUploadDataSize[i];
				}

				logUploadDataSize[i] = 0;
			}
*/
			firstPrint = (upload_data_size*8)/1024;

			if(maxUploadDataSize < firstPrint)
				maxUploadDataSize = firstPrint;
			
			printf("%03u kbps", firstPrint);
			sprintf(print_buffer+strlen(print_buffer), "%03u kbps", firstPrint);

			printf("] Download: [");
			sprintf(print_buffer+strlen(print_buffer), "]\nDownload: [");
/*
			firstPrint = 0;
			for(i=0; i<10; i++)
			{
				if(logDownloadDataSize[i] != 0)
				{
					if(strlen(logDownloadAddr[i]) != 0)
						firstPrint += logDownloadDataSize[i];
				}

				logDownloadDataSize[i] = 0;
			}
*/
			firstPrint = (download_data_size*8)/1024;

			if(maxDownloadDataSize < firstPrint)
				maxDownloadDataSize = firstPrint;

			printf("%03u kbps", firstPrint);
			sprintf(print_buffer+strlen(print_buffer), "%03u kbps", firstPrint);
			
			printf("] Peers: [");
			sprintf(print_buffer+strlen(print_buffer), "]\nPeers: [");
			firstPrint = 0;
			{
				const struct nodeID **neighbours;
				char addr[256];
				int n, i;
				struct peer ** peers;

				peers = peerset_get_peers(peerinfo);
				for (i = 0; i < peerset_size(peerinfo); i++) 
				{
					node_addr(peers[i]->id, addr, 256);

					if(firstPrint == 1)
					{
						printf(",");
						sprintf(print_buffer+strlen(print_buffer), ",");
					}
					printf("%s", addr);
					if(strlen(print_buffer) > 4080)
						break;
					sprintf(print_buffer+strlen(print_buffer), "%s", addr);
				
					printf(" (B: %d)", getBFrameCount(peers[i]->id));
					sprintf(print_buffer+strlen(print_buffer), " (B: %d)", getBFrameCount(peers[i]->id));

					printf(" (L: %d)", getVideoLayer(peers[i]->id));
					sprintf(print_buffer+strlen(print_buffer), " (B: %d)", getVideoLayer(peers[i]->id));					
					
					firstPrint = 1;
				}
			}			
			printf("]\n");
			sprintf(print_buffer+strlen(print_buffer), "]\n");

			if(is_peer_source())
			{				
				sendStatus(send_buffer, print_buffer, src_server_port-2);
			}
#else
			{
				FILE * statFile = fopen(statFilename, "at");
				
				fprintf(statFile, "\n****** APPLICATION ********\n");
				fprintf(statFile, "%s", print_buffer);
				fprintf(statFile, "Max Upload: [%03u kbps]\n", maxUploadDataSize);
				fprintf(statFile, "Max Download: [%03u kbps]\n", maxDownloadDataSize);
				fprintf(statFile, "Chunk Buffer: [%u chunks]\n", max_chunk_buffer_size);
				if(is_peer_source() == 0)
				{
					fprintf(statFile, "Startup Latency: [%u ms]\n", start_up_latency_ms);
					fprintf(statFile, "Playback Delay: [%u ms]\n", playback_delay_ms);
					fprintf(statFile, "Number of chunks dropped: [%u]\n", chunk_miss_ratio);				
					fprintf(statFile, "Chunk Miss Ratio: [%.02f Precent]\n", (double)(chunk_miss_ratio*100.0)/(double)max_chunk_id);		
					if(wan_emulation_enable == 0)
					{
						fprintf(statFile, "CRC Error: 0\n");
						fprintf(statFile, "FEC Correction: 0\n");
					}
					else
					{
						fprintf(statFile, "CRC Error: 0\n");
						fprintf(statFile, "FEC Correction: 0\n");
					}
				}
				else
				{
					fprintf(statFile, "CRC Error: 0\n");
					fprintf(statFile, "FEC Correction: 0\n");
				}

				fprintf(statFile, "------ NW EMULATOR ------\n");
				fprintf(statFile, "Network Upload: [%03u kbps]\n", (nw_upload_data_size*8)/1024);
				fprintf(statFile, "Network Download: [%03u kbps]\n", (nw_download_data_size*8)/1024);
				fprintf(statFile, "Network Delay: [%u ms]\n", network_delay_ms);

				fclose(statFile);
			}
#endif			

			videoFrameCnt = 0;

			cumVideoInputBits += videoInputBits;
			cumNetworkInputBits += networkInputBits;
			total_secs_count++;

			videoInputBits = 0;
			networkInputBits = 0;

			chunkOutputCnt = 0;
			chunkInputCnt = 0;

			upload_data_size = 0;
			download_data_size = 0;			

			nw_upload_data_size = 0;			
			nw_download_data_size = 0;			

			onesecondclock = timer_msec();
		}


		//fprintf(stderr,"... sleep ...\n");
		oSleep(10);
	}
	
	cb_destroy(chunkbuf);
	
	return 0;
}

