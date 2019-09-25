/*
 * AVSpeed.h
 *
 *  Created on: 2017年6月26日
 *      Author: mj
 */

#ifndef AVSPEED_AVSPEED_H_
#define AVSPEED_AVSPEED_H_

#include <stdio.h>
#include <pthread.h>
#include <thread>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
}

#include "../util/const.h"

class AVSpeed {
public:
	AVSpeed();
	virtual ~AVSpeed();
	void initFile(const char* origin, const char* out);
	void setSpeed(float speed);
	void start();
	void join();

private:
	void run();
	void release();
	int open_file();

private:
	AVFormatContext* m_pFormatCtx;
	AVFormatContext* m_pOutFormatCtx;
	AVPacket m_packet;

	int m_iVideoIdxIn;
	int m_iAudioIdxIn;
	int m_iAudioIdxOut;
	int m_iVideoIdxOut;

private:
	char m_pInputVideo[VV_FILENAME_MAX];
	char m_pOutputVideo[VV_FILENAME_MAX];

	float m_fSpeed;

private:
	std::thread* mThread;
};

#endif /* AVSPEED_AVSPEED_H_ */
