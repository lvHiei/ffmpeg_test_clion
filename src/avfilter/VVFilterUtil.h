/*
 * AVFilterUtil.h
 *
 *  Created on: 2017年6月21日
 *      Author: mj
 */

#ifndef AVFILTER_VVFILTERUTIL_H_
#define AVFILTER_VVFILTERUTIL_H_


extern "C"
{
#include <stdio.h>
#include <pthread.h>


//#include "libavfilter/avfilter.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

#include <thread>
#include "../util/const.h"

class VVFilterUtil {
public:
	VVFilterUtil();
	virtual ~VVFilterUtil();

public:
	void initFilter(const char* videoFile, const char* picFile, const char* outVideoFile);
	void setFilterParam(int angle, long* param, int count);
	void setFilterDescr(const char* filterDescr);
	bool createFilter();
	void join();

private:
	void run();

private:
	int open_input_file();
	int init_filters();
	void release();
	void onGotDecodedVideoFrame();

private:
	AVFormatContext* m_pFormatCtx;
	AVFormatContext* m_pOutFormatCtx;
	AVCodecContext* m_pDecodeCodecContext;
	AVCodecContext* m_pEncodeCodecContext;
	AVFilterContext* m_pBufferSinkCtx;
	AVFilterContext* m_pBufferSrcCtx;
	AVFilterGraph* m_pFilterGraph;

	AVFrame* m_pFrame;
	AVFrame* m_pFrameOut;
	AVPacket m_Packet;
	AVPacket m_EncodePacket;

	int m_iVideoIdxIn;
	int m_iAudioIdxIn;
	int m_iAudioIdxOut;
	int m_iVideoIdxOut;

	FILE* m_pYUVFILE;

	char m_pFilterDescr[VV_FILENAME_MAX];

private:
	char m_pVideoFileName[VV_FILENAME_MAX];
	char m_pPicFilterName[VV_FILENAME_MAX];
	char m_pOutVideoFile[VV_FILENAME_MAX];

	int m_iFilterCount;
	int m_iFilterAngle;
	long* m_lFilterParam;

	std::thread* mThread;
};

#endif /* AVFILTER_VVFILTERUTIL_H_ */
