//
// Created by mj on 18-12-21.
//

#include "LhAVCodec.h"

#include "logUtil.h"


namespace videorecorder{

#define CHECK_CODEC(context, str) \
        do {    \
            if(!context){ \
                LOGE("%s : %s is not inited or set..", __func__, str); \
                return -1;\
            }   \
        }while(0)


#define CHECK_ENCODER(context)  CHECK_CODEC(context, "encoder")
#define CHECK_DECODER(context)  CHECK_CODEC(context, "decoder")



LhAVCodec::LhAVCodec() {
    m_pEncCodecCtx = nullptr;
    m_pDecCodecCtx = nullptr;

    m_bAllocEncoder = false;
    m_bAllocDecoder = false;
}

LhAVCodec::~LhAVCodec() {
    if(m_pDecCodecCtx){
        close_decoder();
    }

    if(m_pEncCodecCtx){
        close_encoder();
    }
}

void LhAVCodec::setEncoderContext(AVCodecContext *codecContext) {
    m_pEncCodecCtx = codecContext;
}

int
LhAVCodec::init_video_encoder(AVCodecID codecID, int width, int height,
                              int framerate, int bitrate, int gop, AVDictionary **param) {
    if(!m_pEncCodecCtx){
        AVCodec* codec = avcodec_find_encoder(codecID);
        m_pEncCodecCtx = avcodec_alloc_context3(codec);
        m_bAllocEncoder = true;
    }

    m_pEncCodecCtx->codec_id = codecID;
    m_pEncCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;

    //H264
    if(AV_CODEC_ID_H264 == m_pEncCodecCtx->codec_id){
        av_dict_set(param, "preset", "ultrafast", 0);
        av_dict_set(param, "tune","zerolatency", 0);
        av_dict_set(param, "rc-lookahead", 0, 0);
        av_dict_set(param, "profile", "baseline", 0);
    }

    // copy 房间的设置
    m_pEncCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    m_pEncCodecCtx->width = width;
    m_pEncCodecCtx->height = height;
    m_pEncCodecCtx->time_base.num = 1;
    m_pEncCodecCtx->time_base.den = framerate;
    m_pEncCodecCtx->level=40;

    m_pEncCodecCtx->thread_count = 0;
    //Encoder parameters
    m_pEncCodecCtx->refs = 1;
    m_pEncCodecCtx->gop_size = gop ;

    //码流
    m_pEncCodecCtx->bit_rate = bitrate;
    m_pEncCodecCtx->rc_max_rate = m_pEncCodecCtx->bit_rate*1.5;


    //disable B frame
    m_pEncCodecCtx->max_b_frames = 0;
    m_pEncCodecCtx->b_frame_strategy= 0;

    m_pEncCodecCtx->trellis = 0;
    m_pEncCodecCtx->qmin = 20;         //减少会较大的影响码流变化
    m_pEncCodecCtx->qmax = 30;
    m_pEncCodecCtx->max_qdiff = 10;

    m_pEncCodecCtx->qcompress = 0.8;  //default 0.6
    m_pEncCodecCtx->qblur = 0.5;                             //default 0.5  qblur=1 is a gaussian blur of radius 1.

    //熵编码
    m_pEncCodecCtx->coder_type = FF_CODER_TYPE_AC;

    m_pEncCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

    LOGI("init_video_encoder codec:%d,width:%d,height:%d,fps:%d,bitrate:%d,gop:%d",
            codecID, width, height, framerate, bitrate, gop);
    return 0;
}

int LhAVCodec::init_audio_encoder(AVCodecID codecID, int samplerate, int channels, int bitrate,
                                  int profile) {
    if(!m_pEncCodecCtx){
        AVCodec* codec = avcodec_find_encoder(codecID);
        m_pEncCodecCtx = avcodec_alloc_context3(codec);
        m_bAllocEncoder = true;
    }
    
    m_pEncCodecCtx->codec_id = codecID;
    m_pEncCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
    m_pEncCodecCtx->sample_rate = samplerate;
    m_pEncCodecCtx->channels = channels;
    m_pEncCodecCtx->channel_layout = av_get_default_channel_layout(m_pEncCodecCtx->channels);
    m_pEncCodecCtx->bit_rate = bitrate;
    m_pEncCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    m_pEncCodecCtx->profile = profile;

    LOGI("init_audio_encoder codec:%d,samplerate:%d,channels:%d,bitrate:%d,profile:%d",
         codecID, samplerate, channels, bitrate, profile);
    return 0;
}

int LhAVCodec::open_encoder(AVDictionary *param) {
    CHECK_ENCODER(m_pEncCodecCtx);

    int ret = 0;
    AVCodec* pCodec = avcodec_find_encoder(m_pEncCodecCtx->codec_id);
    if(!pCodec){
        LOGE("VVAVEncoder::open_encoder find encoder failed, codec:%d", m_pEncCodecCtx->codec_id);
        return -1;
    }

    m_pEncCodecCtx->codec = pCodec;
    ret = avcodec_open2(m_pEncCodecCtx, pCodec, &param);
    if(ret < 0){
        char buf[1024];
        av_strerror(ret, buf, 1024);
        LOGE("VVAVEncoder::open_encoder open codec failed,codec:%d,ret:%d(%s)",
             m_pEncCodecCtx->codec_id, ret, buf);
        return ret;
    }

    return 0;
}

int LhAVCodec::close_encoder() {
    CHECK_ENCODER(m_pEncCodecCtx);

    int ret = avcodec_close(m_pEncCodecCtx);
    if(ret < 0){
        LOGE("close encoder error codec:%d,err:%d",
             m_pEncCodecCtx->codec_id, ret);
    }

    if(m_bAllocEncoder){
        m_bAllocEncoder = false;
        avcodec_free_context(&m_pEncCodecCtx);
    }
    m_pEncCodecCtx = nullptr;
    return 0;
}

int LhAVCodec::encode_video_frame(AVPacket *packet, AVFrame *pFrame, int *got_frame) {
    CHECK_ENCODER(m_pEncCodecCtx);

    int ret = 0;

    ret = avcodec_encode_video2(m_pEncCodecCtx, packet, pFrame, got_frame);
    if(ret < 0){
        LOGE("VVAVEncoder::encode_video_frame failed,ret=%d", ret);
        return ret;
    }

//    av_frame_unref(pFrame);

    return 0;
}

int LhAVCodec::encode_audio_frame(AVPacket *packet, AVFrame *pFrame, int *got_frame) {
    CHECK_ENCODER(m_pEncCodecCtx);

    int ret = 0;

    ret = avcodec_encode_audio2(m_pEncCodecCtx, packet, pFrame, got_frame);
    if(ret < 0){
        LOGE("VVAVEncoder::encode_audio_frame failed,ret=%d", ret);
        return ret;
    }

//    av_frame_unref(pFrame);

    return 0;
}

int LhAVCodec::flush_video_encoder(AVPacket *packet, int *got_frame) {
    CHECK_ENCODER(m_pEncCodecCtx);

    int ret = 0;
    ret = avcodec_encode_video2(m_pEncCodecCtx, packet, NULL, got_frame);
    if (ret < 0) {
        LOGE("Error while flush_video_encoder, ret=%d", ret);
        return ret;
    }

    return 0;
}

int LhAVCodec::flush_audio_encoder(AVPacket *packet, int *got_frame) {
    CHECK_ENCODER(m_pEncCodecCtx);

    int ret = 0;
    ret = avcodec_encode_audio2(m_pEncCodecCtx, packet, NULL, got_frame);
    if (ret < 0) {
        LOGE("Error while flush_audio_encoder, ret=%d", ret);
        return ret;
    }

    return 0;
}

void LhAVCodec::setDecoderContext(AVCodecContext* codecContext) {
    m_pDecCodecCtx = codecContext;
}

int LhAVCodec::open_decoder() {
    CHECK_DECODER(m_pDecCodecCtx);

    int ret = 0;

    AVCodec* pCodec = avcodec_find_decoder(m_pDecCodecCtx->codec_id);
    if(!pCodec){
        LOGE("VVAVDecoder::open_decoder, find decoder failed,decoder:%d",
             m_pDecCodecCtx->codec_id);
        return -1;
    }

    ret = avcodec_open2(m_pDecCodecCtx, pCodec, NULL);
    if(ret < 0){
        char buf[1024];
        av_strerror(ret, buf, 1024);
        LOGE("VVAVDecoder::open_decoder avcodec_open2 faield,codec:%d,ret=%d(%s)",
             m_pDecCodecCtx->codec_id, ret, buf);
        return ret;
    }

    return 0;
}

int LhAVCodec::close_decoder() {
    CHECK_DECODER(m_pDecCodecCtx);

    int ret = avcodec_close(m_pDecCodecCtx);
    if(ret < 0){
        LOGE("close encoder error codec:%d,err:%d",
             m_pDecCodecCtx->codec_id, ret);
    }

    return 0;
}

int LhAVCodec::flush_decoder_buffers() {
    CHECK_DECODER(m_pDecCodecCtx);

    avcodec_flush_buffers(m_pDecCodecCtx);
    return 0;
}

int LhAVCodec::decode_video(AVFrame *pFrame, int *got_picture, AVPacket *packet) {
    CHECK_DECODER(m_pDecCodecCtx);

    int ret = 0;
    ret = avcodec_decode_video2(m_pDecCodecCtx, pFrame, got_picture, packet);

    if(ret < 0){
        LOGE("avcodec_decode_video2 faield,ret:%d", ret);
        return ret;
    }

    av_packet_unref(packet);

    return 0;
}

int LhAVCodec::decode_audio(AVFrame *pFrame, int *got_picture, AVPacket *packet) {
    CHECK_DECODER(m_pDecCodecCtx);

    int ret = 0;
    ret = avcodec_decode_audio4(m_pDecCodecCtx, pFrame, got_picture, packet);

    if(ret < 0){
        LOGE("avcodec_decode_audio4 faield,ret:%d", ret);
        return ret;
    }

    av_packet_unref(packet);

    return 0;
}

int LhAVCodec::flush_video_decoder(AVFrame *pFrame, int *got_picture, AVPacket *packet) {
    CHECK_DECODER(m_pDecCodecCtx);

    int ret = 0;
    ret = avcodec_decode_video2(m_pDecCodecCtx, pFrame, got_picture, packet);
    if (ret < 0 ){
        return ret;
    }

    av_packet_unref(packet);
    return 0;
}

int LhAVCodec::flush_audio_decoder(AVFrame *pFrame, int *got_picture, AVPacket *packet) {
    CHECK_DECODER(m_pDecCodecCtx);

    int ret = 0;
    ret = avcodec_decode_audio4(m_pDecCodecCtx, pFrame, got_picture, packet);
    if (ret < 0 ){
        return ret;
    }

    av_packet_unref(packet);
    return 0;
}


} //namespace videorecorder
