/*
 * VVFilterUtilPure.cpp
 *
 *  Created on: 2017年6月27日
 *      Author: mj
 */

#include "VVFilterUtilPure.h"

#include "../util/logUtil.h"

VVFilterUtilPure::VVFilterUtilPure(VVAVFilterType type, AVCodecContext* codecCtx)
	: m_eAVFilterType(type),
	  m_pCodecContext(codecCtx),
	  m_pInputs(NULL),
	  m_pOutputs(NULL),
	  m_pBufferSinkCtx(NULL),
	  m_pBufferSrcCtx(NULL),
	  m_pFilterGraph(NULL)
{
	av_register_all();
	avfilter_register_all();
}

VVFilterUtilPure::~VVFilterUtilPure()
{
	release();
}

void VVFilterUtilPure::setCallback(IVVFilterCallback* callback)
{
	m_pCallback = callback;
}

void VVFilterUtilPure::setFilterDescr(const char* filter)
{
	memset(m_pFilterDescr, 0, VV_FILENAME_MAX);
	strcpy(m_pFilterDescr, filter);
}

int VVFilterUtilPure::initFilter()
{
	int ret;
	if(AVFILTER_VIDEO == m_eAVFilterType){
		ret = initFilterForVideo();
	}else if(AVFILTER_AUDIO == m_eAVFilterType){
		ret = initFilterForAudio();
	}

	if(ret < 0){
		release();
		return ret;
	}

    m_pOutputs = avfilter_inout_alloc();
    m_pInputs  = avfilter_inout_alloc();

    /* Endpoints for the filter graph. */
    m_pOutputs->name       = av_strdup("in");
    m_pOutputs->filter_ctx = m_pBufferSrcCtx;
    m_pOutputs->pad_idx    = 0;
    m_pOutputs->next       = NULL;

    m_pInputs->name       = av_strdup("out");
    m_pInputs->filter_ctx = m_pBufferSinkCtx;
    m_pInputs->pad_idx    = 0;
    m_pInputs->next       = NULL;

    if ((ret = avfilter_graph_parse_ptr(m_pFilterGraph, m_pFilterDescr,
                                    &m_pInputs, &m_pOutputs, NULL)) < 0){
    	LOGE("avfilter_graph_parse_ptr failed,ret=%d", ret);
    	release();
        return ret;
    }

    if ((ret = avfilter_graph_config(m_pFilterGraph, NULL)) < 0){
    	LOGE("avfilter_graph_config failed,ret=%d", ret);
    	release();
        return ret;
    }

	return 0;
}

int VVFilterUtilPure::initFilterForAudio()
{
    char args[512];
    int ret = 0;
    AVFilter *abuffersrc  = avfilter_get_by_name("abuffer");
    AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    static const enum AVSampleFormat out_sample_fmts[] = { AV_SAMPLE_FMT_S16, (AVSampleFormat)-1 };
    static const int64_t out_channel_layouts[] = { AV_CH_LAYOUT_STEREO, -1 };
    static const int out_sample_rates[] = { 44100, -1 };

    m_pFilterGraph = avfilter_graph_alloc();
    if (!m_pFilterGraph) {
        ret = AVERROR(ENOMEM);
        return ret;
    }

    /* buffer audio source: the decoded frames from the decoder will be inserted here. */
    if (!m_pCodecContext->channel_layout){
    	m_pCodecContext->channel_layout = av_get_default_channel_layout(m_pCodecContext->channels);
    }

    snprintf(args, sizeof(args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
             1, 44100, m_pCodecContext->sample_rate,
             av_get_sample_fmt_name(m_pCodecContext->sample_fmt), m_pCodecContext->channel_layout);

    ret = avfilter_graph_create_filter(&m_pBufferSrcCtx, abuffersrc, "in",
                                       args, NULL, m_pFilterGraph);
    if (ret < 0) {
        LOGE("Cannot create audio buffer source");
        return ret;
    }

    /* buffer audio sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&m_pBufferSinkCtx, abuffersink, "out",
                                       NULL, NULL, m_pFilterGraph);
    if (ret < 0) {
    	LOGE("Cannot create audio buffer sink");
        return ret;
    }

    ret = av_opt_set_int_list(m_pBufferSinkCtx, "sample_fmts", out_sample_fmts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
    	LOGE("Cannot set output sample format");
        return ret;
    }

    ret = av_opt_set_int_list(m_pBufferSinkCtx, "channel_layouts", out_channel_layouts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
    	LOGE("Cannot set output channel layout");
        return ret;
    }

    ret = av_opt_set_int_list(m_pBufferSinkCtx, "sample_rates", out_sample_rates, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
    	LOGE("Cannot set output sample rate");
        return ret;
    }

	return 0;
}

int VVFilterUtilPure::initFilterForVideo()
{
    char args[512];
    int ret;
    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("buffersink");

    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    AVBufferSinkParams *buffersink_params;

    m_pFilterGraph = avfilter_graph_alloc();

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            m_pCodecContext->width, m_pCodecContext->height, m_pCodecContext->pix_fmt,
			m_pCodecContext->time_base.num, m_pCodecContext->time_base.den,
			m_pCodecContext->sample_aspect_ratio.num, m_pCodecContext->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&m_pBufferSrcCtx, buffersrc, "in",
                                       args, NULL, m_pFilterGraph);
    if (ret < 0) {
        LOGE("Cannot create buffer source\n");
        return ret;
    }

    /* buffer video sink: to terminate the filter chain. */
    buffersink_params = av_buffersink_params_alloc();
    buffersink_params->pixel_fmts = pix_fmts;
    ret = avfilter_graph_create_filter(&m_pBufferSinkCtx, buffersink, "out",
                                       NULL, buffersink_params, m_pFilterGraph);
    av_free(buffersink_params);
    if (ret < 0) {
    	LOGE("Cannot create buffer sink\n");
        return ret;
    }

	return 0;
}

int VVFilterUtilPure::processFilter(AVFrame* orginFrame, AVFrame* outputFrame)
{
	int ret = -1;
	if(!m_pBufferSinkCtx || !m_pBufferSrcCtx || !m_pFilterGraph){
		return ret;
	}

	if(!orginFrame || !outputFrame){
		return ret;
	}

	ret = av_buffersrc_add_frame(m_pBufferSrcCtx, orginFrame);
	if(ret < 0){
		LOGE("av_buffersrc_add_frame failed, ret=%d", ret);
	}

	while(true)
	{
		ret = av_buffersink_get_frame(m_pBufferSinkCtx, outputFrame);
		if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0){
			break;
		}

		if(m_pCallback){
			m_pCallback->onGotFilteredFrame(outputFrame);
			av_frame_unref(outputFrame);
		}
	}

	return 0;
}

void VVFilterUtilPure::release()
{
	if(m_pFilterGraph){
		avfilter_graph_free(&m_pFilterGraph);
		m_pFilterGraph = NULL;
	}

	if(m_pInputs){
	    avfilter_inout_free(&m_pInputs);
	    m_pInputs = NULL;
	}

	if(m_pOutputs){
	    avfilter_inout_free(&m_pOutputs);
	    m_pOutputs = NULL;
	}
}
