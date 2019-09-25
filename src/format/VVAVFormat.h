//
// Created by mj on 17-8-30.
//

#ifndef SRC_VVAVFORMAT_H
#define SRC_VVAVFORMAT_H

extern "C"
{
#include "libavformat/avformat.h"
#include "libavformat/movenc.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/dict.h"
}

class VVAVFormat {
public:
    VVAVFormat();
    virtual ~VVAVFormat();

public:
    AVFormatContext* alloc_foramt_context();
    void free_format_context(AVFormatContext* context);

public:
    // only input file

    int open_input_file(AVFormatContext** pFormatContext, const char* filename);
    int close_input_file(AVFormatContext** pFormatContext);
    int find_video_stream(AVFormatContext* pFormatContext);
    int find_audio_stream(AVFormatContext* pFormatContext);

public:
    // only output file

    int alloc_output_context(AVFormatContext** pFormatContext, const char* filename);
    AVStream* add_stream(AVFormatContext* pFormatContext, AVStream* pInStream, bool copyContext = true);
    AVStream* add_stream(AVFormatContext* pFormatContext, AVCodecContext* pCodecContext, bool copyContext = true);
    int open_output_file(AVFormatContext* pFormatContext);
    int close_output_file(AVFormatContext* pFormatContext);

public:
    // both input and output file

    int open_in_out_file(AVFormatContext **pInputFormatContext, const char *infilename,
                         AVFormatContext **pOutputFormatContext, const char *outfilename);

    int open_in_out_file(AVFormatContext **pInputFormatContext, const char *infilename, int& iAudioIndex, int& iVideoIndex,
                         AVFormatContext **pOutputFormatContext, const char *outfilename, int& oAudioIndex, int& oVideoIndex);

    int close_in_out_file(AVFormatContext **pInputFormatContext,
                          AVFormatContext **pOutputFormatContext);

public:
    // write method
    int read_packet(AVFormatContext* pFormatContext, AVPacket* packet);

    int write_hearder(AVFormatContext *pFormatContext);
    int write_mp4_header(AVFormatContext* pFormatContext, bool faststart);
    int write_packet(AVFormatContext* pFormatContext, AVPacket packet);
    int write_interleave_packet(AVFormatContext* pFormatContext, AVPacket packet);
    int write_tailer(AVFormatContext *pFormatContext);
};

#endif //SRC_VVAVFORMAT_H
