//
// Created by mj on 18-12-21.
//

#include "LhAVFormat.h"
#include "logUtil.h"


namespace videorecorder{

#define CHECK_FILE_OPEN(context, str, ret) \
        do {    \
            if(!context){ \
                LOGE("%s : %s file not open..", __func__, str); \
                return ret;\
            }   \
        }while(0)


#define CHECK_OUT_FILE_OPEN(context, ret)   CHECK_FILE_OPEN(context, "output", ret)
#define CHECK_IN_FILE_OPEN(context, ret)    CHECK_FILE_OPEN(context, "input", ret)

#define CHECK_OUT_FILE_OPEN_INT(context)    CHECK_OUT_FILE_OPEN(context, -1)
#define CHECK_OUT_FILE_OPEN_NULL(context)   CHECK_OUT_FILE_OPEN(context, nullptr)

#define CHECK_IN_FILE_OPEN_INT(context)     CHECK_IN_FILE_OPEN(context, -1)
#define CHECK_IN_FILE_OPEN_NULL(context)    CHECK_IN_FILE_OPEN(context, nullptr)

LhAVFormat::LhAVFormat() {
    m_pInFmtCtx = nullptr;
    m_pOutFmtCtx = nullptr;
}

LhAVFormat::~LhAVFormat() {
    close_input_file();
    close_output_file();
}

AVFormatContext *LhAVFormat::getInFmtCtx() {
    return m_pInFmtCtx;
}

AVFormatContext *LhAVFormat::getOutFmtCtx() {
    return m_pOutFmtCtx;
}

int LhAVFormat::open_input_file(const char *filename) {
    if(m_pInFmtCtx){
        close_input_file();
    }

    int ret = 0;

    m_pInFmtCtx = avformat_alloc_context();

    ret = avformat_open_input(&m_pInFmtCtx, filename, NULL, NULL);
    if(ret != 0){
        LOGE("avformat_open_input failed ret=%d,filename=%s", ret, filename);
        return ret;
    }

    ret = avformat_find_stream_info(m_pInFmtCtx, NULL);
    if(ret < 0){
        LOGE("avformat_find_stream_info failed ret=%d", ret);
        return ret;
    }

    return 0;
}

int LhAVFormat::close_input_file() {
    if(m_pInFmtCtx)
    {
        avformat_close_input(&m_pInFmtCtx);
        avformat_free_context(m_pInFmtCtx);
        m_pInFmtCtx = nullptr;
    }

    return 0;
}

int LhAVFormat::find_video_stream(int start) {
    CHECK_IN_FILE_OPEN_INT(m_pInFmtCtx);

    int videoindex = -1;
    for(int i = start; i < m_pInFmtCtx->nb_streams; i++)
    {
        if(m_pInFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoindex = i;
            break;
        }
    }

    return videoindex;
}

int LhAVFormat::find_audio_stream(int start) {
    CHECK_IN_FILE_OPEN_INT(m_pInFmtCtx);
    int audioindex = -1;
    for(int i = start; i < m_pInFmtCtx->nb_streams; i++)
    {
        if(m_pInFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioindex = i;
            break;
        }
    }

    return audioindex;
}

int LhAVFormat::seek(int stream_index, int ms) {
    CHECK_IN_FILE_OPEN_INT(m_pInFmtCtx);
    AVRational s_tb = m_pInFmtCtx->streams[stream_index]->time_base;
    int64_t seekPts = av_rescale_q(ms, AVRational{1, 1000}, s_tb);
    int ret = avformat_seek_file(m_pInFmtCtx, stream_index, INT64_MIN, seekPts, INT64_MAX, 0);
    if(ret < 0){
        LOGE("seek faild %d", ret);
    }

    return ret;
}

AVCodecContext *LhAVFormat::getAudioCodecContext() {
    CHECK_IN_FILE_OPEN_NULL(m_pInFmtCtx);

    int idx = find_audio_stream();
    if(idx < 0){
        return nullptr;
    }

    return m_pInFmtCtx->streams[idx]->codec;
}

AVCodecContext *LhAVFormat::getVideoCodecContext() {
    CHECK_IN_FILE_OPEN_NULL(m_pInFmtCtx);

    int idx = find_video_stream();
    if(idx < 0){
        return nullptr;
    }

    return m_pInFmtCtx->streams[idx]->codec;
}

int LhAVFormat::open_output_file(const char *filename) {
    if(m_pOutFmtCtx){
        close_output_file();
    }

    int ret = 0;
    //申请输出文件内存
    m_pOutFmtCtx = avformat_alloc_context();
    ret = avformat_alloc_output_context2(&m_pOutFmtCtx, NULL, NULL, filename);
    if (!m_pOutFmtCtx) {
        LOGE("avformat_alloc_output_context2 failed ret=%d,filename=%s", ret, filename);
        return ret;
    }

    AVOutputFormat* ofmt = m_pOutFmtCtx->oformat;

//    av_dump_format(pFormatContext, 0, filename, 1);

    //打开输出文件
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_pOutFmtCtx->pb, m_pOutFmtCtx->filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOGE("avio_open failed ret=%d,filename=%s", ret, m_pOutFmtCtx->filename);
            return ret;
        }
    }

    return 0;
}

