#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include "root.h"
#include <getopt.h>

#ifdef WIN32
#include <process.h>
#include <io.h>
#include <stdio.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/shm.h>
#endif

#define STAMP_CRC			2490263706

#define CONFIG_FILE_NAME "PlayerConf.txt"

char		srv_ip[256] = {0};
int		srv_port = 0;
char		local_ip[256] = {0};
int		local_port = 0;
char		channel_server_ip[256] = {0};
int		channel_server_port = 0;
int		player_server_port = 0;
char		external_ip[256] = {0};
int		external_port = 0;
int		loopback_disable = 0;
int		headless = 0;

int		wan_emulation_enable = 0;
int		upload_bitrate_limit = 1000000;
int		download_bitrate_limit = 1000000;

int		fec_enable = 0;
int		debug_level = 0;

#ifdef WIN32
PROCESS_INFORMATION p2p_streamer;
#else
pid_t 	p2p_streamer;
#endif

extern unsigned long videoFrameCnt;
extern unsigned long videoInputBits;
extern unsigned long videoInputIBits;
extern unsigned long videoInputPBits;
extern unsigned long videoInputBBits;
extern unsigned long audioFrameCnt;
extern unsigned long audioInputBits;
extern unsigned long networkInputBits;

extern int	 guiSettingsWindow;

extern int errorCounter;

unsigned long cumVideoInputBits;
unsigned long cumAudioInputBits;
unsigned long cumNetworkInputBits;
unsigned long	total_secs_count;

unsigned int	playback_delay_ms = 0;

unsigned int	chunk_miss_ratio = 0;
unsigned int	max_chunk_id = 0; 

unsigned int	videoFrameDrops = 0;
unsigned int	audioFrameDrops = 0;

unsigned int crc_error = 0;
unsigned int fec_correction = 0;

unsigned int currAudioTimeStamp = 0;
unsigned int currVideoTimeStamp = 0;

int dropAVSyncVideoPackets = 0;
int dropAVSyncAudioPackets = 0;
int stopPlayer = 0;

unsigned int	videoFrameErrors = 0;
unsigned int	audioFrameErrors = 0;

unsigned int	stableFrameErrorsCount = 0;
unsigned int	unStableFrameErrorsCount = 0;

int 	videolayerPoint[4] = {0};
int	stable_layer_thr = 0;

int 	numBFrames = 0;
int 	videoLayer = 3;
int 	autoQuality = 1;

typedef struct channelStruct
{
	char     	name[256];

	char     	ip[256];
	int 		port;
	unsigned int id;
} channelStruct_t, *pchannelStruct_t;

channelStruct_t	channelList[256];
unsigned int		totalChannels = 0;
unsigned int		selectChannelIndex = 0;

int systemVolume = 128;

unsigned long 					fecDecHandle;
FECDecSettings_t				fecDecSettings;

unsigned long 					deChunkiserHandle;
GDeChunkiserSettings_t		deChunkiserSettings;

unsigned long					videoDecoderPlayerHandle;
VideoDecoderPlayerSettings_t	videoDecoderPlayerSettings;

unsigned long					audioDecoderPlayerHandle;
AudioDecoderPlayerSettings_t	audioDecoderPlayerSettings;

unsigned long 					guiHandle;
GUISettings_t					guiSettings;


unsigned long					MuxPlayerHandle;

int							network_one_sec_cnt = 0;
int							sum_network_delay = 0;
int							avg_network_delay = -1;

int							video_scalability = -1;

#ifdef WIN32
HANDLE						msg_recv_from_p2p_mutex;
HANDLE						msg_recv_from_p2p_id;
#else
int 							msg_recv_from_p2p_id; 
#endif
char* 						shared_memory_recv_from_p2p;
char *						p2p_buffer;

#ifdef WIN32
HANDLE						cntlr_msg_sent_from_player_mutex;
HANDLE						cntlr_msg_sent_from_player_id;
#else
int 							cntlr_msg_sent_from_player_id; 
#endif
char* 						shared_cntlr_memory_sent_from_player;

unsigned long timer_msec(void){
	struct timeval tdecA;
	unsigned long msec, usec;

#ifdef WIN32
	mingw_gettimeofday(&tdecA, NULL);
#else
	gettimeofday(&tdecA, NULL);
#endif

	usec = (tdecA.tv_sec*1000000)+tdecA.tv_usec;

	msec = usec/1000;

	return msec;
}

static void sigterm_handler(int sig)
{
    stopP2PStreamer();
    exit(123);
}

static void app_defaults(void)
{
	strcpy(local_ip, "127.0.0.1");
	local_port = 26358;

	strcpy(channel_server_ip, "127.0.0.1");
	channel_server_port = 9000;

	strcpy(srv_ip, "127.0.0.1");
	srv_port = 1111;

	strcpy(external_ip, "127.0.0.1");
	external_port = 26358;
	loopback_disable = 0;

	player_server_port = local_port+2;//26360;

	totalChannels = 0;
	selectChannelIndex = 0;

	stable_layer_thr = 0;

	network_one_sec_cnt = 0;
	sum_network_delay = 0;
	avg_network_delay = -1;

	video_scalability = -1;
	debug_level = 0;
}

static void cmdline_parse(int argc, char *argv[])
{
	int o;

	while ((o = getopt(argc, argv, "P:s:I:X:x:C:p:l:h:e:u:d:f:g:")) != -1) {
		switch(o) {
		      case 'P':
				local_port =  atoi(optarg);
				player_server_port = local_port+2;
			break;
#if 0
		      case 'I':
				sprintf(local_ip, "%s", optarg);
			break;
#endif
		      case 's':
				player_server_port =  atoi(optarg);
		      break;
			case 'I':
				sprintf(local_ip, "%s", optarg);
			break;
			case 'X':
				sprintf(external_ip, "%s", optarg);
			break;
			case 'x':
				external_port = atoi(optarg);
			break;
			case 'C':
				sprintf(channel_server_ip, "%s", optarg);
			break;
			case 'p':
				channel_server_port =  atoi(optarg);
			break;
			case 'l':
				loopback_disable = 1;
			break;
			case 'h':
				headless = 1;
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
			case 'f':
				fec_enable = atoi(optarg);
			break;			
			case 'g':
				debug_level = atoi(optarg);
			break;			
			default:
				printf("STAMP player application version 2.4.1\n");
				printf("P2P Audio/Video Player application\n");
				printf("Usage: ./player.bin [options]\n\n");

				printf("Options:\n");
				printf("\t -I <string>\t\t-- local IP (default 12\.0.0.1)\n");
				printf("\t -X <string>\t\t-- external WAN IP (default 127.0.0.1)\n");
				printf("\t -P <int>\t\t-- local port (default 26358)\n");
				printf("\t -x <int>\t\t-- external port (default 26358)\n");
				printf("\t -s <int>\t\t-- `internal port for loopback communication with P2P app (default local port+2)\n");
				printf("\t -C <string>\t\t-- channel server ip (default 127.0.0.1)\n");
				printf("\t -p <int>\t\t-- channel server port (default 9000)\n");
				printf("\t -l <int>\t\t-- use actual IP (1) / 127.0.0.1 (0) in loopback communication (default 0)\n");
				printf("\t -f <int>\t\t-- enable (1) / disable (0) FEC (default 0)\n");
				printf("\t -g <int>\t\t-- enable (1) / disable (0) Debug (default 0)\n");
				printf("\t -h <int>\t\t-- enable (1) / disable (0) headless player mode (default 0)\n");
				printf("\t -e <int>\t\t-- enable (1) / disable (0) network emulation (default 0)\n");
				printf("\t\t -u <int>\t-- upload bitrate limit in bits/sec (default 1000000)\n");
				printf("\t\t -d <int>\t-- download bitrate limit in bits/sec (default 1000000)\n");

				printf("\n\n");

			exit(-1);
		}

	}

	fprintf(stderr,"local_ip : %s\n",local_ip);
	fprintf(stderr,"external_ip : %s\n",external_ip);
	fprintf(stderr,"local_port : %d\n",local_port);
	fprintf(stderr,"external_port : %d\n",external_port);
	fprintf(stderr,"channel_server_ip : %s\n",channel_server_ip);
	fprintf(stderr,"channel_server_port : %d\n",channel_server_port);
	fprintf(stderr,"loopback_disable : %d\n",loopback_disable);
	fprintf(stderr,"headless : %d\n",headless);
	fprintf(stderr,"WAN emulation: %d\n", wan_emulation_enable);
	fprintf(stderr,"upload_bitrate_limit: %d\n", upload_bitrate_limit);
	fprintf(stderr,"download_bitrate_limit: %d\n", download_bitrate_limit);
	fprintf(stderr,"loopback_disable: %d\n", loopback_disable);
	fprintf(stderr,"fec_enable: %d\n", fec_enable);	
	fprintf(stderr,"debug_level: %d\n", debug_level);	

}

