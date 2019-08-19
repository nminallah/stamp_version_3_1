#include <stdio.h>
#include <stdarg.h>
#include <root.h>

unsigned long		videoFrameCnt = 0;
unsigned long 		videoInputBits = 0;

int VideoDecoderThread(void * arg)
{
	int						rval;
	pVideoDecoderSettings_t 	pSettings = (pVideoDecoderSettings_t ) arg;
	
	packet_t					InpacketBuf;
	packet_t					OutpacketBuf;
	unsigned long				InSize = 0;
	unsigned long				OutSize = 0;

	int						i;
	int						got_frame;
	int						len;

	AVCodec 				*codec = NULL;
	AVCodecContext 			*codec_context = NULL;
	AVFrame 				*frame = NULL;
	AVPacket 				pkt;
	unsigned char				*srcPtr;
	unsigned char				*dstPtr;
	
	uint64_t					prevTimestamp = 0;
	int						deltaTimestamp = 0;
	unsigned long				realtimeclock = 0;

	fprintf(stderr, "+++ VideoDecoderThread +++\n");

	pSettings->threadStatus = STOPPED_THREAD;

	/* initialize packet */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

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
				fprintf(stderr, "VideoDecoderThread: Receive error !!!\n");
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
			codec = avcodec_find_decoder(InpacketBuf.codecID);
			if (!codec) 
			{
				fprintf(stderr, "VideoDecoderThread: Video Decoder not found\n");
				exit(1);
			}

			codec_context = avcodec_alloc_context3(codec);

			if (!codec_context) 
			{
				fprintf(stderr, "VideoDecoderThread: Could not allocate video codec context\n");
				exit(1);
			}

			if(codec->capabilities & CODEC_CAP_TRUNCATED) 
			{
    				codec_context->flags |= CODEC_FLAG_TRUNCATED;
  			}
			
			/* open Video Decoder */
			if (avcodec_open2(codec_context, codec, NULL) < 0) 
			{
				fprintf(stderr, "VideoDecoderThread: Could not open codec\n");
				exit(1);
			}

			frame = av_frame_alloc();
			if (!frame) 
			{
				fprintf(stderr, "VideoDecoderThread: Could not allocate video frame\n");
				exit(1);
			}
		
		}


		pkt.data = InpacketBuf.data[0]->packet;
		pkt.size = InpacketBuf.data[0]->packet_size;
		pkt.pts = InpacketBuf.timestamp;
		pkt.dts = InpacketBuf.timestamp;

		videoInputBits += (pkt.size*8);

		if(InpacketBuf.flags == 1)
		{
			prevTimestamp = 0;
			deltaTimestamp = 0;
		}

		while(pkt.size > 0)
		{
			len = avcodec_decode_video2(codec_context, frame, &got_frame, &pkt);
			if(len < 0) 
			{
				fprintf(stderr, "VideoDecoderThread: Error while decoding a frame.\n");
				exit(1);
			}
			
			if(got_frame)
			{
				//printf("Width %d Height %d Stride %d uvStride %d Format %d\n",codec_context->width,codec_context->height,frame->linesize[0], frame->linesize[1], codec_context->pix_fmt);
				
				/* Recieve packet from Idle Queue */
			
				if(getAppStatus())
				{
					rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &OutpacketBuf, sizeof(packet_t), &OutSize, -1);
					if(rval)
					{
						if(rval != TIMEOUT_ERROR)
						{
							fprintf(stderr, "VideoDecoderThread: Receive error !!!\n");
							exit(-1);
						}
					}

					OutpacketBuf.timestamp		= frame->pkt_dts;//InpacketBuf.timestamp;
					OutpacketBuf.pix_fmt			= codec_context->pix_fmt;
					OutpacketBuf.frameRate.num 	= InpacketBuf.frameRate.num;
					OutpacketBuf.frameRate.den 	= InpacketBuf.frameRate.den;
					OutpacketBuf.lastPacket 		= InpacketBuf.lastPacket;
					
					for(i=0; i < 3; i++)
					{
						if((frame->linesize[i]*frame->height) > OutpacketBuf.data[i]->buf_size)
						{
							fprintf(stderr, "Error ... Video decoder packet (%d) data (%d) exceeds maximum buffer limit (%d) \n", i, (frame->linesize[0]*frame->height), OutpacketBuf.data[i]->buf_size);
							exit(1);
						}

						if(codec_context->pix_fmt == AV_PIX_FMT_YUV420P)
						{
							if(i == 0)
							{
								OutpacketBuf.data[i]->width		= frame->width;
								OutpacketBuf.data[i]->height 	= frame->height;
							}
							else
							{
								OutpacketBuf.data[i]->width		= frame->width/2;
								OutpacketBuf.data[i]->height 	= frame->height/2;
							}
						}
						else if(codec_context->pix_fmt == AV_PIX_FMT_YUV422P)
						{
							if(i == 0)
							{
								OutpacketBuf.data[i]->width		= frame->width;
								OutpacketBuf.data[i]->height 	= frame->height;
							}
							else
							{
								OutpacketBuf.data[i]->width		= frame->width/2;
								OutpacketBuf.data[i]->height 	= frame->height;
							}
						}
						else if(codec_context->pix_fmt == AV_PIX_FMT_YUV444P)
						{
							OutpacketBuf.data[i]->width		= frame->width;
							OutpacketBuf.data[i]->height 	= frame->height;
						}
						else
						{
							fprintf(stderr, "VideoDecoderThread: Decoded Format not supported !!!\n");
							exit(-1);
						}

						OutpacketBuf.data[i]->packet_size	= (frame->linesize[i]*OutpacketBuf.data[i]->height);
						memcpy(OutpacketBuf.data[i]->packet, frame->data[i], (frame->linesize[i]*OutpacketBuf.data[i]->height));
						OutpacketBuf.data[i]->stride 		= frame->linesize[i];

					}

					/* Send packet to Live Queue*/

					sendQueue(pSettings->out_Q[LIVE_QUEUE], &OutpacketBuf, OutSize);

				}

				if(pSettings->view_handle != NULL)
				{
						set_YUV2BMP_data(pSettings->view_handle, frame->data, frame->width, frame->height, frame->linesize, codec_context->pix_fmt);
				}

				videoFrameCnt++;
				
				// Maintain frame-rate based on input timestamps
				/****************************************/
				if(pSettings->maintain_input_frame_rate == 1)
#if 1
				{
					if(prevTimestamp != 0)
					{
						unsigned int diff_timestamp = ((unsigned int)(frame->pkt_dts - prevTimestamp))/90;
						unsigned int diff_realtimeclock = timer_msec() - realtimeclock;

						//printf("prevTimestamp: %u timestamp: %u diff_timestamp: %u diff_realtimeclock: %u\n", prevTimestamp, frame->pkt_dts, diff_timestamp, diff_realtimeclock);

						if(diff_realtimeclock < diff_timestamp)
							usleep((diff_timestamp-diff_realtimeclock)*(1000));
					}
					prevTimestamp = frame->pkt_dts;
					realtimeclock = timer_msec();
				}
#endif
#if 0
				{
					if(prevTimestamp != 0)
					{
						unsigned int diff_timestamp = (((unsigned int)(frame->pkt_dts - prevTimestamp))*1000)/90;
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
					
					prevTimestamp = frame->pkt_dts;
				}
#endif
				/****************************************/

			}
			
			if (pkt.data) 
			{
				pkt.size -= len;
				pkt.data += len;
			}
			
		}

		/* Send packet to Idle Queue*/
		sendQueue(pSettings->in_Q[IDLE_QUEUE], &InpacketBuf, InSize);
    	}

	rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &OutpacketBuf, sizeof(packet_t), &OutSize, -1);
	if(rval == 0)
	{
		OutpacketBuf.lastPacket = 1;
		sendQueue(pSettings->out_Q[LIVE_QUEUE], &OutpacketBuf, OutSize);
	}

	avcodec_free_context(&codec_context);

	av_free_packet(&pkt);
	av_frame_free(&frame);

	printf("Video Decoder Break ...\n");

	pSettings->threadStatus = STOPPED_THREAD;
	
}


