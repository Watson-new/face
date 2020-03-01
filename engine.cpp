#include "engine.h"

AVFormatContext *in_format_ctx;
AVPacket *pkt;
AVStream *stream;
AVCodecContext *pCodecCtx;
AVCodec *pCodec;
AVFrame *per_frame;
AVFrame *frame;


void init_register_network()
{
		/*
		*	Init the network
		*	register all av
		*/
       	av_register_all();
       	avformat_network_init();
}


void test_ffmpeg_rtmp_client()
{
	int i;
	int ret = -1;
	int video_stream_index = -1;
	int audio_stream_index = -1;
	int index = 0;
	char filename[32] = "";
	int audio_size_all = 0;
	int video_size_all = 0;

	uint8_t *buffer;
	int numbytes;
	int got_picture;
	struct SwsContext *img_convert_ctx = NULL;
	int j = 0;

	//init register and network
	init_register_network();

	//alloc context
	in_format_ctx = avformat_alloc_context();

	//open video file -> rtmp path
	ret = avformat_open_input(&in_format_ctx, URL, NULL, NULL);
	if (ret < 0)
	{
		fprintf(stderr, "fail to open url: %s, return value: %d\n", URL, ret);
		return;
	}

	// Read packets of a media file to get stream information
	ret = avformat_find_stream_info(in_format_ctx, NULL);
	if (ret < 0) 
	{
		fprintf(stderr, "fail to get stream information: %d\n", ret);
		return;
	}
	
	/*Fetch index of audio and video from streams*/ 
	for (i = 0; i < in_format_ctx->nb_streams; i++) 
	{
		stream = in_format_ctx->streams[i];
		fprintf(stdout, "type of the encoded data: %d\n", stream->codecpar->codec_id);
		//video type
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) 
			video_stream_index = i;
		//audio type
		else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
			audio_stream_index = i;
	}
 
	if (video_stream_index == -1 || audio_stream_index == -1)
	{
		fprintf(stderr, "no video audio stream\n");
		return;
	}
#if RTMP
	av_dump_format(in_format_ctx,0,URL,0);
	AVFormatContext *out_format_ctx = NULL;

	/* set context from OUT to out_format_ctx*/
	avformat_alloc_output_context2(&out_format_ctx, NULL, NULL,OUT);
	AVOutputFormat* ofmt = NULL;
	ofmt = out_format_ctx->oformat;

	/*set out_stream with in_stream's codec*/
	for (i = 0; i < in_format_ctx->nb_streams; i++)
	{
		/*Add more stream channel*/
		AVStream *in_stream;
		AVStream *out_stream;

		in_stream = in_format_ctx->streams[i];
		out_stream = avformat_new_stream(out_format_ctx, in_stream->codec->codec);

		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		out_stream->codec->codec_tag = 0;

		if (out_format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	av_dump_format(out_format_ctx, 0, OUT, 1);

	if (!(ofmt->flags & AVFMT_NOFILE))
	{
		/*open OUT for out_format_ctx's pb just like New something*/
		ret = avio_open(&out_format_ctx->pb,OUT, AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			printf("Could not open output URL '%s'", OUT);
			return;
		}
	}
	/*Write head to file out_format_ctx as OUT*/
	avformat_write_header(out_format_ctx,NULL);
#endif

	/*Fetch the context from decoder and coder from video*/
	pCodecCtx = in_format_ctx->streams[video_stream_index]->codec;
	
	/*identity*/
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (NULL == pCodec)
	{
		printf("could not find decoder......\n");
		return;
	}

	//open decoder 
	if (avcodec_open2(pCodecCtx,pCodec,NULL)<0)
	{
		printf("avcodec open failed\n");
		return;
	}
	printf("The width of video is %d, while the height is %d\n",pCodecCtx->width,pCodecCtx->height);

	/*
	*	read each frame using av_read_frame
	*	check the index of av
	*	write in file 
	*/

	/*avoid segmentation fault*/
	pkt = av_packet_alloc();
	av_dump_format(in_format_ctx,0,OUT,0);

	/*init jpg setting*/
	frame = av_frame_alloc(); 

	while (1) 
	{
		//read each frame

		ret = av_read_frame(in_format_ctx, pkt);
		if (ret < 0)
		{
			fprintf(stderr, "error or end of file: %d\n", ret);
			continue;
		}
		fprintf(stdout, "video stream, packet size: %d\n", pkt->size);
		//index of av 
		
		/*receive the rtmp stream stored in local file:mp4 format*/
#if RTMP
		if (pkt->size != 0)
		{
			AVStream* in_stream, * out_stream;

			in_stream = in_format_ctx->streams[pkt->stream_index];
			out_stream = out_format_ctx->streams[pkt->stream_index];

			/*
			*	PTS as presentation time stamp
			*	DTS as display time stamp
			*	When without B frame, PTS and DTS are the same
			*/
			/*The pts is 44160, the dts is 44160, and duration is 21*/
			/*
			*	The pts and dts should not be changed
			*	because of time_base is the same from the original
			*	If there is no B frame in video, I frame and P frame should be display at the same time.
			*	IF there is B frame in video, I frame should be displayed first,while
			*	B and P frame like this:
			*	B decode last, P decode first
			*	B display first, P display last
			*	The time base should be defined as uint64_t data format
			*/

			pkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			/*Get the delta from frame to frame*/
			pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
			pkt->pos = 1;
			
			/*write frame to the OUT*/
			//ret = av_interleaved_write_frame(out_format_ctx,pkt);

			/*Only the single stream, it can handle the pkt*/
			ret = av_write_frame(out_format_ctx,pkt);
			if (ret < 0) //error code
			{
				switch(ret)
				{
					case -22: continue;

					case -32: 
					printf("socket error\n");
					break;

					defualt:
					break;
				}
			}
		}//end of if
#endif
		/*Check whether the index is video stream*/
		if (pkt->stream_index == video_stream_index)
		{
			fprintf(stdout, "video stream, packet size: %d\n", pkt->size);
			video_size_all += pkt->size;
			printf("The video size totally is %d kb\n",video_size_all/100000);

			/*pkt->size should not be zero, if it is, it should be break*/
			if (ret < 0)/*pkt->size would never be zero*/
			{
				printf("avcodec_decode_video2 error\n");
				return;
			}
			/*If one picture comes it should be storage.*/
			/*decode each frame*/
			avcodec_decode_video2(pCodecCtx,frame,&got_picture,pkt);
			if (got_picture)
			{
				if (++j == 1)
					save_jpeg(frame,"watson.jpg",pCodecCtx->width,pCodecCtx->height);
			}

		}
 
		if (pkt->stream_index == audio_stream_index)
		{
			fprintf(stdout, "audio stream, packet size: %d\n", pkt->size);
			audio_size_all += pkt->size;
			printf("The audio size totally is %d kb\n",audio_size_all/100000);
		}

 
		av_packet_unref(pkt);
	}
#if RTMP
	av_write_trailer(out_format_ctx);
#endif
	//free all 
	avformat_free_context(in_format_ctx);
	avcodec_close(pCodecCtx);
	avformat_close_input(&in_format_ctx);
}

int main(int argc, char **argv)
{
	
#if FACE
	char imgfile[32] = "640x480.NV21";
	char face_mesg[32] = "";
	face_feature_detection(imgfile,face_mesg);
	printf("The face_mesg is %s\n",face_mesg);
#else
	test_ffmpeg_rtmp_client();
#endif
	return 0;
}