static int getWordValue(FILE *filePtr)
{
	char buff[32];
	int		value = -1;
	int 		readSize;

	fgets(buff,31, filePtr);
	
	if(strlen(buff) > 0)
	{
		buff[strlen(buff)-1] = '\0';

		value = atoi(buff);
	}

	return value;
}

static void setWordValue(FILE *filePtr, int value)
{
	fprintf (filePtr, "%d\n", value);
}

static void getStringValue(FILE *filePtr, char *stringVal)
{

	stringVal[0] = '\0';
	
	fgets(stringVal,127, filePtr);

	if(strlen(stringVal) > 0)
		stringVal[strlen(stringVal)-1] = '\0';

	//printf("\n getStringValue :: %s %d",stringVal,strlen(stringVal));

}

static void setStringValue(FILE *filePtr, char *stringVal)
{
	fprintf (filePtr, "%s\n", stringVal);
}

void SaveConfiguration()
{
	FILE *configFileHandle;
	char configFileName[256];

	strcpy(configFileName,CONFIG_FILE_NAME);

	//printf("\n SaveConfiguration ++ ");
	
	configFileHandle = fopen(configFileName, "wb");
		
	setStringValue(configFileHandle,local_ip);
	setStringValue(configFileHandle,external_ip);
	setStringValue(configFileHandle,channel_server_ip);
	setWordValue(configFileHandle,channel_server_port);
	setWordValue(configFileHandle,local_port);
	setWordValue(configFileHandle,external_port);

	fclose(configFileHandle);
}

void InitPlayerConfiguration()
{
	FILE *configFileHandle;
	char configFileName[256];

	char stringValue[256];

	strcpy(configFileName,CONFIG_FILE_NAME);

	if (configFileHandle = fopen(configFileName, "rb"))
	{
		printf("\n Configuration File Found");

		getStringValue(configFileHandle,stringValue);
		strncpy(local_ip,stringValue,15);

		getStringValue(configFileHandle,stringValue);
		strncpy(external_ip,stringValue,15);

		getStringValue(configFileHandle,stringValue);
		strncpy(channel_server_ip,stringValue,15);
		
		channel_server_port = getWordValue(configFileHandle);

		local_port = getWordValue(configFileHandle);

		external_port = getWordValue(configFileHandle);

		fclose(configFileHandle);
	}
	else
	{
		SaveConfiguration();
	}
	
}

void GetConfiguration(char * loc_ip, char * extrn_ip, char * chn_ip, int * chn_port, int *loc_port, int *extrn_port)
{
	strcpy(loc_ip, local_ip);
	strcpy(extrn_ip, external_ip);
	sprintf(chn_ip, channel_server_ip);
	*chn_port = channel_server_port;
	*loc_port = local_port;
	*extrn_port = external_port;
}

void SetConfiguration(char * loc_ip, char * extrn_ip, char * chn_ip, int chn_port, int loc_port, int extrn_port)
{
	strcpy(local_ip, loc_ip);
	strcpy(external_ip, extrn_ip);
	strcpy(channel_server_ip, chn_ip);
	channel_server_port = chn_port;
	local_port = loc_port;
	external_port = extrn_port;
}

#ifdef WIN32 
int startP2PStreamer()
{
	int 						ret = 0;
	char 					params_buffer[1024];
	STARTUPINFO 			si;

	sprintf(params_buffer, "p2p_streamer.exe -I %s -X %s -P %d -x %d -m %u -i %s -p %d  -g %d -c %d -e %d -u %d -d %d", 
						local_ip, external_ip, local_port, external_port,
						channelList[selectChannelIndex].id,
						channelList[selectChannelIndex].ip,
						channelList[selectChannelIndex].port,
						debug_level,
						player_server_port,
						wan_emulation_enable,
						upload_bitrate_limit,
						download_bitrate_limit
						);

   	memset(&si,0,sizeof(si));
	si.cb = sizeof(si);
	memset(&p2p_streamer, 0, sizeof(PROCESS_INFORMATION) );

	printf("startP2PStreamer: %s\n", params_buffer);
	if(!CreateProcess(NULL,
				  	params_buffer,
				  	NULL,
				  	NULL,
				  	TRUE,
				  	0,
				  	NULL,
				  	NULL,
				  	&si,
					&p2p_streamer))
	{
		printf("Error: could not start P2P streamer !!!\n");
		exit(-1);
	}

	return 0;
}

int stopP2PStreamer()
{
	TerminateProcess(((PROCESS_INFORMATION*)&p2p_streamer)->hProcess, 0);

	return 0;
}
#else
int startP2PStreamer()
{
	char p2pbin[256] = {0};
	char* params_array[32];

	char str_local_port[256] 	= {0};
	char str_external_port[256] 	= {0};
	char str_srv_port[256] 	= {0};
	char str_player_port[256] 	= {0};
	char str_metadata[256] 	= {0};
	char str_wan_emulation[256] 	= {0};
	char str_upload_bitrate[256] 	= {0};
	char str_download_bitrate[256] 	= {0};
	char str_shmem[256] 	= {0};
	char str_shcntlrmem[256] 	= {0};
	char str_debug_level[256] 	= {0};

	int indx = 0;

	sprintf(p2pbin, "%s", "p2p_streamer.bin");
	sprintf(str_local_port, "%d", local_port);
	sprintf(str_external_port, "%d", external_port);
	sprintf(str_srv_port, "%d", channelList[selectChannelIndex].port);//srv_port);
	sprintf(str_player_port, "%d", player_server_port);
	sprintf(str_metadata, "%d", channelList[selectChannelIndex].id);
	sprintf(str_wan_emulation, "%d", wan_emulation_enable);
	sprintf(str_upload_bitrate, "%d", upload_bitrate_limit);
	sprintf(str_download_bitrate, "%d", download_bitrate_limit);
	sprintf(str_shmem, "%u", msg_recv_from_p2p_id);
	sprintf(str_shcntlrmem, "%u", cntlr_msg_sent_from_player_id);
	sprintf(str_debug_level, "%d", debug_level);
	
	params_array[indx] = p2pbin;
	indx++;
	params_array[indx] = "-I";
	indx++;
	params_array[indx] = local_ip;
	indx++;
	params_array[indx] = "-X";
	indx++;
	params_array[indx] = external_ip;//external_ip;	
	indx++;
	params_array[indx] = "-P";
	indx++;
	params_array[indx] = str_local_port;
	indx++;
	params_array[indx] = "-x";
	indx++;
	params_array[indx] = str_external_port;
	indx++;
	params_array[indx] = "-i";
	indx++;
	params_array[indx] = channelList[selectChannelIndex].ip;//srv_ip;
	indx++;
	params_array[indx] = "-p";
	indx++;
	params_array[indx] = str_srv_port;
	indx++;
	params_array[indx] = "-g";
	indx++;
	params_array[indx] = str_debug_level;
	indx++;
	params_array[indx] = "-c";
	indx++;
	params_array[indx] = str_player_port;
	indx++;
	params_array[indx] = "-m";
	indx++;
	params_array[indx] = str_metadata;
	indx++;
//*
	params_array[indx] = "-z";
	indx++;
	params_array[indx] = str_shmem;
	indx++;
	params_array[indx] = "-w";
	indx++;
	params_array[indx] = str_shcntlrmem;
	indx++;
//*/
	if(wan_emulation_enable == 1)
	{
		params_array[indx] = "-e";
		indx++;
		params_array[indx] = str_wan_emulation;
		indx++;
		params_array[indx] = "-u";
		indx++;
		params_array[indx] = str_upload_bitrate;
		indx++;
		params_array[indx] = "-d";
		indx++;
		params_array[indx] = str_download_bitrate;
		indx++;
	}
	if(loopback_disable == 1)
	{
		params_array[indx] = "-l";
		indx++;
		params_array[indx] = "1";
		indx++;
	}
	params_array[indx] = NULL;
	
#if 1
	int pid = fork();
	if(pid == 0)
	{
		execv(p2pbin, params_array);
		printf("Error: could not start P2P streamer !!!\n");
		exit(-1);
	}
	else {
		p2p_streamer = pid;
	}
#endif

	return 0;
}

