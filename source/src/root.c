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

#define CONFIG_FILE_NAME "SourceConf.txt"

extern unsigned long audioFrameCnt;
extern unsigned long videoFrameCnt;
extern unsigned long videoEncFrameCnt;
extern unsigned long audioEncFrameCnt;
extern unsigned long videoInputBits;
extern unsigned long audioInputBits;
extern unsigned long videoOutputBits;
extern unsigned long audioOutputBits;
extern unsigned long networkOutputBits;
extern unsigned long chunkOutputCnt;

extern unsigned long videoOutputIBits;
extern unsigned long videoOutputPBits;
extern unsigned long videoOutputBBits;

unsigned long cumVideoOutputBits;
unsigned long cumAudioOutputBits;
unsigned long cumNetworkOutputBits;
unsigned long	total_secs_count;

int					loopback_disable = 0;

int					debug_level = 0;

int					video_rewind = 1;

int					wan_emulation_enable = 0;
int					upload_bitrate_limit = 1000000;
int					download_bitrate_limit = 1000000;

int					fec_enable = 0;
int					svc_enable = 0;
int					svc_enable_saved = 0;

int 					drop_packet_index_0[]    = {1};
int 					drop_packet_index_1[]    = {1,1,1,1,1,1,1,1,1,0};
int 					drop_packet_index_2[]    = {1,1,1,1,0};
int 					drop_packet_index_3[]    = {1,1,0,1,1,0,1,1,0,1};
int 					drop_packet_index_4[]    = {1,0,1,0,1};
int					drop_packet_index_5[]    = {1,0};
int 					drop_packet_index_6[]    = {1,0,1,0,0};
int 					drop_packet_index_7[]    = {1,0,0,0,1,0,0,1,0,0};
int 					drop_packet_index_8[]    = {1,0,0,0,0};
int 					drop_packet_index_9[]    = {1,0,0,0,0,0,0,0,0,0};
int 					drop_packet_index_10[] 	= {0};

int *					drop_packet_array 	= drop_packet_index_0;
int					drop_packet_size 	= sizeof(drop_packet_index_0)/4;

unsigned long			liveViewHandle = NULL;

unsigned long sema4;

#ifdef WIN32
PROCESS_INFORMATION p2p_streamer;
#else
pid_t 	p2p_streamer;
#endif

int						AppStatus = 0; //0: Stop, 1: Start

unsigned long 				demuxHandle;
DemuxSettings_t			demuxSettings;

unsigned long				videoDecoderHandle;
VideoDecoderSettings_t		videoDecoderSettings;

unsigned long				videoEncoderHandle;
VideoEncoderSettings_t		videoEncoderSettings;

unsigned long				audioDecoderHandle;
AudioDecoderSettings_t		audioDecoderSettings;

unsigned long				audioEncoderHandle;
AudioEncoderSettings_t		audioEncoderSettings;

unsigned long 				videoChunkiserHandle;
GChunkiserSettings_t		videoChunkiserSettings;

unsigned long 				audioChunkiserHandle;
GChunkiserSettings_t		audioChunkiserSettings;

unsigned long 				audioFileWriteHandle;
FileWriteSettings_t			audioFileWriteSettings;

unsigned long 				fecEncHandle;
FECEncSettings_t			fecEncSettings;

unsigned long				webServerHandle;
WebServerSettings_t 		webServerSettings;

int						restartApplicationVar = 0;

#ifdef WIN32
HANDLE						msg_recv_from_p2p_mutex;
HANDLE						msg_recv_from_p2p_id;
#else
int 							msg_recv_from_p2p_id; 
#endif
char* 						shared_memory_recv_from_p2p;
char *						p2p_buffer;

typedef struct
{     
	int video_codec;
	int video_bitrate;
	int video_width;
	int video_height;
	
	int audio_codec;
	int audio_bitrate;

	char server_ip[128];
	char external_ip[128];
	int server_port;
	int server_external_port;
	int http_port;
	int src_client_port;

	char channel_file[128];
	int channel_ID;

	int video_scalability;
	int video_bitrate_1;
	int video_width_1;
	int video_height_1;
	int video_bitrate_2;
	int video_width_2;
	int video_height_2;
	int video_bitrate_3;
	int video_width_3;
	int video_height_3;
	int video_bitrate_4;
	int video_width_4;
	int video_height_4;

}Source_Config_t, *pSource_Config_t;

Source_Config_t 	SourceConfigValues;
int OldScalabilityStatus = 0;

char LogBuffer[LOG_BUFFER_SIZE];
char TempLogBuffer[LOG_BUFFER_SIZE];

char LogBuffer2[LOG_BUFFER_SIZE2];

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

uint64_t time_90KHz(void){
	struct timeval tdecA;
	uint64_t result, usec;

#ifdef WIN32
	mingw_gettimeofday(&tdecA, NULL);
#else
	gettimeofday(&tdecA, NULL);
#endif

	usec = (tdecA.tv_sec*1000000)+tdecA.tv_usec;

	result = (usec/1000)*(90000/1000);

	return result;
}

static void sigterm_handler(int sig)
{
    printf("stop P2P Streamer\n");
    stopP2PStreamer();
    exit(123);
}

static void app_defaults(void)
{
	liveViewHandle = 0;

	memset(LogBuffer,'\0',LOG_BUFFER_SIZE);
	memset(TempLogBuffer,'\0',LOG_BUFFER_SIZE);
	memset(LogBuffer2,'\0',LOG_BUFFER_SIZE2);

	AppStatus = 0;

	loopback_disable = 0;

	video_rewind = 1;
	
	debug_level = 0;

	createSemaphore(&sema4, 1);	

}

void setAppStatus(int val)
{
	AppStatus = val;
}

