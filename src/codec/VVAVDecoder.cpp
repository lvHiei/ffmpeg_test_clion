//
// Created by mj on 17-8-31.
//

#include "VVAVDecoder.h"
#include "../util/logUtil.h"


VVAVDecoder::VVAVDecoder()
{
    avcodec_register_all();
}

VVAVDecoder::~VVAVDecoder()
{

}

int VVAVDecoder::open_decoder(AVCodecContext *pCodecContext)
{
    int ret = 0;

    AVCodec* pCodec = avcodec_find_decoder(pCodecContext->codec_id);
    if(!pCodec){
        LOGE("VVAVDecoder::open_decoder, find decoder failed,decoder:%d", pCodecContext->codec_id);
        return -1;
    }

    ret = avcodec_open2(pCodecContext, pCodec, NULL);
    if(ret < 0){
        char buf[1024];
        av_strerror(ret, buf, 1024);
        LOGE("VVAVDecoder::open_decoder avcodec_open2 faield,codec:%d,ret=%d(%s)", pCodecContext->codec_id, ret, buf);
        return ret;
    }

    return 0;
}


int VVAVDecoder::close_decoder(AVCodecContext *pCodecContext)
{
    avcodec_close(pCodecContext);
    return 0;
}


int VVAVDecoder::decode_video(AVCodecContext *pCodecContext, AVFrame *pFrame, int *got_picture,
                              AVPacket *packet)
{
    int ret = 0;
    ret = avcodec_decode_video2(pCodecContext, pFrame, got_picture, packet);

    if(ret < 0){
        LOGE("avcodec_decode_video2 faield,ret:%d", ret);
        return ret;
    }

    av_packet_unref(packet);

    return 0;
}

int VVAVDecoder::decode_audio(AVCodecContext *pCodecContext, AVFrame *pFrame, int *got_picture,
                              AVPacket *packet)
{
    int ret = 0;
    ret = avcodec_decode_audio4(pCodecContext, pFrame, got_picture, packet);

    if(ret < 0){
        LOGE("avcodec_decode_audio4 faield,ret:%d", ret);
        return ret;
    }

    av_packet_unref(packet);

    return 0;
}

int VVAVDecoder::flush_video_decoder(AVCodecContext *pCodecContext, AVFrame *pFrame,
                                     int *got_picture, AVPacket *packet)
{
    int ret = 0;
    ret = avcodec_decode_video2(pCodecContext, pFrame, got_picture, packet);
    if (ret < 0 ){
        return ret;
    }

    av_free_packet(packet);
    return 0;
}

int VVAVDecoder::flush_audio_decoder(AVCodecContext *pCodecContext, AVFrame *pFrame,
                                     int *got_picture, AVPacket *packet)
{
    int ret = 0;
    ret = avcodec_decode_audio4(pCodecContext, pFrame, got_picture, packet);
    if (ret < 0 ){
        return ret;
    }

    av_free_packet(packet);
    return 0;
}

