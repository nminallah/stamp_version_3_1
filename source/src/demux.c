#include <stdio.h>
#include <stdarg.h>
#include <root.h>

int DemuxThread(void * arg)
{
	int					rval;
	DemuxSettings_t *		pdemuxSettings = (DemuxSettings_t *) arg;

	packet_t				vpacketBuf;
	unsigned long			vSize = 0;
	packet_t				apacketBuf;
	unsigned long			aSize = 0;

	AVFormatContext * 	avfmt = NULL;
	
	int 					video_stream_index = -1;
	AVStream *			video_stream = NULL;
	AVCodecContext *		video_dec_ctx = NULL;
	AVCodec *			video_dec = NULL;

	int 					audio_stream_index = -1;
	AVStream *			audio_stream = NULL;
	AVCodecContext *		audio_dec_ctx = NULL;
	AVCodec *			audio_dec = NULL;

	uint64_t				prev_vpacket_timestamp = 0;
	uint64_t				jump_vpacket_timestamp = 0;
	uint64_t				delta_vpacket_timestamp = 0;
	
	uint64_t				prev_apacket_timestamp = 0;
	uint64_t				jump_apacket_timestamp = 0;
	uint64_t				delta_apacket_timestamp = 0;

	int					vfirstPacket, afirstPacket;
	AVPacket 			pkt;

	fprintf(stderr, "+++ DemuxThread +++\n");

	pdemuxSettings->threadStatus = IDLE_THREAD;
	
	/* open input file, and allocate format context */
	if (avformat_open_input(&avfmt, pdemuxSettings->av_filename, NULL, NULL) < 0) {
		fprintf(stderr, "Could not open source file %s\n", pdemuxSettings->av_filename);
		exit(1);
	}

	/* retrieve stream information */
	if (avformat_find_stream_info(avfmt, NULL) < 0) {
		fprintf(stderr, "Could not find stream information in file %s\n", pdemuxSettings->av_filename);
		exit(1);
	}
		
	/* print stream information */
	av_dump_format(avfmt, 0, pdemuxSettings->av_filename, 0);

	/* open video codec */
	video_stream_index = av_find_best_stream(avfmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (video_stream_index < 0) {
		fprintf(stderr, "Could not find video stream in file '%s'\n", pdemuxSettings->av_filename);
		exit(1);
	} 
	else 
	{
		video_stream 		= avfmt->streams[video_stream_index];

		/* find video decoder */
		video_dec_ctx = video_stream->codec;
		video_dec = avcodec_find_decoder(video_dec_ctx->codec_id);
		if (video_dec == 0) 
		{
			fprintf(stderr, "Cannot find video codec\n");
			exit(1);
		}
	}

	/* open audio codec */
	audio_stream_index = av_find_best_stream(avfmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (audio_stream_index < 0) {
		fprintf(stderr, "Could not find audio stream in file '%s'\n", pdemuxSettings->av_filename);
		exit(1);
	} 
	else 
	{
		audio_stream 		= avfmt->streams[audio_stream_index];
	
		/* find audio decoder */
		audio_dec_ctx = audio_stream->codec;
		audio_dec = avcodec_find_decoder(audio_dec_ctx->codec_id);
		if (audio_dec == 0) 
		{
			fprintf(stderr, "Cannot find audio codec\n");
			exit(1);
		}
	}

	jump_vpacket_timestamp = time_90KHz();
	jump_apacket_timestamp = time_90KHz();

start:
	
	/* initialize packet */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	vfirstPacket = 1;
	afirstPacket = 1;

	/* read frames from the file */
	while (av_read_frame(avfmt, &pkt) >= 0) 
	{
		pdemuxSettings->threadStatus = RUNNING_THREAD;
		
		if(pdemuxSettings->pause == TRUE_STATE)
			break;
		
		if(pkt.stream_index == video_stream_index)
		{
			rval = receiveQueue(pdemuxSettings->video_out_Q[IDLE_QUEUE], &vpacketBuf, sizeof(packet_t), &vSize, -1);
			if(rval)
			{
				if(rval != TIMEOUT_ERROR)
				{
					fprintf(stderr, "Receive error !!!\n");
					exit(-1);
				}
			}

			if(rval == 0)
			{
				if(pkt.size > vpacketBuf.data[0]->buf_size)
				{
					fprintf(stderr, "Error ... Video data (%d) exceeds maximum buffer limit (%d) \n", pkt.size, vpacketBuf.data[0]->buf_size);
					exit(1);
				}

				vpacketBuf.frameRate.num	= video_stream->avg_frame_rate.num;
				vpacketBuf.frameRate.den	= video_stream->avg_frame_rate.den;
				vpacketBuf.timestamp 	= pkt.dts + jump_vpacket_timestamp;
				vpacketBuf.codecID 	= video_dec_ctx->codec_id;
				memcpy(vpacketBuf.data[0]->packet, pkt.data, pkt.size);
				vpacketBuf.data[0]->packet_size = pkt.size;
				vpacketBuf.flags		= vfirstPacket;
				prev_vpacket_timestamp = vpacketBuf.timestamp;
				
				sendQueue(pdemuxSettings->video_out_Q[LIVE_QUEUE], &vpacketBuf, vSize);

				vfirstPacket = 0;
			}
		}
		else if(pkt.stream_index == audio_stream_index)
		{
			rval = receiveQueue(pdemuxSettings->audio_out_Q[IDLE_QUEUE], &apacketBuf, sizeof(packet_t), &aSize, -1);
			if(rval)
			{
				if(rval != TIMEOUT_ERROR)
				{
					fprintf(stderr, "Receive error !!!\n");
					exit(-1);
				}
			}

			if(rval == 0)
			{
				if(pkt.size > apacketBuf.data[0]->buf_size)
				{
					fprintf(stderr, "Error ... Audio data (%d) exceeds maximum buffer limit (%d) \n", pkt.size, apacketBuf.data[0]->buf_size);
					exit(1);
				}

				apacketBuf.timestamp 	= pkt.dts + jump_apacket_timestamp;
				apacketBuf.codecID 	= audio_dec_ctx->codec_id;
				memcpy(apacketBuf.data[0]->packet, pkt.data, pkt.size);
				apacketBuf.data[0]->packet_size = pkt.size;
				apacketBuf.flags		= afirstPacket;

				prev_apacket_timestamp = apacketBuf.timestamp;
				
				sendQueue(pdemuxSettings->audio_out_Q[LIVE_QUEUE], &apacketBuf, aSize);

				afirstPacket = 0;
			}
		}

		av_free_packet(&pkt);

    	}

	//av_free_packet(&pkt);
	avformat_close_input(&avfmt);

	if((pdemuxSettings->loop == 1)&&(pdemuxSettings->pause == FALSE_STATE))
	{
		/* open input file, and allocate format context */
		if (avformat_open_input(&avfmt, pdemuxSettings->av_filename, NULL, NULL) < 0) {
			fprintf(stderr, "Could not open source file %s\n", pdemuxSettings->av_filename);
			exit(1);
		}

		/* retrieve stream information */
		if (avformat_find_stream_info(avfmt, NULL) < 0) {
			fprintf(stderr, "Could not find stream information in file %s\n", pdemuxSettings->av_filename);
			exit(1);
		}

		jump_vpacket_timestamp = prev_vpacket_timestamp;
		jump_apacket_timestamp = prev_apacket_timestamp;

		goto start;
	}

	//avcodec_free_context(&video_dec_ctx);
	//avcodec_free_context(&audio_dec_ctx);

	rval = receiveQueue(pdemuxSettings->video_out_Q[IDLE_QUEUE], &vpacketBuf, sizeof(packet_t), &vSize, -1);
	if(rval == 0)
	{
		vpacketBuf.lastPacket = 1;
		sendQueue(pdemuxSettings->video_out_Q[LIVE_QUEUE], &vpacketBuf, vSize);
	}

	rval = receiveQueue(pdemuxSettings->audio_out_Q[IDLE_QUEUE], &apacketBuf, sizeof(packet_t), &aSize, -1);
	if(rval == 0)
	{
		apacketBuf.lastPacket = 1;
		sendQueue(pdemuxSettings->audio_out_Q[LIVE_QUEUE], &apacketBuf, aSize);
	}

	printf("Demux Break ...\n");

	pdemuxSettings->threadStatus = STOPPED_THREAD;

}