int getAppStatus()
{
	return AppStatus;
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

void extractSourceFileName(char *str, char *srcName)
{
	char * pch;
	int len;

	char *startString = "C:\\fakepath\\";

	pch = strstr(str,startString);

	if(pch != NULL)
	{
		pch = pch+strlen("C:\\fakepath\\");
		
		strcpy(srcName, pch);
	}
	else
	{
		strcpy(srcName,str);
	}
	
	//printf("\n extractSourceFileName:: %s",srcName);
	
}

void LoadDefaultConfiguration()
{
	SourceConfigValues.video_codec = AV_CODEC_ID_H264;
	SourceConfigValues.video_bitrate = 512000;
	SourceConfigValues.video_width = 352;
	SourceConfigValues.video_height = 240;

	SourceConfigValues.audio_codec = AV_CODEC_ID_MP3;
	SourceConfigValues.audio_bitrate = 128000;

	strcpy(SourceConfigValues.server_ip,"127.0.0.1");
	strcpy(SourceConfigValues.external_ip,"127.0.0.1");
	SourceConfigValues.server_port = 6666;
	SourceConfigValues.server_external_port = 6666;
	SourceConfigValues.http_port = 8000;

	SourceConfigValues.src_client_port = SourceConfigValues.http_port + 2;

	strcpy(SourceConfigValues.channel_file,"video.ts");
	SourceConfigValues.channel_ID = 100;

	SourceConfigValues.video_scalability = 0;
	
	SourceConfigValues.video_bitrate_1 = 128000;
	SourceConfigValues.video_width_1 = 256;
	SourceConfigValues.video_height_1 = 144;
	
	SourceConfigValues.video_bitrate_2 = 256000;
	SourceConfigValues.video_width_2 = 320;
	SourceConfigValues.video_height_2 = 180;
	
	SourceConfigValues.video_bitrate_3 = 512000;
	SourceConfigValues.video_width_3 = 640;
	SourceConfigValues.video_height_3 = 360;
	
	SourceConfigValues.video_bitrate_4 = 768000;
	SourceConfigValues.video_width_4 = 704;
	SourceConfigValues.video_height_4 = 396;
}

void SetConfiguration(char *value, int configNum)
{
	if(configNum == 0)
	{
		if(strcmp(value,"H.264") == 0)
			SourceConfigValues.video_codec = AV_CODEC_ID_H264;
		else if(strcmp(value,"MPEG-4") == 0)
			SourceConfigValues.video_codec = AV_CODEC_ID_MPEG4;
		else
			SourceConfigValues.video_codec = AV_CODEC_ID_MPEG2VIDEO;
	}
	else if(configNum == 1)
	{
		if(strcmp(value,"CIF (352x240)") == 0)
		{
			SourceConfigValues.video_width = 352;
			SourceConfigValues.video_height = 240;
		}
		else if(strcmp(value,"D1 (704x480)") == 0)
		{
			SourceConfigValues.video_width = 704;
			SourceConfigValues.video_height = 480;
		}
		else
		{
			SourceConfigValues.video_width = 1280;
			SourceConfigValues.video_height = 720;
		}
	}
	else if(configNum == 2)
	{
		if(strcmp(value,"512 Kbps") == 0)
			SourceConfigValues.video_bitrate = 512000;
		else if(strcmp(value,"768 Kbps") == 0)
			SourceConfigValues.video_bitrate = 768000;
		else if(strcmp(value,"1 Mbps") == 0)
			SourceConfigValues.video_bitrate = 1000000;
		else if(strcmp(value,"2 Mbps") == 0)
			SourceConfigValues.video_bitrate = 2000000;
		else if(strcmp(value,"3 Mbps") == 0)
			SourceConfigValues.video_bitrate = 3000000;
		else if(strcmp(value,"4 Mbps") == 0)
			SourceConfigValues.video_bitrate = 4000000;
		else if(strcmp(value,"6 Mbps") == 0)
			SourceConfigValues.video_bitrate = 6000000;
		else
			SourceConfigValues.video_bitrate = 8000000;
	}
	else if(configNum == 3)
	{
		if(strcmp(value,"MP3") == 0)
			SourceConfigValues.audio_codec = AV_CODEC_ID_MP3;
		else if(strcmp(value,"MP2") == 0)
			SourceConfigValues.audio_codec = AV_CODEC_ID_MP2;
		else
			SourceConfigValues.audio_codec = AV_CODEC_ID_AAC;
	}
	else if(configNum == 4)
	{
		if(strcmp(value,"128 Kbps") == 0)
			SourceConfigValues.audio_bitrate = 128000;
		else if(strcmp(value,"256 Kbps") == 0)
			SourceConfigValues.audio_bitrate = 256000;
		else
			SourceConfigValues.audio_bitrate = 384000;
	}
	else if(configNum == 5)
	{
		strcpy(SourceConfigValues.server_ip,value);
	}
	else if(configNum == 6)
	{
		strcpy(SourceConfigValues.external_ip,value);
	}
	else if(configNum == 7)
	{
		SourceConfigValues.server_port = atoi(value);
	}
	else if(configNum == 8)
	{
		SourceConfigValues.http_port = atoi(value);
	}
	else if(configNum == 9)
	{
		SourceConfigValues.channel_ID = atoi(value);
	}
	else if(configNum == 10)
	{
		SourceConfigValues.video_scalability = atoi(value);
	}
	else if(configNum == 11)
	{
		if(strcmp(value,"(256x144)") == 0)
		{
			SourceConfigValues.video_width_1 = 256	;
			SourceConfigValues.video_height_1 = 144;
		}
		else if(strcmp(value,"(320x180)") == 0)
		{
			SourceConfigValues.video_width_1 = 320;
			SourceConfigValues.video_height_1 = 180;
		}
		else if(strcmp(value,"(640x360)") == 0)
		{
			SourceConfigValues.video_width_1 = 640;
			SourceConfigValues.video_height_1 = 360;
		}
		else if(strcmp(value,"(704x396)") == 0)
		{
			SourceConfigValues.video_width_1 = 704;
			SourceConfigValues.video_height_1 = 396;
		}
		else
		{
			SourceConfigValues.video_width_1 = 1280;
			SourceConfigValues.video_height_1 = 720;
		}
	}
	else if(configNum == 12)
	{
		if(strcmp(value,"128 Kbps") == 0)
			SourceConfigValues.video_bitrate_1 = 128000;
		else if(strcmp(value,"256 Kbps") == 0)
			SourceConfigValues.video_bitrate_1 = 256000;
		else if(strcmp(value,"512 Kbps") == 0)
			SourceConfigValues.video_bitrate_1 = 512000;
		else if(strcmp(value,"768 Kbps") == 0)
			SourceConfigValues.video_bitrate_1 = 768000;
		else if(strcmp(value,"1 Mbps") == 0)
			SourceConfigValues.video_bitrate_1 = 1000000;
		else
			SourceConfigValues.video_bitrate_1 = 2000000;
	}
	else if(configNum == 13)
	{
		if(strcmp(value,"(256x144)") == 0)
		{
			SourceConfigValues.video_width_2 = 256	;
			SourceConfigValues.video_height_2 = 144;
		}
		else if(strcmp(value,"(320x180)") == 0)
		{
			SourceConfigValues.video_width_2 = 320;
			SourceConfigValues.video_height_2 = 180;
		}
		else if(strcmp(value,"(640x360)") == 0)
		{
			SourceConfigValues.video_width_2 = 640;
			SourceConfigValues.video_height_2 = 360;
		}
		else if(strcmp(value,"(704x396)") == 0)
		{
			SourceConfigValues.video_width_2 = 704;
			SourceConfigValues.video_height_2 = 396;
		}
		else
		{
			SourceConfigValues.video_width_2 = 1280;
			SourceConfigValues.video_height_2 = 720;
		}
	}
	else if(configNum == 14)
	{
		if(strcmp(value,"128 Kbps") == 0)
			SourceConfigValues.video_bitrate_2 = 128000;
		else if(strcmp(value,"256 Kbps") == 0)
			SourceConfigValues.video_bitrate_2 = 256000;
		else if(strcmp(value,"512 Kbps") == 0)
			SourceConfigValues.video_bitrate_2 = 512000;
		else if(strcmp(value,"768 Kbps") == 0)
			SourceConfigValues.video_bitrate_2 = 768000;
		else if(strcmp(value,"1 Mbps") == 0)
			SourceConfigValues.video_bitrate_2 = 1000000;
		else
			SourceConfigValues.video_bitrate_2 = 2000000;
	}
	else if(configNum == 15)
	{
		if(strcmp(value,"(256x144)") == 0)
		{
			SourceConfigValues.video_width_3 = 256	;
			SourceConfigValues.video_height_3 = 144;
		}
		else if(strcmp(value,"(320x180)") == 0)
		{
			SourceConfigValues.video_width_3 = 320;
			SourceConfigValues.video_height_3 = 180;
		}
		else if(strcmp(value,"(640x360)") == 0)
		{
			SourceConfigValues.video_width_3 = 640;
			SourceConfigValues.video_height_3 = 360;
		}
		else if(strcmp(value,"(704x396)") == 0)
		{
			SourceConfigValues.video_width_3 = 704;
			SourceConfigValues.video_height_3 = 396;
		}
		else
		{
			SourceConfigValues.video_width_3 = 1280;
			SourceConfigValues.video_height_3 = 720;
		}
	}
	else if(configNum == 16)
	{
		if(strcmp(value,"128 Kbps") == 0)
			SourceConfigValues.video_bitrate_3 = 128000;
		else if(strcmp(value,"256 Kbps") == 0)
			SourceConfigValues.video_bitrate_3 = 256000;
		else if(strcmp(value,"512 Kbps") == 0)
			SourceConfigValues.video_bitrate_3 = 512000;
		else if(strcmp(value,"768 Kbps") == 0)
			SourceConfigValues.video_bitrate_3 = 768000;
		else if(strcmp(value,"1 Mbps") == 0)
			SourceConfigValues.video_bitrate_3 = 1000000;
		else
			SourceConfigValues.video_bitrate_3 = 2000000;
	}
	else if(configNum == 17)
	{
		if(strcmp(value,"(256x144)") == 0)
		{
			SourceConfigValues.video_width_4 = 256	;
			SourceConfigValues.video_height_4 = 144;
		}
		else if(strcmp(value,"(320x180)") == 0)
		{
			SourceConfigValues.video_width_4 = 320;
			SourceConfigValues.video_height_4 = 180;
		}
		else if(strcmp(value,"(640x360)") == 0)
		{
			SourceConfigValues.video_width_4 = 640;
			SourceConfigValues.video_height_4 = 360;
		}
		else if(strcmp(value,"(704x396)") == 0)
		{
			SourceConfigValues.video_width_4 = 704;
			SourceConfigValues.video_height_4 = 396;
		}
		else
		{
			SourceConfigValues.video_width_4 = 1280;
			SourceConfigValues.video_height_4 = 720;
		}
	}
	else if(configNum == 18)
	{
		if(strcmp(value,"128 Kbps") == 0)
			SourceConfigValues.video_bitrate_4 = 128000;
		else if(strcmp(value,"256 Kbps") == 0)
			SourceConfigValues.video_bitrate_4 = 256000;
		else if(strcmp(value,"512 Kbps") == 0)
			SourceConfigValues.video_bitrate_4 = 512000;
		else if(strcmp(value,"768 Kbps") == 0)
			SourceConfigValues.video_bitrate_4 = 768000;
		else if(strcmp(value,"1 Mbps") == 0)
			SourceConfigValues.video_bitrate_4 = 1000000;
		else
			SourceConfigValues.video_bitrate_4 = 2000000;
	}
	else if(configNum == 19)
	{
		SourceConfigValues.server_external_port = atoi(value);
	}
	else if(configNum == 20)
	{
		extractSourceFileName(value, SourceConfigValues.channel_file);
	}
}

void GetConfiguration(char *buf)
{
	buf[0]='\0';
	char temp[64];
	
	if(SourceConfigValues.video_codec == AV_CODEC_ID_H264)
		strcat(buf,"H.264");
	else if(SourceConfigValues.video_codec == AV_CODEC_ID_MPEG4)
		strcat(buf,"MPEG-4");
	else if(SourceConfigValues.video_codec == AV_CODEC_ID_MPEG2VIDEO)
		strcat(buf,"MPEG-2");
	else
		strcat(buf,"Invalid");
	
	strcat(buf, "*");
	if((SourceConfigValues.video_width == 352)&&(SourceConfigValues.video_height == 240))
		strcat(buf,"CIF (352x240)");
	else if((SourceConfigValues.video_width == 704)&&(SourceConfigValues.video_height == 480))
		strcat(buf,"D1 (704x480)");
	else if((SourceConfigValues.video_width == 1280)&&(SourceConfigValues.video_height == 720))
		strcat(buf,"HD (1280x720)");
	else
		strcat(buf,"Invalid");

	strcat(buf, "*");
	if(SourceConfigValues.video_bitrate == 512000)
		strcat(buf,"512 Kbps");
	else if(SourceConfigValues.video_bitrate == 768000)
		strcat(buf,"768 Kbps");
	else if(SourceConfigValues.video_bitrate == 1000000)
		strcat(buf,"1 Mbps");
	else if(SourceConfigValues.video_bitrate == 2000000)
		strcat(buf,"2 Mbps");
	else if(SourceConfigValues.video_bitrate == 3000000)
		strcat(buf,"3 Mbps");
	else if(SourceConfigValues.video_bitrate == 4000000)
		strcat(buf,"4 Mbps");
	else if(SourceConfigValues.video_bitrate == 6000000)
		strcat(buf,"6 Mbps");
	else if(SourceConfigValues.video_bitrate == 8000000)
		strcat(buf,"8 Mbps");
	else
		strcat(buf,"Invalid");

	strcat(buf, "*");
	if(SourceConfigValues.audio_codec == AV_CODEC_ID_MP3)
		strcat(buf,"MP3");
	else if(SourceConfigValues.audio_codec == AV_CODEC_ID_MP2)
		strcat(buf,"MP2");
	else if(SourceConfigValues.audio_codec == AV_CODEC_ID_AAC)
		strcat(buf,"AAC");
	else
		strcat(buf,"Invalid");

	strcat(buf, "*");
	if(SourceConfigValues.audio_bitrate == 128000)
		strcat(buf,"128 Kbps");
	else if(SourceConfigValues.audio_bitrate == 256000)
		strcat(buf,"256 Kbps");
	else if(SourceConfigValues.audio_bitrate == 384000)
		strcat(buf,"384 Kbps");
	else
		strcat(buf,"Invalid");

	strcat(buf, "*");
	strcat(buf,SourceConfigValues.server_ip);

	strcat(buf, "*");
	strcat(buf,SourceConfigValues.external_ip);

	strcat(buf, "*");
	sprintf(temp,"%d",SourceConfigValues.server_port);
	strcat(buf,temp);

	strcat(buf, "*");
	sprintf(temp,"%d",SourceConfigValues.http_port);
	strcat(buf,temp);

	strcat(buf, "*");
	strcat(buf,SourceConfigValues.channel_file);

	strcat(buf, "*");
	sprintf(temp,"%d",SourceConfigValues.channel_ID);
	strcat(buf,temp);

	strcat(buf, "*");
	sprintf(temp,"%d",getAppStatus());
	strcat(buf,temp);
	
	strcat(buf, "*");
	sprintf(temp,"%d",SourceConfigValues.video_scalability);
	strcat(buf,temp);

	strcat(buf, "*");
	if((SourceConfigValues.video_width_1 == 256)&&(SourceConfigValues.video_height_1 == 144))
		strcat(buf,"(256x144)");
	else if((SourceConfigValues.video_width_1 == 320)&&(SourceConfigValues.video_height_1 == 180))
		strcat(buf,"(320x180)");
	else if((SourceConfigValues.video_width_1 == 640)&&(SourceConfigValues.video_height_1 == 360))
		strcat(buf,"(640x360)");
	else if((SourceConfigValues.video_width_1 == 704)&&(SourceConfigValues.video_height_1 == 396))
		strcat(buf,"(704x396)");
	else
		strcat(buf,"(1280x720)");

	strcat(buf, "*");
	if(SourceConfigValues.video_bitrate_1 == 128000)
		strcat(buf,"128 Kbps");
	else if(SourceConfigValues.video_bitrate_1 == 256000)
		strcat(buf,"256 Kbps");
	else if(SourceConfigValues.video_bitrate_1 == 512000)
		strcat(buf,"512 Kbps");
	else if(SourceConfigValues.video_bitrate_1 == 768000)
		strcat(buf,"768 Kbps");
	else if(SourceConfigValues.video_bitrate_1 == 1000000)
		strcat(buf,"1 Mbps");
	else
		strcat(buf,"2 Mbps");

	strcat(buf, "*");
	if((SourceConfigValues.video_width_2 == 256)&&(SourceConfigValues.video_height_2 == 144))
		strcat(buf,"(256x144)");
	else if((SourceConfigValues.video_width_2 == 320)&&(SourceConfigValues.video_height_2 == 180))
		strcat(buf,"(320x180)");
	else if((SourceConfigValues.video_width_2 == 640)&&(SourceConfigValues.video_height_2 == 360))
		strcat(buf,"(640x360)");
	else if((SourceConfigValues.video_width_2 == 704)&&(SourceConfigValues.video_height_2 == 396))
		strcat(buf,"(704x396)");
	else
		strcat(buf,"(1280x720)");

	strcat(buf, "*");
	if(SourceConfigValues.video_bitrate_2 == 128000)
		strcat(buf,"128 Kbps");
	else if(SourceConfigValues.video_bitrate_2 == 256000)
		strcat(buf,"256 Kbps");
	else if(SourceConfigValues.video_bitrate_2 == 512000)
		strcat(buf,"512 Kbps");
	else if(SourceConfigValues.video_bitrate_2 == 768000)
		strcat(buf,"768 Kbps");
	else if(SourceConfigValues.video_bitrate_2 == 1000000)
		strcat(buf,"1 Mbps");
	else
		strcat(buf,"2 Mbps");

	strcat(buf, "*");
	if((SourceConfigValues.video_width_3 == 256)&&(SourceConfigValues.video_height_3 == 144))
		strcat(buf,"(256x144)");
	else if((SourceConfigValues.video_width_3 == 320)&&(SourceConfigValues.video_height_3 == 180))
		strcat(buf,"(320x180)");
	else if((SourceConfigValues.video_width_3 == 640)&&(SourceConfigValues.video_height_3 == 360))
		strcat(buf,"(640x360)");
	else if((SourceConfigValues.video_width_3 == 704)&&(SourceConfigValues.video_height_3 == 396))
		strcat(buf,"(704x396)");
	else
		strcat(buf,"(1280x720)");

	strcat(buf, "*");
	if(SourceConfigValues.video_bitrate_3 == 128000)
		strcat(buf,"128 Kbps");
	else if(SourceConfigValues.video_bitrate_3 == 256000)
		strcat(buf,"256 Kbps");
	else if(SourceConfigValues.video_bitrate_3 == 512000)
		strcat(buf,"512 Kbps");
	else if(SourceConfigValues.video_bitrate_3 == 768000)
		strcat(buf,"768 Kbps");
	else if(SourceConfigValues.video_bitrate_3 == 1000000)
		strcat(buf,"1 Mbps");
	else
		strcat(buf,"2 Mbps");

	strcat(buf, "*");
	if((SourceConfigValues.video_width_4 == 256)&&(SourceConfigValues.video_height_4 == 144))
		strcat(buf,"(256x144)");
	else if((SourceConfigValues.video_width_4 == 320)&&(SourceConfigValues.video_height_4 == 180))
		strcat(buf,"(320x180)");
	else if((SourceConfigValues.video_width_4 == 640)&&(SourceConfigValues.video_height_4 == 360))
		strcat(buf,"(640x360)");
	else if((SourceConfigValues.video_width_4 == 704)&&(SourceConfigValues.video_height_4 == 396))
		strcat(buf,"(704x396)");
	else
		strcat(buf,"(1280x720)");

	strcat(buf, "*");
	if(SourceConfigValues.video_bitrate_4 == 128000)
		strcat(buf,"128 Kbps");
	else if(SourceConfigValues.video_bitrate_4 == 256000)
		strcat(buf,"256 Kbps");
	else if(SourceConfigValues.video_bitrate_4 == 512000)
		strcat(buf,"512 Kbps");
	else if(SourceConfigValues.video_bitrate_4 == 768000)
		strcat(buf,"768 Kbps");
	else if(SourceConfigValues.video_bitrate_4 == 1000000)
		strcat(buf,"1 Mbps");
	else
		strcat(buf,"2 Mbps");
	
	strcat(buf, "*");
	sprintf(temp,"%d",SourceConfigValues.server_external_port);
	strcat(buf,temp);
}

void SaveConfiguration()
{
	FILE *configFileHandle;
	char configFileName[256];

	strcpy(configFileName,CONFIG_FILE_NAME);

	//printf("\n SaveConfiguration ++ ");
	
	configFileHandle = fopen(configFileName, "wb");
		
	setWordValue(configFileHandle,SourceConfigValues.video_codec);
	setWordValue(configFileHandle,SourceConfigValues.video_bitrate);
	setWordValue(configFileHandle,SourceConfigValues.video_width);
	setWordValue(configFileHandle,SourceConfigValues.video_height);

	setWordValue(configFileHandle,SourceConfigValues.audio_codec);
	setWordValue(configFileHandle,SourceConfigValues.audio_bitrate);

	setStringValue(configFileHandle,SourceConfigValues.server_ip);
	setStringValue(configFileHandle,SourceConfigValues.external_ip);
	setWordValue(configFileHandle,SourceConfigValues.server_port);
	setWordValue(configFileHandle,SourceConfigValues.http_port);

	SourceConfigValues.src_client_port = SourceConfigValues.http_port+2;
	
	setStringValue(configFileHandle,SourceConfigValues.channel_file);
	setWordValue(configFileHandle,SourceConfigValues.channel_ID);

	setWordValue(configFileHandle,SourceConfigValues.video_scalability);

	setWordValue(configFileHandle,SourceConfigValues.video_bitrate_1);
	setWordValue(configFileHandle,SourceConfigValues.video_width_1);
	setWordValue(configFileHandle,SourceConfigValues.video_height_1);

	setWordValue(configFileHandle,SourceConfigValues.video_bitrate_2);
	setWordValue(configFileHandle,SourceConfigValues.video_width_2);
	setWordValue(configFileHandle,SourceConfigValues.video_height_2);

	setWordValue(configFileHandle,SourceConfigValues.video_bitrate_3);
	setWordValue(configFileHandle,SourceConfigValues.video_width_3);
	setWordValue(configFileHandle,SourceConfigValues.video_height_3);

	setWordValue(configFileHandle,SourceConfigValues.video_bitrate_4);
	setWordValue(configFileHandle,SourceConfigValues.video_width_4);
	setWordValue(configFileHandle,SourceConfigValues.video_height_4);

	setWordValue(configFileHandle,SourceConfigValues.server_external_port);
	
	fclose(configFileHandle);
}

int GetHTTPPort(void)
{
	return SourceConfigValues.http_port;
}

void GetLogs(char *buf)
{
	strcpy(buf,LogBuffer);
}

void GetLogs2(char *buf)
{
	strcpy(buf,LogBuffer2);
}

void SetLogs2(char *buf)
{	
	int len = strlen(buf);
	if(len > LOG_BUFFER_SIZE2)
		len = LOG_BUFFER_SIZE2;
	strncpy(LogBuffer2, buf, len);
	LogBuffer2[len]= '\0';
}

void InitSourceConfiguration()
{
	FILE *configFileHandle;
	char configFileName[256];

	char stringValue[256];

	strcpy(configFileName,CONFIG_FILE_NAME);

	LoadDefaultConfiguration();
	
	if (configFileHandle = fopen(configFileName, "rb"))
	{
		printf("\n Configuration File Found");

		SourceConfigValues.video_codec = getWordValue(configFileHandle);
		SourceConfigValues.video_bitrate = getWordValue(configFileHandle);
		SourceConfigValues.video_width = getWordValue(configFileHandle);
		SourceConfigValues.video_height = getWordValue(configFileHandle);

		SourceConfigValues.audio_codec = getWordValue(configFileHandle);
		SourceConfigValues.audio_bitrate = getWordValue(configFileHandle);
		
		getStringValue(configFileHandle,stringValue);
		strncpy(SourceConfigValues.server_ip,stringValue,15);
		getStringValue(configFileHandle,stringValue);
		strncpy(SourceConfigValues.external_ip,stringValue,15);
		SourceConfigValues.server_port = getWordValue(configFileHandle);
		SourceConfigValues.http_port = getWordValue(configFileHandle);

		SourceConfigValues.src_client_port = SourceConfigValues.http_port + 2;

		getStringValue(configFileHandle,stringValue);
		strncpy(SourceConfigValues.channel_file,stringValue,15);
		SourceConfigValues.channel_ID = getWordValue(configFileHandle);

		SourceConfigValues.video_scalability = getWordValue(configFileHandle);

		SourceConfigValues.video_bitrate_1 = getWordValue(configFileHandle);
		SourceConfigValues.video_width_1 = getWordValue(configFileHandle);
		SourceConfigValues.video_height_1 = getWordValue(configFileHandle);

		SourceConfigValues.video_bitrate_2 = getWordValue(configFileHandle);
		SourceConfigValues.video_width_2 = getWordValue(configFileHandle);
		SourceConfigValues.video_height_2 = getWordValue(configFileHandle);

		SourceConfigValues.video_bitrate_3 = getWordValue(configFileHandle);
		SourceConfigValues.video_width_3 = getWordValue(configFileHandle);
		SourceConfigValues.video_height_3 = getWordValue(configFileHandle);

		SourceConfigValues.video_bitrate_4 = getWordValue(configFileHandle);
		SourceConfigValues.video_width_4 = getWordValue(configFileHandle);
		SourceConfigValues.video_height_4 = getWordValue(configFileHandle);

		SourceConfigValues.server_external_port = getWordValue(configFileHandle);
		
		fclose(configFileHandle);
	}
	else
	{
		SaveConfiguration();
	}
	
}

static void cmdline_parse(int argc, char *argv[])
{
	int o;
	int drop_value;
	
//	while ((o = getopt(argc, argv, "l:e:u:d:f:t:v:r:")) != -1) {
	while ((o = getopt(argc, argv, "l:e:u:d:f:t:r:g:")) != -1) {
		switch(o) {
			case 'l':
				loopback_disable = 1;
			break;
			case 'r':
				video_rewind = atoi(optarg);
			break;
			case 'e':
				wan_emulation_enable = atoi(optarg);
			break;
			case 'u':
				upload_bitrate_limit = atoi(optarg);;
			break;
			case 'd':
				download_bitrate_limit = atoi(optarg);
			break;
			case 'f':
				fec_enable = atoi(optarg);
			break;
/*
			case 'v':
				svc_enable = atoi(optarg);
				svc_enable_saved = svc_enable;
			break;
*/
			case 'g':
				debug_level = atoi(optarg);
			break;
			case 't':
				drop_value = atoi(optarg);
				drop_value = drop_value/10;
				
				if(drop_value == 0)
				{
					drop_packet_array 		= drop_packet_index_0;
					drop_packet_size		= sizeof(drop_packet_index_0)/4;
				}
				else if(drop_value == 1)
				{
					drop_packet_array 		= drop_packet_index_1;
					drop_packet_size		= sizeof(drop_packet_index_1)/4;
				}
				else if(drop_value == 2)
				{
					drop_packet_array 		= drop_packet_index_2;
					drop_packet_size		= sizeof(drop_packet_index_2)/4;
				}
				else if(drop_value == 3)
				{
					drop_packet_array 		= drop_packet_index_3;
					drop_packet_size		= sizeof(drop_packet_index_3)/4;
				}
				else if(drop_value == 4)
				{
					drop_packet_array 		= drop_packet_index_4;
					drop_packet_size		= sizeof(drop_packet_index_4)/4;
				}
				else if(drop_value == 5)
				{
					drop_packet_array 		= drop_packet_index_5;
					drop_packet_size		= sizeof(drop_packet_index_5)/4;
				}
				else if(drop_value == 6)
				{
					drop_packet_array 		= drop_packet_index_6;
					drop_packet_size		= sizeof(drop_packet_index_6)/4;
				}
				else if(drop_value == 7)
				{
					drop_packet_array 		= drop_packet_index_7;
					drop_packet_size		= sizeof(drop_packet_index_7)/4;
				}
				else if(drop_value == 8)
				{
					drop_packet_array 		= drop_packet_index_8;
					drop_packet_size		= sizeof(drop_packet_index_8)/4;
				}
				else if(drop_value == 9)
				{
					drop_packet_array 		= drop_packet_index_9;
					drop_packet_size		= sizeof(drop_packet_index_9)/4;
				}
				else if(drop_value == 10)
				{
					drop_packet_array 		= drop_packet_index_10;
					drop_packet_size		= sizeof(drop_packet_index_10)/4;
				}
			break;

		       default:
				//fprintf(stderr, "Error: unknown option %c\n", o);

				printf("STAMP source application version 2.4.1\n");
				printf("P2P Audio/Video Streaming application\n");
				printf("Usage: ./source.bin [options]\n\n");

				printf("Options:\n");
				printf("\t -l <int>\t\t-- use actual IP (1) / 127.0.0.1 (0) in loopback communication (default 0)\n");
				printf("\t -r <int>\t\t-- enable (1) / disable (0) Video rewind (default 1)\n");
				printf("\t -f <int>\t\t-- enable (1) / disable (0) FEC (default 0)\n");
				printf("\t -g <int>\t\t-- enable (1) / disable (0) Debug (default 0)\n");
				printf("\t\t -t <int>\t-- test FEC by dropping packets in percentage (default 0)\n");
				printf("\t -e <int>\t\t-- enable (1) / disable (0) network emulation (default 0)\n");
				printf("\t\t -u <int>\t-- upload bitrate limit in bits/sec (default 1000000)\n");
				printf("\t\t -d <int>\t-- download bitrate limit in bits/sec (default 1000000)\n");
				printf("\t -v <int>\t\t-- enable (1) / disable (0) SVC (default 0)\n");

				printf("\n\n");

			exit(-1);
		}
	}
	
	printf("WAN emulation: %d\n", wan_emulation_enable);
	printf("upload_bitrate_limit: %d\n", upload_bitrate_limit);
	printf("download_bitrate_limit: %d\n", download_bitrate_limit);
	printf("loopback_disable: %d\n", loopback_disable);
	printf("video_rewind: %d\n", video_rewind);
	printf("fec_enable: %d\n", fec_enable);	
//	printf("svc_enable: %d\n", svc_enable);	
	printf("drop_value: %d\n", drop_value);	
	printf("drop_packet_size: %d\n", drop_packet_size);	

}

void restartApplication(int web_restart) 
{
	int	 				fd = -1;
	int					result, i;
	struct sockaddr_in 		address;
	
	demuxSettings.pause = TRUE_STATE;
	if(web_restart)
		webServerSettings.pause 	= TRUE_STATE;

	printf("\n restartApplication ++");
	
	if(fec_enable == 1)
	{
		if(web_restart)
		{
			while((demuxSettings.threadStatus != STOPPED_THREAD)  || (videoDecoderSettings.threadStatus != STOPPED_THREAD) || (audioDecoderSettings.threadStatus != STOPPED_THREAD) || (videoEncoderSettings.threadStatus != STOPPED_THREAD) || (audioEncoderSettings.threadStatus != STOPPED_THREAD) || 
					   (videoChunkiserSettings.threadStatus != STOPPED_THREAD)  || (audioChunkiserSettings.threadStatus != STOPPED_THREAD)  || (fecEncSettings.threadStatus != STOPPED_THREAD) || (webServerSettings.threadStatus != STOPPED_THREAD))
			{
				/*
				printf("\n DM: %d VD: %d AD: %d VE: %d AE: %d VC: %d AC: %d",
					demuxSettings.threadStatus, 
					videoDecoderSettings.threadStatus,
					audioDecoderSettings.threadStatus,
					videoEncoderSettings.threadStatus,
					audioEncoderSettings.threadStatus,
					videoChunkiserSettings.threadStatus,
					audioChunkiserSettings.threadStatus);
					*/
				usleep(1000);
			}
			
		}
		else
		{
			while((demuxSettings.threadStatus != STOPPED_THREAD)  || (videoDecoderSettings.threadStatus != STOPPED_THREAD) || (audioDecoderSettings.threadStatus != STOPPED_THREAD) || (videoEncoderSettings.threadStatus != STOPPED_THREAD) || (audioEncoderSettings.threadStatus != STOPPED_THREAD) || 
					   (videoChunkiserSettings.threadStatus != STOPPED_THREAD)  || (audioChunkiserSettings.threadStatus != STOPPED_THREAD) || (fecEncSettings.threadStatus != STOPPED_THREAD))
			{
				/*
				printf("\n DM: %d VD: %d AD: %d VE: %d AE: %d VC: %d AC: %d",
					demuxSettings.threadStatus, 
					videoDecoderSettings.threadStatus,
					audioDecoderSettings.threadStatus,
					videoEncoderSettings.threadStatus,
					audioEncoderSettings.threadStatus,
					videoChunkiserSettings.threadStatus,
					audioChunkiserSettings.threadStatus);
					*/
				usleep(1000);
			}
			
		}

	}
	else
	{
		if(web_restart)
		{
			while((demuxSettings.threadStatus != STOPPED_THREAD)  || (videoDecoderSettings.threadStatus != STOPPED_THREAD) || (audioDecoderSettings.threadStatus != STOPPED_THREAD) || (videoEncoderSettings.threadStatus != STOPPED_THREAD) || (audioEncoderSettings.threadStatus != STOPPED_THREAD) || 
					   (videoChunkiserSettings.threadStatus != STOPPED_THREAD)  || (audioChunkiserSettings.threadStatus != STOPPED_THREAD) || (webServerSettings.threadStatus != STOPPED_THREAD))
			{
				/*
				printf("\n DM: %d VD: %d AD: %d VE: %d AE: %d VC: %d AC: %d",
					demuxSettings.threadStatus, 
					videoDecoderSettings.threadStatus,
					audioDecoderSettings.threadStatus,
					videoEncoderSettings.threadStatus,
					audioEncoderSettings.threadStatus,
					videoChunkiserSettings.threadStatus,
					audioChunkiserSettings.threadStatus);
					*/
				usleep(1000);
			}
			
		}
		else
		{
			while((demuxSettings.threadStatus != STOPPED_THREAD)  || (videoDecoderSettings.threadStatus != STOPPED_THREAD) || (audioDecoderSettings.threadStatus != STOPPED_THREAD) || (videoEncoderSettings.threadStatus != STOPPED_THREAD) || (audioEncoderSettings.threadStatus != STOPPED_THREAD) || 
					   (videoChunkiserSettings.threadStatus != STOPPED_THREAD)  || (audioChunkiserSettings.threadStatus != STOPPED_THREAD))
			{
				/*
				printf("\n DM: %d VD: %d AD: %d VE: %d AE: %d VC: %d AC: %d",
					demuxSettings.threadStatus, 
					videoDecoderSettings.threadStatus,
					audioDecoderSettings.threadStatus,
					videoEncoderSettings.threadStatus,
					audioEncoderSettings.threadStatus,
					videoChunkiserSettings.threadStatus,
					audioChunkiserSettings.threadStatus);
					*/
				usleep(1000);
			}
		}
	}
	
	stopP2PStreamer();

	if(web_restart)
	{
		close(videoChunkiserSettings.socketHandle);
		
		//Open UDP socket
		fd = socket(AF_INET, SOCK_DGRAM, 0);

#if 1
		char laddress[256] = {0};

		if(loopback_disable == 0)
			strcpy(laddress, "127.0.0.1");
		else
			strcpy(laddress, SourceConfigValues.external_ip);

		memset(&address, 0, sizeof(address));
		 
		// Filling server information
		address.sin_family 	= AF_INET;
		address.sin_port 	= htons(SourceConfigValues.src_client_port);
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
		//SourceConfigValues.src_client_port += 2;
		address.sin_port	= htons(SourceConfigValues.src_client_port);
		 
		result = connect(fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in));
		if(result == -1){
			fprintf(stderr, "Root: could not UDP connect to the streamer: ip: %s port: %d\n", "127.0.0.1", SourceConfigValues.src_client_port);
			exit(-1);
		}
#endif
		int sndBufSize 	= 4*1024*1024;
		setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&sndBufSize, sizeof(sndBufSize));

	}
	
	// Demux settings
	sprintf(demuxSettings.av_filename, "%s", SourceConfigValues.channel_file);
	demuxSettings.pause 				= FALSE_STATE;
	
	
	if(SourceConfigValues.video_scalability)
	{
		// Video encoder 1 settings
		videoEncoderSettings.codec_id		= SourceConfigValues.video_codec;

		videoEncoderSettings.bit_rate		= SourceConfigValues.video_bitrate_1; //SourceConfigValues.video_bitrate;
		videoEncoderSettings.width		= SourceConfigValues.video_width_1;//SourceConfigValues.video_width;
		videoEncoderSettings.height 		= SourceConfigValues.video_height_1; //SourceConfigValues.video_height;

		videoEncoderSettings.bit_rate_2		= SourceConfigValues.video_bitrate_2; //SourceConfigValues.video_bitrate;
		videoEncoderSettings.width_2		= SourceConfigValues.video_width_2;//SourceConfigValues.video_width;
		videoEncoderSettings.height_2 		= SourceConfigValues.video_height_2; //SourceConfigValues.video_height;

		videoEncoderSettings.bit_rate_3		= SourceConfigValues.video_bitrate_3; //SourceConfigValues.video_bitrate;
		videoEncoderSettings.width_3		= SourceConfigValues.video_width_3;//SourceConfigValues.video_width;
		videoEncoderSettings.height_3 		= SourceConfigValues.video_height_3; //SourceConfigValues.video_height;

		videoEncoderSettings.bit_rate_4		= SourceConfigValues.video_bitrate_4; //SourceConfigValues.video_bitrate;
		videoEncoderSettings.width_4		= SourceConfigValues.video_width_4;//SourceConfigValues.video_width;
		videoEncoderSettings.height_4 		= SourceConfigValues.video_height_4; //SourceConfigValues.video_height;

		videoEncoderSettings.scalability	= SourceConfigValues.video_scalability;
	}
	else
	{
		
		// Video encoder settings
		videoEncoderSettings.codec_id	= SourceConfigValues.video_codec;
		videoEncoderSettings.bit_rate		= SourceConfigValues.video_bitrate;
		videoEncoderSettings.width		= SourceConfigValues.video_width;
		videoEncoderSettings.height 		= SourceConfigValues.video_height;
		videoEncoderSettings.scalability	= SourceConfigValues.video_scalability;
	}

	// Audio encoder settings
	audioEncoderSettings.codec_id 	= SourceConfigValues.audio_codec;
	audioEncoderSettings.bit_rate 		= SourceConfigValues.audio_bitrate;

	if(web_restart)
	{
		// Video chunkiser settings
		videoChunkiserSettings.socketHandle 				= fd;
		videoChunkiserSettings.servaddr					= &address;

		// Audio chunkiser settings
		audioChunkiserSettings.socketHandle 				= fd;
		audioChunkiserSettings.servaddr					= &address;

		// interface settings
		webServerSettings.threadStatus	= IDLE_THREAD;
		webServerSettings.pause			= FALSE_STATE;
		sprintf(webServerSettings.httpPort,"%d",SourceConfigValues.http_port);
	}	
		
	if(SourceConfigValues.video_codec != AV_CODEC_ID_H264)
	{
		if(svc_enable_saved == 1)
		{
			fprintf(stderr, "*****************************************************\n");
			fprintf(stderr, "Scalable Video Coding (SVC) not supported for this video codec\n");
			fprintf(stderr, "*****************************************************\n");

			svc_enable = 0;
		}
		else
		{
			svc_enable = 0;
		}
	}
	else
	{
		svc_enable = svc_enable_saved;
	}

	//2Start Components

	startP2PStreamer();

	if(fec_enable == 1)
	{
		createThread(FECEncThread, 
					&fecEncSettings,
					16,
					(32*1024*1024), // stack size
					&fecEncHandle);		
	}
	
	createThread(GChunkiserThread, 
				&videoChunkiserSettings,
				14,
				(32*1024*1024), // stack size
				&videoChunkiserHandle);

	createThread(GChunkiserThread, 
				&audioChunkiserSettings,
				14,
				(32*1024*1024), // stack size
				&audioChunkiserHandle);

	/*
	createThread(FileWriteThread, 
				&audioFileWriteSettings,
				14,
				(32*1024*1024), // stack size
				&audioFileWriteHandle);
	*/
	
	createThread(VideoEncoderThread, 
				&videoEncoderSettings,
				12,
				(32*1024*1024), // stack size
				&videoEncoderHandle);

	createThread(AudioEncoderThread, 
				&audioEncoderSettings,
				12,
				(32*1024*1024), // stack size
				&audioEncoderHandle);

	createThread(VideoDecoderThread, 
				&videoDecoderSettings,
				12,
				(32*1024*1024), // stack size
				&videoDecoderHandle);

	createThread(AudioDecoderThread, 
				&audioDecoderSettings,
				12,
				(32*1024*1024), // stack size
				&audioDecoderHandle);

	createThread(DemuxThread, 
				&demuxSettings,
				10,
				(32*1024*1024), // stack size
				&demuxHandle);

	if(web_restart)
	{
		createThread(WebServerThread, 
					&webServerSettings,
					10,
					(32*1024*1024), // stack size
					&webServerHandle);
	}

	OldScalabilityStatus = SourceConfigValues.video_scalability;
}

