//
// Created by mj on 17-8-31.
//

#include "VVAVEncoder.h"
#include "../util/logUtil.h"


VVAVEncoder::VVAVEncoder()
{
    avcodec_register_all();
}

VVAVEncoder::~VVAVEncoder()
{

}


int VVAVEncoder::init_video_encoder(AVCodecContext *pCodecContext, AVCodecID codecID, int width,
                                    int height, int framerate, int bitrate, AVDictionary **param)
{
    if(!pCodecContext){
        LOGE("VVAVEncoder::init_video_encoder null codecContext");
        return -1;
    }

    pCodecContext->codec_id = codecID;
    pCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;

    //H264
    if(AV_CODEC_ID_H264 == pCodecContext->codec_id){
        av_dict_set(param, "preset", "ultrafast", 0);
        av_dict_set(param, "tune","zerolatency", 0);
        av_dict_set(param, "rc-lookahead", 0, 0);
        av_dict_set(param, "profile", "baseline", 0);
    }

    // copy 房间的设置
    pCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    pCodecContext->width = width;
    pCodecContext->height = height;
    pCodecContext->time_base.num = 1;
    pCodecContext->time_base.den = framerate;
    pCodecContext->level=40;

    pCodecContext->thread_count = 0;
    //Encoder parameters
    pCodecContext->refs = 1;
    pCodecContext->gop_size = framerate ;

    //码流
    pCodecContext->bit_rate = bitrate;
    pCodecContext->rc_max_rate = pCodecContext->bit_rate*1.5;


    //disable B frame
    pCodecContext->max_b_frames = 0;
    pCodecContext->b_frame_strategy= 0;

    pCodecContext->trellis = 0;
    pCodecContext->qmin = 20;         //减少会较大的影响码流变化
    pCodecContext->qmax = 30;
    pCodecContext->max_qdiff = 10;

    pCodecContext->qcompress = 0.8;  //default 0.6
    pCodecContext->qblur = 0.5;                             //default 0.5  qblur=1 is a gaussian blur of radius 1.

    //熵编码
    pCodecContext->coder_type = FF_CODER_TYPE_AC;

    pCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
    return 0;
}

int VVAVEncoder::init_audio_encoder(AVCodecContext *pCodecContext, AVCodecID codecID,
                                    int samplerate, int channels, int bitrate, int aacType)
{
    if(!pCodecContext){
        LOGE("VVAVEncoder::init_audio_encoder null codecContext");
        return -1;
    }

    pCodecContext->codec_id = codecID;
    pCodecContext->sample_fmt = AV_SAMPLE_FMT_S16;
    pCodecContext->sample_rate = samplerate;
    pCodecContext->channels = channels;
    pCodecContext->channel_layout = av_get_default_channel_layout(pCodecContext->channels);
    pCodecContext->bit_rate = bitrate;
    pCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;

    if(aacType == 0){
        pCodecContext->profile = FF_PROFILE_AAC_LOW;
    }else{
        pCodecContext->profile = FF_PROFILE_AAC_HE;
    }


    return 0;
}

int VVAVEncoder::open_encoder(AVCodecContext *pCodecContext, AVDictionary* param)
{
    int ret = 0;
    AVCodec* pCodec = avcodec_find_encoder(pCodecContext->codec_id);
    if(!pCodec){
        LOGE("VVAVEncoder::open_encoder find encoder failed, codec:%d", pCodecContext->codec_id);
        return -1;
    }

    pCodecContext->codec = pCodec;
    ret = avcodec_open2(pCodecContext, pCodec, &param);
    if(ret < 0){
        char buf[1024];
        av_strerror(ret, buf, 1024);
        LOGE("VVAVEncoder::open_encoder open codec failed,codec:%d,ret:%d(%s)", pCodecContext->codec_id, ret, buf);
        return ret;
    }

    return 0;
}


int VVAVEncoder::close_encoder(AVCodecContext *pCodecContext)
{
    avcodec_close(pCodecContext);
    return 0;
}


int VVAVEncoder::init_and_open_video_encoder(AVCodecContext *pCodecContext, AVCodecID codecID,
                                             int width, int height,
                                             int framerate, int bitrate)
{
    AVDictionary* param = NULL;

    int ret = 0;

    ret = init_video_encoder(pCodecContext, codecID, width, height, framerate, bitrate, &param);

    if(ret < 0){
        return ret;
    }

    ret = open_encoder(pCodecContext, param);
    if(ret < 0){
        return ret;
    }

    return 0;
}

int VVAVEncoder::init_and_open_audio_encoder(AVCodecContext *pCodecContext, AVCodecID codecID,
                                             int samplerate, int channels,
                                             int bitrate, int aacType)
{
    int ret = 0;

    ret = init_audio_encoder(pCodecContext, codecID, samplerate, channels, bitrate, aacType);

    if(ret < 0){
        return ret;
    }

    ret = open_encoder(pCodecContext);

    if(ret < 0){
        return ret;
    }

    return 0;
}

int VVAVEncoder::encode_video_frame(AVCodecContext *pCodecContext, AVPacket *packet,
                                    AVFrame *pFrame, int *got_frame)
{
    int ret = 0;

    ret = avcodec_encode_video2(pCodecContext, packet, pFrame, got_frame);
    if(ret < 0){
        LOGE("VVAVEncoder::encode_video_frame failed,ret=%d", ret);
        return ret;
    }

//    av_frame_unref(pFrame);

    return 0;
}

int VVAVEncoder::encode_audio_frame(AVCodecContext *pCodecContext, AVPacket *packet,
                                    AVFrame *pFrame, int *got_frame)
{
    int ret = 0;

    ret = avcodec_encode_audio2(pCodecContext, packet, pFrame, got_frame);
    if(ret < 0){
        LOGE("VVAVEncoder::encode_audio_frame failed,ret=%d", ret);
        return ret;
    }

//    av_frame_unref(pFrame);

    return 0;
}


int VVAVEncoder::flush_video_encoder(AVCodecContext *pCodecContext, AVPacket *packet, int* got_frame)
{
    int ret = 0;
    ret = avcodec_encode_video2(pCodecContext, packet, NULL, got_frame);
    if (ret < 0) {
        LOGE("Error while flush_video_encoder, ret=%d", ret);
        return ret;
    }

    return 0;
}

int VVAVEncoder::flush_audio_encoder(AVCodecContext *pCodecContext, AVPacket *packet, int* got_frame)
{
    int ret = 0;
    ret = avcodec_encode_audio2(pCodecContext, packet, NULL, got_frame);
    if (ret < 0) {
        LOGE("Error while flush_audio_encoder, ret=%d", ret);
        return ret;
    }

    return 0;
}



