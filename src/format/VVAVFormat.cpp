//
// Created by mj on 17-8-30.
//

#include "VVAVFormat.h"

#include "../util/logUtil.h"

VVAVFormat::VVAVFormat()
{
    av_register_all();
}

VVAVFormat::~VVAVFormat()
{

}

AVFormatContext *VVAVFormat::alloc_foramt_context()
{
    return avformat_alloc_context();
}


void VVAVFormat::free_format_context(AVFormatContext* context)
{
    avformat_free_context(context);
}


int VVAVFormat::open_input_file(AVFormatContext **pFormatContext, const char *filename)
{
    int ret = 0;

    ret = avformat_open_input(pFormatContext, filename, NULL, NULL);
    if(ret != 0){
        LOGE("avformat_open_input failed ret=%d,filename=%s", ret, filename);
        return ret;
    }

    ret = avformat_find_stream_info(*pFormatContext, NULL);
    if(ret < 0){
        LOGE("avformat_find_stream_info failed ret=%d", ret);
        return ret;
    }

    return 0;
}

int VVAVFormat::close_input_file(AVFormatContext **pFormatContext)
{
    if(pFormatContext)
    {
        avformat_close_input(pFormatContext);
    }

    return 0;
}


int VVAVFormat::find_video_stream(AVFormatContext *pFormatContext)
{
    int videoindex = -1;
    for(int i = 0; i < pFormatContext->nb_streams; i++)
    {
        if(pFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoindex = i;
            break;
        }
    }

    return videoindex;
}

int VVAVFormat::find_audio_stream(AVFormatContext *pFormatContext)
{
    int audioindex = -1;
    for(int i = 0; i < pFormatContext->nb_streams; i++)
    {
        if(pFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioindex = i;
            break;
        }
    }

    return audioindex;
}


int VVAVFormat::alloc_output_context(AVFormatContext **pFormatContext, const char *filename)
{
    int ret = 0;
    //申请输出文件内存
    ret = avformat_alloc_output_context2(pFormatContext, NULL, NULL, filename);
    if (!pFormatContext) {
        LOGE("avformat_alloc_output_context2 failed ret=%d,filename=%s", ret, filename);
        return ret;
    }

    return 0;
}


AVStream *VVAVFormat::add_stream(AVFormatContext *pFormatContext, AVStream *pInStream,
                                 bool copyContext)
{
    AVCodecContext* pCodecContext = pInStream == NULL ? NULL : pInStream->codec;
    AVStream* out_stream = add_stream(pFormatContext, pCodecContext, copyContext);

    if(out_stream && pInStream){
        out_stream->time_base = pInStream->time_base;
    }

    return out_stream;
}


AVStream* VVAVFormat::add_stream(AVFormatContext *pFormatContext, AVCodecContext *pCodecContext, bool copyContext)
{
    int ret = 0;

    const AVCodec* pCodec = pCodecContext == NULL ? NULL : pCodecContext->codec;

    AVStream *out_stream = avformat_new_stream(pFormatContext, pCodec);
    if (!out_stream) {
        LOGE("avformat_new_stream failed");
        return out_stream;
    }

    //Copy the settings of AVCodecContext
    if(copyContext && pCodecContext){
        ret = avcodec_copy_context(out_stream->codec, pCodecContext);
        if(ret < 0){
            LOGE("avcodec_copy_context failed ret=%d", ret);
        }
    }

    out_stream->codec->codec_tag = 0;
    if (pFormatContext->oformat->flags & AVFMT_GLOBALHEADER){
        out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    return out_stream;
}

int VVAVFormat::open_output_file(AVFormatContext *pFormatContext)
{
    int ret = 0;

    AVOutputFormat* ofmt = pFormatContext->oformat;

//    av_dump_format(pFormatContext, 0, filename, 1);

    //打开输出文件
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&pFormatContext->pb, pFormatContext->filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOGE("avio_open failed ret=%d,filename=%s", ret, pFormatContext->filename);
            return ret;
        }
    }

    return 0;
}


int VVAVFormat::close_output_file(AVFormatContext* pFormatContext)
{
    int ret = 0;

    if (pFormatContext && !(pFormatContext->oformat->flags & AVFMT_NOFILE)){
        ret = avio_close(pFormatContext->pb);

        if(ret < 0){
            LOGE("avio_close failed ret=%d", ret);
            return ret;
        }
    }

    return 0;
}