int stopP2PStreamer()
{
	if(p2p_streamer > 0)
	{
		char cmd[255]; 
		sprintf(cmd, "kill %d", p2p_streamer);
#if 1
		system(cmd);
#endif
	}

	return 0;
}
#endif

int loadChannelList(pchannelStruct_t list)
{
	fprintf(stderr, "+++ loadChannelList +++\n");

	int sockfd = 0;
	int bytesReceived = 0;
	char recvBuff[1024*10] = {NULL};
	char singleLine[1024] = {NULL};
	char *ptrBuff = NULL;
	
	int 		err = 0;

	char 	fstr0[256];
	char 	fstr1[256];
	char 	fstr2[256];
	char 	fstr3[256];

	selectChannelIndex = 0;

	memset(recvBuff, '0', sizeof(recvBuff));
	struct sockaddr_in serv_addr;

	/* Create a socket first */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0))< 0)
	{
		printf("\n Error : Could not create socket \n");
		return -1;
	}

	/* Initialize sockaddr_in data structure */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(channel_server_port); // port
//	serv_addr.sin_addr.s_addr = inet_addr(channel_server_ip);
#ifdef WIN32
	serv_addr.sin_addr.S_un.S_addr 	= inet_addr(channel_server_ip);
#else
	//serv_addr.sin_addr.s_addr = inet_addr(channel_server_ip);//htonl(INADDR_ANY);
#endif

	if(inet_pton(AF_INET, channel_server_ip, &serv_addr.sin_addr)<=0)
	{
		fprintf(stderr, "player: Error inet_pton set IP address %s for TCP socket\n", channel_server_ip);
		close(sockfd);

		return;
	} 

	/* Attempt a connection */
	if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)
	{
		printf("\n Error : Connect Failed \n");
		return -1;
	}

	//printf("Connected to ip: %s : %d\n",inet_ntoa(serv_addr.sin_addr.s_addr),ntohs(serv_addr.sin_port));


	long double sz=1;
	/* Receive data in chunks of 256 bytes */

	ptrBuff = recvBuff;

	while(1)
	{
#ifdef WIN32
		bytesReceived = recv(sockfd, ptrBuff , 1024, 0);
#else
		bytesReceived = read(sockfd, ptrBuff , 1024);
#endif

		if(bytesReceived <= 0)
		{
			ptrBuff[0] = '\0';
			break;
		}

		ptrBuff += bytesReceived;
	}

	//printf("\n Reading Done ... \n");

	ptrBuff = recvBuff;
	
	while(1)
	{		
		//printf("\n 1. %s \n",ptrBuff);
		
		err = sscanf(ptrBuff,"%s %s %s %s\n", fstr0, fstr1, fstr2, fstr3);
		if(err < 0)
			break;

		sprintf(singleLine,"%s %s %s %s\n", fstr0, fstr1, fstr2, fstr3);

		//printf("\n 2. %s \n",singleLine);
		
		ptrBuff+=(strlen(singleLine));

		sprintf(list[totalChannels].name, "%s", fstr0);
		sprintf(list[totalChannels].ip, "%s", fstr1);
		list[totalChannels].port = atoi(fstr2);
		list[totalChannels].id = atoi(fstr3);

		fprintf(stderr, "%d. %s %s %d %d\n", totalChannels, list[totalChannels].name, list[totalChannels].ip, list[totalChannels].port, list[totalChannels].id);	
		totalChannels++;
	}

	return 0;

/*
	FILE *	ifptr = NULL;
	int 		err;

	char 	fstr0[256];
	char 	fstr1[256];
	char 	fstr2[256];
	char 	fstr3[256];

	selectChannelIndex = 0;
	
	ifptr=fopen("channel.txt","rt");
	if(ifptr == NULL)
	{
		fprintf(stderr, "loadChannelList not found !!!\n");
		return -1;
	}
	
	while(1)
	{				
		err = fscanf(ifptr,"%s %s %s %s\n", fstr0, fstr1, fstr2, fstr3);
		if(err < 0)
			break;

		sprintf(list[totalChannels].name, "%s", fstr0);
		sprintf(list[totalChannels].ip, "%s", fstr1);
		list[totalChannels].port = atoi(fstr2);
		list[totalChannels].id = atoi(fstr3);

		fprintf(stderr, "%d. %s %s %d %d\n", totalChannels, list[totalChannels].name, list[totalChannels].ip, list[totalChannels].port, list[totalChannels].id);	
		totalChannels++;
	}

	fclose(ifptr);

	return 0;
*/
}

int getChannelName(char * buff, int inc)
{
	int ret = 1;
	int index;
	
	if(inc == -1)
	{
		index = selectChannelIndex-1;
		if(index >= 0)
		{
			selectChannelIndex = index;
			if(headless == 0)
				sprintf(buff, "%s       ", channelList[selectChannelIndex].name);
			else	
				sprintf(buff, "%s", channelList[selectChannelIndex].name);
			ret = 1;
		}
		else
		{
			ret = 0;
		}
	}
	else if(inc == 1)
	{
		index = selectChannelIndex+1;
		if(index < totalChannels)
		{
			selectChannelIndex = index;
			if(headless == 0)
				sprintf(buff, "%s       ", channelList[selectChannelIndex].name);
			else	
				sprintf(buff, "%s", channelList[selectChannelIndex].name);
			ret = 1;
		}
		else
		{
			ret = 0;
		}
	}
	else
	{
		if(headless == 0)
			sprintf(buff, "%s       ", channelList[selectChannelIndex].name);
		else	
			sprintf(buff, "%s", channelList[selectChannelIndex].name);
		
		ret = 0;
	}

	return ret;
}

