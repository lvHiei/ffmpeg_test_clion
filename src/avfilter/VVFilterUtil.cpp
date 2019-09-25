/*
 * AVFilterUtil.cpp
 *
 *  Created on: 2017年6月21日
 *      Author: mj
 */

#include "VVFilterUtil.h"

#include "../util/logUtil.h"

VVFilterUtil::VVFilterUtil()
	: m_pFormatCtx(NULL),
	  m_pOutFormatCtx(NULL),
	  m_pDecodeCodecContext(NULL),
	  m_pEncodeCodecContext(NULL),
	  m_pBufferSinkCtx(NULL),
	  m_pBufferSrcCtx(NULL),
	  m_pFilterGraph(NULL),
	  m_iVideoIdxIn(-1),
	  m_iFilterAngle(45),
	  m_lFilterParam(NULL),
	  m_iFilterCount(0)
{

}

VVFilterUtil::~VVFilterUtil()
{
	if(m_lFilterParam){
		free(m_lFilterParam);
		m_lFilterParam = NULL;
	}
}

void VVFilterUtil::initFilter(const char* videoFile, const char* picFile, const char* outVideoFile)
{
	av_register_all();
    avfilter_register_all();

	memset(m_pVideoFileName, 0, VV_FILENAME_MAX);
	memset(m_pPicFilterName, 0, VV_FILENAME_MAX);
	memset(m_pOutVideoFile, 0, VV_FILENAME_MAX);

	strcpy(m_pVideoFileName, videoFile);
	strcpy(m_pPicFilterName, picFile);
	strcpy(m_pOutVideoFile, outVideoFile);

	memset(m_pFilterDescr, 0, VV_FILENAME_MAX);
//	sprintf(m_pFilterDescr,
//			"movie=%s[mf];[mf]rotate=ow=300:oh=300:a=%d:fillcolor=red[rf];[rf]colorkey=red:0.1:0.2[cf];[in][cf]overlay=x=40:y=100[out]",
//			m_pPicFilterName, m_iFilterAngle
//			);
	sprintf(m_pFilterDescr,
			"movie=%s[mf];[mf]rotate=ow=300:oh=300:a=%d:fillcolor=none[rf];[in][rf]overlay=x=40:y=100[out]",
			m_pPicFilterName, m_iFilterAngle
			);
}

void VVFilterUtil::setFilterParam(int angle, long* param, int count)
{
	m_iFilterAngle = angle;
	m_lFilterParam = (long*)malloc(sizeof(long)*count);
	m_iFilterCount = count;
}

void VVFilterUtil::setFilterDescr(const char* filterDescr)
{
	memset(m_pFilterDescr, 0, VV_FILENAME_MAX);
	strcpy(m_pFilterDescr, filterDescr);
}

bool VVFilterUtil::createFilter()
{
    int res = pthread_create(&m_ThreadID, NULL, (void* (*)(void*))&VVFilterUtil::run, this);
//	run();
	return true;
}

void VVFilterUtil::run()
{
	int ret = 0;
	if(open_input_file() != 0){
		release();
		return;
	}

	if(init_filters() != 0){
		release();
		return;
	}

	m_pFrame = av_frame_alloc();
	m_pFrameOut = av_frame_alloc();

	int got_frame = 0;
	while(true){
		ret = av_read_frame(m_pFormatCtx, &m_Packet);
		if(ret < 0){
			break;
		}

		if(m_Packet.stream_index == m_iVideoIdxIn){
			got_frame = 0;
			ret = avcodec_decode_video2(m_pDecodeCodecContext, m_pFrame, &got_frame, &m_Packet);
			if(ret < 0){
				LOGE("decode failed, ret=%d", ret);
			}

			if(got_frame){
				onGotDecodedVideoFrame();
			}
			else{
				LOGI("avcodec_decode_video2 success but not got frame");
			}
		}
		else{
			ret = av_write_frame(m_pOutFormatCtx, &m_Packet);
			if(ret < 0){
				LOGE("av_write_frame audio failed, ret=%d", ret);
			}
		}
		av_free_packet(&m_Packet);
	}

	// clear decode cache
    while (true){
        ret = avcodec_decode_video2(m_pDecodeCodecContext, m_pFrame, &got_frame, &m_Packet);
        if (ret < 0 || !got_frame){
            break;
        }

        onGotDecodedVideoFrame();

        av_free_packet(&m_Packet);
    }

	// clear encode cache
	while(true){
        ret = avcodec_encode_video2(m_pEncodeCodecContext, &m_EncodePacket, NULL, &got_frame);
        if (ret < 0 || !got_frame){
            break;
        }

		ret = av_write_frame(m_pOutFormatCtx, &m_EncodePacket);
		if(ret < 0){
			LOGE("av_write_frame video failed, ret=%d", ret);
		}

        av_free_packet(&m_EncodePacket);
	}

	av_write_trailer(m_pOutFormatCtx);

	//end
	release();

	LOGI("VVFilterUtil run ended");
}

