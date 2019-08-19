#include <stdio.h>
#include <stdarg.h>
#include <root.h>
#include <put_bits.h>

#define ADTS_HEADER_SIZE 7
#define ADTS_MAX_FRAME_BYTES ((1 << 13) - 1)

unsigned int sampling_freq[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,7350,0,0,0};

#ifdef WIN32
DECLARE_ALIGNED(16,uint8_t,audio_buf1)[MAX_RAW_AUDIO_FRAME];
DECLARE_ALIGNED(16,uint8_t,audio_buf2)[MAX_RAW_AUDIO_FRAME];
DECLARE_ALIGNED(16,uint8_t,audio_buftemp)[MAX_RAW_AUDIO_FRAME];
#endif

static int adts_frame_header(uint8_t *buf, int size, int objecttype, int sample_rate_index, int channel_conf)
{
    PutBitContext pb;

    unsigned full_frame_size = (unsigned)ADTS_HEADER_SIZE + size;
    if (full_frame_size > ADTS_MAX_FRAME_BYTES) {
        printf("ADTS frame size too large: %u (max %d)\n", full_frame_size, ADTS_MAX_FRAME_BYTES);
        exit(-1);
    }

    init_put_bits(&pb, buf, ADTS_HEADER_SIZE);

    /* adts_fixed_header */
    put_bits(&pb, 12, 0xfff);   /* syncword */
    put_bits(&pb, 1, 0);        /* ID */
    put_bits(&pb, 2, 0);        /* layer */
    put_bits(&pb, 1, 1);        /* protection_absent */
    put_bits(&pb, 2, objecttype); /* profile_objecttype */
    put_bits(&pb, 4, sample_rate_index);
    put_bits(&pb, 1, 0);        /* private_bit */
    put_bits(&pb, 3, channel_conf); /* channel_configuration */
    put_bits(&pb, 1, 0);        /* original_copy */
    put_bits(&pb, 1, 0);        /* home */

    /* adts_variable_header */
    put_bits(&pb, 1, 0);        /* copyright_identification_bit */
    put_bits(&pb, 1, 0);        /* copyright_identification_start */
    put_bits(&pb, 13, full_frame_size); /* aac_frame_length */
    put_bits(&pb, 11, 0x7ff);   /* adts_buffer_fullness */
    put_bits(&pb, 2, 0);        /* number_of_raw_data_blocks_in_frame */

    flush_put_bits(&pb);

    return ADTS_HEADER_SIZE;
}

unsigned long 		audioEncFrameCnt = 0;
unsigned long 		audioOutputBits = 0;

unsigned int 		audioEncFrameSumCnt = 1;

