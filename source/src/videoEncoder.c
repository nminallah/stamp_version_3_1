#include <stdio.h>
#include <stdarg.h>
#include <root.h>

#define	MAX_ENC_FRAME_TIME 	8

unsigned long 		videoEncFrameCnt = 0;
unsigned long 		videoOutputBits = 0;

unsigned long 		videoOutputIBits = 0;
unsigned long 		videoOutputPBits = 0;
unsigned long 		videoOutputBBits = 0;

unsigned int 		videoEncFrameSumCnt = 1;

extern int					svc_enable;

int VideoEncoderThread(void * arg)
{
	int						rval;
	pVideoEncoderSettings_t 	pSettings = (pVideoEncoderSettings_t ) arg;

	packet_t					inPacketBuf;
	unsigned long				inSize = 0;

	packet_t					outPacketBuf;
	unsigned long				outSize = 0;

	int						i;
	
	AVCodec *				codec = NULL;
	AVCodec *				codec2 = NULL;
	AVCodec *				codec3 = NULL;
	AVCodec *				codec4 = NULL;

	
	AVCodecContext *			codecCtx = NULL;
	AVCodecContext *			codecCtx2 = NULL;
	AVCodecContext *			codecCtx3 = NULL;
	AVCodecContext *			codecCtx4 = NULL;
	
	AVFrame *				decframe = NULL;
	AVFrame *				scaledframe = NULL;
	AVFrame *				scaledframe2 = NULL;
	AVFrame *				scaledframe3 = NULL;
	AVFrame *				scaledframe4 = NULL;
	
	AVPacket 				pkt;
	AVPacket 				pkt2;
	AVPacket 				pkt3;
	AVPacket 				pkt4;
	
	int 						frameFinished;

	struct SwsContext *		imgCtx = NULL;
	struct SwsContext *		imgCtx2 = NULL;
	struct SwsContext *		imgCtx3 = NULL;
	struct SwsContext *		imgCtx4 = NULL;
	
	int						video_frame_size;

	int64_t					prevFrameTimems = 0;
	unsigned long				dropFrames = 0;
	int						enc_frame_time[MAX_ENC_FRAME_TIME] = {0};
	int						enc_frame_index = 0;

	int						bFrameNum = 2;

	fprintf(stderr, "+++ VideoEncoderThread +++\n");

	pSettings->threadStatus = IDLE_THREAD;

	decframe = av_frame_alloc();
	if(decframe == NULL) 
	{
		fprintf(stderr, "VideoEncoderThread: avcodec_alloc_frame (decframe) error !!!\n");
		exit(1);
	}

	scaledframe = av_frame_alloc();
	if(scaledframe == NULL) 
	{
		fprintf(stderr, "VideoEncoderThread: avcodec_alloc_frame (scaling) error !!!\n");
		exit(-1);
	}

	scaledframe2 = av_frame_alloc();
	if(scaledframe2 == NULL) 
	{
		fprintf(stderr, "VideoEncoderThread: avcodec_alloc_frame (scaling) error !!!\n");
		exit(-1);
	}

	scaledframe3 = av_frame_alloc();
	if(scaledframe3 == NULL) 
	{
		fprintf(stderr, "VideoEncoderThread: avcodec_alloc_frame (scaling) error !!!\n");
		exit(-1);
	}

	scaledframe4 = av_frame_alloc();
	if(scaledframe4 == NULL) 
	{
		fprintf(stderr, "VideoEncoderThread: avcodec_alloc_frame (scaling) error !!!\n");
		exit(-1);
	}

	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;
			
	av_init_packet(&pkt2);
	pkt2.data = NULL;
	pkt2.size = 0;

	av_init_packet(&pkt3);
	pkt3.data = NULL;
	pkt3.size = 0;

	av_init_packet(&pkt4);
	pkt4.data = NULL;
	pkt4.size = 0;
			
	/* read frames from the file */
	while (1) 
	{
		pSettings->threadStatus = RUNNING_THREAD;

		rval = receiveQueue(pSettings->in_Q[LIVE_QUEUE], &inPacketBuf, sizeof(packet_t), &inSize, -1);
		if(rval)
		{
			if(rval != TIMEOUT_ERROR)
			{
				fprintf(stderr, "VideoEncoderThread: Receive error !!!\n");
				exit(-1);
			}
		}

		if(inPacketBuf.lastPacket == 1)
		{
			inPacketBuf.lastPacket = 0;
			sendQueue(pSettings->in_Q[IDLE_QUEUE], &inPacketBuf, inSize);
			break;
		}

		// Decoded frame
		decframe->data[0] 	= inPacketBuf.data[0]->packet;
		decframe->data[1] 	= inPacketBuf.data[1]->packet;
		decframe->data[2] 	= inPacketBuf.data[2]->packet;

		decframe->width  		= inPacketBuf.data[0]->width;
		decframe->height 		= inPacketBuf.data[0]->height;

		decframe->linesize[0] 	= inPacketBuf.data[0]->stride;
		decframe->linesize[1] 	= inPacketBuf.data[1]->stride;
		decframe->linesize[2] 	= inPacketBuf.data[2]->stride;

		decframe->pts 		= inPacketBuf.timestamp;
		decframe->format		= inPacketBuf.pix_fmt;
		
		if(codec == NULL)
		{	
			/* find the video encoder */
			codec = avcodec_find_encoder(pSettings->codec_id);
			if (!codec) 
			{
				fprintf(stderr, "VideoEncoderThread: codec not found\n");
				exit(1);
			}

 		    	codecCtx = avcodec_alloc_context3(codec);
			if(!codecCtx) 
			{
				fprintf(stderr, "VideoEncoderThread: could not allocate video context codec id: %d\n", pSettings->codec_id);
				exit(1);
			}

			codecCtx->codec_type 		= AVMEDIA_TYPE_VIDEO;
			codecCtx->codec_id 		= pSettings->codec_id;
			codecCtx->bit_rate 		= pSettings->bit_rate;
			codecCtx->rc_min_rate 	= pSettings->bit_rate;
			codecCtx->rc_max_rate 	= pSettings->bit_rate;
			codecCtx->width 			= pSettings->width;
			codecCtx->height 			= pSettings->height;
			codecCtx->time_base.num 	= inPacketBuf.frameRate.den;
			codecCtx->time_base.den 	= inPacketBuf.frameRate.num;
   	 		codecCtx->framerate.num 	= inPacketBuf.frameRate.num;
   	 		codecCtx->framerate.den 	= inPacketBuf.frameRate.den;
			codecCtx->gop_size 		= inPacketBuf.frameRate.num/inPacketBuf.frameRate.den;
			codecCtx->max_b_frames	= 0;
			codecCtx->refs 			= 1; 
			codecCtx->pix_fmt 		= inPacketBuf.pix_fmt;

			if(svc_enable == 1)
			{
				codecCtx->max_b_frames		= 2;
				codecCtx->b_frame_strategy  	= 0;
			}

			if (codec->id == AV_CODEC_ID_H264)
				av_opt_set(codecCtx->priv_data, "tune", "zerolatency", 0);

			if(avcodec_open2(codecCtx, codec, NULL) < 0) 
			{
				fprintf(stderr, "VideoEncoderThread: could not open video codec id: %d\n", pSettings->codec_id);
				exit(1);
			}

			/* Scaler Initialization if input output dimensions vary */
			if((decframe->height != pSettings->height) || (decframe->width != pSettings->width)) 
			{
				scaledframe->width 	= pSettings->width;
				scaledframe->height 	= pSettings->height;
				scaledframe->format	= codecCtx->pix_fmt;

				av_image_alloc(scaledframe->data, scaledframe->linesize, pSettings->width, pSettings->height, scaledframe->format, 1);

				imgCtx = sws_getCachedContext(imgCtx, decframe->width, decframe->height, decframe->format, scaledframe->width, scaledframe->height, scaledframe->format, SWS_BICUBIC, NULL, NULL, NULL);
				if(imgCtx == NULL) 
				{
					fprintf(stderr, "Cannot initialize the conversion context!\n");
					exit(1);
				}
			}

		}

		if(pSettings->scalability)
		{
			if(codec2 == NULL)
			{
				/* find the video encoder */
				codec2 = avcodec_find_encoder(pSettings->codec_id);
				if (!codec2) 
				{
					fprintf(stderr, "VideoEncoderThread: codec not found\n");
					exit(1);
				}

	 		    	codecCtx2 = avcodec_alloc_context3(codec2);
				if(!codecCtx2) 
				{
					fprintf(stderr, "VideoEncoderThread: could not allocate video context codec id: %d\n", pSettings->codec_id);
					exit(1);
				}

				codecCtx2->codec_type 		= AVMEDIA_TYPE_VIDEO;
				codecCtx2->codec_id 		= pSettings->codec_id;
				codecCtx2->bit_rate 		= pSettings->bit_rate_2;
				codecCtx2->rc_min_rate 	= pSettings->bit_rate_2;
				codecCtx2->rc_max_rate 	= pSettings->bit_rate_2;
				codecCtx2->width 			= pSettings->width_2;
				codecCtx2->height 		= pSettings->height_2;
				codecCtx2->time_base.num = inPacketBuf.frameRate.den;
				codecCtx2->time_base.den 	= inPacketBuf.frameRate.num;
	   	 		codecCtx2->framerate.num 	= inPacketBuf.frameRate.num;
	   	 		codecCtx2->framerate.den 	= inPacketBuf.frameRate.den;
				codecCtx2->gop_size 		= inPacketBuf.frameRate.num/inPacketBuf.frameRate.den;
				codecCtx2->max_b_frames	= 0;
				codecCtx2->refs 			= 1; 
				codecCtx2->pix_fmt 		= inPacketBuf.pix_fmt;

				if(svc_enable == 1)
				{
					codecCtx2->max_b_frames		= 2;
					codecCtx2->b_frame_strategy  	= 0;
				}

				if (codec2->id == AV_CODEC_ID_H264)
					av_opt_set(codecCtx2->priv_data, "tune", "zerolatency", 0);

				if(avcodec_open2(codecCtx2, codec2, NULL) < 0) 
				{
					fprintf(stderr, "VideoEncoderThread: could not open video codec id: %d\n", pSettings->codec_id);
					exit(1);
				}

				/* Scaler Initialization if input output dimensions vary */
				if((decframe->height != pSettings->height_2) || (decframe->width != pSettings->width_2)) 
				{
					scaledframe2->width 	= pSettings->width_2;
					scaledframe2->height 	= pSettings->height_2;
					scaledframe2->format	= codecCtx2->pix_fmt;

					av_image_alloc(scaledframe2->data, scaledframe2->linesize, pSettings->width_2, pSettings->height_2, scaledframe2->format, 1);

					imgCtx2 = sws_getCachedContext(imgCtx2, decframe->width, decframe->height, decframe->format, scaledframe2->width, scaledframe2->height, scaledframe2->format, SWS_BICUBIC, NULL, NULL, NULL);
					if(imgCtx2 == NULL) 
					{
						fprintf(stderr, "Cannot initialize the conversion context!\n");
						exit(1);
					}
				}
			}
			
			if(codec3 == NULL)
			{
				/* find the video encoder */
				codec3 = avcodec_find_encoder(pSettings->codec_id);
				if (!codec3) 
				{
					fprintf(stderr, "VideoEncoderThread: codec not found\n");
					exit(1);
				}

	 		    	codecCtx3 = avcodec_alloc_context3(codec3);
				if(!codecCtx3) 
				{
					fprintf(stderr, "VideoEncoderThread: could not allocate video context codec id: %d\n", pSettings->codec_id);
					exit(1);
				}

				codecCtx3->codec_type 		= AVMEDIA_TYPE_VIDEO;
				codecCtx3->codec_id 		= pSettings->codec_id;
				codecCtx3->bit_rate 		= pSettings->bit_rate_3;
				codecCtx3->rc_min_rate 	= pSettings->bit_rate_3;
				codecCtx3->rc_max_rate 	= pSettings->bit_rate_3;
				codecCtx3->width 			= pSettings->width_3;
				codecCtx3->height 		= pSettings->height_3;
				codecCtx3->time_base.num = inPacketBuf.frameRate.den;
				codecCtx3->time_base.den 	= inPacketBuf.frameRate.num;
	   	 		codecCtx3->framerate.num 	= inPacketBuf.frameRate.num;
	   	 		codecCtx3->framerate.den 	= inPacketBuf.frameRate.den;
				codecCtx3->gop_size 		= inPacketBuf.frameRate.num/inPacketBuf.frameRate.den;
				codecCtx3->max_b_frames	= 0;
				codecCtx3->refs 			= 1; 
				codecCtx3->pix_fmt 		= inPacketBuf.pix_fmt;

				if(svc_enable == 1)
				{
					codecCtx3->max_b_frames		= 2;
					codecCtx3->b_frame_strategy  	= 0;
				}

				if (codec3->id == AV_CODEC_ID_H264)
					av_opt_set(codecCtx3->priv_data, "tune", "zerolatency", 0);

				if(avcodec_open2(codecCtx3, codec3, NULL) < 0) 
				{
					fprintf(stderr, "VideoEncoderThread: could not open video codec id: %d\n", pSettings->codec_id);
					exit(1);
				}

				/* Scaler Initialization if input output dimensions vary */
				if((decframe->height != pSettings->height_3) || (decframe->width != pSettings->width_3)) 
				{
					scaledframe3->width 	= pSettings->width_3;
					scaledframe3->height 	= pSettings->height_3;
					scaledframe3->format	= codecCtx3->pix_fmt;

					av_image_alloc(scaledframe3->data, scaledframe3->linesize, pSettings->width_3, pSettings->height_3, scaledframe3->format, 1);

					imgCtx3 = sws_getCachedContext(imgCtx3, decframe->width, decframe->height, decframe->format, scaledframe3->width, scaledframe3->height, scaledframe3->format, SWS_BICUBIC, NULL, NULL, NULL);
					if(imgCtx3 == NULL) 
					{
						fprintf(stderr, "Cannot initialize the conversion context!\n");
						exit(1);
					}
				}
			}

			if(codec4 == NULL)
			{
				/* find the video encoder */
				codec4 = avcodec_find_encoder(pSettings->codec_id);
				if (!codec4) 
				{
					fprintf(stderr, "VideoEncoderThread: codec not found\n");
					exit(1);
				}

	 		    	codecCtx4 = avcodec_alloc_context3(codec4);
				if(!codecCtx4) 
				{
					fprintf(stderr, "VideoEncoderThread: could not allocate video context codec id: %d\n", pSettings->codec_id);
					exit(1);
				}

				codecCtx4->codec_type 		= AVMEDIA_TYPE_VIDEO;
				codecCtx4->codec_id 		= pSettings->codec_id;
				codecCtx4->bit_rate 		= pSettings->bit_rate_4;
				codecCtx4->rc_min_rate 	= pSettings->bit_rate_4;
				codecCtx4->rc_max_rate 	= pSettings->bit_rate_4;
				codecCtx4->width 			= pSettings->width_4;
				codecCtx4->height 		= pSettings->height_4;
				codecCtx4->time_base.num = inPacketBuf.frameRate.den;
				codecCtx4->time_base.den 	= inPacketBuf.frameRate.num;
	   	 		codecCtx4->framerate.num 	= inPacketBuf.frameRate.num;
	   	 		codecCtx4->framerate.den 	= inPacketBuf.frameRate.den;
				codecCtx4->gop_size 		= inPacketBuf.frameRate.num/inPacketBuf.frameRate.den;
				codecCtx4->max_b_frames	= 0;
				codecCtx4->refs 			= 1; 
				codecCtx4->pix_fmt 		= inPacketBuf.pix_fmt;

				if(svc_enable == 1)
				{
					codecCtx4->max_b_frames		= 2;
					codecCtx4->b_frame_strategy  	= 0;
				}

				if (codec4->id == AV_CODEC_ID_H264)
					av_opt_set(codecCtx4->priv_data, "tune", "zerolatency", 0);

				if(avcodec_open2(codecCtx4, codec4, NULL) < 0) 
				{
					fprintf(stderr, "VideoEncoderThread: could not open video codec id: %d\n", pSettings->codec_id);
					exit(1);
				}

				/* Scaler Initialization if input output dimensions vary */
				if((decframe->height != pSettings->height_4) || (decframe->width != pSettings->width_4)) 
				{
					scaledframe4->width 	= pSettings->width_4;
					scaledframe4->height 	= pSettings->height_4;
					scaledframe4->format	= codecCtx4->pix_fmt;

					av_image_alloc(scaledframe4->data, scaledframe4->linesize, pSettings->width_4, pSettings->height_4, scaledframe4->format, 1);

					imgCtx4 = sws_getCachedContext(imgCtx4, decframe->width, decframe->height, decframe->format, scaledframe4->width, scaledframe4->height, scaledframe4->format, SWS_BICUBIC, NULL, NULL, NULL);
					if(imgCtx4 == NULL) 
					{
						fprintf(stderr, "Cannot initialize the conversion context!\n");
						exit(1);
					}
				}
			}
		}


		if(dropFrames == 0)
		{
			unsigned int 	startTS 				= timer_msec();
			int 			sum 				= 0;
			unsigned long	diffavgEncTime 		= 0;
			unsigned int 	diffpktframems		= (int)(((float)(inPacketBuf.timestamp-prevFrameTimems)/90.0)+0.5);

			pkt.data = NULL;    // packet data will be allocated by the encoder
			pkt.buf = NULL;
			pkt.size = 0;		
				
			if(imgCtx != NULL)
			{
				/* Scale Image */
				sws_scale(imgCtx, decframe->data, decframe->linesize, 0, decframe->height, scaledframe->data, scaledframe->linesize);
				scaledframe->pts 			= decframe->pts;
				scaledframe->pict_type 	= AV_PICTURE_TYPE_NONE;
				video_frame_size = avcodec_encode_video2(codecCtx, &pkt, scaledframe, &frameFinished);
			}
			else
			{
				decframe->pict_type 		= AV_PICTURE_TYPE_NONE;
				video_frame_size = avcodec_encode_video2(codecCtx, &pkt, decframe, &frameFinished);
			}

			//printf("display time: %d ms dropFrames: %d\n", timer_msec()-startTime, dropFrames);

		       if (frameFinished) 
			{
				///*
				rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
				if(rval)
				{
					if(rval != TIMEOUT_ERROR)
					{
						fprintf(stderr, "VideoDecoderThread: Receive error !!!\n");
						exit(-1);
					}
				}

				if(pkt.size > outPacketBuf.data[0]->buf_size)
				{
					fprintf(stderr, "Error ... Video encoder packet data (%d) exceeds maximum buffer limit (%d) \n", pkt.size, outPacketBuf.data[0]->buf_size);
					exit(1);
				}

				outPacketBuf.frame_count		= videoEncFrameSumCnt;
				outPacketBuf.frameRate.num	= inPacketBuf.frameRate.num;
				outPacketBuf.frameRate.den		= inPacketBuf.frameRate.den;
				outPacketBuf.pix_fmt			= codecCtx->pix_fmt;
				outPacketBuf.codecID 			= pSettings->codec_id;
				outPacketBuf.timestamp 		= inPacketBuf.timestamp;
				if(pSettings->scalability)
					outPacketBuf.codecType 		=  AVMEDIA_TYPE_VIDEO;
				else
					outPacketBuf.codecType 		=  AVMEDIA_TYPE_VIDEO_US;
				
				if(((pkt.data[5]==0x9e)||(pkt.data[5]==0x9f))&&(pkt.flags==0))// B-Frames
				{
					outPacketBuf.flags				= bFrameNum;
					bFrameNum++;

					if(bFrameNum > 3)
						bFrameNum = 2;
				}
				else
				{
					outPacketBuf.flags				= pkt.flags & AV_PKT_FLAG_KEY;
				}

				outPacketBuf.data[0]->width		= pSettings->width;
				outPacketBuf.data[0]->height	= pSettings->height;

				memcpy(outPacketBuf.data[0]->packet, pkt.data, pkt.size);
				outPacketBuf.data[0]->packet_size = pkt.size;

			       videoOutputBits+=(pkt.size*8);

				if(outPacketBuf.flags > 1)// B-Frames
					videoOutputBBits += (pkt.size*8);
				else if(outPacketBuf.flags == 1)// I-Frames
					videoOutputIBits += (pkt.size*8);
				else
					videoOutputPBits += (pkt.size*8);

			
				sendQueue(pSettings->out_Q[LIVE_QUEUE], &outPacketBuf, outSize);
				//*/
				
				//fwrite(packetBuf.data[0]->packet, packetBuf.data[0]->packet_size, 1, fptr);
			}

			if(pSettings->scalability)
			{
				// 2

				pkt2.data = NULL;    // packet data will be allocated by the encoder
				pkt2.buf = NULL;
				pkt2.size = 0;
			
				if(imgCtx2 != NULL)
				{
					/* Scale Image */
					sws_scale(imgCtx2, decframe->data, decframe->linesize, 0, decframe->height, scaledframe2->data, scaledframe2->linesize);
					scaledframe2->pts 			= decframe->pts;
					scaledframe2->pict_type 	= AV_PICTURE_TYPE_NONE;
					video_frame_size = avcodec_encode_video2(codecCtx2, &pkt2, scaledframe2, &frameFinished);
				}
				else
				{
					decframe->pict_type 		= AV_PICTURE_TYPE_NONE;
					video_frame_size = avcodec_encode_video2(codecCtx2, &pkt2, decframe, &frameFinished);
				}

				//printf("display time: %d ms dropFrames: %d\n", timer_msec()-startTime, dropFrames);

			       if (frameFinished) 
				{
					///*
					rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
					if(rval)
					{
						if(rval != TIMEOUT_ERROR)
						{
							fprintf(stderr, "VideoDecoderThread: Receive error !!!\n");
							exit(-1);
						}
					}

					if(pkt2.size > outPacketBuf.data[0]->buf_size)
					{
						fprintf(stderr, "Error ... Video encoder packet data (%d) exceeds maximum buffer limit (%d) \n", pkt2.size, outPacketBuf.data[0]->buf_size);
						exit(1);
					}

					outPacketBuf.frame_count		= videoEncFrameSumCnt;
					outPacketBuf.frameRate.num	= inPacketBuf.frameRate.num;
					outPacketBuf.frameRate.den		= inPacketBuf.frameRate.den;
					outPacketBuf.pix_fmt			= codecCtx2->pix_fmt;
					outPacketBuf.codecID 			= pSettings->codec_id;
					outPacketBuf.timestamp 		= inPacketBuf.timestamp;
					outPacketBuf.codecType 		=  AVMEDIA_TYPE_VIDEO_1;
					
					if(((pkt2.data[5]==0x9e)||(pkt2.data[5]==0x9f))&&(pkt2.flags==0))// B-Frames
					{
						outPacketBuf.flags				= bFrameNum;
						bFrameNum++;

						if(bFrameNum > 3)
							bFrameNum = 2;
					}
					else
					{
						outPacketBuf.flags				= pkt2.flags & AV_PKT_FLAG_KEY;
					}

					outPacketBuf.data[0]->width		= pSettings->width_2;
					outPacketBuf.data[0]->height	= pSettings->height_2;

					memcpy(outPacketBuf.data[0]->packet, pkt2.data, pkt2.size);
					outPacketBuf.data[0]->packet_size = pkt2.size;

				       videoOutputBits+=(pkt2.size*8);

					if(outPacketBuf.flags > 1)// B-Frames
						videoOutputBBits += (pkt2.size*8);
					else if(outPacketBuf.flags == 1)// I-Frames
						videoOutputIBits += (pkt2.size*8);
					else
						videoOutputPBits += (pkt2.size*8);

					sendQueue(pSettings->out_Q[LIVE_QUEUE], &outPacketBuf, outSize);
					//*/
					
					//fwrite(packetBuf.data[0]->packet, packetBuf.data[0]->packet_size, 1, fptr);
				}

				// 3

				pkt3.data = NULL;    // packet data will be allocated by the encoder
				pkt3.buf = NULL;
				pkt3.size = 0;
				
				if(imgCtx3 != NULL)
				{
					/* Scale Image */
					sws_scale(imgCtx3, decframe->data, decframe->linesize, 0, decframe->height, scaledframe3->data, scaledframe3->linesize);
					scaledframe3->pts 			= decframe->pts;
					scaledframe3->pict_type 	= AV_PICTURE_TYPE_NONE;
					video_frame_size = avcodec_encode_video2(codecCtx3, &pkt3, scaledframe3, &frameFinished);
				}
				else
				{
					decframe->pict_type 		= AV_PICTURE_TYPE_NONE;
					video_frame_size = avcodec_encode_video2(codecCtx3, &pkt3, decframe, &frameFinished);
				}

				//printf("display time: %d ms dropFrames: %d\n", timer_msec()-startTime, dropFrames);

			       if (frameFinished) 
				{
					///*
					rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
					if(rval)
					{
						if(rval != TIMEOUT_ERROR)
						{
							fprintf(stderr, "VideoDecoderThread: Receive error !!!\n");
							exit(-1);
						}
					}

					if(pkt3.size > outPacketBuf.data[0]->buf_size)
					{
						fprintf(stderr, "Error ... Video encoder packet data (%d) exceeds maximum buffer limit (%d) \n", pkt3.size, outPacketBuf.data[0]->buf_size);
						exit(1);
					}

					outPacketBuf.frame_count		= videoEncFrameSumCnt;
					outPacketBuf.frameRate.num	= inPacketBuf.frameRate.num;
					outPacketBuf.frameRate.den		= inPacketBuf.frameRate.den;
					outPacketBuf.pix_fmt			= codecCtx3->pix_fmt;
					outPacketBuf.codecID 			= pSettings->codec_id;
					outPacketBuf.timestamp 		= inPacketBuf.timestamp;
					outPacketBuf.codecType 		=  AVMEDIA_TYPE_VIDEO_2;
					
					if(((pkt3.data[5]==0x9e)||(pkt3.data[5]==0x9f))&&(pkt3.flags==0))// B-Frames
					{
						outPacketBuf.flags				= bFrameNum;
						bFrameNum++;

						if(bFrameNum > 3)
							bFrameNum = 2;
					}
					else
					{
						outPacketBuf.flags				= pkt3.flags & AV_PKT_FLAG_KEY;
					}

					outPacketBuf.data[0]->width		= pSettings->width_3;
					outPacketBuf.data[0]->height	= pSettings->height_3;

					memcpy(outPacketBuf.data[0]->packet, pkt3.data, pkt3.size);
					outPacketBuf.data[0]->packet_size = pkt3.size;

				       videoOutputBits+=(pkt3.size*8);

					if(outPacketBuf.flags > 1)// B-Frames
						videoOutputBBits += (pkt3.size*8);
					else if(outPacketBuf.flags == 1)// I-Frames
						videoOutputIBits += (pkt3.size*8);
					else
						videoOutputPBits += (pkt3.size*8);

					sendQueue(pSettings->out_Q[LIVE_QUEUE], &outPacketBuf, outSize);
					//*/
					
					//fwrite(packetBuf.data[0]->packet, packetBuf.data[0]->packet_size, 1, fptr);
				}

				// 4
				
				pkt4.data = NULL;    // packet data will be allocated by the encoder
				pkt4.buf = NULL;
				pkt4.size = 0;
				
				if(imgCtx4 != NULL)
				{
					/* Scale Image */
					sws_scale(imgCtx4, decframe->data, decframe->linesize, 0, decframe->height, scaledframe4->data, scaledframe4->linesize);
					scaledframe4->pts 			= decframe->pts;
					scaledframe4->pict_type 	= AV_PICTURE_TYPE_NONE;
					video_frame_size = avcodec_encode_video2(codecCtx4, &pkt4, scaledframe4, &frameFinished);
				}
				else
				{
					decframe->pict_type 		= AV_PICTURE_TYPE_NONE;
					video_frame_size = avcodec_encode_video2(codecCtx4, &pkt4, decframe, &frameFinished);
				}

				//printf("display time: %d ms dropFrames: %d\n", timer_msec()-startTime, dropFrames);

			       if (frameFinished) 
				{
					///*
					rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
					if(rval)
					{
						if(rval != TIMEOUT_ERROR)
						{
							fprintf(stderr, "VideoDecoderThread: Receive error !!!\n");
							exit(-1);
						}
					}

					if(pkt4.size > outPacketBuf.data[0]->buf_size)
					{
						fprintf(stderr, "Error ... Video encoder packet data (%d) exceeds maximum buffer limit (%d) \n", pkt4.size, outPacketBuf.data[0]->buf_size);
						exit(1);
					}

					outPacketBuf.frame_count		= videoEncFrameSumCnt;
					outPacketBuf.frameRate.num	= inPacketBuf.frameRate.num;
					outPacketBuf.frameRate.den		= inPacketBuf.frameRate.den;
					outPacketBuf.pix_fmt			= codecCtx4->pix_fmt;
					outPacketBuf.codecID 			= pSettings->codec_id;
					outPacketBuf.timestamp 		= inPacketBuf.timestamp;
					outPacketBuf.codecType 		=  AVMEDIA_TYPE_VIDEO_3;
					
					if(((pkt4.data[5]==0x9e)||(pkt4.data[5]==0x9f))&&(pkt4.flags==0))// B-Frames
					{
						outPacketBuf.flags				= bFrameNum;
						bFrameNum++;

						if(bFrameNum > 3)
							bFrameNum = 2;
					}
					else
					{
						outPacketBuf.flags				= pkt4.flags & AV_PKT_FLAG_KEY;
					}

					outPacketBuf.data[0]->width		= pSettings->width_4;
					outPacketBuf.data[0]->height	= pSettings->height_4;

					memcpy(outPacketBuf.data[0]->packet, pkt4.data, pkt4.size);
					outPacketBuf.data[0]->packet_size = pkt4.size;

				       videoOutputBits+=(pkt4.size*8);

					if(outPacketBuf.flags > 1)// B-Frames
						videoOutputBBits += (pkt4.size*8);
					else if(outPacketBuf.flags == 1)// I-Frames
						videoOutputIBits += (pkt4.size*8);
					else
						videoOutputPBits += (pkt4.size*8);
				
				sendQueue(pSettings->out_Q[LIVE_QUEUE], &outPacketBuf, outSize);
				//*/
				
				//fwrite(packetBuf.data[0]->packet, packetBuf.data[0]->packet_size, 1, fptr);
			}
			}

			videoEncFrameCnt++;

			//printf("Encoder %d\n",videoEncFrameSumCnt);

			videoEncFrameSumCnt++;

			enc_frame_time[enc_frame_index] = (timer_msec() - startTS);
			enc_frame_index++;
			if(enc_frame_index > MAX_ENC_FRAME_TIME)
				enc_frame_index = 0;

			for(i=0;i<MAX_ENC_FRAME_TIME;i++)
				sum += enc_frame_time[i];

			diffavgEncTime = sum/MAX_ENC_FRAME_TIME;
			if((diffavgEncTime > diffpktframems) && (prevFrameTimems != 0))
			{
				dropFrames = (int)((float)(diffavgEncTime)/(float)(diffpktframems)+0.5);
			}
		}
		else
		{
			dropFrames--;
			if(dropFrames < 0)
				dropFrames = 0;
			//printf("\t dropFrames: %d\n", dropFrames);
		}

		prevFrameTimems = inPacketBuf.timestamp;

		sendQueue(pSettings->in_Q[IDLE_QUEUE], &inPacketBuf, inSize);
		
    	}

	rval = receiveQueue(pSettings->out_Q[IDLE_QUEUE], &outPacketBuf, sizeof(packet_t), &outSize, -1);
	if(rval == 0)
	{
		outPacketBuf.lastPacket = 1;
		sendQueue(pSettings->out_Q[LIVE_QUEUE], &outPacketBuf, outSize);
	}

	if(codecCtx != NULL)
	{
		avcodec_free_context(&codecCtx);
		av_free(codecCtx);
	}

	av_free_packet(&pkt);
	av_free_packet(&pkt2);
	av_free_packet(&pkt3);
	av_free_packet(&pkt4);

	if(decframe != NULL)
		av_frame_free(&decframe);

	if(scaledframe != NULL)
		av_frame_free(&scaledframe);

	if(scaledframe2 != NULL)
		av_frame_free(&scaledframe2);

	if(scaledframe3 != NULL)
		av_frame_free(&scaledframe3);

	if(scaledframe4 != NULL)
		av_frame_free(&scaledframe4);

	printf("Video Encoder Break ...\n");
	
	pSettings->threadStatus = STOPPED_THREAD;

}


