#include <stdio.h>
#include <stdarg.h>
#include <root.h>

//unsigned long		audioFrameCnt = 0;
//unsigned long 		audioInputBits = 0;

unsigned long			idleQ_Audio_Decoder_2_Player = 0;
unsigned long			liveQ_Audio_Decoder_2_Player = 0;
	
static uint8_t audio_buf_prev[MAX_RAW_AUDIO_FRAME];
static unsigned int audio_buf_prev_size;

int audioRunning = 0;

//FILE *				fptr;

unsigned long idleQ_Counter = 0;
unsigned long liveQ_Counter = 0;

int silence_packet = 0;
	
int errorCounter = 0;

float gAVSync_lag_msec;

unsigned long		audioRecvQueue = 0;

extern unsigned long audioSendQueue;
extern unsigned int currAudioTimeStamp;
extern unsigned int currVideoTimeStamp;

extern int dropAVSyncAudioPackets;
extern int stopPlayer;

extern unsigned int	audioFrameErrors;

int getAudioBuffering(void)
{
	return (int)(liveQ_Counter-idleQ_Counter);
}

void audio_callback(void *userdata, Uint8 *stream, int len)
{

	int rval;

	unsigned long	*			audio_sdl_Q = (unsigned long	* ) userdata;

	packet_t					AudioPacket;
	int						AudioSize = 0;

	while((len > 0)&&(audioRunning == 1)) 
	{

		if(silence_packet == 0)
		{
			silence_packet = getAudioBuffering() < 10;
		}

		if(silence_packet == 1)
		{
			memset(stream, 0, len);
			len = 0;

			if(getAudioBuffering() > 32)
				silence_packet = 0;
			
			return;
		}

		rval = receiveQueue(audio_sdl_Q[LIVE_QUEUE], &AudioPacket, sizeof(packet_t), &AudioSize, -1);
		if(rval)
		{
#if 1
			if(audio_buf_prev_size != 0)
			{
				SDL_MixAudio(stream, audio_buf_prev, audio_buf_prev_size, getSystemVolume());

				len -= audio_buf_prev_size;
				stream += audio_buf_prev_size;
			}
#endif			
			errorCounter++;

			return;
		}

		SDL_MixAudio(stream, AudioPacket.data[0]->packet, AudioPacket.data[0]->packet_size, getSystemVolume());
		//memcpy(stream, AudioPacket.data[0]->packet, AudioPacket.data[0]->packet_size);

		//memcpy(audio_buf_prev, AudioPacket.data[0]->packet, AudioPacket.data[0]->packet_size);
		memset(audio_buf_prev, 0, AudioPacket.data[0]->packet_size);
		audio_buf_prev_size = AudioPacket.data[0]->packet_size;
		

		len -= AudioPacket.data[0]->packet_size;
		stream += AudioPacket.data[0]->packet_size;


		currAudioTimeStamp = (unsigned int)AudioPacket.timestamp; 

		sendQueue(audio_sdl_Q[IDLE_QUEUE], &AudioPacket, AudioSize);


/*
		if(currAudioTimeStamp != 0)
		{
			float AVSync_lag_90KHz = gAVSync_lag_msec*90.0;
			int turn_on_condition = (int)(AVSync_lag_90KHz*1.5);

			while((int)(currAudioTimeStamp - currVideoTimeStamp) > turn_on_condition)
			{
				if(stopPlayer == 1)
					break;

				usleep(1);

				dropAVSyncAudioPackets++;
				
			}
		}

*/
		idleQ_Counter++;

	}

}

