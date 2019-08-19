#include <stdio.h>
#include <stdarg.h>
#include <root.h>

#define DROP_LATE_FRAMES

//unsigned long		videoFrameCnt = 0;
//unsigned long 		videoInputBits = 0;

unsigned long		videoRecvQueue = 0;

extern unsigned long videoSendQueue;
extern unsigned long videoSendQueue;
extern unsigned int currVideoTimeStamp;
extern unsigned int currAudioTimeStamp;

extern unsigned int	videoFrameErrors;

extern int dropAVSyncVideoPackets;
extern int stopPlayer;

int getVideoBuffering(void)
{
	return (int)(videoSendQueue-videoRecvQueue);
}

int VideoDecoderPlayerThread(void * arg)
{
	int						rval;
	pVideoDecoderPlayerSettings_t 	pSettings = (pVideoDecoderPlayerSettings_t ) arg;
	pSDLParams_t				pParams = (pSDLParams_t ) pSettings->pgui_vdec_struct;
	
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
	int						firstKeyFrame = 0;

	struct SWSContext 		*sws_ctx = NULL;
	SDL_Overlay     			*bmp = NULL;
	AVPicture 				pict;
	SDL_Rect 				rect;
	int		 				curr_disp_width = 0; 
	int		 				curr_disp_height = 0;

	int64_t					prevTimestamp = 0;
	unsigned long				realtimeclock = 0;
	int64_t					prevFrameTimems = 0;
	unsigned long				dropFrames = 0;

	unsigned int				currWidth = 0;
	unsigned int 				currHeight = 0;

	unsigned long				vdSema4;

	int						initDelay = 1;
	//int						bmpinit  = 1;

	createSemaphore(&vdSema4, 1);

	fprintf(stderr, "+++ VideoDecoderThread +++\n");

	pSettings->threadStatus = IDLE_THREAD;

	/* initialize packet */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	videoRecvQueue = 0;

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
		
		videoRecvQueue++;

		unsigned long SSTime = timer_msec();

		if(InpacketBuf.lastPacket == 1)
		{
			fprintf(stderr, "VideoDecoderThread: Last Packet !!!\n");
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
			//printf("WAIT ...\n");
		}

		//printf("VQ: %d\n", (videoSendQueue - videoRecvQueue));
		
		//printf("$$$ currWidth %d currHeight %d  New: %dx%d$$$$\n", currWidth, currHeight, InpacketBuf.data[0]->width, InpacketBuf.data[0]->height);
		//*
		if((currWidth != InpacketBuf.data[0]->width) && (currHeight != InpacketBuf.data[0]->height))
		{
			curr_disp_width = 0;
			curr_disp_height = 0;
			
			firstKeyFrame = 0;
				
			currWidth = InpacketBuf.data[0]->width;
			currHeight = InpacketBuf.data[0]->height;

			//printf("$$$ Width %d Height %d $$$$\n", currWidth, currHeight);
		}
//*/
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
	
		if(firstKeyFrame == 0)
		{
			if(InpacketBuf.flags == 1)
			{
				firstKeyFrame = 1;
			}
			else
			{
				/* Send packet to Idle Queue*/
				sendQueue(pSettings->in_Q[IDLE_QUEUE], &InpacketBuf, InSize);

				continue;
			}
		}

		//videoInputBits += (pkt.size*8);

		while(pkt.size > 0)
		{
			len = avcodec_decode_video2(codec_context, frame, &got_frame, &pkt);
			if(len < 0) 
			{
				fprintf(stderr, "VideoDecoderThread: Error while decoding a frame.\n");
				videoFrameErrors++;
				
				break;
				//exit(1);
			}

			currVideoTimeStamp = (unsigned int)frame->pkt_dts;

			if(stopPlayer == 1)
				dropFrames = 1;

			//printf("frame->width: %d,%d frame->height: %d,%d\n", frame->width, currWidth, frame->height, currHeight);
			//printf("curr_disp_width: %d,%d curr_disp_height: %d,%d\n", curr_disp_width, pParams->display_width, curr_disp_height, pParams->display_height);
			
			if((currWidth != frame->width) && (currHeight != frame->height))
			{
				curr_disp_width = 0;
				curr_disp_height = 0;
									
				currWidth = frame->width;
				currHeight = frame->height;

				//printf("$$$ Width %d Height %d $$$$\n", currWidth, currHeight);
			}

			if(dropFrames == 0)
			{
				if(got_frame)
				{	
					//printf("Width %d Height %d Stride %d uvStride %d Format %d\n",codec_context->width,codec_context->height,frame->linesize[0], frame->linesize[1], codec_context->pix_fmt);

					// Render Video Frame
					if(pParams->mutex != NULL)
					{
						SDL_LockMutex(pParams->mutex);

						if((curr_disp_width != pParams->display_width) && (curr_disp_height != pParams->display_height))
						{
							printf("######### display_width: %d display_height: %d #########\n", pParams->display_width, pParams->display_height);
							//if(bmpinit == 1)
							{
								if(bmp != NULL)
								{
									SDL_FreeYUVOverlay(bmp);
									bmp = NULL;
								}

								bmp = SDL_CreateYUVOverlay(pParams->display_width, pParams->display_height, SDL_YV12_OVERLAY, pParams->screenHandle);

								//bmpinit = 0;
							}
							
							if(sws_ctx != NULL)
							{
								sws_freeContext(sws_ctx);
								sws_ctx = NULL;
							}

							// initialize SWS context for software scaling
							sws_ctx = sws_getContext(frame->width, frame->height, 
										codec_context->pix_fmt, pParams->display_width, pParams->display_height,
										codec_context->pix_fmt, SWS_BILINEAR,
										 NULL,
										 NULL,
										 NULL
										 );

							curr_disp_width = pParams->display_width;
							curr_disp_height = pParams->display_height;
						}


						SDL_LockYUVOverlay(bmp);
						//bmp = *sbmp;

						pict.data[0] = bmp->pixels[0];
						pict.data[1] = bmp->pixels[2];
						pict.data[2] = bmp->pixels[1];

						pict.linesize[0] = bmp->pitches[0];
						pict.linesize[1] = bmp->pitches[2];
						pict.linesize[2] = bmp->pitches[1];

						// Convert the image into YUV format that SDL uses
						sws_scale(sws_ctx, (uint8_t const * const *)frame->data, frame->linesize, 0, frame->height, pict.data, pict.linesize);

						SDL_UnlockYUVOverlay(bmp);

						if(pParams->sdl_flags&SDL_FULLSCREEN)
						{
							float aspect_ratio = (float)frame->width/(float)frame->height;
							int new_width = (int)((float)(pParams->display_height)*aspect_ratio);
							int diff_width = (pParams->display_width - new_width)/2;

							rect.x = diff_width;
							rect.y = 0;
							rect.w = pParams->display_width-diff_width*2;
							rect.h = pParams->display_height;
						}
						else
						{
							//if((frame->width > pParams->display_width) || (frame->height > pParams->display_height))
							{
								float aspect_ratio = (float)frame->width/(float)frame->height;
								int new_width = (int)((float)(pParams->display_height)*aspect_ratio);
								int diff_width = (pParams->display_width - new_width)/2;

								rect.x = diff_width;
								rect.y = 0;
								rect.w = pParams->display_width-diff_width*2;
								rect.h = pParams->display_height;
							}
							/*
							else
							{
								int diff_width = (pParams->display_width - frame->width)/2;
								int diff_height = (pParams->display_height - frame->height)/2;

								rect.x = diff_width;
								rect.y = diff_height;
								rect.w = pParams->display_width-diff_width*2;
								rect.h = pParams->display_height-diff_height*2;
							}
							*/
						}

						// 80,000 if f=0 20,000 if f=1
						float AVSync_lag_90KHz = pSettings->AVSync_lag_msec*90.0;
						int turn_on_condition = (int)(AVSync_lag_90KHz*0.8);
//*
						
						if(currVideoTimeStamp != 0)
						{
							while((int)(currAudioTimeStamp - currVideoTimeStamp) < turn_on_condition)
							{
								if(stopPlayer == 1)
									break;

								usleep(1);

								dropAVSyncVideoPackets++;
							}
						}
//*/						


#ifdef DROP_LATE_FRAMES
						{
							unsigned long startTime 			= timer_msec();
							unsigned long	diffVideoDispTime 	= 0;
							unsigned int 	diffpktframems		= (int)(((float)(frame->pkt_dts-prevFrameTimems)/90.0)+0.5);


							SDL_DisplayYUVOverlay(bmp, &rect);

							diffVideoDispTime = timer_msec() - startTime;
							if((diffVideoDispTime > diffpktframems) && (prevFrameTimems != 0) && (diffpktframems != 0))
							{
								dropFrames = (int)((float)(diffVideoDispTime)/(float)(diffpktframems)+0.5);
							}
							//printf("pkt_dts: %u prevFrameTimems: %u display time: %u diffpktframems: %u dropFrames: %d\n", frame->pkt_dts, prevFrameTimems, diffVideoDispTime, diffpktframems, dropFrames);
						}
#else
						SDL_DisplayYUVOverlay(bmp, &rect);
#endif

						SDL_UnlockMutex(pParams->mutex);
					}
					//videoFrameCnt++;
					
//					printf("Video playback rate: %d\n", timer_msec() - ((unsigned int)(frame->pkt_dts ))/90);

					// Maintain frame-rate based on input timestamps
					/****************************************/
					if(pSettings->maintain_input_frame_rate == 1)
					{
						if(prevTimestamp != 0)
						{
							unsigned int diff_timestamp = ((unsigned int)(frame->pkt_dts - prevTimestamp))/90;
							unsigned int diff_realtimeclock = timer_msec() - realtimeclock;

							//printf("prevTimestamp: %u timestamp: %u diff_timestamp: %u diff_realtimeclock: %u\n", prevTimestamp, frame->pkt_dts, diff_timestamp, diff_realtimeclock);

							if((diff_timestamp-diff_realtimeclock) < diff_timestamp)
								acquireSemaphore(vdSema4, (diff_timestamp-diff_realtimeclock));
							//	usleep((diff_timestamp-diff_realtimeclock)*(1000));
						}
						prevTimestamp = frame->pkt_dts;
						realtimeclock = timer_msec();
					}
					/****************************************/

				}
			}
			else
			{
				dropFrames--;
				if(dropFrames < 0)
					dropFrames = 0;
				//printf("\t dropFrames: %d\n", dropFrames);
			}
			
			if(got_frame)
				prevFrameTimems = frame->pkt_dts;

			if (pkt.data) 
			{
				pkt.size -= len;
				pkt.data += len;
			}
			
		}

		/* Send packet to Idle Queue*/
		sendQueue(pSettings->in_Q[IDLE_QUEUE], &InpacketBuf, InSize);
    	}

	// Clear YUV Overlay Area
	{
		char * ptrY = pict.data[0];
		char * ptrU = pict.data[1];
		char * ptrV = pict.data[2];

		if(pParams->mutex != NULL)
		{
			SDL_LockMutex(pParams->mutex);
			if(bmp != NULL)
			{
				SDL_LockYUVOverlay(bmp);

				pict.data[0] = bmp->pixels[0];
				pict.data[1] = bmp->pixels[2];
				pict.data[2] = bmp->pixels[1];

				pict.linesize[0] = bmp->pitches[0];
				pict.linesize[1] = bmp->pitches[2];
				pict.linesize[2] = bmp->pitches[1];

				// Set YUV 420 to Black Colour
				for(i=0; i < pParams->display_height; i++)
				{
					memset(ptrY, 0, pict.linesize[0]);
					ptrY += pict.linesize[0];
				}

				for(i=0; i < pParams->display_height/2; i++)
				{
					memset(ptrU, 128, pict.linesize[1]);
					ptrU += pict.linesize[1];

					memset(ptrV, 128, pict.linesize[2]);
					ptrV += pict.linesize[2];
				}
				
				SDL_UnlockYUVOverlay(bmp);

				rect.x = 0;
				rect.y = 0;
				rect.w = pParams->display_width;
				rect.h = pParams->display_height;

				SDL_DisplayYUVOverlay(bmp, &rect);
			}
			SDL_UnlockMutex(pParams->mutex);
		}
	}
	
	if(bmp) SDL_FreeYUVOverlay(bmp);
	if(sws_ctx) sws_freeContext(sws_ctx);
	
	avcodec_free_context(&codec_context);

	av_free_packet(&pkt);
	av_frame_free(&frame);

	printf("Video Decoder Break ...\n");

	pSettings->threadStatus = STOPPED_THREAD;
	
}


