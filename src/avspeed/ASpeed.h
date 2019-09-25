/*
 * ASpeed.h
 *
 *  Created on: 2017年6月28日
 *      Author: mj
 */

#ifndef AVSPEED_ASPEED_H_
#define AVSPEED_ASPEED_H_

#include <pthread.h>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
}

#include "../avfilter/VVFilterUtilPure.h"

class ASpeed : public IVVFilterCallback{
public:
	ASpeed();
	virtual ~ASpeed();

	void initFile(const char* inputFile, const char* outputFile);
	void start();

	pthread_t getThreadId(){return m_ThreadID;}
public:
	virtual void onGotFilteredFrame(AVFrame* frame);

private:
	void run();
	int open_file();
	void release();

private:
	AVFormatContext* m_pInputFormatContext;
	AVCodecContext* m_pDecodeCodecContext;

	AVFormatContext* m_pOutputFormatContext;
	AVCodecContext* m_pEncodeCodecContext;
	AVPacket m_sReadPacket;
	AVPacket m_sEncodePacket;

	int m_iAudioInIdx;
	int m_iAudioOutIdx;

private:
	VVFilterUtilPure* m_pAudioFilter;

private:
	char m_pInputFile[VV_FILENAME_MAX];
	char m_pOutputFile[VV_FILENAME_MAX];

private:
	pthread_t m_ThreadID;
};

#endif /* AVSPEED_ASPEED_H_ */
