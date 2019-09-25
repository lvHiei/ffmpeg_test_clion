/*
 * AVSpeed.cpp
 *
 *  Created on: 2017年6月26日
 *      Author: mj
 */

#include "AVSpeed.h"

#include "../util/logUtil.h"

AVSpeed::AVSpeed()
	: m_pFormatCtx(NULL),
	  m_pOutFormatCtx(NULL),
	  m_iVideoIdxIn(-1),
	  m_iAudioIdxIn(-1),
	  m_iVideoIdxOut(-1),
	  m_iAudioIdxOut(-1),
	  m_fSpeed(1.0f)
{
	av_register_all();
}

AVSpeed::~AVSpeed()
{
	release();
}

void AVSpeed::initFile(const char* origin, const char* out)
{
	memset(m_pInputVideo, 0, VV_FILENAME_MAX);
	memset(m_pOutputVideo, 0, VV_FILENAME_MAX);

	strcpy(m_pInputVideo, origin);
	strcpy(m_pOutputVideo, out);
}

void AVSpeed::setSpeed(float speed)
{
	if(speed > 2.0 || speed < 0.5){
		return;
	}

	m_fSpeed = 1.0 / speed;
}

void AVSpeed::start()
{
    int res = pthread_create(&m_ThreadID, NULL, (void* (*)(void*))&AVSpeed::run, this);
}

void AVSpeed::run()
{
	int ret;
	if((ret = open_file() != 0)){
		release();
		return;
	}

	int64_t lastVideoPts = AV_NOPTS_VALUE;
	int64_t lastVideoDts = AV_NOPTS_VALUE;
	int64_t lastAudioPts = AV_NOPTS_VALUE;

	while(true)
	{
		ret = av_read_frame(m_pFormatCtx, &m_packet);
		if(ret < 0){
			break;
		}

		if(m_packet.stream_index == m_iVideoIdxIn){
			if(m_packet.pts != AV_NOPTS_VALUE){
				m_packet.pts *= m_fSpeed;
			}

			if(m_packet.dts != AV_NOPTS_VALUE){
				m_packet.dts *= m_fSpeed;
			}

			if(lastVideoDts != AV_NOPTS_VALUE && lastVideoDts >= m_packet.dts){
				LOGW("add vframe dts from %lld to %lld", m_packet.dts, lastVideoDts + 1);
				m_packet.dts = lastVideoDts + 1;
			}

			if(m_packet.pts < m_packet.dts){
				LOGW("add vframe pts from %lld to %lld", m_packet.pts, m_packet.dts);
				m_packet.pts = m_packet.dts;
			}

			if(m_packet.pts == lastVideoPts){
				LOGW("add vframe pts from %lld to %lld..", m_packet.pts, lastVideoPts + 1);
				m_packet.pts = lastVideoPts + 1;
			}

			m_packet.duration *= m_fSpeed;
			m_packet.stream_index = m_iVideoIdxOut;

		}else if(m_packet.stream_index == m_iAudioIdxIn){
			if(m_packet.pts != AV_NOPTS_VALUE){
				m_packet.pts *= m_fSpeed;
			}

			if(m_packet.dts != AV_NOPTS_VALUE){
				m_packet.dts *= m_fSpeed;
			}

			if(m_packet.pts <= lastAudioPts){
				LOGW("add aframe pts from %lld to %lld", m_packet.pts, lastAudioPts + 1);
				m_packet.pts = lastAudioPts + 1;
				m_packet.dts = m_packet.pts;
			}

			m_packet.duration *= m_fSpeed;
			m_packet.stream_index = m_iAudioIdxOut;
		}

		ret = av_write_frame(m_pOutFormatCtx, &m_packet);
		if(ret < 0){
			LOGE("av_write_frame failed, ret=%d", ret);
		}

		av_free_packet(&m_packet);
	}

	av_write_trailer(m_pOutFormatCtx);

	release();
	LOGI("AVSpeed::run ended");
}

void AVSpeed::release()
{
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

int AVSpeed::open_file()
{
	int ret;
    if ((ret = avformat_open_input(&m_pFormatCtx, m_pInputVideo, 0, 0)) < 0) {
        LOGE("open input video file failed.ret=%d", ret);
        return ret;
    }
    if ((ret = avformat_find_stream_info(m_pFormatCtx, 0))  < 0) {
        LOGE("find input video stream info failed.ret=%d", ret);
        return ret;
    }

    av_dump_format(m_pFormatCtx, 0, m_pInputVideo, 0);

    //申请输出文件内存
    ret = avformat_alloc_output_context2(&m_pOutFormatCtx, NULL, NULL, m_pOutputVideo);
    if (!m_pOutFormatCtx) {
        LOGE("alloc output context failed.ret=%d", ret);
        return ret;
    }

	AVOutputFormat* ofmt = m_pOutFormatCtx->oformat;

    // 找到输入视频的视频流索引，并拷贝编码上下文到输出文件视频流
    for (int i = 0; i < m_pFormatCtx->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if(m_pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            AVStream *in_stream = m_pFormatCtx->streams[i];
            AVStream *out_stream = avformat_new_stream(m_pOutFormatCtx, in_stream->codec->codec);
            if (!out_stream) {
                LOGE("alloc output video stream failed.");
                return ret;
            }

            m_iVideoIdxIn = i;
            m_iVideoIdxOut = out_stream->index;

            //Copy the settings of AVCodecContext
            if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                LOGE("copy video context failed.");
                return ret;
            }

            out_stream->codec->codec_tag = 0;
            if (m_pOutFormatCtx->oformat->flags & AVFMT_GLOBALHEADER){
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }

            break;
        }
    }

    // 找到输入音频的音频流索引，并拷贝编码上下文到输出文件音频流
    for (int i = 0; i < m_pFormatCtx->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if(m_pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
            AVStream *in_stream = m_pFormatCtx->streams[i];

            AVStream *out_stream = avformat_new_stream(m_pOutFormatCtx, in_stream->codec->codec);
            if (!out_stream) {
                LOGE("alloc output audio stream failed.");
                return ret;
            }

            m_iAudioIdxIn = i;
            m_iAudioIdxOut = out_stream->index;

            //Copy the settings of AVCodecContext
            if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                LOGE("copy audio context failed.");
                return ret;
            }

            out_stream->codec->codec_tag = 0;
            if (m_pOutFormatCtx->oformat->flags & AVFMT_GLOBALHEADER){
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }

            break;
        }
    }

    av_dump_format(m_pOutFormatCtx, 0, m_pOutputVideo, 1);

    //打开输出文件
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_pOutFormatCtx->pb, m_pOutputVideo, AVIO_FLAG_WRITE) < 0) {
            LOGE("avio_open failed.");
            return ret;
        }
    }

    AVDictionary* opt = NULL;
//    if(!strcmp(ofmt->name, "mp4")){
//        av_dict_set_int(&opt, "movflags", FF_MOV_FLAG_FASTSTART, 0);
//    }

    //Write file header
    if (avformat_write_header(m_pOutFormatCtx, &opt) < 0) {
        LOGE("avformat_write_header failed.");
        return ret;
    }

    return 0;
}
