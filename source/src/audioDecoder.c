#include <stdio.h>
#include <stdarg.h>
#include <root.h>

unsigned long		audioFrameCnt = 0;
unsigned long 		audioInputBits = 0;

extern int svc_enable;

int AudioDecoderThread(void * arg)
{
	int						rval;
	pAudioDecoderSettings_t 	pSettings = (pAudioDecoderSettings_t ) arg;
	
	packet_t					InpacketBuf;
	packet_t					OutpacketBuf;
	int						InSize = 0;
	int						OutSize = 0;

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

	uint64_t					prevTimestamp = 0;
	int						deltaTimestamp = 0;
	unsigned long				realtimeclock = 0;
	

	fprintf(stderr, "+++ AudioDecoderThread +++\n");

	/* initialize packet */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	pSettings->threadStatus = STOPPED_THREAD;
	
	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;
		
		/* Recieve packet from Live Queue */
		
		rval = receiveQueue(pSettings->in_Q[LIVE_QUEUE], &InpacketBuf, sizeof(packet_t), &InSize, -1);
		if(rval)
		{
			if(rval != TIMEOUT_ERROR)
			{
				fprintf(stderr, "AudioDecoderThread: Receive error !!!\n");
				exit(-1);
			}
		}

		if(InpacketBuf.lastPacket == 1)
		{	
			InpacketBuf.lastPacket = 0;
			sendQueue(pSettings->in_Q[IDLE_QUEUE], &InpacketBuf, InSize);		

			break;
		}
		
		
		if(codec == NULL)
		{
			if(svc_enable == 1)
				usleep(1000*400);

			codec = avcodec_find_decoder(InpacketBuf.codecID);
			if (!codec) 
			{
				fprintf(stderr, "AudioDecoderThread: Video Decoder not found\n");
				exit(1);
			}

			codec_context = avcodec_alloc_context3(codec);

			if (!codec_context) 
			{
				fprintf(stderr, "AudioDecoderThread: Could not allocate video codec context\n");
				exit(1);
			}
			
			/* open Audio Decoder */
			if (avcodec_open2(codec_context, codec, NULL) < 0) 
			{
				fprintf(stderr, "AudioDecoderThread: Could not open codec\n");
				exit(1);
			}

			 frame = av_frame_alloc();
			if (!frame) 
			{
				fprintf(stderr, "AudioDecoderThread: Could not allocate video frame\n");
				exit(1);
			}

			/* create resampler context */
			swr_ctx = swr_alloc();
			if (!swr_ctx) 
			{
			    fprintf(stderr, "AudioDecoderThread: Could not allocate resampler context\n");
			    exit(1);
			}
			

			printf("++++ Audio Decoder Initialization done ++++\n");
			// printf("Width %d Height %d Stride %d Format %d\n",codec_context->width,codec_context->height,frame->linesize[0],codec_context->pix_fmt);
			
		}


		pkt.data = InpacketBuf.data[0]->packet;
		pkt.size = InpacketBuf.data[0]->packet_size;

		audioInputBits += (pkt.size*8);

		if(InpacketBuf.flags == 1)
		{
			prevTimestamp = 0;
			deltaTimestamp = 0;
		}

		len = avcodec_decode_audio4(codec_context, frame, &got_frame, &pkt);
		if(len < 0) 
		{
			fprintf(stderr, "AudioDecoderThread: Error while decoding a frame.\n");
			exit(1);
		}

		if(got_frame)
		{

#if 0
			if(firstFrame)
			{
				/* set options */
				av_opt_set_int(swr_ctx, "in_channel_layout",    codec_context->channels, 0);
				av_opt_set_int(swr_ctx, "in_sample_rate",       codec_context->sample_rate, 0);
				av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", codec_context->sample_fmt, 0);
				av_opt_set_int(swr_ctx, "out_channel_layout",    codec_context->channels, 0);
				av_opt_set_int(swr_ctx, "out_sample_rate",       codec_context->sample_rate, 0);
				av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

				/* initialize the resampling context */
				if ((rval = swr_init(swr_ctx)) < 0) 
				{
				    fprintf(stderr, "AudioDecoderThread: Failed to initialize the resampling context\n");
				    exit(1);
				}
				firstFrame = 0;
			}
#endif

			#if 0
			{
			     int dsize = 2;
			     int ch;
			     FILE * outfile = fopen("audio.raw", "ab");
			     short * sdata = NULL;
			     short sval;

			     //fwrite(frame->extended_data[0], 1, frame->linesize[0], outfile);
			     for (i=0; i<frame->linesize[0]/2; i++)
			     {
		                for (ch=0; ch<2; ch++)
		                {
					sdata = (short *)(frame->extended_data[ch]); 
					sval = (short)(sdata[i]);
		                   // fwrite(frame->extended_data[ch] + i, 1, dsize, outfile);				
		                     fwrite(&sval, 1, 2, outfile);	
		                }
			     }

			     fclose(outfile);
			}
			#endif

			#if 0
			{
			     int dsize = 4;
			     int ch = 0;
			     FILE * outfile = fopen("audio2.raw", "ab");
			     float * fdata = NULL;
			     short sval;
			     static int gcnt = 0;

			     //fwrite(frame->extended_data[0], 1, frame->linesize[0], outfile);
			     for (i=0; i<frame->linesize[0]/8; i++)
			     {
		                for (ch=0; ch<2; ch++)
		                {
					fdata = (float *)(frame->extended_data[ch]); 
					sval = (short)(floor(fdata[i] * 32767));
					//printf("%d: float: %f %d\n", gcnt, fdata[i], sval);
					//if(fdata[i] != 0.0)
					//	exit(-1);
		                     fwrite(&sval, 1, 2, outfile);	
							 gcnt++;
		                }
			     }

			     fclose(outfile);
			}
			#endif

#if 0
			/* Recieve packet from Idle Queue */

			//printf("AudioDecoderThread: Channels %d Format %d nb_samples %d\n",codec_context->channels, codec_context->sample_fmt,frame->nb_samples);

			data_size = av_samples_get_buffer_size(NULL, codec_context->channels,frame->nb_samples, codec_context->sample_fmt, 1);
			if (data_size < 0) 
			{
			    /* This should not occur, checking just for paranoia */
			    fprintf(stderr, "AudioDecoderThread: Failed to calculate data size\n");
			    exit(1);
			}

			//printf("AudioDecoderThread: Decoded Data Size %d\n",data_size);

			/* compute destination number of samples */
			dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, 44100) + frame->nb_samples, 44100, 44100, AV_ROUND_UP);
			
			//printf("AudioDecoderThread: dst_nb_samples %d\n",dst_nb_samples);
			
			rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &OutpacketBuf, sizeof(packet_t), &OutSize, -1);
			if(rval)
			{
				if(rval != TIMEOUT_ERROR)
				{
					fprintf(stderr, "AudioDecoderThread: Receive error !!!\n");
					exit(-1);
				}
			}

			/* convert to destination format*/
			rval = swr_convert(swr_ctx, (const uint8_t **)(&(OutpacketBuf.data[0]->packet)), dst_nb_samples, (const uint8_t **)(&(frame->data[0])), frame->nb_samples);
			if (rval < 0) 
			{
				fprintf(stderr, "AudioDecoderThread: Error while converting\n");
				exit(1);
			} 
			
			dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels, rval, AV_SAMPLE_FMT_S16, 1);

			OutpacketBuf.data[0]->packet_size = dst_bufsize; //data_size;
