#include <stdio.h>
#include <stdarg.h>
#include <root.h>

#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;
    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;
    AVFrame *frame;
    AVFrame *tmp_frame;
    float t, tincr, tincr2;
    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;

#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

char 				filename[128] = {0};
AVOutputFormat *		fmt = NULL;
AVFormatContext *	oc = NULL;
OutputStream 		video_st = { 0 }, audio_st = { 0 };
AVCodec *			audio_codec, *video_codec;
int 					have_video = 0, have_audio = 0;
int 					encode_video = 0, encode_audio = 0;
AVDictionary *		opt = NULL;
AVPacket 			pkt;
AVPacket 			Audiopkt;

/* Add an output stream. */
static void add_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,
                       enum AVCodecID codec_id, unsigned int bitrate, unsigned int samplerate, unsigned int width, unsigned int height)
{
    AVCodecContext *c;
    int i;
    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        exit(1);
    }
    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams-1;
    c = avcodec_alloc_context3(*codec);
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
    ost->enc = c;
    switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt  = (*codec)->sample_fmts ?
            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate    = 64000;
        c->sample_rate = samplerate; //44100;
        if ((*codec)->supported_samplerates) {
            c->sample_rate = (*codec)->supported_samplerates[0];
            for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                if ((*codec)->supported_samplerates[i] == 44100)
                    c->sample_rate = 44100;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        c->channel_layout = AV_CH_LAYOUT_STEREO;
        if ((*codec)->channel_layouts) {
            c->channel_layout = (*codec)->channel_layouts[0];
            for (i = 0; (*codec)->channel_layouts[i]; i++) {
                if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                    c->channel_layout = AV_CH_LAYOUT_STEREO;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        ost->st->time_base = (AVRational){ 1, c->sample_rate };
        break;
    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;
        c->bit_rate = 400000;
        /* Resolution must be a multiple of two. */
        c->width    = 352;
        c->height   = 288;
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
        c->time_base       = ost->st->time_base;
        c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt       = STREAM_PIX_FMT;
        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
            /* just for testing, we also add B-frames */
            c->max_b_frames = 2;
        }
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
            /* Needed to avoid using macroblocks in which some coeffs overflow.
             * This does not happen with normal video, it just happens here as
             * the motion of the chroma plane does not match the luma plane. */
            c->mb_decision = 2;
        }
    break;
    default:
        break;
    }
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

/* audio output */
static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    int ret;
    if (!frame) {
        fprintf(stderr, "Error allocating an audio frame\n");
        exit(1);
    }
    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;
    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            fprintf(stderr, "Error allocating an audio buffer\n");
            exit(1);
        }
    }
    return frame;
}


static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;
    c = ost->enc;
    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
        exit(1);
    }
    /* init signal generator */
    ost->t     = 0;
    ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
    /* increment frequency by 110 Hz per second */
    ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;
    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = c->frame_size;
    ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                       c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
                                       c->sample_rate, nb_samples);
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
    /* create resampler context */
        ost->swr_ctx = swr_alloc();
        if (!ost->swr_ctx) {
            fprintf(stderr, "Could not allocate resampler context\n");
            exit(1);
        }
        /* set options */
        av_opt_set_int       (ost->swr_ctx, "in_channel_count",   c->channels,       0);
        av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
        av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
        av_opt_set_int       (ost->swr_ctx, "out_channel_count",  c->channels,       0);
        av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
        av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);
        /* initialize the resampling context */
        if ((ret = swr_init(ost->swr_ctx)) < 0) {
            fprintf(stderr, "Failed to initialize the resampling context\n");
            exit(1);
        }
}

/* video output */
static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    int ret;
    picture = av_frame_alloc();
    if (!picture)
        return NULL;
    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;
    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        exit(1);
    }
    return picture;
}

static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;
    av_dict_copy(&opt, opt_arg, 0);
    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }
    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            exit(1);
        }
    }
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
   //av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;
    /* Write the compressed frame to the media file. */
    //log_packet(fmt_ctx, pkt);
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_audio_frame(AVFormatContext *oc, OutputStream *ost, AVPacket *pkt)
{
    AVCodecContext *c;
    //AVPacket pkt = { 0 }; // data and size must be 0;
    AVFrame *frame;
    int ret;
    int got_packet = 1;
    int dst_nb_samples;
    //av_init_packet(&pkt);
    c = ost->enc;
    
    if (got_packet) {
        ret = write_frame(oc, &c->time_base, ost->st, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error while writing audio frame: %s\n",av_err2str(ret));
            //exit(1);
        }
    }
    return (frame || got_packet) ? 0 : 1;
}

static int write_video_frame(AVFormatContext *oc, OutputStream *ost, AVPacket *pkt)
{
    int ret;
    AVCodecContext *c;
    AVFrame *frame;
    int got_packet = 1;
    //AVPacket pkt = { 0 };
    c = ost->enc;
    if (got_packet) {
        ret = write_frame(oc, &c->time_base, ost->st, pkt);
    } else {
        ret = 0;
    }
    if (ret < 0) {
        fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
        //exit(1);
    }
    return (frame || got_packet) ? 0 : 1;
}