int VVAVFormat::open_in_out_file(AVFormatContext **pInputFormatContext, const char *infilename,
                                 AVFormatContext **pOutputFormatContext, const char *outfilename)
{
    int iAudioIndex;
    int iVideoIndex;
    int oAudioIndex;
    int oVideoIndex;

    return open_in_out_file(pInputFormatContext, infilename, iAudioIndex, iVideoIndex,
                            pOutputFormatContext, outfilename, oAudioIndex, oVideoIndex);
}


int VVAVFormat::open_in_out_file(AVFormatContext **pInputFormatContext, const char *infilename,
                                 int &iAudioIndex, int &iVideoIndex,
                                 AVFormatContext **pOutputFormatContext, const char *outfilename,
                                 int &oAudioIndex, int &oVideoIndex)
{
    int ret = 0;

    iAudioIndex = -1;
    iVideoIndex = -1;
    oAudioIndex = -1;
    oVideoIndex = -1;

    ret = open_input_file(pInputFormatContext, infilename);
    if(ret < 0){
        return ret;
    }

    iVideoIndex = find_video_stream(*pInputFormatContext);
    iAudioIndex = find_audio_stream(*pInputFormatContext);

    ret = alloc_output_context(pOutputFormatContext, outfilename);
    if(ret < 0){
        return ret;
    }

    AVFormatContext* pInFmtCtx = *pInputFormatContext;
    AVFormatContext* pOutFmtCtx = *pOutputFormatContext;


    if(iVideoIndex >= 0){
        AVStream* out_video_stream = add_stream(pOutFmtCtx, pInFmtCtx->streams[iVideoIndex]);
        if(!out_video_stream){
            LOGE("add video stream failed");
            return -1;
        }
        oVideoIndex = out_video_stream->index;
    }

    if(iAudioIndex >= 0){
        AVStream* out_audio_stream = add_stream(pOutFmtCtx, pInFmtCtx->streams[iAudioIndex]);
        if(!out_audio_stream){
            LOGE("add audio stream failed");
            return -1;
        }
        oAudioIndex = out_audio_stream->index;
    }

    ret = open_output_file(pOutFmtCtx);
    if(ret < 0){
        return ret;
    }

    return 0;
}


int VVAVFormat::close_in_out_file(AVFormatContext **pInputFormatContext,
                                  AVFormatContext **pOutputFormatContext)
{
    int ret = 0;

    ret = close_input_file(pInputFormatContext);
    if(ret < 0){
        return ret;
    }

    ret = close_output_file(*pOutputFormatContext);

    return ret;
}


int VVAVFormat::read_packet(AVFormatContext *pFormatContext, AVPacket *packet)
{
    return av_read_frame(pFormatContext, packet);
}


int VVAVFormat::write_hearder(AVFormatContext *pFormatContext)
{
    int ret = avformat_write_header(pFormatContext, NULL);
    if (ret < 0) {
        LOGE("avformat_write_header failed ret=%d", ret);
        return ret;
    }

    return 0;
}

int VVAVFormat::write_mp4_header(AVFormatContext *pFormatContext, bool faststart)
{
    AVDictionary* opt = NULL;
    if(!strcmp(pFormatContext->oformat->name, "mp4") && faststart){
        av_dict_set_int(&opt, "movflags", FF_MOV_FLAG_FASTSTART, 0);
    }

    int ret = avformat_write_header(pFormatContext, &opt);
    //Write file header
    if (ret < 0) {
        LOGE("avformat_write_header mp4 failed ret=%d,faststart=%d", ret, faststart);
        return ret;
    }

    return 0;
}

int VVAVFormat::write_packet(AVFormatContext *pFormatContext, AVPacket packet)
{
    int ret = av_write_frame(pFormatContext, &packet);

    if(ret < 0){
        LOGE("av_write_frame failed ret=%d", ret);
        return ret;
    }

    return 0;
}

int VVAVFormat::write_interleave_packet(AVFormatContext *pFormatContext, AVPacket packet)
{
    int ret = av_interleaved_write_frame(pFormatContext, &packet);

    if(ret < 0){
        LOGE("av_interleaved_write_frame failed ret=%d", ret);
        return ret;
    }

    return 0;
}

int VVAVFormat::write_tailer(AVFormatContext *pFormatContext)
{
    int ret = av_write_trailer(pFormatContext);

    if(ret != 0){
        LOGE("av_write_trailer failed ret=%d", ret);
        return ret;
    }

    return 0;
}