#endif

			if(getAppStatus())
			{
#if 1 
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
			}

			audioFrameCnt++;

			frameCnt++;
		}

		// Maintain frame-rate based on input timestamps
		/****************************************/
		if(pSettings->maintain_input_frame_rate == 1)
#if 1
		{
			if(prevTimestamp != 0)
			{
				unsigned int diff_timestamp = ((unsigned int)(InpacketBuf.timestamp - prevTimestamp))/90;
				unsigned int diff_realtimeclock = timer_msec() - realtimeclock;

				//printf("prevTimestamp: %u timestamp: %u diff_timestamp: %u diff_realtimeclock: %u\n", prevTimestamp, packetBuf.data[0]->timestamp, diff_timestamp, diff_realtimeclock);

				if(diff_realtimeclock < diff_timestamp)
					usleep((diff_timestamp-diff_realtimeclock)*(1000));
			}
			prevTimestamp = InpacketBuf.timestamp;
			realtimeclock = timer_msec();
		}
#endif
#if 0
		{
			if(prevTimestamp != 0)
			{
				unsigned int diff_timestamp = (((unsigned int)(InpacketBuf.timestamp - prevTimestamp))*1000)/90;
				unsigned int diff_wait;

				diff_wait = diff_timestamp + deltaTimestamp;

				if(diff_wait > 0)
				{
					unsigned int start = timer_msec();
					usleep(diff_wait);
					unsigned int stop = timer_msec();

					deltaTimestamp += (diff_timestamp-((stop-start)*1000));
				}
			}
			
			prevTimestamp = InpacketBuf.timestamp;

		}
#endif

		/****************************************/

	
		/* Send packet to Idle Queue*/
		
		sendQueue(pSettings->in_Q[IDLE_QUEUE], &InpacketBuf, InSize);
		
    }

	rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &OutpacketBuf, sizeof(packet_t), &OutSize, -1);
	if(rval == 0)
	{
		OutpacketBuf.lastPacket = 1;
		sendQueue(pSettings->out_Q[LIVE_QUEUE], &OutpacketBuf, OutSize);
	}

	avcodec_close(codec_context);
	av_free(codec_context);
	av_free_packet(&pkt);
	av_frame_free(&frame);

	printf("Audio Decoder Break ...\n");

	pSettings->threadStatus = STOPPED_THREAD;
	
}


