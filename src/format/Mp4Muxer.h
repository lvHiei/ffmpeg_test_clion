/*
 * Mp4Muxer.h
 *
 *  Created on: 2017年9月6日
 *      Author: mj
 */

#ifndef MP4MUXER_H_
#define MP4MUXER_H_

#include <stdint.h>
#include "VVAVFormat.h"

class Mp4Muxer {
public:
	Mp4Muxer();
	virtual ~Mp4Muxer();

public:
	bool init(const char* filename, int width, int height, int fps, int bitrate, AVRational timebase, uint8_t* data, int size);
	AVCodecContext* open();
	bool write_packet(uint8_t* data, uint32_t size, int64_t pts, int64_t dts);
	bool write_packet(AVPacket packet);
	bool close();

private:
	VVAVFormat* m_pFormat;
	AVFormatContext* m_pFormatCtx;

	int m_iVideoWidth;
	int m_iVideoHeight;
	int m_iVideoFps;
	int m_iVideoBitrate;

	char m_pOutName[1024];
	AVRational m_rTimebase;

	uint8_t* m_pExtraData;
	int m_uExtraSize;
};

#endif /* MP4MUXER_H_ */