int quitCallback(void)
{
	FECDecSettings_t *			pfecDecSettings				= &fecDecSettings;
	GDeChunkiserSettings_t *		pdeChunkiserSettings		= &deChunkiserSettings;
	VideoDecoderPlayerSettings_t *	pvideoDecoderPlayerSettings	= &videoDecoderPlayerSettings;
	AudioDecoderPlayerSettings_t *	paudioDecoderPlayerSettings	= &audioDecoderPlayerSettings;

	stopPlayer = 1;
	currAudioTimeStamp = 0;
	currVideoTimeStamp = 0;

	if(fec_enable == 1)
	{
		fprintf(stderr, "Stopping FEC Decoder ...\n");
		pfecDecSettings->exit = TRUE_STATE;

		while( (pfecDecSettings->exit != DONE_STATE) || (pdeChunkiserSettings->threadStatus != STOPPED_THREAD) || (pdeChunkiserSettings->threadStatus != STOPPED_THREAD)  || (pvideoDecoderPlayerSettings->threadStatus != STOPPED_THREAD) || (paudioDecoderPlayerSettings->threadStatus != STOPPED_THREAD) )
		{
			//fprintf(stderr,"... sleep ...\n");
			usleep(100);
		}
	}
	else
	{
		fprintf(stderr, "Stopping Dechunkiser ...\n");
		pdeChunkiserSettings->exit = TRUE_STATE;

		while( (pdeChunkiserSettings->exit != DONE_STATE) || (pdeChunkiserSettings->threadStatus != STOPPED_THREAD)  || (pvideoDecoderPlayerSettings->threadStatus != STOPPED_THREAD) || (paudioDecoderPlayerSettings->threadStatus != STOPPED_THREAD) )
		{
			//fprintf(stderr,"... sleep ...\n");
			usleep(100);
		}
	}

	stopP2PStreamer();
	
	SDL_Quit();

	exit(0);
}

int changeChannelCallback(SDL_Surface *	screen, char * buff, int inc) 
{
	int	ret;

	unsigned long 	*				pfecDecHandle 				= &fecDecHandle;
	FECDecSettings_t *			pfecDecSettings				= &fecDecSettings;
	
	unsigned long 	*				pdeChunkiserHandle 			= &deChunkiserHandle;
	GDeChunkiserSettings_t *		pdeChunkiserSettings		= &deChunkiserSettings;

	unsigned long	*				pvideoDecoderPlayerHandle	= &videoDecoderPlayerHandle;
	VideoDecoderPlayerSettings_t *	pvideoDecoderPlayerSettings	= &videoDecoderPlayerSettings;

	unsigned long	*				paudioDecoderPlayerHandle	= &audioDecoderPlayerHandle;
	AudioDecoderPlayerSettings_t *	paudioDecoderPlayerSettings	= &audioDecoderPlayerSettings;

	ret = getChannelName(buff, inc);
	if(ret != 0)
	{
		stopPlayer = 1;
		
		if(fec_enable == 1)
		{
			fprintf(stderr, "Pausing FEC Decoder ...\n");
			pfecDecSettings->pause 	= TRUE_STATE;

			while( (pfecDecSettings->pause != DONE_STATE) || (pdeChunkiserSettings->threadStatus != STOPPED_THREAD) || (pvideoDecoderPlayerSettings->threadStatus != STOPPED_THREAD) || (paudioDecoderPlayerSettings->threadStatus != STOPPED_THREAD) )
			{
				//fprintf(stderr,"... sleep %d %d ...\n", pvideoDecoderPlayerSettings->threadStatus, paudioDecoderSettings->threadStatus );
				usleep(1000);
			}
		}
		else
		{
			fprintf(stderr, "Pausing Dechunkiser ...\n");
			pdeChunkiserSettings->pause 	= TRUE_STATE;

			if(headless == 0)
			{
				while( (pdeChunkiserSettings->pause != DONE_STATE) || (pvideoDecoderPlayerSettings->threadStatus != STOPPED_THREAD) || (paudioDecoderPlayerSettings->threadStatus != STOPPED_THREAD) )
				{
					//fprintf(stderr,"... sleep %d %d ...\n", pvideoDecoderPlayerSettings->threadStatus, paudioDecoderSettings->threadStatus );
					usleep(1000);
				}
			}
			else
			{
				while(pdeChunkiserSettings->pause != DONE_STATE)
				{
					//fprintf(stderr,"... sleep %d %d ...\n", pvideoDecoderPlayerSettings->threadStatus, paudioDecoderSettings->threadStatus );
					usleep(1000);
				}
			}
		}

		stopP2PStreamer();

		//oSleep(2000);

		stopPlayer = 0;
		currAudioTimeStamp = 0;
		currVideoTimeStamp = 0;

		if(fec_enable == 1)
		{
			fprintf(stderr, "Unpausing FEC Decoder ...\n");
			pfecDecSettings->pause 		= FALSE_STATE;
		}
		else
		{
			fprintf(stderr, "Unpausing Dechuniser ...\n");
			pdeChunkiserSettings->pause 	= FALSE_STATE;
		}
		
		startP2PStreamer();

		if(headless == 0)
		{
			pvideoDecoderPlayerSettings->threadStatus 	= IDLE_THREAD;
			createThread(VideoDecoderPlayerThread, 
						pvideoDecoderPlayerSettings,
						12,
						(32*1024*1024), // stack size
						pvideoDecoderPlayerHandle);

			paudioDecoderPlayerSettings->threadStatus 		= IDLE_THREAD;
			createThread(AudioDecoderPlayerThread, 
						paudioDecoderPlayerSettings,
						12,
						(32*1024*1024), // stack size
						paudioDecoderPlayerHandle);
		}

		if(fec_enable == 1)
		{
			pdeChunkiserSettings->threadStatus 		= IDLE_THREAD;
			createThread(GDeChunkiserThread, 
						pdeChunkiserSettings,
						10,
						(32*1024*1024), // stack size
						pdeChunkiserHandle);
		}
	
		videoFrameErrors = 0;
		audioFrameErrors = 0;

		stableFrameErrorsCount = 0;
		unStableFrameErrorsCount = 0;

		videolayerPoint[0] = videolayerPoint[1] = videolayerPoint[2]= videolayerPoint[3] = 0;	

		numBFrames = 0;

		if(autoQuality == 1)
			videoLayer = 3;

		if(screen != NULL)
			updateQualityLevel(screen, videoLayer, autoQuality, 1);
		
		network_one_sec_cnt = 0;
		sum_network_delay = 0;
		avg_network_delay = -1;
		
		video_scalability = -1;	
	}

	return ret;
}

int getSystemVolume()
{
	return systemVolume;
}

int volumeCallback(int volume_level)
{
	//printf("\n volume_level %d",volume_level);

	systemVolume = volume_level*16;
	return 0;
}

int channelServerCallback()
{
	loadChannelList(&channelList[0]);
	printf("channelServerCallback\n");
	return 0;
}