#ifdef WIN32 
int startP2PStreamer()
{
	int 						ret = 0;
	char 					params_buffer[1024];
	STARTUPINFO 			si;

	sprintf(params_buffer, "p2p_streamer.exe -I %s -X %s -P %d -x %d -m %u -s %d -e %d -u %d -d %d -g %d", 
						SourceConfigValues.server_ip, SourceConfigValues.external_ip, SourceConfigValues.server_port, SourceConfigValues.server_external_port,  
						SourceConfigValues.channel_ID,
						SourceConfigValues.src_client_port,
						wan_emulation_enable,
						upload_bitrate_limit,
						download_bitrate_limit,
						debug_level
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
	char str_src_client_port[256] 	= {0};
	char str_metadata[256] 	= {0};
	char str_wan_emulation[256] 	= {0};
	char str_upload_bitrate[256] 	= {0};
	char str_download_bitrate[256] 	= {0};
	char str_shmem[256] 	= {0};
	char str_debug_level[256] 	= {0};

	int indx = 0;

	sprintf(p2pbin, "%s", "p2p_streamer.bin");
	sprintf(str_local_port, "%d", SourceConfigValues.server_port);
	sprintf(str_external_port, "%d", SourceConfigValues.server_external_port);
	sprintf(str_src_client_port, "%d", SourceConfigValues.src_client_port);
	sprintf(str_metadata, "%d", SourceConfigValues.channel_ID);
	sprintf(str_wan_emulation, "%d", wan_emulation_enable);
	sprintf(str_upload_bitrate, "%d", upload_bitrate_limit);
	sprintf(str_download_bitrate, "%d", download_bitrate_limit);
	sprintf(str_shmem, "%u", msg_recv_from_p2p_id);
	sprintf(str_debug_level, "%d", debug_level);

	params_array[indx] = p2pbin;
	indx++;
	params_array[indx] = "-I";
	indx++;
	params_array[indx] = SourceConfigValues.server_ip;//local_ip;
	indx++;
	params_array[indx] = "-X";
	indx++;
	params_array[indx] = SourceConfigValues.external_ip;//external_ip;
	indx++;
	params_array[indx] = "-P";
	indx++;
	params_array[indx] = str_local_port;
	indx++;
	params_array[indx] = "-x";
	indx++;
	params_array[indx] = str_external_port;
	indx++;
	params_array[indx] = "-s";
	indx++;
	params_array[indx] = str_src_client_port;
	indx++;
	params_array[indx] = "-m";
	indx++;
	params_array[indx] = str_metadata;
	indx++;
	params_array[indx] = "-g";
	indx++;
	params_array[indx] = str_debug_level;
	indx++;
//*
	params_array[indx] = "-z";
	indx++;
	params_array[indx] = str_shmem;
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


int main(int argc, char *argv[])
{
	int 						ret = 0;

	unsigned long				idleQ_Demux_2_Video_Decoder;
	unsigned long				liveQ_Demux_2_Video_Decoder;

	unsigned long				idleQ_Demux_2_Audio_Decoder;
	unsigned long				liveQ_Demux_2_Audio_Decoder;

	unsigned long				idleQ_Audio_Decoder_2_Audio_Encoder;
	unsigned long				liveQ_Audio_Decoder_2_Audio_Encoder;
	
	unsigned long				idleQ_Video_Decoder_2_Video_Encoder;
	unsigned long				liveQ_Video_Decoder_2_Video_Encoder;

	unsigned long				idleQ_Video_Encoder_2_Video_Chunkiser;
	unsigned long				liveQ_Video_Encoder_2_Video_Chunkiser;

	unsigned long				idleQ_Audio_Encoder_2_Audio_Chunkiser;
	unsigned long				liveQ_Audio_Encoder_2_Audio_Chunkiser;
	
	unsigned long				idleQ_Chunkiser_2_FEC_Encoder;
	unsigned long				liveQ_Chunkiser_2_FEC_Encoder;
	
	//unsigned long				idleQ_Audio_Encoder_2_File_Write;
	//unsigned long				liveQ_Audio_Encoder_2_File_Write;

	unsigned long 				totalpackets;
	int						bufSize;

 	unsigned long				onesecondclock;

	int	 					fd = -1;
	int						result, i;
	struct sockaddr_in 		address;

	int 						sndBufSize;

	char						tempBuffer[LOG_BUFFER_SIZE];
	char						*tempPtr;

	char 					statFilename[1024] = {0};

	char						systemURL[100];

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

	sprintf(statFilename, "StatsSource.txt");

	FILE * fptr = fopen(statFilename, "wt");
	fclose(fptr);

	signal(SIGINT , sigterm_handler); /* Interrupt (ANSI).    */
	signal(SIGTERM, sigterm_handler); /* Termination (ANSI).  */

	fprintf(stderr,"--- Source Application ---\n");

	//2Defaults
	app_defaults();
	cmdline_parse(argc, argv);

	InitSourceConfiguration();

	liveViewHandle 			= init_YUV2BMP(352, 240);

#ifdef WIN32
	WSADATA            			wsaData;
	short              			wVersionRequested = 0x101;

	// Initialize Winsock version 2.2
	if (WSAStartup( wVersionRequested, &wsaData ) == -1) 
	{
		printf("Root: WSAStartup failed with error %ld\n", WSAGetLastError());
		return -1;
	}
#endif
	
	/* register all formats and codecs */
	av_register_all();

	//Open UDP socket
	fd = socket(AF_INET, SOCK_DGRAM, 0);

#if 1
	char laddress[256] = {0};

	if(loopback_disable == 0)
		strcpy(laddress, "127.0.0.1");
	else
		strcpy(laddress, SourceConfigValues.external_ip);

	memset(&address, 0, sizeof(address));
	 
	// Filling server information
	address.sin_family 	= AF_INET;
	address.sin_port 	= htons(SourceConfigValues.src_client_port);
	if(inet_pton(AF_INET, laddress, &address.sin_addr)<=0)
	{
		printf("\n inet_pton error occured\n");
		return 1;
	}
	printf("src_client_port: %d\n", SourceConfigValues.src_client_port);
#else
	address.sin_family 	= AF_INET; 
#ifdef WIN32
	address.sin_addr.S_un.S_addr 	= inet_addr("127.0.0.1");
#else
	address.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
	address.sin_port 				= htons(SourceConfigValues.src_client_port);
	 
	result = connect(fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in));
	if(result == -1){
		fprintf(stderr, "Root: could not UDP connect to the streamer: ip: %s port: %d\n", "127.0.0.1", SourceConfigValues.src_client_port);
		exit(-1);
	}
#endif

	sndBufSize 	= 4*1024*1024;
	setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&sndBufSize, sizeof(sndBufSize));

	//2Queues Allocation
	// Queues Demux to Video Decoder
	fprintf(stderr,"Allocate demux 2 video decoder queue\n");
	totalpackets 	= 10;
	bufSize 		= MAX_VIDEO_BITRATE/(8*30);
	createQueue(&liveQ_Demux_2_Video_Decoder,	 	totalpackets,  	sizeof(packet_t));
	createQueue(&idleQ_Demux_2_Video_Decoder, 		totalpackets,  	sizeof(packet_t));
	allocatePacketsonQueue(idleQ_Demux_2_Video_Decoder, 
							totalpackets, 
							bufSize, 1);

	// Queues Demux to Audio Decoder
	fprintf(stderr,"Allocate demux 2 audio decoder queue\n");
	totalpackets 	= 60;
	bufSize 		= MAX_AUDIO_BITRATE/(8*44);
	createQueue(&liveQ_Demux_2_Audio_Decoder,	 	totalpackets,  	sizeof(packet_t));
	createQueue(&idleQ_Demux_2_Audio_Decoder, 		totalpackets,  	sizeof(packet_t));
	allocatePacketsonQueue(idleQ_Demux_2_Audio_Decoder, 
							totalpackets, 
							bufSize, 1);

	// Queues Audio Decoder to Audio Encoder
	fprintf(stderr,"Allocate audio decoder 2 audio encoder queue\n");
	totalpackets 	= 10;
	bufSize 		= MAX_RAW_AUDIO_FRAME;
	createQueue(&liveQ_Audio_Decoder_2_Audio_Encoder,	 	totalpackets,  	sizeof(packet_t));
	createQueue(&idleQ_Audio_Decoder_2_Audio_Encoder, 		totalpackets,  	sizeof(packet_t));
	allocatePacketsonQueue(idleQ_Audio_Decoder_2_Audio_Encoder, 
							totalpackets, 
							bufSize, 2);

	// Queues Video Decoder to Video Encoder
	fprintf(stderr,"Allocate video decoder 2 video encoder queue\n");
	totalpackets 	= 10;
	bufSize 		= MAX_RAW_VIDEO_FRAME;
	createQueue(&liveQ_Video_Decoder_2_Video_Encoder,	 	totalpackets,  	sizeof(packet_t));
	createQueue(&idleQ_Video_Decoder_2_Video_Encoder, 		totalpackets,  	sizeof(packet_t));
	allocatePacketsonQueue(idleQ_Video_Decoder_2_Video_Encoder, 
							totalpackets, 
							bufSize, 3);
	
	// Queues Video Encoder to Video Chunkiser
	fprintf(stderr,"Allocate video encoder 2 video chunkiser queue\n");
	totalpackets 	= 40;
	bufSize 		= MAX_VIDEO_BITRATE/(8*30);
	createQueue(&liveQ_Video_Encoder_2_Video_Chunkiser,	totalpackets,  	sizeof(packet_t));
	createQueue(&idleQ_Video_Encoder_2_Video_Chunkiser, 	totalpackets,  	sizeof(packet_t));
	allocatePacketsonQueue(idleQ_Video_Encoder_2_Video_Chunkiser, 
							totalpackets, 
							bufSize, 1);

	// Queues Audio Encoder to Audio Chunkiser
	fprintf(stderr,"Allocate video encoder 2 video chunkiser queue\n");
	totalpackets 	= 10;
	bufSize 		= MAX_AUDIO_BITRATE/(8*44);
	createQueue(&liveQ_Audio_Encoder_2_Audio_Chunkiser,	totalpackets,  	sizeof(packet_t));
	createQueue(&idleQ_Audio_Encoder_2_Audio_Chunkiser, 	totalpackets,  	sizeof(packet_t));
	allocatePacketsonQueue(idleQ_Audio_Encoder_2_Audio_Chunkiser, 
							totalpackets, 
							bufSize, 1);

	if(fec_enable == 1)
	{
		// Queues Chunkiser to FEC Encoder
		fprintf(stderr,"Allocate chunkiser 2 FEC encoder queue\n");
		totalpackets 	= 500;
		bufSize 		= MAX_RAW_VIDEO_FRAME;
		createQueue(&liveQ_Chunkiser_2_FEC_Encoder,  	totalpackets,  sizeof(packet_t));
		createQueue(&idleQ_Chunkiser_2_FEC_Encoder, 	totalpackets,  	sizeof(packet_t));
		allocatePacketsonQueue(idleQ_Chunkiser_2_FEC_Encoder, 
								totalpackets, 
								bufSize, 1);
	}
	
	/*
	// Queues Audio Encoder to File Writer
	fprintf(stderr,"Allocate audio encoder 2 file writer queue\n");
	totalpackets	= 10;
	bufSize 		= MAX_AUDIO_BITRATE/(8*44);
	createQueue(&liveQ_Audio_Encoder_2_File_Write, totalpackets,	sizeof(packet_t));
	createQueue(&idleQ_Audio_Encoder_2_File_Write, 	totalpackets,	sizeof(packet_t));
	allocatePacketsonQueue(idleQ_Audio_Encoder_2_File_Write, 
							totalpackets, 
							bufSize, 1);
	*/
	
	//2Setting Components
	// Demux settings
	sprintf(demuxSettings.av_filename, "%s", SourceConfigValues.channel_file);
	if(video_rewind == 0)
		demuxSettings.loop 	= 0;
	else
		demuxSettings.loop 	= 1;
	demuxSettings.pause = FALSE_STATE;
	demuxSettings.video_out_Q[LIVE_QUEUE] 	= liveQ_Demux_2_Video_Decoder;
	demuxSettings.video_out_Q[IDLE_QUEUE] 	= idleQ_Demux_2_Video_Decoder;
	demuxSettings.audio_out_Q[LIVE_QUEUE] 	= liveQ_Demux_2_Audio_Decoder;
	demuxSettings.audio_out_Q[IDLE_QUEUE] 	= idleQ_Demux_2_Audio_Decoder;

	// Video decoder settings
	videoDecoderSettings.maintain_input_frame_rate = 1;
	videoDecoderSettings.in_Q[LIVE_QUEUE] 	= liveQ_Demux_2_Video_Decoder;
	videoDecoderSettings.in_Q[IDLE_QUEUE] 	= idleQ_Demux_2_Video_Decoder;
	videoDecoderSettings.out_Q[LIVE_QUEUE] 	= liveQ_Video_Decoder_2_Video_Encoder;
	videoDecoderSettings.out_Q[IDLE_QUEUE] 	= idleQ_Video_Decoder_2_Video_Encoder;
	videoDecoderSettings.view_handle 			= liveViewHandle;
	
	// Audio decoder settings
	audioDecoderSettings.maintain_input_frame_rate = 1;
	audioDecoderSettings.in_Q[LIVE_QUEUE] 	= liveQ_Demux_2_Audio_Decoder;
	audioDecoderSettings.in_Q[IDLE_QUEUE] 	= idleQ_Demux_2_Audio_Decoder;
	audioDecoderSettings.out_Q[LIVE_QUEUE] 	= liveQ_Audio_Decoder_2_Audio_Encoder;
	audioDecoderSettings.out_Q[IDLE_QUEUE] 	= idleQ_Audio_Decoder_2_Audio_Encoder;

	if(SourceConfigValues.video_scalability)
	{
	// Video encoder settings
		videoEncoderSettings.codec_id		= SourceConfigValues.video_codec;

		videoEncoderSettings.bit_rate		= SourceConfigValues.video_bitrate_1; //SourceConfigValues.video_bitrate;
		videoEncoderSettings.width		= SourceConfigValues.video_width_1;//SourceConfigValues.video_width;
		videoEncoderSettings.height 		= SourceConfigValues.video_height_1; //SourceConfigValues.video_height;

		videoEncoderSettings.bit_rate_2	= SourceConfigValues.video_bitrate_2; //SourceConfigValues.video_bitrate;
		videoEncoderSettings.width_2		= SourceConfigValues.video_width_2;//SourceConfigValues.video_width;
		videoEncoderSettings.height_2 	= SourceConfigValues.video_height_2; //SourceConfigValues.video_height;

		videoEncoderSettings.bit_rate_3	= SourceConfigValues.video_bitrate_3; //SourceConfigValues.video_bitrate;
		videoEncoderSettings.width_3		= SourceConfigValues.video_width_3;//SourceConfigValues.video_width;
		videoEncoderSettings.height_3 	= SourceConfigValues.video_height_3; //SourceConfigValues.video_height;

		videoEncoderSettings.bit_rate_4	= SourceConfigValues.video_bitrate_4; //SourceConfigValues.video_bitrate;
		videoEncoderSettings.width_4		= SourceConfigValues.video_width_4;//SourceConfigValues.video_width;
		videoEncoderSettings.height_4 	= SourceConfigValues.video_height_4; //SourceConfigValues.video_height;

		videoEncoderSettings.scalability	= SourceConfigValues.video_scalability;
		
	}
	else
	{
		// Video encoder settings
		videoEncoderSettings.codec_id 			= SourceConfigValues.video_codec;
		videoEncoderSettings.bit_rate 				= SourceConfigValues.video_bitrate;
		videoEncoderSettings.width 				= SourceConfigValues.video_width;
		videoEncoderSettings.height 				= SourceConfigValues.video_height;

		videoEncoderSettings.scalability	= SourceConfigValues.video_scalability;
	}

	videoEncoderSettings.in_Q[LIVE_QUEUE] 	= liveQ_Video_Decoder_2_Video_Encoder;
	videoEncoderSettings.in_Q[IDLE_QUEUE] 	= idleQ_Video_Decoder_2_Video_Encoder;
	videoEncoderSettings.out_Q[LIVE_QUEUE] 	= liveQ_Video_Encoder_2_Video_Chunkiser;
	videoEncoderSettings.out_Q[IDLE_QUEUE]	= idleQ_Video_Encoder_2_Video_Chunkiser;

	// Audio encoder settings
	audioEncoderSettings.codec_id 			= SourceConfigValues.audio_codec;
	audioEncoderSettings.bit_rate 				= SourceConfigValues.audio_bitrate;
	audioEncoderSettings.in_Q[LIVE_QUEUE] 	= liveQ_Audio_Decoder_2_Audio_Encoder;
	audioEncoderSettings.in_Q[IDLE_QUEUE] 	= idleQ_Audio_Decoder_2_Audio_Encoder;
	audioEncoderSettings.out_Q[LIVE_QUEUE] 	= liveQ_Audio_Encoder_2_Audio_Chunkiser;
	audioEncoderSettings.out_Q[IDLE_QUEUE]	= idleQ_Audio_Encoder_2_Audio_Chunkiser;

	// Video chunkiser settings
	videoChunkiserSettings.type 					= AVMEDIA_TYPE_VIDEO;
	videoChunkiserSettings.num_frames_in_payload 	= 1;
	videoChunkiserSettings.max_chunk_size			= (MAX_VIDEO_BITRATE*2)/(8*30);

	videoChunkiserSettings.in_Q[LIVE_QUEUE]		= liveQ_Video_Encoder_2_Video_Chunkiser;
	videoChunkiserSettings.in_Q[IDLE_QUEUE]		= idleQ_Video_Encoder_2_Video_Chunkiser;

	if(fec_enable == 1)
	{
		videoChunkiserSettings.out_Q[LIVE_QUEUE]	= liveQ_Chunkiser_2_FEC_Encoder;
		videoChunkiserSettings.out_Q[IDLE_QUEUE]	= idleQ_Chunkiser_2_FEC_Encoder;	
	}
	else
	{
		videoChunkiserSettings.out_Q[LIVE_QUEUE]	= NULL;
		videoChunkiserSettings.out_Q[IDLE_QUEUE]	= NULL;	
	}
	videoChunkiserSettings.socketHandle 			= fd;
	videoChunkiserSettings.servaddr				= &address;

	// Audio chunkiser settings
	audioChunkiserSettings.type 					= AVMEDIA_TYPE_AUDIO;
	audioChunkiserSettings.num_frames_in_payload 	= 1;
	audioChunkiserSettings.max_chunk_size			= (MAX_VIDEO_BITRATE*2)/(8*30);
	audioChunkiserSettings.in_Q[LIVE_QUEUE]		= liveQ_Audio_Encoder_2_Audio_Chunkiser;
	audioChunkiserSettings.in_Q[IDLE_QUEUE]		= idleQ_Audio_Encoder_2_Audio_Chunkiser;
	if(fec_enable == 1)
	{
		audioChunkiserSettings.out_Q[LIVE_QUEUE]	= liveQ_Chunkiser_2_FEC_Encoder;
		audioChunkiserSettings.out_Q[IDLE_QUEUE]	= idleQ_Chunkiser_2_FEC_Encoder;	
	}
	else
	{
		audioChunkiserSettings.out_Q[LIVE_QUEUE]	= NULL;
		audioChunkiserSettings.out_Q[IDLE_QUEUE]	= NULL;	
	}	
	audioChunkiserSettings.socketHandle 			= fd;
	audioChunkiserSettings.servaddr				= &address;

	if(fec_enable == 1)
	{
		// fec encoder settings
		fecEncSettings.max_chunk_size				= (MAX_VIDEO_BITRATE*2)/(8*30);
		fecEncSettings.in_Q[LIVE_QUEUE]			= liveQ_Chunkiser_2_FEC_Encoder;
		fecEncSettings.in_Q[IDLE_QUEUE]			= idleQ_Chunkiser_2_FEC_Encoder;
		fecEncSettings.initial_crc					= STAMP_CRC;
		fecEncSettings.k 							= 8;
		fecEncSettings.n 							= 12;
		fecEncSettings.socketHandle 				= fd;
		fecEncSettings.servaddr					= &address;
	}

	// interface settings
	webServerSettings.threadStatus			= IDLE_THREAD;
	webServerSettings.pause 					= FALSE_STATE;
	sprintf(webServerSettings.httpPort,"%d",SourceConfigValues.http_port);
	webServerSettings.view_handle			= liveViewHandle;

	/*
	// Audio file write settings
	sprintf(audioFileWriteSettings.filename, "%s", "audio.mp3");
	audioFileWriteSettings.in_Q[LIVE_QUEUE] = liveQ_Audio_Encoder_2_File_Write;
	audioFileWriteSettings.in_Q[IDLE_QUEUE] = idleQ_Audio_Encoder_2_File_Write;
	*/

	if(SourceConfigValues.video_codec != AV_CODEC_ID_H264)
	{
		if(svc_enable_saved == 1)
		{
			fprintf(stderr, "*****************************************************\n");
			fprintf(stderr, "Scalable Video Coding (SVC) not supported for this video codec\n");
			fprintf(stderr, "*****************************************************\n");

			svc_enable = 0;
		}
		else
		{
			svc_enable = 0;
		}
	}
	else
	{
		svc_enable = svc_enable_saved;
	}

	//2Start Components

	startP2PStreamer();

	if(fec_enable == 1)
	{
		createThread(FECEncThread, 
					&fecEncSettings,
					16,
					(32*1024*1024), // stack size
					&fecEncHandle);	
	}
	
	createThread(GChunkiserThread, 
				&videoChunkiserSettings,
				14,
				(32*1024*1024), // stack size
				&videoChunkiserHandle);

	createThread(GChunkiserThread, 
				&audioChunkiserSettings,
				14,
				(32*1024*1024), // stack size
				&audioChunkiserHandle);

	/*
	createThread(FileWriteThread, 
				&audioFileWriteSettings,
				14,
				(32*1024*1024), // stack size
				&audioFileWriteHandle);
	*/
	
	createThread(VideoEncoderThread, 
				&videoEncoderSettings,
				12,
				(32*1024*1024), // stack size
				&videoEncoderHandle);

	createThread(AudioEncoderThread, 
				&audioEncoderSettings,
				12,
				(32*1024*1024), // stack size
				&audioEncoderHandle);

	createThread(VideoDecoderThread, 
				&videoDecoderSettings,
				12,
				(32*1024*1024), // stack size
				&videoDecoderHandle);

	createThread(AudioDecoderThread, 
				&audioDecoderSettings,
				12,
				(32*1024*1024), // stack size
				&audioDecoderHandle);

	createThread(DemuxThread, 
				&demuxSettings,
				10,
				(32*1024*1024), // stack size
				&demuxHandle);

	createThread(WebServerThread, 
				&webServerSettings,
				10,
				(32*1024*1024), // stack size
				&webServerHandle);

	OldScalabilityStatus = SourceConfigValues.video_scalability;

	cumVideoOutputBits = 0;
	cumAudioOutputBits = 0;
	cumNetworkOutputBits = 0;
	total_secs_count = 1;

	onesecondclock = timer_msec();

	//while((demuxSettings.threadStatus != STOPPED_THREAD)  || (videoDecoderSettings.threadStatus != STOPPED_THREAD) || (audioDecoderSettings.threadStatus != STOPPED_THREAD) || (videoEncoderSettings.threadStatus != STOPPED_THREAD) || (audioEncoderSettings.threadStatus != STOPPED_THREAD) || 
		//   (videoChunkiserSettings.threadStatus != STOPPED_THREAD)  || (audioChunkiserSettings.threadStatus != STOPPED_THREAD) )
	while(1)	
	{
		if((timer_msec() - onesecondclock) > 1000)
		{
			//printf("Dec: [V:%d fps, A:%d fps, V:%07.2f kbps, A:%07.2f kbps] Enc: [V:%d fps, A:%d fps, V:%07.2f kbps, A:%07.2f kbps] NW:[%07.2f kbps] CumBR:[V:%07.2f kbps, A:%07.2f kbps, NW:%07.2f kbps] ChunkRate: [%d/s]\n", videoFrameCnt, audioFrameCnt, (float)(videoInputBits)/1024.0, (float)(audioInputBits)/1024.0, videoEncFrameCnt, audioEncFrameCnt, (float)(videoOutputBits)/1024.0, (float)(audioOutputBits)/1024.0, (float)(networkOutputBits/1024.0), (float)(cumVideoOutputBits)/(total_secs_count*1024.0), (float)(cumAudioOutputBits)/(total_secs_count*1024.0), (float)(cumNetworkOutputBits)/(total_secs_count*1024.0), chunkOutputCnt);
			sprintf(tempBuffer,"Dec: [V:%d fps, A:%d fps, V:%07.2f kbps, A:%07.2f kbps] Enc: [V:%d fps, A:%d fps, V:%07.2f kbps (I:%07.2f kbps, P:%07.2f kbps, B:%07.2f kbps), A:%07.2f kbps] NW:[%07.2f kbps] CumBR:[V:%07.2f kbps, A:%07.2f kbps, NW:%07.2f kbps] ChunkRate: [%d/s]\n\n", videoFrameCnt, audioFrameCnt, (float)(videoInputBits)/1024.0, (float)(audioInputBits)/1024.0, videoEncFrameCnt, audioEncFrameCnt, (float)(videoOutputBits)/1024.0, (float)(videoOutputIBits)/1024.0, (float)(videoOutputPBits)/1024.0, (float)(videoOutputBBits)/1024.0, (float)(audioOutputBits)/1024.0, (float)(networkOutputBits/1024.0), (float)(cumVideoOutputBits)/(total_secs_count*1024.0), (float)(cumAudioOutputBits)/(total_secs_count*1024.0), (float)(cumNetworkOutputBits)/(total_secs_count*1024.0), chunkOutputCnt);

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
				char temp[1024] = {0};
				
				fprintf(statFile, "\n****** APPLICATION ********\n");

				cptr1 = p2p_buffer;
				cptr2 = strstr(p2p_buffer,"$");
				if(cptr2 == NULL)
					goto BYPASS;
				memset(temp, 0, 1024);
				memcpy(temp, cptr1, (cptr2-cptr1));

				fprintf(statFile, "%s", temp);

				fprintf(statFile, "Video Bitrate: %07.2f kbps (I: %07.2f kbps, P: %07.2f kbps, B: %07.2f kbps)\n", (float)(videoOutputBits/1024.0), (float)(videoOutputIBits/1024.0), (float)(videoOutputPBits/1024.0), (float)(videoOutputBBits/1024.0));
				fprintf(statFile, "Audio Bitrate: %07.2f kbps\n", (float)(audioOutputBits/1024.0));

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
				
BYPASS:
				fclose(statFile);
			}					


			strcpy(TempLogBuffer,LogBuffer);
			TempLogBuffer[(LOG_BUFFER_SIZE-1)-strlen(tempBuffer)] = '\0';
			
			strcpy(LogBuffer,tempBuffer);
			strcat(LogBuffer,TempLogBuffer);

			cumVideoOutputBits += videoOutputBits;
			cumAudioOutputBits += audioOutputBits;
			cumNetworkOutputBits += networkOutputBits;

			if(getAppStatus())
				total_secs_count++;

			audioFrameCnt = 0;
			videoFrameCnt = 0;
			audioEncFrameCnt = 0;
			videoEncFrameCnt = 0;
			videoInputBits = 0;
			audioInputBits = 0;
			videoOutputBits = 0;
			videoOutputIBits = 0;
			videoOutputPBits = 0;
			videoOutputBBits = 0;
			audioOutputBits = 0;
			networkOutputBits = 0;
			chunkOutputCnt = 0;

			onesecondclock = timer_msec();
		}

		if(restartApplicationVar > 0)
		{
			restartApplication(restartApplicationVar == 2);
			restartApplicationVar = 0;
		}

		//fprintf(stderr,"... sleep ...\n");
		oSleep(100);
	}

	close(fd);
	
	return 0;
}

