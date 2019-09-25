//
// Created by mj on 17-8-31.
//

#ifndef VVMUSIC_ANDROID_APP_VVAVCODEC_H
#define VVMUSIC_ANDROID_APP_VVAVCODEC_H


extern "C"
{
//#include "libavformat/avformat.h"
//#include "libavformat/movenc.h"
#include "libavcodec/avcodec.h"
#include "libavutil/frame.h"
//#include "libavutil/avutil.h"
#include "libavutil/dict.h"
#include "libavutil/channel_layout.h"
}

class VVAVEncoder {
public:
    VVAVEncoder();
    virtual ~VVAVEncoder();

public:
    int init_video_encoder(AVCodecContext* pCodecContext, AVCodecID codecID, int width, int height, int framerate, int bitrate, AVDictionary** param);
    int init_audio_encoder(AVCodecContext* pCodecContext, AVCodecID codecID, int samplerate, int channels, int bitrate, int aacType = 1);

public:
    int open_encoder(AVCodecContext* pCodecContext, AVDictionary* param = NULL);
    int close_encoder(AVCodecContext* pCodecContext);

public:
    int init_and_open_video_encoder(AVCodecContext *pCodecContext, AVCodecID codecID, int width,
                                    int height, int framerate, int bitrate);
    int init_and_open_audio_encoder(AVCodecContext *pCodecContext, AVCodecID codecID,
                                    int samplerate, int channels, int bitrate, int aacType = 1);

public:
    int encode_video_frame(AVCodecContext* pCodecContext, AVPacket* packet, AVFrame* pFrame, int* got_frame);
    int encode_audio_frame(AVCodecContext* pCodecContext, AVPacket* packet, AVFrame* pFrame, int* got_frame);

public:
    // this flush methond only flush one frame per call

    int flush_video_encoder(AVCodecContext* pCodecContext, AVPacket* packet, int* got_frame);
    int flush_audio_encoder(AVCodecContext* pCodecContext, AVPacket* packet, int* got_frame);
};


#endif //VVMUSIC_ANDROID_APP_VVAVCODEC_H