int initMuxPlayer(void)
{
	fprintf(stderr, "initMuxPlayer++\n");
	
	getChannelName(filename, 0);
	strcat(filename,".ts");
	
	/* allocate the output media context */
	avformat_alloc_output_context2(&oc, NULL, NULL, filename);
	if (!oc) 
	{
		printf("Could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
	}
	if (!oc)
		exit(-1);
	
	fmt = oc->oformat;

	av_init_packet(&pkt);
	av_init_packet(&Audiopkt);
	
	pkt.data = NULL;
	pkt.size = 0;

	have_audio = 0;
	have_video = 0;

	fprintf(stderr, "initMuxPlayer--\n");

	return 0;
}

int closeMuxPlayer(void)
{
	fprintf(stderr, "closeMuxPlayer++\n");
	
	/* Write the trailer, if any. The trailer must be written before you
	 * close the CodecContexts open when you wrote the header; otherwise
	 * av_write_trailer() may try to use memory that was freed on
	 * av_codec_close(). */
	 
	av_write_trailer(oc);
	
	/* Close each codec. */
	if (have_video)
	    close_stream(oc, &video_st);
	if (have_audio)
	    close_stream(oc, &audio_st);
	if (!(fmt->flags & AVFMT_NOFILE))
		
	/* Close the output file. */
	avio_closep(&oc->pb);
	
	/* free the stream */
	avformat_free_context(oc);

	fprintf(stderr, "closeMuxPlayer--\n");

	return 0;
}

int muxVideoPlayer(unsigned char * data, unsigned int size, int64_t timestamp_90KHz, int codec_id, int width, int height)
{
	int ret;
	
	pkt.data 	= data;
	pkt.size	= size;
	pkt.pts 	= timestamp_90KHz;
	pkt.dts 	= timestamp_90KHz;

	PayloadHeader_t 		payloadHeader;

	if(have_video == 0)
	{
		add_stream(&video_st, oc, &video_codec, codec_id, 0, 0, (unsigned int)width, (unsigned int)height);
		have_video = 1;

		/* Now that all the parameters are set, we can open the audio and
		 * video codecs and allocate the necessary encode buffers. */
		
		open_video(oc, video_codec, &video_st, opt);	

		if(have_audio == 1)
		{
			av_dump_format(oc, 0, filename, 1);
					
			/* open the output file, if needed */
			if (!(fmt->flags & AVFMT_NOFILE)) 
			{
				ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
				if (ret < 0) {
					fprintf(stderr, "Could not open '%s': %s\n", filename,av_err2str(ret));
					exit(-1);
				}
			}
			
			/* Write the stream header, if any. */
			ret = avformat_write_header(oc, &opt);
			if (ret < 0) 
			{
				fprintf(stderr, "Error occurred when opening output file: %s\n",av_err2str(ret));
				exit(-1);
			}
		}
		
	}

	if((have_audio == 1)&&(have_video == 1))
	{
		//if(av_compare_ts(video_st.next_pts, video_st.enc->time_base, audio_st.next_pts, audio_st.enc->time_base) <= 0)
			write_video_frame(oc, &video_st, &pkt);
	}

	return 0;
}

int muxAudioPlayer(unsigned char * data, unsigned int size, int64_t timestamp_90KHz, int codec_id, int sample_rate)
{
	int ret;

	Audiopkt.data = data;
	Audiopkt.size = size;
	Audiopkt.pts = timestamp_90KHz;//outPacketBuf.timestamp;
	Audiopkt.dts = timestamp_90KHz;

	if(have_audio == 0)
	{
		add_stream(&audio_st, oc, &audio_codec, codec_id, 0, (unsigned int)sample_rate, 0, 0);
		have_audio = 1;

		open_audio(oc, audio_codec, &audio_st, opt);

		if(have_video == 1)
		{
			av_dump_format(oc, 0, filename, 1);
					
			/* open the output file, if needed */
			if (!(fmt->flags & AVFMT_NOFILE)) 
			{
				ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
				if (ret < 0) {
					fprintf(stderr, "Could not open '%s': %s\n", filename,av_err2str(ret));
					exit(-1);
				}
			}
			
			/* Write the stream header, if any. */
			ret = avformat_write_header(oc, &opt);
			if (ret < 0) 
			{
				fprintf(stderr, "Error occurred when opening output file: %s\n",av_err2str(ret));
				exit(-1);
			}
		}
		
	}

	if((have_audio == 1)&&(have_video == 1))
		write_audio_frame(oc, &audio_st, &Audiopkt);

	return 0;
}

int MuxPlayerThread(void * arg)
{	
	char ch;
	int ret;
	char channelName[1024];

	while (1) 
	{
		scanf("%c", &ch);

		if((ch == 'n')||(ch == 'N'))
		{
			ret = changeChannelCallback(NULL, channelName, 1);
			if(ret != 0)
			{
				printf("---------- Next Channel :: %s ---------\n",channelName);		
			}
			
		}
		else if((ch == 'p')||(ch == 'P'))
		{
			ret = changeChannelCallback(NULL, channelName, -1);
			if(ret != 0)
			{
				printf("---------- Previous Channel :: %s ---------\n",channelName);
			}
			
		}

		usleep(1000*1000);

	}
}

