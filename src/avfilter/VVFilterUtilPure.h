/*
 * VVFilterUtilPure.h
 *
 *  Created on: 2017年6月27日
 *      Author: mj
 */

#ifndef AVFILTER_VVFILTERUTILPURE_H_
#define AVFILTER_VVFILTERUTILPURE_H_

extern "C"
{
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
//#include "libavcodec/avcodec.h"
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
//#include "libswscale/swscale.h"
}

#include "../util/const.h"

class IVVFilterCallback{
public:
	virtual void onGotFilteredFrame(AVFrame* frame) = 0;
};

enum VVAVFilterType{
	AVFILTER_VIDEO,
	AVFILTER_AUDIO,
};

class VVFilterUtilPure {
public:
	VVFilterUtilPure(VVAVFilterType type, AVCodecContext* codecCtx);
	virtual ~VVFilterUtilPure();

public:
	void setCallback(IVVFilterCallback* callback);
	void setFilterDescr(const char* filter);
	int initFilter();
	int processFilter(AVFrame* orginFrame, AVFrame* outputFrame);
	void release();

private:
	int initFilterForAudio();
	int initFilterForVideo();

private:
	VVAVFilterType m_eAVFilterType;
	char m_pFilterDescr[VV_FILENAME_MAX];

	AVFilterInOut* m_pInputs;
	AVFilterInOut* m_pOutputs;
	AVFilterContext* m_pBufferSinkCtx;
	AVFilterContext* m_pBufferSrcCtx;
	AVFilterGraph* m_pFilterGraph;
	AVCodecContext* m_pCodecContext;

private:
	IVVFilterCallback* m_pCallback;
};

#endif /* AVFILTER_VVFILTERUTILPURE_H_ */