void VVFilterUtil::onGotDecodedVideoFrame()
{
	int ret;
	int got_frame;
	m_pFrame->pts = m_pFrame->pkt_pts;
	ret = av_buffersrc_add_frame(m_pBufferSrcCtx, m_pFrame);
	if(ret < 0){
		LOGE("av_buffersrc_add_frame failed, ret=%d", ret);
	}

	while(true){
		ret = av_buffersink_get_frame(m_pBufferSinkCtx, m_pFrameOut);

		if(ret < 0){
//			LOGE("av_buffersink_get_frame failed, ret=%d", ret);
			break;
		}

		got_frame = 0;
		ret = avcodec_encode_video2(m_pEncodeCodecContext, &m_EncodePacket, m_pFrameOut, &got_frame);
		if(ret < 0){
			LOGE("encode video failed, ret=%d", ret);
		}

		if(got_frame){
			ret = av_write_frame(m_pOutFormatCtx, &m_EncodePacket);
			if(ret < 0){
				LOGE("av_write_frame video failed, ret=%d", ret);
			}

			av_free_packet(&m_EncodePacket);
		}else{
			LOGI("avcodec_encode_video2 success but not got frame");
		}

		av_frame_unref(m_pFrameOut);
	}
	av_frame_unref(m_pFrame);
}

int VVFilterUtil::open_input_file()
{
	int ret;
	AVCodec* pEncodeCodec;

	if((ret = avformat_open_input(&m_pFormatCtx, m_pVideoFileName, NULL, NULL)) < 0){
		LOGE("avformat_open_input failed, ret=%d,file=%s", ret, m_pVideoFileName);
		return ret;
	}

	if((ret = avformat_find_stream_info(m_pFormatCtx, NULL)) < 0){
		LOGE("avformat_open_input failed, ret=%d", ret);
		return ret;
	}

    av_dump_format(m_pFormatCtx, 0, m_pVideoFileName, 0);

    //申请输出文件内存
    if ((ret = avformat_alloc_output_context2(&m_pOutFormatCtx, NULL, NULL, m_pOutVideoFile)) < 0) {
    	LOGE("avformat_alloc_output_context2 failed,ret=%d,file=%s", ret, m_pOutVideoFile);
    	return ret;
    }

    // find video encoder
    for (int i = 0; i < m_pFormatCtx->nb_streams; i++) {
    	if(m_pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
    		AVStream *in_stream = m_pFormatCtx->streams[i];
            pEncodeCodec = avcodec_find_encoder(in_stream->codec->codec_id);
            if(!pEncodeCodec){
            	LOGE("find video encoder failed!");
            	return -1;
            }

            break;
    	}
    }

    // find video stream and set context
    for (int i = 0; i < m_pFormatCtx->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if(m_pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            AVStream *in_stream = m_pFormatCtx->streams[i];
            AVStream *out_stream = avformat_new_stream(m_pOutFormatCtx, pEncodeCodec);
            if (!out_stream) {
                LOGE("alloc output video stream failed.");
            }

            m_iVideoIdxIn = ret;
            m_iVideoIdxOut = out_stream->index;

        	m_pDecodeCodecContext = in_stream->codec;
            m_pEncodeCodecContext = out_stream->codec;

            out_stream->codec->codec_tag = 0;
            if (m_pOutFormatCtx->oformat->flags & AVFMT_GLOBALHEADER){
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }
            break;
        }
    }

    // find audio stream and copy context
    for (int i = 0; i < m_pFormatCtx->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if(m_pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
            AVStream *in_stream = m_pFormatCtx->streams[i];
            AVStream *out_stream = avformat_new_stream(m_pOutFormatCtx, in_stream->codec->codec);
            if (!out_stream) {
                LOGE("alloc output audio stream failed.");
            }

            m_iAudioIdxIn = i;
            m_iAudioIdxOut = out_stream->index;

            //Copy the settings of AVCodecContext
            if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                LOGE("copy audio context failed.");
            }

            out_stream->codec->codec_tag = 0;
            if (m_pOutFormatCtx->oformat->flags & AVFMT_GLOBALHEADER){
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }

            break;
        }
    }

    // find and open decoder
    AVCodec* pDecodeCodec = avcodec_find_decoder(m_pDecodeCodecContext->codec_id);
    if(pDecodeCodec == NULL){
        LOGE("pDecodeCodec not found.");
        return -1;
    }

	if((ret = avcodec_open2(m_pDecodeCodecContext, pDecodeCodec, NULL)) < 0){
		LOGE("avcodec_open2 decoder failed, ret=%d", ret);
		return ret;
	}

	// open encoder
//	m_pEncodeCodecContext = avcodec_alloc_context3(pEncodeCodec);

    //H264
    AVDictionary* param = NULL;
    if(AV_CODEC_ID_H264 == m_pEncodeCodecContext->codec_id){
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune","zerolatency", 0);
        av_dict_set(&param, "rc-lookahead", 0, 0);
        av_dict_set(&param, "profile", "baseline", 0);
    }

    // copy 房间的设置
    m_pEncodeCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_pEncodeCodecContext->width = m_pDecodeCodecContext->width;
    m_pEncodeCodecContext->height = m_pDecodeCodecContext->height;
    m_pEncodeCodecContext->time_base.num = 1;
    m_pEncodeCodecContext->time_base.den = 15;
