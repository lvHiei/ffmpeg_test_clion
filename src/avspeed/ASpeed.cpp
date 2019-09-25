/*
 * ASpeed.cpp
 *
 *  Created on: 2017年6月28日
 *      Author: mj
 */

#include "ASpeed.h"

#include "../util/logUtil.h"

ASpeed::ASpeed()
	: m_pInputFormatContext(NULL),
	  m_pOutputFormatContext(NULL),
	  m_pDecodeCodecContext(NULL),
	  m_pEncodeCodecContext(NULL),
	  m_pAudioFilter(NULL),
	  m_iAudioInIdx(-1),
	  m_iAudioOutIdx(-1)
{
}

ASpeed::~ASpeed()
{
	release();
}

void ASpeed::initFile(const char* inputFile, const char* outputFile)
{
	memset(m_pInputFile, 0, VV_FILENAME_MAX);
	memset(m_pOutputFile, 0, VV_FILENAME_MAX);

	strcpy(m_pInputFile, inputFile);
	strcpy(m_pOutputFile, outputFile);
}

void ASpeed::start()
{
    int res = pthread_create(&m_ThreadID, NULL, (void* (*)(void*))&ASpeed::run, this);
}

void ASpeed::run()
{
	int ret;
	if((ret = open_file()) != 0){
		release();
		return;
	}

	while(true){
		ret = av_read_frame(m_pInputFormatContext, &m_sReadPacket);
		if(ret < 0){
			break;
		}


	}
}

int ASpeed::open_file()
{
	return 0;
}

void ASpeed::onGotFilteredFrame(AVFrame* pframe)
{

}

void ASpeed::release()
{
	if(m_pAudioFilter){
		delete m_pAudioFilter;
		m_pAudioFilter = NULL;
	}

	if(m_pDecodeCodecContext){
		avcodec_close(m_pDecodeCodecContext);
		m_pDecodeCodecContext = NULL;
	}

	if(m_pEncodeCodecContext){
		avcodec_close(m_pEncodeCodecContext);
		m_pEncodeCodecContext = NULL;
	}

	if(m_pInputFormatContext){
		avformat_close_input(&m_pInputFormatContext);
		m_pInputFormatContext = NULL;
	}

	if(m_pOutputFormatContext){
	    if (m_pOutputFormatContext && !(m_pOutputFormatContext->oformat->flags & AVFMT_NOFILE)){
	        avio_close(m_pOutputFormatContext->pb);
	    }
	    avformat_free_context(m_pOutputFormatContext);
	    m_pOutputFormatContext = NULL;
	}
}