int videoLayerEstimator(SDLParams_t * pgui_vdec_struct, int network_delay, int chunk_buffered, int multfactor_1_sec)
{
	int inc_br_network_delay_thr 	= 0;
	int dec_br_network_delay_thr 	= 0;
	
	if(avg_network_delay == -1)
		return 0;

	if(video_scalability != 1)
		return 0;

	//printf("chunks: %d\n", chunk_buffered); 

	network_delay -= avg_network_delay;
	if(network_delay < 0)
		network_delay = 0;

	dec_br_network_delay_thr 	= 1000;// 2*avg_network_delay;
	inc_br_network_delay_thr 	= 800;// 2*avg_network_delay+50;		// 2
/*
	if(avg_network_delay < 50)
	{
		dec_br_network_delay_thr 	= 100;
		inc_br_network_delay_thr 	= 150;		// 2
	}
	else if(avg_network_delay < 100)
	{
		dec_br_network_delay_thr 	= 200;
		inc_br_network_delay_thr 	= 250;
	}
	else// if(avg_network_delay < 200)
	{
		dec_br_network_delay_thr 	= 300;
		inc_br_network_delay_thr 	= 350;
	}
*/	
#if 0	
	else if(avg_network_delay < 500)
	{
		dec_br_network_delay_thr 	= 800;
		inc_br_network_delay_thr 	= 800;
	}
/*
	else if(avg_network_delay < 800)
	{
		dec_br_network_delay_thr 	= 800;
		inc_br_network_delay_thr 	= 800;
	}
	else if(avg_network_delay < 1500)
	{
		dec_br_network_delay_thr 	= 1500;
		inc_br_network_delay_thr 	= 1500;
	}
*/	
	else //if(avg_network_delay < 3000)
	{
		dec_br_network_delay_thr 	= 2000;
		inc_br_network_delay_thr 	= 2000;
	}
#endif
	//printf("network_delay: %d avg_network_delay: %d\n", network_delay, avg_network_delay);

	//printf("********** Errors: V: %d A: %d playback_delay_ms: %d unStableFrameErrorsCount: %d **********\n", videoFrameErrors, audioFrameErrors, playback_delay_ms, unStableFrameErrorsCount);
//	if(((videoFrameErrors > 0) | (audioFrameErrors > 0)) /*|| (playback_delay_ms > 1500) || (network_delay > 100)*/)
//	if((playback_delay_ms > 800) || (network_delay > 30))
//	if((network_delay > 300))
//	if(network_delay > dec_br_network_delay_thr)
//	if(((videoFrameErrors > 0) | (audioFrameErrors > 0)) || (network_delay > dec_br_network_delay_thr))
	int minChunks;
	if(videoLayer == 0)
		minChunks = 35;
	else if(videoLayer == 1)
		minChunks = 28;
	else if(videoLayer == 2)
		minChunks = 20;
	else if(videoLayer == 3)
		minChunks = 15;
	
	if(((videoFrameErrors > 0) | (audioFrameErrors > 0)) || (network_delay > dec_br_network_delay_thr) || (chunk_buffered < minChunks))
	{

		if(unStableFrameErrorsCount > 0)
		{
			unStableFrameErrorsCount--;
		}
		else
		{
#if 0				
			if(numBFrames > 0)
			{
				numBFrames--;
			}
#else
			if(videoLayer < 3)
			{
				//int u;
				//for(u=videoLayer; u >=0; u--)
					//videolayerPoint[u] = 0;

				//videolayerPoint[videoLayer] = videolayerPoint[videoLayer]-1;
				//if(videolayerPoint[videoLayer] > 0 )
					//videolayerPoint[videoLayer] = videolayerPoint[videoLayer]-1;

				videoLayer++;

				unStableFrameErrorsCount = 5*multfactor_1_sec;

				if(pgui_vdec_struct->screenHandle != NULL)
					updateQualityLevel(pgui_vdec_struct->screenHandle, videoLayer, autoQuality, 1);

				//printf("VideoLayerEstimator: Decrease Bitrate: unStableFrameErrorsCount: %d, videoLayer: %d\n", unStableFrameErrorsCount, videoLayer);
			}
#endif					

			stableFrameErrorsCount  = 0;
		}
	}

	stable_layer_thr = (10 + 10*videolayerPoint[videoLayer])*multfactor_1_sec;

	if(stable_layer_thr > (120*multfactor_1_sec))
		stable_layer_thr  = (120*multfactor_1_sec);

	//if((videoFrameErrors == 0)&&(audioFrameErrors == 0))
	//if((playback_delay_ms <= 1000) && (network_delay <= 50))
//	if((videoFrameErrors == 0)&&(audioFrameErrors == 0) && (network_delay <= inc_br_network_delay_thr))
	if((videoFrameErrors == 0)&&(audioFrameErrors == 0) && (network_delay <= inc_br_network_delay_thr) && (chunk_buffered >= minChunks))
	{
		//printf("####### stableFrameErrorsCount: %d stableFrameErrorsCount: %d ##########\n", stableFrameErrorsCount, stableFrameErrorsCount);
		stableFrameErrorsCount++;

		//printf("VideoLayerEstimator: Increase Bitrate: stableFrameErrorsCount: %d, videoLayer: %d\n", stableFrameErrorsCount, videoLayer);
#if 0
		if((stableFrameErrorsCount >= 10)&&(numBFrames < 2))
		{
			numBFrames++;

			stableFrameErrorsCount = 0;

			unStableFrameErrorsCount = 2;
		}	
#else
		if(stableFrameErrorsCount >= stable_layer_thr)
		{
			//if(videoLayer == (3-stable_layer))
			{
				videolayerPoint[videoLayer] = videolayerPoint[videoLayer]+1;
			}

			if(videoLayer != 3)
			{
				/*
				int u;
				for(u=videoLayer+1; u < 4; u++)
				{
					if(videolayerPoint[u] > 0)
						videolayerPoint[u] = videolayerPoint[u]-1;
				}
				*/
				if(videolayerPoint[videoLayer+1] > 0)
					videolayerPoint[videoLayer+1] = videolayerPoint[videoLayer+1]-1;
			}

			if(videoLayer > 0)
			{
				videoLayer--;

				stableFrameErrorsCount = 0;

				unStableFrameErrorsCount = 2*multfactor_1_sec;

				if(pgui_vdec_struct->screenHandle != NULL)
					updateQualityLevel(pgui_vdec_struct->screenHandle, videoLayer, autoQuality, 1);				
			}
		}
#endif
	}


	return 0;
}

int main(int argc, char *argv[])
{
	int 							ret = 0;

	unsigned long 	*				pfecDecHandle 				= &fecDecHandle;
	FECDecSettings_t *			pfecDecSettings				= &fecDecSettings;

	unsigned long 	*				pdeChunkiserHandle 			= &deChunkiserHandle;
	GDeChunkiserSettings_t *		pdeChunkiserSettings		= &deChunkiserSettings;

	unsigned long	*				pvideoDecoderPlayerHandle	= &videoDecoderPlayerHandle;
	VideoDecoderPlayerSettings_t *	pvideoDecoderPlayerSettings	= &videoDecoderPlayerSettings;

	unsigned long	*				paudioDecoderPlayerHandle	= &audioDecoderPlayerHandle;
	AudioDecoderPlayerSettings_t*	paudioDecoderPlayerSettings	= &audioDecoderPlayerSettings;

	unsigned long *				pguiHandle					= &guiHandle;
	GUISettings_t	*				pguiSettings					= &guiSettings;

	unsigned long	*				pMuxPlayerHandle			= &MuxPlayerHandle;

	unsigned long					idleQ_FEC_Decoder_2_DeChunkiser;
	unsigned long					liveQ_FEC_Decoder_2_DeChunkiser;

	unsigned long					idleQ_DeChunkiser_2_Video_Decoder_Player;
	unsigned long					liveQ_DeChunkiser_2_Video_Decoder_Player;

	unsigned long					idleQ_DeChunkiser_2_Audio_Decoder;
	unsigned long					liveQ_DeChunkiser_2_Audio_Decoder;
	
	unsigned long 					totalpackets;
	int							bufSize;

	int							network_delay 				= 0;
	int							chunk_buffered				= 0;
	SDLParams_t					gui_vdec_struct;

	float							AVSync_lag_msec 			= 30;//166.2222;
	
 	unsigned long					onesecondclock;
 	unsigned long					point2secondclock;
	int							avg_network_delay_reset	= 0;

	char 						statFilename[1024] = {0};

#ifdef WIN32
	msg_recv_from_p2p_mutex 		= CreateMutex(NULL, FALSE, "SRC_P2P_SHARED_MUTEX");
	msg_recv_from_p2p_id 			= CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, "SRC_P2P_SHARED_MEM");
	shared_memory_recv_from_p2p 	= (char*) MapViewOfFile(msg_recv_from_p2p_id, FILE_MAP_ALL_ACCESS, 0, 0, 1024);