//    m_pEncodeCodecContext->bit_rate = 800*1000;
    m_pEncodeCodecContext->bit_rate = m_pDecodeCodecContext->bit_rate;
//    m_pEncodeCodecContext->time_base = m_pDecodeCodecContext->framerate;

    m_pEncodeCodecContext->thread_count = 0;
    //Encoder parameters
    m_pEncodeCodecContext->refs = 1;
    m_pEncodeCodecContext->gop_size = m_pDecodeCodecContext->gop_size;

    //码流

    //disable B frame
    m_pEncodeCodecContext->max_b_frames = 0;
    m_pEncodeCodecContext->b_frame_strategy= 0;

    //Optional Param
    if (!strcmp(m_pOutFormatCtx->oformat->name, "mp4") || !strcmp(m_pOutFormatCtx->oformat->name, "mov") || !strcmp(m_pOutFormatCtx->oformat->name, "3gp"))
    {
    	m_pEncodeCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

	if((ret = avcodec_open2(m_pEncodeCodecContext, pEncodeCodec, &param)) < 0){
		char es[1024];
		av_strerror(ret, es, 1024);
		LOGE("avcodec_open2 encoder failed, ret=%d(%s)", ret, es);
		return ret;
	}

	av_dump_format(m_pOutFormatCtx, 0, m_pOutVideoFile, 1);

    //打开输出文件
    if (!(m_pOutFormatCtx->oformat->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&m_pOutFormatCtx->pb, m_pOutVideoFile, AVIO_FLAG_WRITE)) < 0) {
            LOGE("avio_open failed.ret=%d", ret);
            return ret;
        }
    }

    AVDictionary* opt = NULL;
//    if(!strcmp(m_pOutFormatCtx->oformat->name, "mp4")){
//        av_dict_set_int(&opt, "movflags", FF_MOV_FLAG_FASTSTART, 0);
//    }

    //Write file header
    if ((ret = avformat_write_header(m_pOutFormatCtx, &opt)) < 0) {
        LOGE("avformat_write_header failed. ret=%d", ret);
        return ret;
    }


	return 0;
}

int VVFilterUtil::init_filters()
{
    char args[512];
    int ret;
    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
//    AVFilter *buffersink = avfilter_get_by_name("ffbuffersink");
    AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    AVBufferSinkParams *buffersink_params;

    m_pFilterGraph = avfilter_graph_alloc();

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            m_pDecodeCodecContext->width, m_pDecodeCodecContext->height, m_pDecodeCodecContext->pix_fmt,
			m_pDecodeCodecContext->time_base.num, m_pDecodeCodecContext->time_base.den,
			m_pDecodeCodecContext->sample_aspect_ratio.num, m_pDecodeCodecContext->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&m_pBufferSrcCtx, buffersrc, "in",
                                       args, NULL, m_pFilterGraph);
    if (ret < 0) {
        printf("Cannot create buffer source\n");
        return ret;
    }

    /* buffer video sink: to terminate the filter chain. */
    buffersink_params = av_buffersink_params_alloc();
    buffersink_params->pixel_fmts = pix_fmts;
    ret = avfilter_graph_create_filter(&m_pBufferSinkCtx, buffersink, "out",
                                       NULL, buffersink_params, m_pFilterGraph);
    av_free(buffersink_params);
    if (ret < 0) {
        printf("Cannot create buffer sink\n");
        return ret;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = m_pBufferSrcCtx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = m_pBufferSinkCtx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if ((ret = avfilter_graph_parse_ptr(m_pFilterGraph, m_pFilterDescr,
                                    &inputs, &outputs, NULL)) < 0){
    	LOGE("avfilter_graph_parse_ptr failed,ret=%d", ret);
        return ret;
    }

    if ((ret = avfilter_graph_config(m_pFilterGraph, NULL)) < 0){
    	LOGE("avfilter_graph_config failed,ret=%d", ret);
        return ret;
    }

	return 0;
}

void VVFilterUtil::release()
{
	if(m_pFilterGraph){
		avfilter_graph_free(&m_pFilterGraph);
		m_pFilterGraph = NULL;
	}

	if(m_pDecodeCodecContext){
		avcodec_close(m_pDecodeCodecContext);
		m_pDecodeCodecContext = NULL;
	}

	if(m_pEncodeCodecContext){
		avcodec_close(m_pEncodeCodecContext);
//		avcodec_free_context(&m_pEncodeCodecContext);
		m_pEncodeCodecContext = NULL;
	}

	if(m_pFormatCtx){
		avformat_close_input(&m_pFormatCtx);
		m_pFormatCtx = NULL;
	}

	if(m_pOutFormatCtx){
	    /* close output */
	    if (m_pOutFormatCtx && !(m_pOutFormatCtx->oformat->flags & AVFMT_NOFILE)){
	        avio_close(m_pOutFormatCtx->pb);
	    }
	    avformat_free_context(m_pOutFormatCtx);

	    m_pOutFormatCtx = NULL;
	}
}