AVStream *LhAVFormat::add_stream_by_stream(AVStream *pInStream, bool copyContext) {
    if(!pInStream){
        return nullptr;
    }

    AVStream* out_stream = add_stream_by_codecContext(pInStream->codec, copyContext);

    if(out_stream && pInStream){
        out_stream->time_base = pInStream->time_base;
    }

    return out_stream;
}

AVStream *LhAVFormat::add_stream_by_codecContext(AVCodecContext *pCodecContext, bool copyContext) {
    if(!pCodecContext){
        return nullptr;
    }

    int ret = 0;
    AVStream *out_stream = add_stream_by_codec(pCodecContext->codec);

    //Copy the settings of AVCodecContext
    if(copyContext && pCodecContext){
        ret = avcodec_copy_context(out_stream->codec, pCodecContext);
        if(ret < 0){
            LOGE("avcodec_copy_context failed ret=%d", ret);
        }
    }

    out_stream->codec->codec_tag = 0;
    if (m_pOutFmtCtx->oformat->flags & AVFMT_GLOBALHEADER){
        out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    return out_stream;
}

AVStream *LhAVFormat::add_stream_by_codec(const AVCodec *codec) {
    CHECK_OUT_FILE_OPEN_NULL(m_pOutFmtCtx);

    AVStream *out_stream = avformat_new_stream(m_pOutFmtCtx, codec);
    if (!out_stream) {
        LOGE("avformat_new_stream failed");
        return nullptr;
    }

    return out_stream;
}

int LhAVFormat::close_output_file() {
    int ret = 0;

    if (m_pOutFmtCtx && !(m_pOutFmtCtx->oformat->flags & AVFMT_NOFILE)){
        ret = avio_close(m_pOutFmtCtx->pb);

        if(ret < 0){
            LOGE("avio_close failed ret=%d", ret);
            return ret;
        }

        avformat_free_context(m_pOutFmtCtx);
        m_pOutFmtCtx = nullptr;
    }

    return 0;
}

int LhAVFormat::read_packet(AVPacket *packet) {
    CHECK_OUT_FILE_OPEN_INT(m_pInFmtCtx);

    return av_read_frame(m_pInFmtCtx, packet);
}

int LhAVFormat::write_hearder() {
    CHECK_OUT_FILE_OPEN_INT(m_pOutFmtCtx);

    int ret = avformat_write_header(m_pOutFmtCtx, NULL);
    if (ret < 0) {
        LOGE("avformat_write_header failed ret=%d", ret);
        return ret;
    }

    return 0;
}

int LhAVFormat::write_mp4_header(bool faststart) {
    CHECK_OUT_FILE_OPEN_INT(m_pOutFmtCtx);


    AVDictionary* opt = NULL;
    if(!strcmp(m_pOutFmtCtx->oformat->name, "mp4") && faststart){
        av_dict_set_int(&opt, "movflags", FF_MOV_FLAG_FASTSTART, 0);
    }

    int ret = avformat_write_header(m_pOutFmtCtx, &opt);
    //Write file header
    if (ret < 0) {
        LOGE("avformat_write_header mp4 failed ret=%d,faststart=%d", ret, faststart);
        return ret;
    }

    return 0;
}

int LhAVFormat::write_packet(AVPacket *packet) {
    CHECK_OUT_FILE_OPEN_INT(m_pOutFmtCtx);

    int ret = av_write_frame(m_pOutFmtCtx, packet);
    if(ret < 0){
        LOGE("av_write_frame failed ret=%d", ret);
        return ret;
    }

    return 0;
}

int LhAVFormat::write_interleave_packet(AVPacket *packet) {
    CHECK_OUT_FILE_OPEN_INT(m_pOutFmtCtx);

    int ret = av_interleaved_write_frame(m_pOutFmtCtx, packet);
    if(ret < 0){
        LOGE("av_interleaved_write_frame failed ret=%d", ret);
        return ret;
    }

    return 0;
}

int LhAVFormat::write_tailer() {
    CHECK_OUT_FILE_OPEN_INT(m_pOutFmtCtx);

    int ret = av_write_trailer(m_pOutFmtCtx);
    if(ret != 0){
        LOGE("av_write_trailer failed ret=%d", ret);
        return ret;
    }

    return 0;
}


} //namespace videorecorder