#else
	msg_recv_from_p2p_id = shmget (IPC_PRIVATE, 1024, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); 
	/* Attach the shared memory segment.  */ 
  	shared_memory_recv_from_p2p = (char*) shmat (msg_recv_from_p2p_id, 0, 0); 
#endif

 	p2p_buffer = malloc(1024);
	memset(shared_memory_recv_from_p2p, 0, 1024);

#ifdef WIN32
	cntlr_msg_sent_from_player_mutex 		= CreateMutex(NULL, FALSE, "PLR_P2P_CNTLR_SHARED_MUTEX");
	cntlr_msg_sent_from_player_id 			= CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, "PLR_P2P_CNTLR_SHARED_MEM");
	shared_cntlr_memory_sent_from_player 	= (char*) MapViewOfFile(cntlr_msg_sent_from_player_id, FILE_MAP_ALL_ACCESS, 0, 0, 1024);
#else
	cntlr_msg_sent_from_player_id = shmget (IPC_PRIVATE, 1024, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); 
	/* Attach the shared memory segment.  */ 
  	shared_cntlr_memory_sent_from_player = (char*) shmat (cntlr_msg_sent_from_player_id, 0, 0); 
#endif
	
	memset(shared_cntlr_memory_sent_from_player, 0, 1024);

	sprintf(statFilename, "StatsPlayer.txt");

	FILE * fptr = fopen(statFilename, "wt");
	fclose(fptr);

	signal(SIGINT , sigterm_handler); /* Interrupt (ANSI).    */
	signal(SIGTERM, sigterm_handler); /* Termination (ANSI).  */

#ifdef WIN32
	WSADATA            				wsaData;
	short              				wVersionRequested = 0x101;

	// Initialize Winsock version 2.2
	if (WSAStartup( wVersionRequested, &wsaData ) == -1) 
	{
		printf("Root: WSAStartup failed with error %ld\n", WSAGetLastError());
		return -1;
	}
