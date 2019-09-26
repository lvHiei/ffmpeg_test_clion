//
// Created by mj on 18-12-21.
//

#ifndef MYTESTSERVICE_LHAVCODEC_H
#define MYTESTSERVICE_LHAVCODEC_H

#include "ffmpegHeader.h"


namespace videorecorder{

class LhAVCodec {
public:
    LhAVCodec();
    virtual ~LhAVCodec();

public:
    // encoder
    void setEncoderContext(AVCodecContext* codecContext);

    int init_video_encoder(AVCodecID codecID, int width, int height,
                           int framerate, int bitrate, int gop,
                           AVDictionary** param);
    int init_audio_encoder(AVCodecID codecID, int samplerate,
                           int channels, int bitrate, int profile);

    int open_encoder(AVDictionary* param = NULL);
    int close_encoder();

    int encode_video_frame(AVPacket* packet, AVFrame* pFrame, int* got_frame);
    int encode_audio_frame(AVPacket* packet, AVFrame* pFrame, int* got_frame);

    // this flush methond only flush one frame per call
    int flush_video_encoder(AVPacket* packet, int* got_frame);
    int flush_audio_encoder(AVPacket* packet, int* got_frame);

public:
    //decoder
    void setDecoderContext(AVCodecContext* codecContext);

    int open_decoder();
    int close_decoder();

    int flush_decoder_buffers();

    int decode_video(AVFrame* pFrame, int* got_picture, AVPacket* packet);
    int decode_audio(AVFrame* pFrame, int* got_picture, AVPacket* packet);

    int flush_video_decoder(AVFrame* pFrame, int* got_picture, AVPacket* packet);
    int flush_audio_decoder(AVFrame* pFrame, int* got_picture, AVPacket* packet);


private:
    AVCodecContext* m_pEncCodecCtx;
    AVCodecContext* m_pDecCodecCtx;

    bool m_bAllocEncoder;
    bool m_bAllocDecoder;
};




} //namespace videorecorder
#endif //MYTESTSERVICE_LHAVCODEC_H
