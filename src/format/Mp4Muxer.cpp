/*
 * Mp4Muxer.cpp
 *
 *  Created on: 2017年9月6日
 *      Author: mj
 */

#include <string.h>
#include "Mp4Muxer.h"
#include "../util/logUtil.h"

Mp4Muxer::Mp4Muxer() {
	m_pFormat = new VVAVFormat();
	m_pFormatCtx = m_pFormat->alloc_foramt_context();
}

Mp4Muxer::~Mp4Muxer() {
	m_pFormat->free_format_context(m_pFormatCtx);
	delete m_pFormat;
}

bool Mp4Muxer::init(const char* filename, int width, int height, int fps, int bitrate,
		AVRational timebase, uint8_t* data, int size)
{
	memset(m_pOutName, 0, 1024);
	strcpy(m_pOutName, filename);
	m_iVideoWidth = width;
	m_iVideoHeight = height;
	m_iVideoFps = fps;
	m_iVideoBitrate = bitrate;
	m_rTimebase = timebase;

	if(data){
		m_pExtraData = (uint8_t*)malloc(size);
		memcpy(m_pExtraData, data, size);
		m_uExtraSize = size;
	}else{
		m_pExtraData = NULL;
		m_uExtraSize = 0;
	}

	return true;
}


AVCodecContext* Mp4Muxer::open()
{
	m_pFormat->alloc_output_context(&m_pFormatCtx, m_pOutName);

	AVStream* pStream = m_pFormat->add_stream(m_pFormatCtx, (AVStream*)NULL, false);
	pStream->time_base = m_rTimebase;

	AVCodec* pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);

	AVCodecContext* pCodecCtx = pStream->codec;

	pCodecCtx->width = m_iVideoWidth;
	pCodecCtx->height = m_iVideoHeight;
	pCodecCtx->bit_rate = m_iVideoBitrate;
	pCodecCtx->time_base = AVRational{1, m_iVideoFps};
	pCodecCtx->codec = pCodec;
	pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	pCodecCtx->codec_id = AV_CODEC_ID_H264;
	pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	pCodecCtx->profile = FF_PROFILE_H264_BASELINE;
	pCodecCtx->level = 30;
	pCodecCtx->extradata = m_pExtraData;
	pCodecCtx->extradata_size = m_uExtraSize;



	m_pFormat->open_output_file(m_pFormatCtx);

	m_pFormat->write_hearder(m_pFormatCtx);
	return pCodecCtx;
}


bool Mp4Muxer::write_packet(uint8_t* data, uint32_t size, int64_t pts, int64_t dts)
{
	AVPacket packet;
	av_init_packet(&packet);
	packet.data = data;
	packet.size = size;
	packet.pts = pts;
	packet.dts = dts;
	packet.stream_index = 0;


	int i = 4 ;
//	LOGI("write_packet(%u):%x %x %x %x %x",size, data[i++], data[i++],data[i++],data[i++],data[i++]);
	LOGI("write_packet(%u):%x %x %x %x %x",size, data[i--], data[i--],data[i--],data[i--],data[i--]);
	m_pFormat->write_packet(m_pFormatCtx, packet);
	return true;
}

bool Mp4Muxer::write_packet(AVPacket packet)
{
	m_pFormat->write_packet(m_pFormatCtx, packet);
	return true;
}

bool Mp4Muxer::close()
{
	m_pFormat->write_tailer(m_pFormatCtx);
	m_pFormat->close_output_file(m_pFormatCtx);
	return true;
}
