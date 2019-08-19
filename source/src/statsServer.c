#include <stdio.h>
#include <stdarg.h>
#include <root.h>

int StatsServerThread(void * arg)
{
	int						rval;
	StatsServerSettings_t *		pstatsSettings = (StatsServerSettings_t *) arg;

	fprintf(stderr, "+++ StatsServerThread +++\n");

	pstatsSettings->threadStatus = IDLE_THREAD;
	
	/* read frames from the file */
	while (1) 
	{
		pstatsSettings->threadStatus = RUNNING_THREAD;
		
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