int AudioDecoderPlayerThread(void * arg)
{
	int						rval;
	pAudioDecoderPlayerSettings_t 	pSettings = (pAudioDecoderPlayerSettings_t ) arg;
	
	packet_t					InpacketBuf;
	packet_t					OutpacketBuf;
	packet_t					AudioPacket;
	int						InSize = 0;
	int						OutSize = 0;
	int						AudioSize = 0;

	int						i, ch;
	int						got_frame;
	int						len;

	AVCodec 				*codec = NULL;
	AVCodecContext 			*codec_context = NULL;
	AVFrame 				*frame = NULL;
	AVPacket 				pkt;
	int 						data_size;
	unsigned int				frameCnt = 0;

	struct SwrContext 		*swr_ctx;
	int						dst_nb_samples,dst_nb_channels;
	int						dst_linesize, dst_bufsize;
	int						firstFrame = 1;
	int						nb_planes;

	int64_t					prevTimestamp = 0;
	unsigned long				realtimeclock = 0;
	
	int 						audioFirstPacket = 1;
	
	SDL_AudioSpec 			wanted_spec, obtained;

	unsigned char 			**dst_data;

	char 					*tempPtr;

	unsigned long				audio_sdl_Q[2];

	unsigned long				adSema4;

	int						initDelay = 1;

	createSemaphore(&adSema4, 1);	

	//fptr = fopen("testAudio.pcm","wb");

	fprintf(stderr, "+++ AudioDecoderPlayerThread +++\n");

	pSettings->threadStatus = IDLE_THREAD;

	audio_buf_prev_size = 0;
	silence_packet = 0;

	/* initialize packet */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	if((liveQ_Audio_Decoder_2_Player == 0) && (idleQ_Audio_Decoder_2_Player == 0))
	{
		// Allocate queues Audio Decoder to Player
		fprintf(stderr,"Allocate audio decoder 2 Player queue\n");
	
		int totalpackets 	= 160;
		
		createQueue(&liveQ_Audio_Decoder_2_Player,		totalpackets,  	sizeof(packet_t));
		createQueue(&idleQ_Audio_Decoder_2_Player, 	totalpackets,  	sizeof(packet_t));	
		allocatePacketsonQueue(idleQ_Audio_Decoder_2_Player, 
								totalpackets, 
								MAX_RAW_AUDIO_FRAME, 1);
	}

	audio_sdl_Q[LIVE_QUEUE] = liveQ_Audio_Decoder_2_Player;
	audio_sdl_Q[IDLE_QUEUE] = idleQ_Audio_Decoder_2_Player;
	
	while(get_GUIStatus() != RUNNING_THREAD)
	{
		usleep(10);
	}
	
	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;
		
		audioRunning = 1;
		
		/* Recieve packet from Live Queue */
		
		rval = receiveQueue(pSettings->in_Q[LIVE_QUEUE], &InpacketBuf, sizeof(packet_t), &InSize, -1);
		if(rval)
		{
			if(rval != TIMEOUT_ERROR)
			{
				fprintf(stderr, "AudioDecoderPlayerThread: Receive error !!!\n");
				exit(-1);
			}
		}

		audioRecvQueue++;
		//printf("\t\t ... %d\n", audioRecvQueue);

		if(InpacketBuf.lastPacket == 1)
		{	
			InpacketBuf.lastPacket = 0;			
			sendQueue(pSettings->in_Q[IDLE_QUEUE], &InpacketBuf, InSize);		

			break;
		}
		
		if(InpacketBuf.data[0]->packet_size == 0)
		{
			sendQueue(pSettings->in_Q[IDLE_QUEUE], &InpacketBuf, InSize);		

			continue;
		}
		
		if(initDelay == 1)
		{
			usleep(200*1000);
			initDelay = 0;
		}

		//printf("AQ: %d\n", (audioSendQueue - audioRecvQueue));		
		
		if(codec == NULL)
		{
			codec = avcodec_find_decoder(InpacketBuf.codecID);
			if (!codec) 
			{
				fprintf(stderr, "AudioDecoderPlayerThread: Video Decoder not found\n");
				exit(1);
			}

			codec_context = avcodec_alloc_context3(codec);

			if (!codec_context) 
			{
				fprintf(stderr, "AudioDecoderPlayerThread: Could not allocate video codec context\n");
				exit(1);
			}
			
			/* open Audio Decoder */
			if (avcodec_open2(codec_context, codec, NULL) < 0) 
			{
				fprintf(stderr, "AudioDecoderPlayerThread: Could not open codec\n");
				exit(1);
			}

			 frame = av_frame_alloc();
			if (!frame) 
			{
				fprintf(stderr, "AudioDecoderPlayerThread: Could not allocate video frame\n");
				exit(1);
			}

			/* create resampler context */
			swr_ctx = swr_alloc();
			if (!swr_ctx) 
			{
			    fprintf(stderr, "AudioDecoderPlayerThread: Could not allocate resampler context\n");
			    exit(1);
			}
			

			printf("++++ Audio Decoder Player Initialization done ++++\n");
			// printf("Width %d Height %d Stride %d Format %d\n",codec_context->width,codec_context->height,frame->linesize[0],codec_context->pix_fmt);
			
		}

		gAVSync_lag_msec = pSettings->AVSync_lag_msec;
		pkt.data = InpacketBuf.data[0]->packet;
		pkt.size = InpacketBuf.data[0]->packet_size;

		//fwrite(pkt.data, pkt.size, 1, fptr);
		//audioInputBits += (pkt.size*8);

		len = avcodec_decode_audio4(codec_context, frame, &got_frame, &pkt);
		if(len <= 0) 
		{
			fprintf(stderr, "AudioDecoderPlayerThread: Error while decoding a frame.\n");
			audioFrameErrors++;
		}

		if(len > 0) 
		{
			if(got_frame)
			{
				if(audioFirstPacket == 1)
				{
					# if 0
					/* set options */
					av_opt_set_int(swr_ctx, "in_channel_layout",    codec_context->channels, 0);
					av_opt_set_int(swr_ctx, "in_sample_rate",       codec_context->sample_rate, 0);
					//av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", codec_context->sample_fmt, 0);
					av_opt_set_int(swr_ctx, "in_sample_fmt", codec_context->sample_fmt, 0);
					av_opt_set_int(swr_ctx, "out_channel_layout",    codec_context->channels, 0);
					av_opt_set_int(swr_ctx, "out_sample_rate",       codec_context->sample_rate, 0);
					//av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
					av_opt_set_int(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
					#endif

					swr_ctx = swr_alloc_set_opts(NULL,
		                                                 codec_context->channel_layout, 
		                                                 AV_SAMPLE_FMT_S16,
		                                                 codec_context->sample_rate,
		                                                 codec_context->channel_layout,          
		                                                 codec_context->sample_fmt,   
		                                                 codec_context->sample_rate,
		                                                 0, NULL);

		              

					/* initialize the resampling context */
					if ((rval = swr_init(swr_ctx)) < 0) 
					{
					    fprintf(stderr, "AudioDecoderPlayerThread: Failed to initialize the resampling context\n");
					    exit(1);
					}

					dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, codec_context->sample_rate) + frame->nb_samples, codec_context->sample_rate, codec_context->sample_rate, AV_ROUND_UP);

					rval = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, codec_context->channels, dst_nb_samples, AV_SAMPLE_FMT_S16, 1);

					dst_bufsize = av_samples_get_buffer_size(NULL, codec_context->channels, dst_nb_samples, AV_SAMPLE_FMT_S16, 1);

					//printf("\n dst_bufsize %d frame->nb_samples %d",dst_bufsize,frame->nb_samples);

					memset(audio_buf_prev, 0, dst_bufsize);
					audio_buf_prev_size = dst_bufsize;
					
					wanted_spec.freq = codec_context->sample_rate;
					wanted_spec.format = AUDIO_S16SYS;
					wanted_spec.channels = codec_context->channels;
					wanted_spec.silence = 0;
					wanted_spec.samples = dst_bufsize; //dst_bufsize/2; //frame->nb_samples;
					wanted_spec.callback = audio_callback;
					wanted_spec.userdata = audio_sdl_Q;

					if(SDL_OpenAudio(&wanted_spec, &obtained) < 0) 
					{
					  	fprintf(stderr, "AudioDecoderPlayerThread: SDL_OpenAudio: %s\n", SDL_GetError());
					  	exit(1);
					}

					SDL_PauseAudio(0);
				
					audioFirstPacket = 0;
				}

				data_size = av_samples_get_buffer_size(NULL,codec_context->channels,frame->nb_samples, codec_context->sample_fmt,1);
										
				/* convert to destination format*/

				rval = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)(frame->data), frame->nb_samples);

				if (rval < 0) 
				{
					fprintf(stderr, "AudioDecoderPlayerThread: Error while converting\n");
					//exit(1);
				}
				else
				{
					/* Planar to Interleaved */
						
					rval = receiveQueue(audio_sdl_Q[IDLE_QUEUE], &AudioPacket, sizeof(packet_t), &AudioSize, -1);
					if(rval)
					{
						if(rval != TIMEOUT_ERROR)
						{
							fprintf(stderr, "AudioDecoderPlayerThread: Receive error !!!\n");
							exit(-1);
						}
					}

					tempPtr = AudioPacket.data[0]->packet;

					if(0) //codec_context->sample_fmt >= AV_SAMPLE_FMT_U8P)
					{
						char*src_ptr = dst_data[0];
						char*src_ptr2 = src_ptr + (dst_bufsize/2);
						int j;
						
						for(i=0,j=0;i<dst_bufsize;i+=4,j+=2)
						{
							tempPtr[i] = src_ptr[j];
							tempPtr[i+1] = src_ptr[j+1];
							
							tempPtr[i+2] = src_ptr2[j];
							tempPtr[i+3] = src_ptr2[j+1];
						}
					}
					else
					{
						memcpy(tempPtr, dst_data[0], dst_bufsize);
					}

					AudioPacket.data[0]->packet_size = dst_bufsize;
					AudioPacket.timestamp = InpacketBuf.timestamp;

					sendQueue(audio_sdl_Q[LIVE_QUEUE], &AudioPacket, AudioSize);

					liveQ_Counter++;

					//printf("\n Q Fullness : %d",(liveQ_Counter-idleQ_Counter));

					//fwrite(audio_buf, audio_buf_size, 1, fptr);	

				}