int AudioEncoderThread(void * arg)
{
	int						rval;
	pAudioEncoderSettings_t 	pSettings = (pAudioEncoderSettings_t ) arg;
	
	packet_t					InpacketBuf;
	packet_t					OutpacketBuf;
	int						InSize = 0;
	int						OutSize = 0;
	unsigned char *			optr;
	unsigned int				optrsize;

	int						got_frame;

	AVCodec 				*codec = NULL;
	AVCodecContext 			*codec_context = NULL;
	AVFrame 				*frame = NULL;
	AVPacket 				pkt;
	unsigned int				frameCnt = 0;
	int						i, nb_planes;

	unsigned int 				dst_nb_samples;
	unsigned int				sample_counts;
	int						sample_rate_index;
	
#ifndef WIN32
    	DECLARE_ALIGNED(16,uint8_t,audio_buf1)[MAX_RAW_AUDIO_FRAME];
    	DECLARE_ALIGNED(16,uint8_t,audio_buf2)[MAX_RAW_AUDIO_FRAME];
    	DECLARE_ALIGNED(16,uint8_t,audio_buftemp)[MAX_RAW_AUDIO_FRAME];
#endif

	struct SwrContext *		swr_ctx;

	fprintf(stderr, "+++ AudioEncoderThread +++\n");

	frame = av_frame_alloc();
	if (!frame) 
	{
		fprintf(stderr, "AudioEncoderThread: Could not allocate video frame\n");
		exit(1);
	}
	
	/* initialize packet */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	sample_counts = 0;
	
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
			
			/* find the mp3 encoder */
			codec = avcodec_find_encoder(pSettings->codec_id);
			if (!codec) 
			{
				fprintf(stderr, "AudioEncoderThread: codec not found\n");
				exit(1);
			}
		
			codec_context = avcodec_alloc_context3(codec);
		
			/* put sample parameters */
			codec_context->bit_rate 			= pSettings->bit_rate;
		
			/* check that the encoder supports s16 pcm input */
			if(pSettings->codec_id == AV_CODEC_ID_MP2)
				codec_context->sample_fmt 		= AV_SAMPLE_FMT_S16;
			else if(pSettings->codec_id == AV_CODEC_ID_AAC)
				codec_context->sample_fmt 		= AV_SAMPLE_FMT_FLTP;
			else				
				codec_context->sample_fmt 		= InpacketBuf.sample_fmt;
			
			/* select other audio parameters supported by the encoder */
			codec_context->sample_rate	  	= InpacketBuf.sampleRate;//pSettings->sample_rate; //select_sample_rate(codec);
			codec_context->channel_layout 		= InpacketBuf.channel_layout;//AV_CH_LAYOUT_STEREO; //select_channel_layout(codec);
			codec_context->channels 	  		= InpacketBuf.channels;//pSettings->channels;////av_get_channel_layout_nb_channels(codec_context->channel_layout); 
		
			codec_context->codec_type 			= AVMEDIA_TYPE_AUDIO;
		
			codec_context->time_base.num 		= 1;
			codec_context->time_base.den 		= codec_context->sample_rate;
		
			/* open it */
			if (avcodec_open2(codec_context, codec, NULL) < 0) 
			{
				fprintf(stderr, "AudioEncoderThread: Could not open codec\n");
				exit(1);
			}

			for(i=0; i<sampling_freq; i++)
			{
				if(sampling_freq[i] == codec_context->sample_rate)
				{
					sample_rate_index = i;
					break;
				}
			}

	              swr_ctx = swr_alloc_set_opts(NULL,
	                                                 codec_context->channel_layout, 
	                                                 codec_context->sample_fmt,
	                                                 codec_context->sample_rate,
	                                                 codec_context->channel_layout,          
	                                                 InpacketBuf.sample_fmt,   
	                                                 codec_context->sample_rate,
	                                                 0, NULL);

	              swr_init(swr_ctx);

			//printf("frame_size: %d\n", codec_context->frame_size);
			//printf("sample_fmt: %d channels: %d sampleRate: %d packet_size: %d frame_size: %d\n", InpacketBuf.sample_fmt, InpacketBuf.channels, InpacketBuf.sampleRate, InpacketBuf.data[0]->packet_size, InpacketBuf.frame_size);
		}

		if(pSettings->codec_id == AV_CODEC_ID_MP2)
			dst_nb_samples = 1152;
		else				
			dst_nb_samples = 1024;

		//Format conversion / planar to non-planar conversions
		if(InpacketBuf.sample_fmt != codec_context->sample_fmt)
		{
			uint8_t * out[] = {NULL, NULL};
			uint8_t * in[] = { InpacketBuf.data[0]->packet, InpacketBuf.data[1]->packet };
			int out_count, nb_samples;
			
			if(av_sample_fmt_is_planar(codec_context->sample_fmt))
			{
				out[0] = audio_buf1 + sample_counts*av_get_bytes_per_sample(codec_context->sample_fmt);
				out[1] = audio_buf2 + sample_counts*av_get_bytes_per_sample(codec_context->sample_fmt);
			}
			else
			{
				out[0] = audio_buf1 + sample_counts*codec_context->channels*av_get_bytes_per_sample(codec_context->sample_fmt);
			}
		
			out_count = sizeof(audio_buf1) / codec_context->channels / av_get_bytes_per_sample(codec_context->sample_fmt);

			nb_samples = swr_convert(swr_ctx, out, out_count, (const uint8_t **)in, InpacketBuf.frame_size);
			if (nb_samples < 0) {
				printf("audioEncoderThread: swr convert failed: %s.\n", av_err2str(nb_samples));
				exit(1);
			}

			sample_counts += nb_samples;
		}
		else
		{
			uint8_t * out[] = {NULL, NULL};
			uint8_t * in[] = { InpacketBuf.data[0]->packet, InpacketBuf.data[1]->packet };
			int inSize[] = { InpacketBuf.data[0]->packet_size, InpacketBuf.data[1]->packet_size };
			int nb_samples;
			
			if(av_sample_fmt_is_planar(codec_context->sample_fmt))
			{
				out[0] = audio_buf1 + sample_counts*av_get_bytes_per_sample(codec_context->sample_fmt);
				out[1] = audio_buf2 + sample_counts*av_get_bytes_per_sample(codec_context->sample_fmt);
			}
			else
			{
				out[0] = audio_buf1 + sample_counts*codec_context->channels*av_get_bytes_per_sample(codec_context->sample_fmt);
			}
		
			nb_samples = InpacketBuf.frame_size;
			if(out[0] != NULL)
				memcpy(out[0], in[0], inSize[0]);
			if(out[1] != NULL)
				memcpy(out[1], in[1], inSize[1]);
			
			sample_counts += nb_samples;
		}

		//Encode desired number of samples as per codec requirement
		while(sample_counts  >= dst_nb_samples)
		{
			unsigned int	data_size;
			unsigned int 	diff_samples;
			
			frame->nb_samples 	= dst_nb_samples;
			frame->format         	= codec_context->sample_fmt;
			frame->channels 		= codec_context->channels;

			nb_planes = av_sample_fmt_is_planar(codec_context->sample_fmt) ? codec_context->channels : 1;

			if(nb_planes == 2)
			{
				data_size = dst_nb_samples*av_get_bytes_per_sample(codec_context->sample_fmt);
				
				if(frame->extended_data[0] == NULL)
					frame->extended_data[0] = av_malloc(data_size);
				if(frame->extended_data[1] == NULL)
					frame->extended_data[1] = av_malloc(data_size);

				memcpy(frame->extended_data[0], audio_buf1, data_size);
				memcpy(frame->extended_data[1], audio_buf2, data_size);
				
				frame->linesize[0]			= data_size;
				frame->linesize[1]			= data_size;
			}
			else
			{
				data_size = dst_nb_samples*codec_context->channels*av_get_bytes_per_sample(codec_context->sample_fmt);
				
				if(frame->extended_data[0] == NULL)
					frame->extended_data[0] = av_malloc(data_size);

				memcpy(frame->extended_data[0], audio_buf1, data_size);
				
				frame->linesize[0]			= data_size;
			}

			/* encode the samples */
			avcodec_encode_audio2(codec_context,&pkt,frame,&got_frame);

			if(got_frame)
			{
				rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &OutpacketBuf, sizeof(packet_t), &OutSize, -1);
				if(rval)
				{
					if(rval != TIMEOUT_ERROR)
					{
						fprintf(stderr, "AudioEncoderThread: Receive error !!!\n");
						exit(-1);
					}
				}

				OutpacketBuf.frame_count	= audioEncFrameSumCnt;
				OutpacketBuf.codecID 		= pSettings->codec_id;
				OutpacketBuf.timestamp 	= InpacketBuf.timestamp;
				OutpacketBuf.codecType 	=  AVMEDIA_TYPE_AUDIO;
				OutpacketBuf.channels		= InpacketBuf.channels;
				OutpacketBuf.sampleRate	= InpacketBuf.sampleRate;
				OutpacketBuf.flags			= 0;

				optr 		= OutpacketBuf.data[0]->packet;
				optrsize 	= pkt.size;
				
				if(pSettings->codec_id == AV_CODEC_ID_AAC)
				{
					int len = adts_frame_header(optr, pkt.size, 1, sample_rate_index, codec_context->channels);

					optr 		+= len;
					optrsize 	= len + pkt.size;
				}

				memcpy(optr,pkt.data,pkt.size);
				OutpacketBuf.data[0]->packet_size = optrsize; 

				audioEncFrameCnt++;
			       audioOutputBits+=(optrsize*8);

				audioEncFrameSumCnt++;
				
				/* Send packet to Live Queue*/				
				sendQueue(pSettings->out_Q[LIVE_QUEUE], &OutpacketBuf, OutSize);

				av_free_packet(&pkt);		
			}

			diff_samples = (sample_counts - dst_nb_samples);
			if(diff_samples > 0)
			{
				if(nb_planes == 2)
				{
					memcpy(audio_buftemp, (unsigned char *)audio_buf1+ dst_nb_samples*av_get_bytes_per_sample(codec_context->sample_fmt), diff_samples*av_get_bytes_per_sample(codec_context->sample_fmt));
					memcpy(audio_buf1, (unsigned char *)audio_buftemp, diff_samples*av_get_bytes_per_sample(codec_context->sample_fmt));

					memcpy(audio_buftemp, (unsigned char *)audio_buf2+ dst_nb_samples*av_get_bytes_per_sample(codec_context->sample_fmt), diff_samples*av_get_bytes_per_sample(codec_context->sample_fmt));
					memcpy(audio_buf2, (unsigned char *)audio_buftemp, diff_samples*av_get_bytes_per_sample(codec_context->sample_fmt));
				}
				else
				{
					memcpy(audio_buftemp, (unsigned char *)audio_buf1+ dst_nb_samples*codec_context->channels*av_get_bytes_per_sample(codec_context->sample_fmt), diff_samples*codec_context->channels*av_get_bytes_per_sample(codec_context->sample_fmt));
					memcpy(audio_buf1, (unsigned char *)audio_buftemp, diff_samples*codec_context->channels*av_get_bytes_per_sample(codec_context->sample_fmt));
				}
			}
			
			sample_counts = diff_samples;

		}	

		/* Send packet to Idle Queue*/
		sendQueue(pSettings->in_Q[IDLE_QUEUE], &InpacketBuf, InSize);
		
    }

	#if 0
	/* Flush Remaining Packets */
	while(1)
	{
		
		/* encode the samples */
		avcodec_encode_audio2(codec_context,&pkt,NULL,&got_frame);

		if(got_frame)
		{
			
			rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &OutpacketBuf, sizeof(packet_t), &OutSize, -1);
			if(rval)
			{
				if(rval != TIMEOUT_ERROR)
				{
					fprintf(stderr, "AudioEncoderThread: Receive error !!!\n");
					exit(-1);
				}
			}

			OutpacketBuf.codecID = pSettings->codec_id;
			OutpacketBuf.timestamp = InpacketBuf.timestamp;
			
			memcpy(OutpacketBuf.data[0]->packet,pkt.data,pkt.size);
			OutpacketBuf.data[0]->packet_size = pkt.size; 
			
			/* Send packet to Live Queue*/
			
			sendQueue(pSettings->out_Q[LIVE_QUEUE], &OutpacketBuf, OutSize);

			av_free_packet(&pkt);		
			
		}
		else
		{
			break;
		}

	}
	#endif
	
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
	swr_free(&swr_ctx);
	
	printf("Audio Encoder Break ...\n");

	pSettings->threadStatus = STOPPED_THREAD;
}



