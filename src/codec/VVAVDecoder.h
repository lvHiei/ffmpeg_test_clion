//
// Created by mj on 17-8-31.
//

#ifndef VVMUSIC_ANDROID_APP_VVAVDECODER_H
#define VVMUSIC_ANDROID_APP_VVAVDECODER_H


extern "C"
{
//#include "libavformat/avformat.h"
//#include "libavformat/movenc.h"
#include "libavcodec/avcodec.h"
#include "libavutil/frame.h"
//#include "libavutil/avutil.h"
//#include "libavutil/dict.h"
//#include "libavutil/channel_layout.h"
}

class VVAVDecoder {
public:
    VVAVDecoder();
    virtual ~VVAVDecoder();

public:
    int open_decoder(AVCodecContext* pCodecContext);
    int close_decoder(AVCodecContext* pCodecContext);

public:
    int decode_video(AVCodecContext* pCodecContext, AVFrame* pFrame, int* got_picture, AVPacket* packet);
    int decode_audio(AVCodecContext* pCodecContext, AVFrame* pFrame, int* got_picture, AVPacket* packet);

public:
    int flush_video_decoder(AVCodecContext* pCodecContext, AVFrame* pFrame, int* got_picture, AVPacket* packet);
    int flush_audio_decoder(AVCodecContext* pCodecContext, AVFrame* pFrame, int* got_picture, AVPacket* packet);
};


#endif //VVMUSIC_ANDROID_APP_VVAVDECODER_H