#endif

	fprintf(stderr,"--- Player Application ---\n");

	//2Defaults
	app_defaults();
	cmdline_parse(argc, argv);

	InitPlayerConfiguration();

	if(headless)
		GetConfiguration(local_ip, external_ip, channel_server_ip, &channel_server_port, &local_port, &external_port);

	if(headless)
		SaveConfiguration();
	
	/* register all formats and codecs */
	av_register_all();
	avformat_network_init();

	//2Queues Allocation

	if(fec_enable == 1)
	{
		// Queues Chunkiser to FEC Encoder
		fprintf(stderr,"Allocate FEC decoder 2 DeChunkiser queue\n");
		totalpackets 	= 128;
		bufSize 		= MAX_RAW_VIDEO_FRAME;
		createQueue(&liveQ_FEC_Decoder_2_DeChunkiser,  	totalpackets,  sizeof(packet_t));
		createQueue(&idleQ_FEC_Decoder_2_DeChunkiser, 	totalpackets,  	sizeof(packet_t));
		allocatePacketsonQueue(idleQ_FEC_Decoder_2_DeChunkiser, 
								totalpackets, 
								bufSize, 1);
	}

	// Queues DeChunkiser to Video Decoder Player
	fprintf(stderr,"Allocate DeChunkiser 2 Video Decoder Player queue\n");
	totalpackets 	= 300;
	bufSize 		= MAX_VIDEO_BITRATE/(8*30);
	createQueue(&liveQ_DeChunkiser_2_Video_Decoder_Player,	 	totalpackets,  	sizeof(packet_t));
	createQueue(&idleQ_DeChunkiser_2_Video_Decoder_Player, 		totalpackets,  	sizeof(packet_t));
	allocatePacketsonQueue(idleQ_DeChunkiser_2_Video_Decoder_Player, 
							totalpackets, 
							bufSize, 1);

	// Queues DeChunkiser to Audio Decoder
	fprintf(stderr,"Allocate DeChunkiser 2 Audio Decoder queue\n");
	totalpackets 	= 160;
	bufSize 		= MAX_AUDIO_BITRATE/(8*44);
	createQueue(&liveQ_DeChunkiser_2_Audio_Decoder,	 	totalpackets,  	sizeof(packet_t));
	createQueue(&idleQ_DeChunkiser_2_Audio_Decoder, 		totalpackets,  	sizeof(packet_t));
	allocatePacketsonQueue(idleQ_DeChunkiser_2_Audio_Decoder, 
							totalpackets, 
							bufSize, 1);


	//2Setting Components
	gui_vdec_struct.screenHandle 							= NULL;
	gui_vdec_struct.mutex									= NULL;

	if(fec_enable == 1)
	{
		// fec decoder settings
		pfecDecSettings->threadStatus 						= IDLE_THREAD;
		pfecDecSettings->max_chunk_size					= (MAX_VIDEO_BITRATE*4)/(8*30);
		pfecDecSettings->initial_crc							= STAMP_CRC;
		pfecDecSettings->k 								= 8;
		pfecDecSettings->n 								= 12;
		pfecDecSettings->player_listen_port					= player_server_port;
		pfecDecSettings->exit								= FALSE_STATE;
		pfecDecSettings->pause							= FALSE_STATE;
		pfecDecSettings->out_Q[LIVE_QUEUE]				= liveQ_FEC_Decoder_2_DeChunkiser;
		pfecDecSettings->out_Q[IDLE_QUEUE]				= idleQ_FEC_Decoder_2_DeChunkiser;
	}
	
	// Dechunkiser settings
	pdeChunkiserSettings->threadStatus 						= IDLE_THREAD;
	pdeChunkiserSettings->max_chunk_size 					= (MAX_VIDEO_BITRATE*4)/(8*30);
	pdeChunkiserSettings->player_listen_port					= player_server_port;
	pdeChunkiserSettings->exit								= FALSE_STATE;
	pdeChunkiserSettings->pause							= FALSE_STATE;
	if(fec_enable == 1)
	{
		pdeChunkiserSettings->in_Q[LIVE_QUEUE]				= liveQ_FEC_Decoder_2_DeChunkiser;
		pdeChunkiserSettings->in_Q[IDLE_QUEUE]				= idleQ_FEC_Decoder_2_DeChunkiser;
	}
	else
	{
		pdeChunkiserSettings->in_Q[LIVE_QUEUE]				= NULL;
		pdeChunkiserSettings->in_Q[IDLE_QUEUE]				= NULL;
	}
	pdeChunkiserSettings->out_Video_Q[LIVE_QUEUE]			= liveQ_DeChunkiser_2_Video_Decoder_Player;
	pdeChunkiserSettings->out_Video_Q[IDLE_QUEUE]			= idleQ_DeChunkiser_2_Video_Decoder_Player;
	pdeChunkiserSettings->out_Audio_Q[LIVE_QUEUE]			= liveQ_DeChunkiser_2_Audio_Decoder;
	pdeChunkiserSettings->out_Audio_Q[IDLE_QUEUE]			= idleQ_DeChunkiser_2_Audio_Decoder;
	pdeChunkiserSettings->headless							= headless;
	
	// Video decoder player settings
	pvideoDecoderPlayerSettings->threadStatus 				= IDLE_THREAD;
	pvideoDecoderPlayerSettings->maintain_input_frame_rate 	= 0;
	pvideoDecoderPlayerSettings->AVSync_lag_msec			= AVSync_lag_msec;
	pvideoDecoderPlayerSettings->pgui_vdec_struct			= &gui_vdec_struct;
	pvideoDecoderPlayerSettings->in_Q[LIVE_QUEUE] 			= liveQ_DeChunkiser_2_Video_Decoder_Player;
	pvideoDecoderPlayerSettings->in_Q[IDLE_QUEUE] 			= idleQ_DeChunkiser_2_Video_Decoder_Player;

	// Audio decoder settings
	paudioDecoderPlayerSettings->threadStatus 				= IDLE_THREAD;
	paudioDecoderPlayerSettings->maintain_input_frame_rate 	= 0;
	paudioDecoderPlayerSettings->AVSync_lag_msec			= AVSync_lag_msec;
	paudioDecoderPlayerSettings->in_Q[LIVE_QUEUE] 			= liveQ_DeChunkiser_2_Audio_Decoder;
	paudioDecoderPlayerSettings->in_Q[IDLE_QUEUE] 			= idleQ_DeChunkiser_2_Audio_Decoder;
	
	// GUI settings
	pguiSettings->threadStatus 								= IDLE_THREAD;
	pguiSettings->pgui_vdec_struct							= &gui_vdec_struct;
	pguiSettings->maximize_window							= FALSE_STATE;
	pguiSettings->volume									= 70;
	pguiSettings->auto_quality								= &autoQuality;
	pguiSettings->quality_level								= &videoLayer;
	pguiSettings->quitAppcbk								= quitCallback;	
	pguiSettings->switchChannelCbk							= changeChannelCallback;	
	pguiSettings->changeVolumeCbk							= volumeCallback;	
	pguiSettings->channelServerCbk							= channelServerCallback;	

	//2Start Components
	if(headless == 0)
	{
		createThread(GUIThread, 
					pguiSettings,
					14,
					(32*1024*1024), // stack size
					pguiHandle);

		while(guiSettingsWindow == 0)
		{
			//printf("WAITING ...\n");
			usleep(2000000);
		}
	}
	else
	{
		char				channel_name[1024] = {0};

		channelServerCallback();
		changeChannelCallback(NULL, channel_name, 0);
		printf("Channel name: %s\n", channel_name);
	}

	if(headless == 0)
	{
		createThread(VideoDecoderPlayerThread, 
					pvideoDecoderPlayerSettings,
					12,
					(32*1024*1024), // stack size
					pvideoDecoderPlayerHandle);

		createThread(AudioDecoderPlayerThread, 
					paudioDecoderPlayerSettings,
					12,
					(32*1024*1024), // stack size
					paudioDecoderPlayerHandle);
	}
	else
	{
		createThread(MuxPlayerThread, 
					NULL,
					0,
					(32*1024*1024), // stack size
					pMuxPlayerHandle);
	}

	createThread(GDeChunkiserThread, 
				pdeChunkiserSettings,
				10,
				(32*1024*1024), // stack size
				pdeChunkiserHandle);


	if(fec_enable == 1)
	{
		createThread(FECDecThread, 
					pfecDecSettings,
					8,
					(32*1024*1024), // stack size
					pfecDecHandle);
	}

	startP2PStreamer();
	
	cumVideoInputBits = 0;
	cumAudioInputBits = 0;
	cumNetworkInputBits = 0;
	total_secs_count = 1;

	stable_layer_thr = 10*5;

	onesecondclock = timer_msec();

	while(1)
	{
		if((timer_msec() - onesecondclock) > 1000)
		{
			if(((float)(videoInputBits)) == 0.000)
			{
				usleep(100);
				continue;
			}

			//printf("Enc: [V:%02d fps, A:%02d fps, V:%07.2f kbps, A:%07.2f kbps] Chunk: [%02d/s] NW: [%07.2f kbps] CumBR: [V:%07.2f kbps, A:%07.2f kbps, NW:%07.2f kbps]\n", videoFrameCnt, audioFrameCnt, (float)(videoInputBits)/1024.0, (float)(audioInputBits)/1024.0, (videoFrameCnt+audioFrameCnt), (float)(networkInputBits/1024.0), (float)(cumVideoInputBits)/(total_secs_count*1024.0), (float)(cumAudioInputBits)/(total_secs_count*1024.0), (float)(cumNetworkInputBits)/(total_secs_count*1024.0));

#ifdef WIN32
			WaitForSingleObject(msg_recv_from_p2p_mutex, INFINITE);
#endif

			memcpy(p2p_buffer, shared_memory_recv_from_p2p, strlen(shared_memory_recv_from_p2p));

#ifdef WIN32
			ReleaseMutex(msg_recv_from_p2p_mutex);
#endif

			//printf("p2p_buffer: %s\n", p2p_buffer);

			{
				FILE * statFile = fopen(statFilename, "at");
				char * cptr1, *cptr2;
				char temp[128] = {0};
				
				fprintf(statFile, "\n****** APPLICATION ********\n");

				cptr1 = p2p_buffer;
				cptr2 = strstr(p2p_buffer,"$");
				if(cptr2 == NULL)
					goto BYPASS;
				memset(temp, 0, 128);
				memcpy(temp, cptr1, (cptr2-cptr1));

				fprintf(statFile, "%s", temp);

				fprintf(statFile, "Video Bitrate: %07.2f kbps (I: %07.2f kbps, P: %07.2f kbps, B: %07.2f kbps)\n", (float)(videoInputBits/1024.0), (float)(videoInputIBits/1024.0), (float)(videoInputPBits/1024.0), (float)(videoInputBBits/1024.0));
				fprintf(statFile, "Audio Bitrate: %07.2f kbps\n", (float)(audioInputBits/1024.0));

				float AVSync_lag_KHz = (float)(AVSync_lag_msec)*90.0; 
				fprintf(statFile, "A-V Drift: %2.2f\n", (AVSync_lag_KHz- (float)(currAudioTimeStamp - currVideoTimeStamp))/AVSync_lag_KHz);
				//fprintf(statFile, "A-V Drift: %d\n", (int)(currAudioTimeStamp - currVideoTimeStamp));

				cptr2++;
				cptr1 = cptr2;
				cptr2 = strstr(cptr2,"$");
				if(cptr2 == NULL)
					goto BYPASS;
				memset(temp, 0, 128);
				memcpy(temp, cptr1, (cptr2-cptr1));

				fprintf(statFile, "Max Upload: [%s kbps]\n", temp);

				cptr2++;
				cptr1 = cptr2;
				cptr2 = strstr(cptr2,"$");
				if(cptr2 == NULL)
					goto BYPASS;
				memset(temp, 0, 128);
				memcpy(temp, cptr1, (cptr2-cptr1));

				fprintf(statFile, "Max Download: [%s kbps]\n", temp);
				
				cptr2++;
				cptr1 = cptr2;
				cptr2 = strstr(cptr2,"$");
				if(cptr2 == NULL)
					goto BYPASS;
				memset(temp, 0, 128);
				memcpy(temp, cptr1, (cptr2-cptr1));

				fprintf(statFile, "Chunk Buffer: [%s chunks]\n",temp);

				cptr2++;
				cptr1 = cptr2;
				cptr2 = strstr(cptr2,"$");
				if(cptr2 == NULL)
					goto BYPASS;
				memset(temp, 0, 128);
				memcpy(temp, cptr1, (cptr2-cptr1));

				fprintf(statFile, "Startup Latency: [%s ms]\n", temp);

				fprintf(statFile, "Playback Delay: [%u ms]\n", playback_delay_ms);
				fprintf(statFile, "Number of chunks dropped: [%u]\n", chunk_miss_ratio);				
				fprintf(statFile, "Chunk Miss Ratio: [%.02f Precent]\n", (double)(chunk_miss_ratio*100.0)/(double)max_chunk_id);		
				fprintf(statFile, "Number of B Frames: [%u]\n", numBFrames);		
 				fprintf(statFile, "Video Buffering: [%u]\n", getVideoBuffering());		
 				fprintf(statFile, "Audio Buffering: [%u]\n", getAudioBuffering());
				fprintf(statFile, "Scalability: [%d]\n", video_scalability);		
				fprintf(statFile, "Auto Quality: [%u]\n", autoQuality);		
				fprintf(statFile, "Video Layer: [%u]\n", videoLayer);		
 				fprintf(statFile, "Stable Layer: [%d/%d :: %d %d %d %d]\n", stableFrameErrorsCount/5, stable_layer_thr/5, videolayerPoint[3], videolayerPoint[2], videolayerPoint[1], videolayerPoint[0]);		
 				fprintf(statFile, "Number of Video Frame Drops: [%u]\n", videoFrameDrops);		
 				fprintf(statFile, "Number of Audio Frame Drops: [%u]\n", audioFrameDrops);		
 
 				if(fec_enable == 1)
 				{
 					fprintf(statFile, "CRC Error: %d\n", crc_error);
 					fprintf(statFile, "FEC Correction: %d\n", fec_correction);
 				}
 				else
 				{
 					fprintf(statFile, "CRC Error: 0\n");
 					fprintf(statFile, "FEC Correction: 0\n");
 				}
 
 				fprintf(statFile, "------ NW EMULATOR ------\n");

				cptr2++;
				cptr1 = cptr2;
				cptr2 = strstr(cptr2,"$");
				if(cptr2 == NULL)
					goto BYPASS;
				memset(temp, 0, 128);
				memcpy(temp, cptr1, (cptr2-cptr1));

				fprintf(statFile, "Network Upload: [%s kbps]\n", temp);

				cptr2++;
				cptr1 = cptr2;
				cptr2 = strstr(cptr2,"$");
				if(cptr2 == NULL)
					goto BYPASS;
				memset(temp, 0, 128);
				memcpy(temp, cptr1, (cptr2-cptr1));

				fprintf(statFile, "Network Download: [%s kbps]\n", temp);
				
				cptr2++;
				cptr1 = cptr2;
				cptr2 = strstr(cptr2,"$");
				if(cptr2 == NULL)
					goto BYPASS;
				memset(temp, 0, 128);
				memcpy(temp, cptr1, (cptr2-cptr1));

				network_delay = atoi(temp);
//*
				if(network_one_sec_cnt < 4)
				{
					sum_network_delay += network_delay;
					network_one_sec_cnt++;
						
					if(network_one_sec_cnt >= 4)
					{
						avg_network_delay = sum_network_delay/network_one_sec_cnt;
					}

					//printf("Delay: %d SumDelay: %d Cnt: %d avg_network_delay: %d\n", atoi(temp), sum_network_delay, network_one_sec_cnt, avg_network_delay);
				}

				avg_network_delay_reset++;
				if(avg_network_delay_reset > 60)
				{
					sum_network_delay 		= 0;
					network_one_sec_cnt 	= 0;
					
					avg_network_delay_reset 	= 0;
				}
//*/
				
				if(avg_network_delay == -1)
				{
					fprintf(statFile, "Network Delay: [-]\n");
				}
				else
				{
					network_delay -= avg_network_delay;
					if(network_delay < 0)
						network_delay = 0;
					fprintf(statFile, "Network Delay: [%d ms]\n", network_delay);
				}

				//fprintf(statFile, "Average Network Delay: [%d ms]\n", avg_network_delay);

BYPASS:
				fclose(statFile);
			}					
			
			//printf("A/V Sync: %2.2f\n", (20000.0 - (float)(currAudioTimeStamp - currVideoTimeStamp))/20000.0);

			videoFrameCnt = 0;
			audioFrameCnt = 0;

			cumVideoInputBits += videoInputBits;
			cumAudioInputBits += audioInputBits;
			cumNetworkInputBits += networkInputBits;
			total_secs_count++;

			videoInputBits = 0;
			videoInputIBits = 0;
			videoInputPBits = 0;
			videoInputBBits = 0;
			audioInputBits = 0;
			networkInputBits = 0;

			videoFrameErrors = 0;
			audioFrameErrors = 0;

			onesecondclock = timer_msec();
		}

		if((timer_msec() - point2secondclock) > 200)
		{
#ifdef WIN32
			WaitForSingleObject(msg_recv_from_p2p_mutex, INFINITE);
#endif

			memcpy(p2p_buffer, shared_memory_recv_from_p2p, strlen(shared_memory_recv_from_p2p));

#ifdef WIN32
			ReleaseMutex(msg_recv_from_p2p_mutex);
#endif

			char * cptr1, *cptr2;
			char temp[128] = {0};
			 
			cptr1 = p2p_buffer;
			cptr2 = strstr(p2p_buffer,"$");
			if(cptr2 == NULL)
				goto BYPASS2;

			cptr2++;
			cptr1 = cptr2;
			cptr2 = strstr(cptr2,"$");
			if(cptr2 == NULL)
				goto BYPASS2;

				cptr2++;
			cptr1 = cptr2;
			cptr2 = strstr(cptr2,"$");
			if(cptr2 == NULL)
				goto BYPASS2;
				
			cptr2++;
			cptr1 = cptr2;
			cptr2 = strstr(cptr2,"$");
			if(cptr2 == NULL)
				goto BYPASS2;
			memset(temp, 0, 128);
			memcpy(temp, cptr1, (cptr2-cptr1));

			chunk_buffered = atoi(temp);

			cptr2++;
			cptr1 = cptr2;
			cptr2 = strstr(cptr2,"$");
			if(cptr2 == NULL)
				goto BYPASS2;

			cptr2++;
			cptr1 = cptr2;
			cptr2 = strstr(cptr2,"$");
			if(cptr2 == NULL)
				goto BYPASS2;

			cptr2++;
			cptr1 = cptr2;
			cptr2 = strstr(cptr2,"$");
			if(cptr2 == NULL)
				goto BYPASS2;

				cptr2++;
			cptr1 = cptr2;
			cptr2 = strstr(cptr2,"$");
			if(cptr2 == NULL)
				goto BYPASS2;
			memset(temp, 0, 128);
			memcpy(temp, cptr1, (cptr2-cptr1));

			network_delay = atoi(temp);
			
			BYPASS2:

			if(autoQuality == 1)
			{
				videoLayerEstimator(&gui_vdec_struct, network_delay, chunk_buffered, 5);
			}
			else
			{
				stableFrameErrorsCount = 0;
				unStableFrameErrorsCount = 0;

				videolayerPoint[0] = videolayerPoint[1] = videolayerPoint[2]= videolayerPoint[3] = 0;				
			}
			
#ifdef WIN32
			WaitForSingleObject(cntlr_msg_sent_from_player_mutex, INFINITE);
#endif

			sprintf(shared_cntlr_memory_sent_from_player, "%02d.%02d$", videoLayer,numBFrames);

#ifdef WIN32
			ReleaseMutex(cntlr_msg_sent_from_player_mutex);
#endif

			//videoFrameErrors = 0;
			//audioFrameErrors = 0;
			
			point2secondclock = timer_msec();
		}	
	
		usleep(100);
	}

	return 0;
}