#if 0
				rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &OutpacketBuf, sizeof(packet_t), &OutSize, -1);
				if(rval)
				{
					if(rval != TIMEOUT_ERROR)
					{
						fprintf(stderr, "AudioDecoderThread: Receive error !!!\n");
						exit(-1);
					}
				}

	        		nb_planes = av_sample_fmt_is_planar(codec_context->sample_fmt) ? codec_context->channels : 1;
				for(i=0; i < nb_planes; i++)
				{
					memcpy(OutpacketBuf.data[i]->packet, frame->extended_data[i], frame->linesize[0]);
					OutpacketBuf.data[i]->packet_size = frame->linesize[0];
				}
				
				OutpacketBuf.timestamp	= InpacketBuf.timestamp;
				OutpacketBuf.channel_layout= codec_context->channel_layout;
				OutpacketBuf.channels		= codec_context->channels;
				OutpacketBuf.sampleRate	= codec_context->sample_rate;
				OutpacketBuf.sample_fmt	= codec_context->sample_fmt;
				OutpacketBuf.frame_size	= codec_context->frame_size;
				
				/* Send packet to Live Queue*/
				
				sendQueue(pSettings->out_Q[LIVE_QUEUE], &OutpacketBuf, OutSize);
#endif
				//audioFrameCnt++;

				//printf("\n errorCounter %d",errorCounter);

				frameCnt++;
			}

			// Maintain frame-rate based on input timestamps
			/****************************************/
			if(pSettings->maintain_input_frame_rate == 1)
			{
				if(prevTimestamp != 0)
				{
					unsigned int diff_timestamp = ((unsigned int)(InpacketBuf.timestamp - prevTimestamp))/90;
					unsigned int diff_realtimeclock = timer_msec() - realtimeclock;

					//sprintf("Audio: prevTimestamp: %u timestamp: %u diff_timestamp: %u diff_realtimeclock: %u\n", prevTimestamp, InpacketBuf.timestamp, diff_timestamp, diff_realtimeclock);

					if((diff_timestamp-diff_realtimeclock) < diff_timestamp)
						acquireSemaphore(adSema4, (diff_timestamp-diff_realtimeclock));
					//	usleep((diff_timestamp-diff_realtimeclock)*(1000));
				}
				prevTimestamp = InpacketBuf.timestamp;
				realtimeclock = timer_msec();
			}
			/****************************************/

		}

	
		/* Send packet to Idle Queue*/
		
		sendQueue(pSettings->in_Q[IDLE_QUEUE], &InpacketBuf, InSize);
		
    }

#if 0
	rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &OutpacketBuf, sizeof(packet_t), &OutSize, -1);
	if(rval == 0)
	{
		OutpacketBuf.lastPacket = 1;
		sendQueue(pSettings->out_Q[LIVE_QUEUE], &OutpacketBuf, OutSize);
	}
#endif

	audioRunning = 0;

	avcodec_close(codec_context);
	av_free(codec_context);
	av_free_packet(&pkt);
	av_frame_free(&frame);
	if (dst_data)
	   av_freep(&dst_data[0]);

	av_freep(&dst_data);
	SDL_CloseAudio();

	printf("Audio Decoder Player Break ...\n");

	pSettings->threadStatus = STOPPED_THREAD;
	
}


