/**
 * Created by mj on 17-4-18.
 */

#include <stdint.h>
#include <unistd.h>
#include "LocalVideo.h"
#include "../util/TimeUtil.h"
#include "../util/AVUtil.h"

#define LOGD printf
#define LOGI printf
#define LOGW printf
#define LOGE printf

LocalVideo* LocalVideo::ms_instance = NULL;

LocalVideo *LocalVideo::getInstance()
{
    if(NULL == ms_instance)
    {
        ms_instance = new LocalVideo();
    }

    return ms_instance;
}

LocalVideo::LocalVideo()
{
    m_pFmtContext = NULL;
    m_pAVFrame = NULL;
    m_pAVPacket = NULL;
    m_pYuvFrame = NULL;
    m_pImgConvertCtx = NULL;
    m_pH264Bsfc = NULL;
    m_pAVNALUPacket = NULL;

    m_pYuvData = NULL;
    m_pRGBAData = NULL;

    m_bStopped = true;
    m_bWantStop = false;
    m_bFirstFrame = true;
    m_bCanceled = false;
    m_bStartClipVideo = false;

    m_uEncodeVideoFPS = VV_LOCAL_VIDEO_DEFAULT_FPS;

    m_iSeekedTime = 0;

    m_iStartTimeMs = 0;
    m_iStopTimeMs = 0;
    memset(m_pOutFilename, 0, VV_FILENAME_MAX);

    setDecodeStartTime(0);
    m_bDecodeEnded = false;
    m_bSeekedChanged = false;
    m_bNeedReEncode = false;
    m_bNeedMuxAudioVideo = false;
    m_bHasError = false;
}

LocalVideo::~LocalVideo()
{
    release();

    if(NULL != ms_instance)
    {
        delete ms_instance;
        ms_instance = NULL;
    }

}


bool LocalVideo::prepare(const char *filename)
{
    av_register_all();
    memset(m_pFilename, 0, VV_FILENAME_MAX);
    strcpy(m_pFilename, filename);
    return true;
}

void LocalVideo::start()
{
    if(!m_bStopped){
        LOGE("test_video: LocalVideo::prepare not finished");
        return;
    }

    LOGI("LocalVideo::start");

    m_iStartTimeMs = 0;
    m_iStopTimeMs = 0;
    memset(m_pOutFilename, 0, VV_FILENAME_MAX);

    m_iSeekedTime = 0;

    setDecodeStartTime(0);


    m_bWantStop = false;
    m_bStopped = false;
    m_bFirstFrame = true;
    m_bCanceled = false;
    m_bStartClipVideo = false;
    m_bDecodeEnded = false;
    m_bSeekedChanged = false;
    m_bNeedReEncode = false;
    m_bNeedMuxAudioVideo = false;
    m_bHasError = false;
    int res = pthread_create(&m_ThreadID, NULL, (void* (*)(void*))&LocalVideo::run, this);
    if (0 != res) {
        m_bStopped = true;
        LOGI("pthread_create error.");
    }
}

void LocalVideo::stop()
{
    m_bWantStop = true;
}

void LocalVideo::release()
{
    if(m_pYuvData)
    {
        free(m_pYuvData);
        m_pYuvData = NULL;
    }

    if(m_pRGBAData)
    {
        free(m_pRGBAData);
        m_pRGBAData = NULL;
    }

    if(m_pAVPacket)
    {
        av_free(m_pAVPacket);
        m_pAVPacket = NULL;
    }

    if(m_pAVNALUPacket)
    {
        av_free(m_pAVNALUPacket);
        m_pAVNALUPacket = NULL;
    }

    if(m_pAVFrame)
    {
        av_free(m_pAVFrame);
        m_pAVFrame = NULL;
    }

    if(m_pYuvFrame){
        av_free(m_pYuvFrame);
        m_pYuvFrame = NULL;
    }

    if(m_pImgConvertCtx){
        sws_freeContext(m_pImgConvertCtx);
        m_pImgConvertCtx = NULL;
    }

    if(m_pH264Bsfc){
        av_bitstream_filter_close(m_pH264Bsfc);
        m_pH264Bsfc = NULL;
    }

    if(m_pCodecContext)
    {
        avcodec_close(m_pCodecContext);
        m_pCodecContext = NULL;
    }

    if(m_pFmtContext)
    {
        avformat_close_input(&m_pFmtContext);
        m_pFmtContext = NULL;
    }
}

uint32_t LocalVideo::getVideoWidth()
{
    return m_uVideoWidth;
}

uint32_t LocalVideo::getVideoHeight()
{
    return m_uVideoHeight;
}

pthread_t LocalVideo::getThreadId()
{
    return m_ThreadID;
}


void LocalVideo::setDecodeStartTime(int64_t milliseconds)
{
    m_iDecodeStartTimeMs = milliseconds;
    m_iDecodeStopTimeMs = m_iDecodeStartTimeMs + VV_LOCAL_VIDEO_UPLOAD_MAX_TIME;
}


void LocalVideo::run()
{
    if(!initDecoder())
    {
        release();
        m_bStopped = true;
//        CallbackManager::callback(LOCAL_VIDEO_UPLOAD_ERROR, LOCAL_VIDEO_DEMUX_ERROR);
        m_bHasError = true;
        return;
    }

    return;

    int got_picture;
    int ret;

    int64_t firstPts = -1;
    int64_t thisMilliseconds = -1;
    int waitTime = 0;
    while (!m_bWantStop && !m_bCanceled){
        if(m_bStartClipVideo){
            break;
        }

        if(!m_bStartClipVideo){
            TimeUtil::sleep(40);
            continue;
        }

        if(m_bSeekedChanged){
            onSeekChanged();
        }

        if(m_bDecodeEnded){
            LOGI("decode ended waiting seek or clip ....");
            TimeUtil::sleep(40);
//            ++waitTime;
//            if(waitTime == 10){
//                seek(60000);
//            }else if(waitTime == 20){
//                seek(70000);
//            }
            continue;
        }

        if(ret = av_read_frame(m_pFmtContext, m_pAVPacket) >= 0)
        {
            if(m_pAVPacket->stream_index != m_iVideoIndex){
                continue;
            }

            ret = av_bitstream_filter_filter(m_pH264Bsfc, m_pCodecContext, NULL, &m_pAVNALUPacket->data, &m_pAVNALUPacket->size, m_pAVPacket->data, m_pAVPacket->size, 0);
//            LOGI("decode video av_bitstream_filter_filter ret:%d", ret);

//            if(!isKeyFrame(m_pAVNALUPacket->data, m_pAVNALUPacket->size, 0)){
            if(!isKeyFrame(*m_pAVPacket)){
//                LOGI("decode video not key frame, skip....");
                continue;
            }
//            LOGI("decode video got key frame.... ");

            ret = avcodec_decode_video2(m_pCodecContext, m_pAVFrame, &got_picture, m_pAVPacket);
            if(ret < 0){
                LOGE("decode video failed, ret=%d", ret);

            }

            if(firstPts == -1 && AV_NOPTS_VALUE != m_pAVPacket->pts){
                firstPts = m_pAVPacket->pts;
                setDecodeStartTime(AVUtil::getMillisecondsFromPts(m_pFmtContext->streams[m_iVideoIndex]->time_base, firstPts));
            }

//            uint32_t pts = m_pAVFrame->pts;
//            uint32_t packetPts = m_pAVPacket->pts;
            LOGI("test_video : decode video ret=%d,got_picture=%d, pts:%u, pktPts:%u, timebase:(%d/%d),timebase_stream:(%d/%d) duration:%u", ret, got_picture, (uint32_t)m_pAVFrame->pts, (uint32_t)m_pAVPacket->pts,
                 m_pCodecContext->time_base.num, m_pCodecContext->time_base.den,
                 m_pFmtContext->streams[m_iVideoIndex]->time_base.num, m_pFmtContext->streams[m_iVideoIndex]->time_base.den,
                 (uint32_t)m_pFmtContext->duration);

            if(got_picture)
            {
                onGotYuvData();
            }

            av_free_packet(m_pAVPacket);
            continue;
        }

        LOGI("test_video av_read_frame ret:%d", ret);

        ret = avcodec_decode_video2(m_pCodecContext, m_pAVFrame, &got_picture, m_pAVPacket);
        if (ret < 0 || !got_picture){
            m_bDecodeEnded = true;
            continue;
        }
        onGotYuvData();
        av_free_packet(m_pAVPacket);
    }

    release();

    if(m_bStartClipVideo){
        if(!m_bNeedReEncode){
            internalClipVideo();
        }else{
            internalClipAndEncodeVideo();
        }
    }

    if(!m_bHasError && !m_bCanceled){
//        CallbackManager::callback(LOCAL_VIDEO_UPLOAD_FINISHED);
    }

    if(m_bCanceled){
//        CallbackManager::callback(LOCAL_VIDEO_UPLOAD_CANCELED);
    }

    m_bStopped = true;

    LOGI("test_video Mp4Decoder::run ended, m_bWantStop=%d", m_bWantStop);
}

static void ff_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    LOGI(fmt, vl);
}

bool LocalVideo::initDecoder()
{
    av_log_set_callback(ff_log_callback);
    m_pFmtContext = avformat_alloc_context();
    if(!m_pFmtContext)
    {
        LOGE("avformat_alloc_context failed");
        return false;
    }

    if(avformat_open_input(&m_pFmtContext, m_pFilename, NULL, NULL) != 0)
    {
        LOGE("Couldn't open input stream.");
        return false;
    }

    m_pFmtContext->probesize = m_pFmtContext->probesize * 20;

    LOGI("m_pFmtContext:probesize:%lld", m_pFmtContext->probesize);

    if(avformat_find_stream_info(m_pFmtContext, NULL) < 0){
        LOGE("Couldn't find stream information");
        return false;
    }

    int videoindex=-1;
    for(int i=0; i<m_pFmtContext->nb_streams; i++)
    {
        if(m_pFmtContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoindex=i;
            break;
        }
    }

    if(videoindex == -1){
        LOGE("Didn't find a video stream");
        return false;
    }

    m_iVideoIndex = videoindex;

    int audioIndex = -1;
    for(int i=0; i<m_pFmtContext->nb_streams; i++)
    {
        if(m_pFmtContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioIndex = i;
            break;
        }
    }

    if(audioIndex == -1){
        LOGW("Didn't find a audio stream");
//        return false;
    }

    m_iAudioIndex = audioIndex;

    m_bNeedMuxAudioVideo = m_iAudioIndex != -1;

    m_pCodecContext = m_pFmtContext->streams[videoindex]->codec;
    LOGI("pf:%d", m_pCodecContext->pix_fmt);
    AVCodec* pCodec = avcodec_find_decoder(m_pCodecContext->codec_id);

    if(pCodec == NULL){
        LOGE("Codec not found.");
        return false;
    }


    if(avcodec_open2(m_pCodecContext, pCodec, NULL)<0){
        LOGE("Could not open codec.");
        return false;
    }

    if(m_pCodecContext->pix_fmt == AV_PIX_FMT_NONE && m_pCodecContext->codec_id == AV_CODEC_ID_H264){
        m_pCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
        LOGE("h264 video pixel fmt AV_PIX_FMT_NONE, change to yuv420p .");
    }

    m_pAVFrame = av_frame_alloc();
    if(!m_pAVFrame){
        LOGE("av_frame_alloc failed");
        return false;
    }

    m_pYuvFrame = av_frame_alloc();
    if(!m_pYuvFrame){
        LOGE("av_frame_alloc failed");
        return false;
    }

    m_pAVPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
    if(!m_pAVPacket){
        LOGE("malloc m_pAVPacket failed");
        return false;
    }

    av_init_packet(m_pAVPacket);

    m_pAVNALUPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
    if(!m_pAVNALUPacket){
        LOGE("malloc m_pAVNALUPacket failed");
        return false;
    }

    av_init_packet(m_pAVNALUPacket);

    m_uVideoWidth = m_pCodecContext->width;
    m_uVideoHeight = m_pCodecContext->height;
    m_uYLen = m_uVideoWidth * m_uVideoHeight;

//    LOGI("test_video : w:%u,h:%u", m_uVideoWidth, m_uVideoHeight);

//    m_uTextureWidth = CHROMAKEY_VIDEO_WIDTH;
//    m_uTextureHeight = CHROMAKEY_VIDEO_HEIGHT;
    m_uTextureWidth = m_uVideoWidth;
    m_uTextureHeight = m_uVideoHeight;
    m_uTextureYLen = m_uTextureWidth*m_uTextureHeight;

    m_bNeedReEncode = m_uVideoWidth > m_uVideoHeight
                      ? m_uVideoWidth > VV_LOCAL_VIDEO_MAX_WH
                      : m_uVideoHeight > VV_LOCAL_VIDEO_MAX_WH;


    if(m_bNeedReEncode){
        if(m_uVideoWidth > m_uVideoHeight){
            m_uEncodeVideoWidth = VV_LOCAL_VIDEO_MAX_WH;
            m_uEncodeVideoHeight = 1.0 * m_uEncodeVideoWidth * m_uVideoHeight/m_uVideoWidth;
        }else{
            m_uEncodeVideoHeight = VV_LOCAL_VIDEO_MAX_WH;
            m_uEncodeVideoWidth = 1.0 * m_uEncodeVideoHeight * m_uVideoWidth/m_uVideoHeight;
        }

        m_uEncodeVideoWidth = m_uEncodeVideoWidth + m_uEncodeVideoWidth % 2;
        m_uEncodeVideoHeight = m_uEncodeVideoHeight + m_uEncodeVideoHeight % 2;

        m_uEncodeVideoFPS = m_pCodecContext->time_base.num != 0 ?  m_pCodecContext->time_base.den / m_pCodecContext->time_base.num : VV_LOCAL_VIDEO_DEFAULT_FPS;
    }


    m_pYuvData = (uint8_t *) malloc(m_uTextureYLen * 3 / 2);
    if(!m_pYuvData){
        LOGE("malloc avdata failed");
        return false;
    }

    // 这里由于libyuv转换i420->rgba uv倒置，所以在这里先倒置一下
    m_pYuvFrame->data[0] = m_pYuvData;
    m_pYuvFrame->data[1] = m_pYuvData + m_uTextureYLen + m_uTextureYLen / 4;
    m_pYuvFrame->data[2] = m_pYuvData + m_uTextureYLen;

    m_pYuvFrame->linesize[0] = m_uTextureWidth;
    m_pYuvFrame->linesize[1] = m_uTextureWidth / 2;
    m_pYuvFrame->linesize[2] = m_uTextureWidth / 2;

    m_pRGBAData = (uint8_t *) malloc(m_uTextureYLen * 4);
    if(!m_pRGBAData){
        LOGE("malloc avdata failed");
        return false;
    }

    LOGI("initDecoder sws_getContexting pw:%d,ph:%d,pf:%d,tw:%u,th:%u,tf:%d", m_pCodecContext->width, m_pCodecContext->height, m_pCodecContext->pix_fmt,
         m_uTextureWidth, m_uTextureHeight, AV_PIX_FMT_YUV420P);
    m_pImgConvertCtx = sws_getContext(m_pCodecContext->width, m_pCodecContext->height, m_pCodecContext->pix_fmt,
                                      m_uTextureWidth, m_uTextureHeight, AV_PIX_FMT_YUV420P,
                                      SWS_BICUBIC, NULL, NULL, NULL);

    LOGI("initDecoder sws_getContexted ");
    if(!m_pImgConvertCtx){
        LOGE("sws_getContext failed");
        return false;
    }

    m_pH264Bsfc = av_bitstream_filter_init("h264_mp4toannexb");
    if(!m_pH264Bsfc){
        LOGE("av_bitstream_filter_init failed");
        return false;
    }

    m_pVideoMetaData = NULL;

    LOGI("initDecoder success ");

    return true;
}

void LocalVideo::onGotYuvData()
{
    if(!m_pAVFrame->key_frame){
        return;
    }

    int64_t pts = m_pAVFrame->pkt_pts;
    int64_t ms = AVUtil::getMillisecondsFromPts(m_pFmtContext->streams[m_iVideoIndex]->time_base, pts);
    int64_t getedPts = AVUtil::getPtsFromMilliseconds(m_pFmtContext->streams[m_iVideoIndex]->time_base, ms);
//


    LOGI("onGetYuvFrame pts:%u, ms:%u, getPts:%u", (uint32_t)pts, (uint32_t)ms, (uint32_t)getedPts);
    sws_scale(m_pImgConvertCtx, (const uint8_t* const*)m_pAVFrame->data, m_pAVFrame->linesize, 0, m_pCodecContext->height, m_pYuvFrame->data, m_pYuvFrame->linesize);
//    YuvHelper::getInstance()->i420toRGBA(m_pYuvData, m_pRGBAData, m_uTextureWidth, m_uTextureHeight);

    savePicture(ms);

    int64_t thisMilliseconds = AVUtil::getMillisecondsFromPts(m_pFmtContext->streams[m_iVideoIndex]->time_base, pts);

    if(thisMilliseconds > m_iDecodeStopTimeMs){
        m_bDecodeEnded = true;
    }
}


void LocalVideo::savePicture()
{
    static int index = 0;

    if(index > 10){
//        clipVideo(56000, 90000, "/sdcard/vvlive/localvideo/clip.mkv");
//        clipVideo(60000, 90000, "/sdcard/vvlive/localvideo/clip.mkv");
//        clipVideo(60000, 90000, "/sdcard/vvlive/localvideo/clip.mp4");
//        clipVideo(60000, 90000, "/sdcard/vvlive/localvideo/clip.flv");
//        clipVideo(60000, 90000, "/sdcard/vvlive/localvideo/clip_no_audio.flv");
    }

    uint8_t* rgb = (uint8_t *)malloc(m_uTextureWidth*m_uTextureHeight*3);

    int n=0;
    for (int i = 0; i < m_uTextureWidth*m_uTextureHeight*4; ++i) {
        if (i % 4 != 3){
            rgb[n++] = m_pRGBAData[i];
        }
    }

    char filename[128];
    sprintf(filename, "/sdcard/vvlive/localvideo/image/image_%d.ppm", index++);

    FILE* pFile = fopen(filename, "w");
    fprintf(pFile, "P6\n%d %d\n255\n", m_uTextureWidth, m_uTextureHeight);

    fwrite(rgb, 1, m_uTextureWidth * m_uTextureHeight * 3, pFile);
    fclose(pFile);
    free(rgb);
}


void LocalVideo::savePicture(int64_t ms)
{
//    clipVideo(60000, 90000, "/sdcard/vvlive/localvideo/clip.mp4");
//    clipVideo(60000, 90000, "/sdcard/vvlive/localvideo/clip.mkv");
//    clipVideo(60000, 90000, "/sdcard/vvlive/localvideo/clip.mov");
//    clipVideo(60000, 90000, "/sdcard/vvlive/localvideo/clip.flv");

    char filename[128];
    sprintf(filename, "/sdcard/vvlive/localvideo/image/image_%lld.ppm", ms);

    if(access(filename, F_OK) == 0){
        LOGI("file %s is exits, ignore", filename);
        return;
    }


    uint8_t* rgb = (uint8_t *)malloc(m_uTextureWidth*m_uTextureHeight*3);

    int n=0;
    for (int i = 0; i < m_uTextureWidth*m_uTextureHeight*4; ++i) {
        if (i % 4 != 3){
            rgb[n++] = m_pRGBAData[i];
        }
    }


    FILE* pFile = fopen(filename, "w");
    fprintf(pFile, "P6\n%d %d\n255\n", m_uTextureWidth, m_uTextureHeight);

    fwrite(rgb, 1, m_uTextureWidth * m_uTextureHeight * 3, pFile);
    fclose(pFile);
    free(rgb);
}


void LocalVideo::cancel()
{
    m_bCanceled = true;
}


bool LocalVideo::isKeyFrame(AVPacket packet)
{
//    packet.flags & AV_PKT_FLAG_KEY;
    return packet.flags & AV_PKT_FLAG_KEY;
}


bool LocalVideo::isKeyFrame(uint8_t* data, int size, int startPos)
{
    if(!data){
        return false;
    }

    if(startPos < 0 || startPos + 4 >= size){
        return false;
    }

    int idx = startPos + 2;

    uint8_t nalu = data[idx] == 0x01 ? data[idx + 1] : data[idx + 2];
    uint8_t nalu_type = nalu & 0x1F;

//    LOGI("decode_video_nalu :%02x, type:%02x, startPos:%d : (%02x:%02x:%02x:%02x:%02x)", nalu, nalu_type, startPos,
//         data[startPos], data[startPos+1],data[startPos+2],data[startPos+3], data[startPos + 4]);

    if(NALU_NON_IDR == nalu_type){
        return false;
    }

    if(NALU_IDR == nalu_type){
        return true;
    }

    if(NALU_SEI == nalu_type || NALU_SPS == nalu_type || NALU_PPS == nalu_type){
        int pos = findNALUStartCode(data, size, startPos + 4);
        if(pos > 0){
            return isKeyFrame(data, size, pos);
        }
        else{
            return false;
        }
    }

    return false;
}


int LocalVideo::findNALUStartCode(uint8_t* data, int size, int startPos)
{

    if(!data){
        return -1;
    }

    if(startPos < 0 || startPos + 4 >= size){
        return -1;
    }

    int idx = startPos;
    int start_cdoe_status = 0;
    bool findStartCode = false;
    for (int i= startPos; i < size; ++i)
    {
        if(0 == start_cdoe_status){
            if(0x00 == data[i]){
                start_cdoe_status++;
            }
        }else if(1 == start_cdoe_status){
            if(0x00 == data[i]){
                start_cdoe_status++;
            }else{
                start_cdoe_status = 0;
            }
        }else if(2 == start_cdoe_status){
            if(0x01 == data[i]){
                findStartCode = true;
                idx = i - 2;
                break;
            }else if (0x00 == data[i]){
                start_cdoe_status++;
            }else{
                start_cdoe_status = 0;
            }
        }else if(3 == start_cdoe_status){
            if(0x01 == data[i]){
                findStartCode = true;
                idx = i - 3;
                break;
            }else if(0x00 == data[i]){
                // 对于h264流来说 0x00000000是不会存在的 ????? 我不清楚
                // 0x0000将会被扩展为0x000003
                // 参见http://blog.csdn.net/wudebao5220150/article/details/13810671
                continue;
            }
            else{
                start_cdoe_status = 0;
            }
        }
    }

    if(findStartCode)
    {
        return idx;
    }

    return -1;
}

void LocalVideo::clipVideo(int64_t startTime, int64_t stopTime, const char* outFilename)
{
    m_iStartTimeMs = startTime;
    m_iStopTimeMs = stopTime;
    memset(m_pOutFilename, 0, VV_FILENAME_MAX);
    strcpy(m_pOutFilename, outFilename);
    m_bStartClipVideo  = true;

    LOGI("clip video : start_time:%lld,stoptime:%lld", m_iStartTimeMs, m_iStopTimeMs);
}


void LocalVideo::seek(int64_t time)
{
    if(m_iSeekedTime != time){
        m_iSeekedTime = time;
        m_bSeekedChanged = true;
    }
}


void LocalVideo::internalClipVideo()
{
    // 输出文件format
    AVOutputFormat *ofmt = NULL;

    // 输入视频文件上下文
    AVFormatContext *iFmtCtx = NULL;

    // 输入文件上下文
    AVFormatContext *oFmtCtx = NULL;

    // AAC in some container format (FLV, MP4, MKV etc.) need "aac_adtstoasc" bitstream filter (BSF)
    AVBitStreamFilterContext* pAacbsfc = NULL;

    AVPacket pkt;

    // 输入视频文件视频流索引
    int videoIdxIn = -1;
    // 输出视频文件视频流索引
    int videoIdxOut = -1;

    // 输入音频文件音频流索引
    int audioIdxIn = -1;
    // 输出视频文件音频流索引av_log_callback
    int audioIdxOut = -1;

    // 帧索引
    int frameIdx = 0;

    // 当前视频的pts
    int64_t iCurVideoPts = 0;
    // 当前音频的pts
    int64_t iCurAudioPts = 0;

    try{

        av_register_all();
        //打开输入文件
        if (avformat_open_input(&iFmtCtx, m_pFilename, 0, 0) < 0) {
            LOGE("open input video file failed.");
            throw 1;
        }
        if (avformat_find_stream_info(iFmtCtx, 0) < 0) {
            LOGE("find input video stream info failed.");
            throw 1;
        }



        av_dump_format(iFmtCtx, 0, m_pFilename, 0);

        //申请输出文件内存
        avformat_alloc_output_context2(&oFmtCtx, NULL, NULL, m_pOutFilename);
        if (!oFmtCtx) {
            LOGE("alloc output context failed.");
            throw 1;
        }
        ofmt = oFmtCtx->oformat;

        // 找到输入视频的视频流索引，并拷贝编码上下文到输出文件视频流
        for (int i = 0; i < iFmtCtx->nb_streams; i++) {
            //Create output AVStream according to input AVStream
            if(iFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
                AVStream *in_stream = iFmtCtx->streams[i];
                AVStream *out_stream = avformat_new_stream(oFmtCtx, in_stream->codec->codec);
                if (!out_stream) {
                    LOGE("alloc output video stream failed.");
                    throw 1;
                }

                videoIdxIn = i;
                videoIdxOut = out_stream->index;

                //Copy the settings of AVCodecContext
                if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                    LOGE("copy video context failed.");
                    throw 1;
                }

                out_stream->codec->codec_tag = 0;
                if (oFmtCtx->oformat->flags & AVFMT_GLOBALHEADER){
                    out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
                }

                break;
            }
        }

        // 找到输入音频的音频流索引，并拷贝编码上下文到输出文件音频流
        for (int i = 0; i < iFmtCtx->nb_streams; i++) {
            //Create output AVStream according to input AVStream
            if(iFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
                AVStream *in_stream = iFmtCtx->streams[i];
                AVStream *out_stream = avformat_new_stream(oFmtCtx, in_stream->codec->codec);
                if (!out_stream) {
                    LOGE("alloc output audio stream failed.");
                    throw 1;
                }

                audioIdxIn = i;
                audioIdxOut = out_stream->index;

                //Copy the settings of AVCodecContext
                if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                    LOGE("copy audio context failed.");
                    throw 1;
                }

                out_stream->codec->codec_tag = 0;
                if (oFmtCtx->oformat->flags & AVFMT_GLOBALHEADER){
                    out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
                }

                break;
            }
        }

        av_dump_format(oFmtCtx, 0, m_pOutFilename, 1);

        //打开输出文件
        if (!(ofmt->flags & AVFMT_NOFILE)) {
            if (avio_open(&oFmtCtx->pb, m_pOutFilename, AVIO_FLAG_WRITE) < 0) {
                LOGE("avio_open failed.");
                throw 1;
            }
        }

        //Write file header
        if (avformat_write_header(oFmtCtx, NULL) < 0) {
            LOGE("avformat_write_header failed.");
            throw 1;
        }

        int64_t startPts_v = AVUtil::getPtsFromMilliseconds(iFmtCtx->streams[videoIdxIn]->time_base, m_iStartTimeMs);
        int64_t stopPts_v = AVUtil::getPtsFromMilliseconds(iFmtCtx->streams[videoIdxIn]->time_base, m_iStopTimeMs);

        int64_t startPts_a = audioIdxIn == -1 ? 0 : AVUtil::getPtsFromMilliseconds(iFmtCtx->streams[audioIdxIn]->time_base, m_iStartTimeMs);
        int64_t stopPts_a = audioIdxIn == -1 ? 0 : AVUtil::getPtsFromMilliseconds(iFmtCtx->streams[audioIdxIn]->time_base, m_iStopTimeMs);


        avformat_seek_file(iFmtCtx, videoIdxIn, INT64_MIN, startPts_v, INT64_MAX, 0);

        int64_t startPts = 0;
        int64_t stopPts = 0;

        int64_t lastPts_a = AV_NOPTS_VALUE;
        int64_t lastPts_v = AV_NOPTS_VALUE;
        int64_t lastPts = 0;

        int64_t firstVideoPts = AV_NOPTS_VALUE;
        int64_t firstAudioPts = AV_NOPTS_VALUE;

        int64_t startDts = 0;
        int64_t startDts_a = AV_NOPTS_VALUE;
        int64_t startDts_v = AV_NOPTS_VALUE;

        int64_t lastDts = 0;
        int64_t lastDts_a = AV_NOPTS_VALUE;
        int64_t lastDts_v = AV_NOPTS_VALUE;


        bool isFirstVideoFrame = true;
        bool isFirstAudioFrame = true;
        bool isAudioFrame = false;
        while (!m_bCanceled) {
            AVFormatContext *ifmt_ctx;
            int stream_index=0;
            AVStream *in_stream, *out_stream;

            if(av_read_frame(iFmtCtx, &pkt) < 0){
                LOGI("av_read_frame ended");
                break;
            }

            isAudioFrame = pkt.stream_index == audioIdxIn;

            in_stream = iFmtCtx->streams[pkt.stream_index];
            out_stream = isAudioFrame ? oFmtCtx->streams[audioIdxOut] : oFmtCtx->streams[videoIdxOut];
            stream_index = isAudioFrame ? audioIdxOut : videoIdxOut;
            startPts = isAudioFrame ? firstAudioPts : firstVideoPts;
            stopPts = isAudioFrame ? stopPts_a : stopPts_v;
            lastPts = isAudioFrame ? lastPts_a : lastPts_v;
            lastDts = isAudioFrame ? lastDts_a : lastDts_v;
            startDts = isAudioFrame ? startDts_a : startDts_v;

            if(isFirstAudioFrame && isAudioFrame){
                isFirstAudioFrame = false;
                av_dict_copy(&out_stream->metadata, in_stream->metadata, 0);
            }

            if(isFirstVideoFrame && pkt.stream_index == videoIdxIn){
                isFirstVideoFrame = false;
                av_dict_copy(&out_stream->metadata, in_stream->metadata, 0);
            }


            // todo  这里应该减去一个音视频的时间差，现在这么处理有可能会出现音视频不同步的现象
            if(startPts == AV_NOPTS_VALUE){
                startPts = pkt.pts;
                if( isAudioFrame){
                    firstAudioPts = startPts;
                }else{
                    firstVideoPts = startPts;
                }
            }

            if(pkt.dts != AV_NOPTS_VALUE && startDts == AV_NOPTS_VALUE){
                if(isAudioFrame){
                    startDts_a = pkt.dts;
                }else{
                    startDts_v = pkt.dts;
                }

                startDts = pkt.dts;
            }


            if(pkt.pts > stopPts){
                LOGI("pts stoped : pkt pts:%lld,stopPts:%lld,a_start:%lld,a_stop:%lld,v_start:%lld,v_stop:%lld",
                    pkt.pts, stopPts, startPts_a, stopPts_a, startPts_v, stopPts_v);
                break;
            }

//            LOGI("read_frame : pts:%lld,dts:%lld,start:%lld,stop:%lld,idx:%d,start_dts:%lld", pkt.pts, pkt.dts, startPts, stopPts, stream_index, startDts);

            if(pkt.pts != AV_NOPTS_VALUE && startPts > 0){
                pkt.pts -= startPts;

                if(pkt.dts != AV_NOPTS_VALUE && startDts > 0){
                    pkt.dts -= startDts;
                }
            }

            //Convert PTS/DTS
            pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,(AVRounding) (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
            pkt.pos = -1;
            pkt.stream_index = stream_index;

            if(pkt.dts != AV_NOPTS_VALUE)
            {
                pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding) (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            }


            if(lastDts != AV_NOPTS_VALUE && pkt.dts != AV_NOPTS_VALUE && pkt.dts <= lastDts){
                if(pkt.pts == pkt.dts)
                {
                    LOGW("add stream pts from %lld to %lld, stream_index=%d,lastdts:%lld,last_a:%lld,last_v:%lld,a_i:%d", pkt.pts, lastDts + 1, stream_index, lastDts, lastDts_a, lastDts_v, audioIdxIn);
                    pkt.pts = lastDts + 1;
                }

                LOGW("add stream dts from %lld to %lld, stream_index=%d,lastdts:%lld,last_a:%lld,last_v:%lld", pkt.dts, lastDts + 1, stream_index, lastDts, lastDts_a, lastDts_v);
                pkt.dts = lastDts + 1;
            }

            if(lastPts != AV_NOPTS_VALUE && pkt.pts != AV_NOPTS_VALUE && pkt.pts == lastPts){
                LOGW("add stream pts from %lld to %lld, stream_index=%d", pkt.pts, lastPts + 1, stream_index);
                pkt.pts = lastPts + 1;
            }

            if(pkt.pts < pkt.dts){
                LOGW("frame pts=%lld < dts=%lld , add pts to %lld,stream_index=%d", pkt.pts, pkt.dts, pkt.dts, stream_index);
                pkt.pts = pkt.dts;
            }

            if(isAudioFrame){
                lastPts_a = pkt.pts;
            }else{
                lastPts_v = pkt.pts;
            }

            if(pkt.dts != AV_NOPTS_VALUE){
                if(isAudioFrame){
                    lastDts_a = pkt.dts;
                }else{
                    lastDts_v = pkt.dts;
                }
            }


            LOGI("Write 1 Packet. size:%5d,pts:%lld,dts:%lld,sindex:%d,duration:%lld,in_t:(%d/%d),out:(%d/%d)",pkt.size,pkt.pts, pkt.dts, stream_index, pkt.duration,
                 in_stream->time_base.num, in_stream->time_base.den, out_stream->time_base.num, out_stream->time_base.den);

            //Write
            // 这里用av_interleaved_write_frame回报错，用av_write_frame则没问题 在这里就用av_write_frame
            if (av_interleaved_write_frame(oFmtCtx, &pkt) < 0) {
//            if (av_write_frame(oFmtCtx, &pkt) < 0) {
                char buf[1024];
                av_strerror(errno, buf, 1024);
                LOGE("av_interleaved_write_frame failed.err=%d,%s", errno, buf);
                throw 1;
            }
            av_free_packet(&pkt);

        }
        //Write file trailer
        av_write_trailer(oFmtCtx);
    }
    catch(...)
    {
        if(NULL != pAacbsfc){
            av_bitstream_filter_close(pAacbsfc);
            pAacbsfc = NULL;
        }

        avformat_close_input(&iFmtCtx);
        /* close output */
        if (oFmtCtx && !(ofmt->flags & AVFMT_NOFILE)){
            avio_close(oFmtCtx->pb);
        }

        avformat_free_context(oFmtCtx);
//        m_bMuxStopped = true;

//        CallbackManager::callback(LOCAL_VIDEO_UPLOAD_ERROR, LOCAL_VIDEO_MUX_ERROR);
        m_bHasError = true;
        return;
    }

    if(NULL != pAacbsfc){
        av_bitstream_filter_close(pAacbsfc);
        pAacbsfc = NULL;
    }

    avformat_close_input(&iFmtCtx);

    /* close output */
    if (oFmtCtx && !(ofmt->flags & AVFMT_NOFILE)){
        avio_close(oFmtCtx->pb);
    }
    avformat_free_context(oFmtCtx);

    return;
}


void LocalVideo::onSeekChanged()
{
    if(m_iSeekedTime > m_iDecodeStopTimeMs){
        // clear decode cache
        int ret;
        int got_picture = 0;
        while (true){
            ret = avcodec_decode_video2(m_pCodecContext, m_pAVFrame, &got_picture, m_pAVPacket);
            if (ret < 0 || !got_picture){
                break;
            }
            LOGI("clear cache pts:%lld", m_pAVFrame->pkt_pts);
            av_free_packet(m_pAVPacket);
        }
    }

    setDecodeStartTime(m_iSeekedTime);

    int64_t startPts_v = AVUtil::getPtsFromMilliseconds(m_pFmtContext->streams[m_iVideoIndex]->time_base, m_iSeekedTime);
    avformat_seek_file(m_pFmtContext, m_iVideoIndex, INT64_MIN, startPts_v, INT64_MAX, 0);

    m_bDecodeEnded = false;
    m_bSeekedChanged = false;
}


void LocalVideo::initTempFile()
{

    int len = strlen(m_pOutFilename);

    int delimiterPos = -1;
    for(int i = len; i > 0; --i){
        if(m_pOutFilename[i] == FILE_DELIMITER){
            delimiterPos = i;
            break;
        }
    }

    if(delimiterPos < 0){
        return;
    }

    char header[VV_FILENAME_MAX];
    char tail[VV_FILENAME_MAX];

    memset(header, 0, VV_FILENAME_MAX);
    memset(tail, 0, VV_FILENAME_MAX);

    strncpy(header, m_pOutFilename, delimiterPos);
    strcpy(tail, m_pOutFilename + delimiterPos);


    memset(m_pTempVideoFilename, 0, VV_FILENAME_MAX);
    memset(m_pTempAudioFilename, 0, VV_FILENAME_MAX);

    sprintf(m_pTempVideoFilename, "%s_tempvideo%s", header, tail);
    sprintf(m_pTempAudioFilename, "%s_tempaudio%s", header, tail);
}


void LocalVideo::clearTempFile()
{
    unlink(m_pTempAudioFilename);
    unlink(m_pTempVideoFilename);
}


void LocalVideo::internalClipAndEncodeVideo()
{
    // 输出文件format
    AVOutputFormat *ofmt = NULL;

    // 输入视频文件上下文
    AVFormatContext *iFmtCtx = NULL;

    // 输入文件上下文
    AVFormatContext *oFmtCtx = NULL;

    // AAC in some container format (FLV, MP4, MKV etc.) need "aac_adtstoasc" bitstream filter (BSF)
    AVBitStreamFilterContext* pAacbsfc = NULL;

    AVPacket pkt;
    AVFrame* pFrame = NULL;
    AVFrame* pFrameYUV = NULL;
    uint8_t* pFrameYUVData = NULL;

    AVCodecContext* pCodecContext = NULL;

    SwsContext* pImgConvertCtx = NULL;

    // 输入视频文件视频流索引
    int videoIdxIn = -1;
    // 输出视频文件视频流索引
    int videoIdxOut = -1;

    // 输入音频文件音频流索引
    int audioIdxIn = -1;
    // 输出视频文件音频流索引av_log_callback
    int audioIdxOut = -1;

    // 帧索引
    int frameIdx = 0;

    // 当前视频的pts
    int64_t iCurVideoPts = 0;
    // 当前音频的pts
    int64_t iCurAudioPts = 0;

    uint32_t yuvDataLen = 0;

    m_bReEncodeDecodeEnded = false;

    try{

        initTempFile();

        av_register_all();
        //打开输入文件
        if (avformat_open_input(&iFmtCtx, m_pFilename, 0, 0) < 0) {
            LOGE("open input video file failed.");
            throw 1;
        }
        if (avformat_find_stream_info(iFmtCtx, 0) < 0) {
            LOGE("find input video stream info failed.");
            throw 1;
        }



        av_dump_format(iFmtCtx, 0, m_pFilename, 0);

        //申请输出文件内存
        avformat_alloc_output_context2(&oFmtCtx, NULL, NULL, m_pTempAudioFilename);
        if (!oFmtCtx) {
            LOGE("alloc output context failed.");
            throw 1;
        }
        ofmt = oFmtCtx->oformat;

        // 找到输入视频的视频流索引，并拷贝编码上下文到输出文件视频流
        for (int i = 0; i < iFmtCtx->nb_streams; i++) {
            //Create output AVStream according to input AVStream
            if(iFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
                AVStream *in_stream = iFmtCtx->streams[i];
                AVStream *out_stream = avformat_new_stream(oFmtCtx, in_stream->codec->codec);
                if (!out_stream) {
                    LOGE("alloc output video stream failed.");
                    throw 1;
                }

                videoIdxIn = i;
                videoIdxOut = out_stream->index;

                //Copy the settings of AVCodecContext
                if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                    LOGE("copy video context failed.");
                    throw 1;
                }

                out_stream->codec->codec_tag = 0;
                if (oFmtCtx->oformat->flags & AVFMT_GLOBALHEADER){
                    out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
                }

                break;
            }
        }

        // 找到输入音频的音频流索引，并拷贝编码上下文到输出文件音频流
        for (int i = 0; i < iFmtCtx->nb_streams; i++) {
            //Create output AVStream according to input AVStream
            if(iFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
                AVStream *in_stream = iFmtCtx->streams[i];
                AVStream *out_stream = avformat_new_stream(oFmtCtx, in_stream->codec->codec);
                if (!out_stream) {
                    LOGE("alloc output audio stream failed.");
                    throw 1;
                }

                audioIdxIn = i;
                audioIdxOut = out_stream->index;

                //Copy the settings of AVCodecContext
                if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                    LOGE("copy audio context failed.");
                    throw 1;
                }

                out_stream->codec->codec_tag = 0;
                if (oFmtCtx->oformat->flags & AVFMT_GLOBALHEADER){
                    out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
                }

                break;
            }
        }

        av_dump_format(oFmtCtx, 0, m_pTempAudioFilename, 1);

        //打开输出文件
        if (!(ofmt->flags & AVFMT_NOFILE)) {
            if (avio_open(&oFmtCtx->pb, m_pTempAudioFilename, AVIO_FLAG_WRITE) < 0) {
                LOGE("avio_open failed.");
                throw 1;
            }
        }

        //Write file header
        if (avformat_write_header(oFmtCtx, NULL) < 0) {
            LOGE("avformat_write_header failed.");
            throw 1;
        }

        pFrame = av_frame_alloc();
        if(!pFrame){
            LOGE("av_frame_alloc failed");
            throw 1;
        }

        pCodecContext = iFmtCtx->streams[videoIdxIn]->codec;

        if(pCodecContext->pix_fmt == AV_PIX_FMT_NONE && pCodecContext->codec_id == AV_CODEC_ID_H264){
            pCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
//            LOGE("h264 video pixel fmt AV_PIX_FMT_NONE, change to yuv420p .");
        }

        AVCodec* pCodec = avcodec_find_decoder(pCodecContext->codec_id);

        if(pCodec == NULL){
            LOGE("Codec not found.");
            throw 1;
        }

        if(avcodec_open2(pCodecContext, pCodec, NULL)<0){
            LOGE("Could not open codec.");
            throw 1;
        }

        pFrameYUV = av_frame_alloc();
        if(!pFrameYUV){
            LOGE("av_frame_alloc failed");
            throw 1;
        }

        yuvDataLen = m_uEncodeVideoWidth * m_uEncodeVideoHeight * 3 / 2;

        pFrameYUVData = (uint8_t *) malloc(yuvDataLen);
        if(!pFrameYUVData){
            LOGE("malloc frame data failed ");
            throw 1;
        }

        pFrameYUV->data[0] = pFrameYUVData;
        pFrameYUV->data[1] = pFrameYUVData + m_uEncodeVideoWidth * m_uEncodeVideoHeight;
        pFrameYUV->data[2] = pFrameYUVData + m_uEncodeVideoWidth * m_uEncodeVideoHeight * 5 / 4;


        pFrameYUV->linesize[0] = m_uEncodeVideoWidth;
        pFrameYUV->linesize[1] = m_uEncodeVideoWidth / 2;
        pFrameYUV->linesize[2] = m_uEncodeVideoWidth / 2;

//        LOGI("sws_getContext pImgConvertCtxing,ow:%u,oh:%u,ew:%u,eh:%u, of:%d", m_uVideoWidth, m_uVideoHeight, m_uEncodeVideoWidth, m_uEncodeVideoHeight, pCodecContext->pix_fmt);
        pImgConvertCtx = sws_getContext(m_uVideoWidth, m_uVideoHeight, pCodecContext->pix_fmt,
                                          m_uEncodeVideoWidth, m_uEncodeVideoHeight, AV_PIX_FMT_YUV420P,
                                          SWS_BICUBIC, NULL, NULL, NULL);

//        LOGI("sws_getContext pImgConvertCtx success,ow:%u,oh:%u,ew:%u,eh:%u", m_uVideoWidth, m_uVideoHeight, m_uEncodeVideoWidth, m_uEncodeVideoHeight);


        // create encode thread
        pthread_create(&m_encodeThreadId, NULL, (void* (*)(void*))&LocalVideo::encodeThread, this);


        int64_t startPts_v = AVUtil::getPtsFromMilliseconds(iFmtCtx->streams[videoIdxIn]->time_base, m_iStartTimeMs);
        int64_t stopPts_v = AVUtil::getPtsFromMilliseconds(iFmtCtx->streams[videoIdxIn]->time_base, m_iStopTimeMs);

        int64_t startPts_a = audioIdxIn == -1 ? 0 : AVUtil::getPtsFromMilliseconds(iFmtCtx->streams[audioIdxIn]->time_base, m_iStartTimeMs);
        int64_t stopPts_a = audioIdxIn == -1 ? 0 : AVUtil::getPtsFromMilliseconds(iFmtCtx->streams[audioIdxIn]->time_base, m_iStopTimeMs);


        avformat_seek_file(iFmtCtx, videoIdxIn, INT64_MIN, startPts_v, INT64_MAX, 0);

        int64_t startPts = 0;
        int64_t stopPts = 0;

        int64_t lastPts_a = AV_NOPTS_VALUE;
        int64_t lastPts_v = AV_NOPTS_VALUE;
        int64_t lastPts = 0;

        int64_t firstVideoPts = AV_NOPTS_VALUE;
        int64_t firstAudioPts = AV_NOPTS_VALUE;

        int64_t startDts = 0;
        int64_t startDts_a = AV_NOPTS_VALUE;
        int64_t startDts_v = AV_NOPTS_VALUE;

        int64_t lastDts = 0;
        int64_t lastDts_a = AV_NOPTS_VALUE;
        int64_t lastDts_v = AV_NOPTS_VALUE;


        int ret;
        int got_picture = 0;

        bool isFirstVideoFrame = true;
        bool isFirstAudioFrame = true;
        bool isAudioFrame = false;

        bool audioEnded = false;
        bool videoEnded = false;

        while (!m_bCanceled) {
            AVFormatContext *ifmt_ctx;
            int stream_index=0;
            AVStream *in_stream, *out_stream;

            if(av_read_frame(iFmtCtx, &pkt) < 0){
                LOGI("av_read_frame ended");
                break;
            }

            isAudioFrame = pkt.stream_index == audioIdxIn;
            out_stream = isAudioFrame ? oFmtCtx->streams[audioIdxOut] : oFmtCtx->streams[videoIdxOut];
            stream_index = isAudioFrame ? audioIdxOut : videoIdxOut;
            startPts = isAudioFrame ? firstAudioPts : firstVideoPts;
            stopPts = isAudioFrame ? stopPts_a : stopPts_v;
            lastPts = isAudioFrame ? lastPts_a : lastPts_v;
            lastDts = isAudioFrame ? lastDts_a : lastDts_v;
            startDts = isAudioFrame ? startDts_a : startDts_v;
            LOGI("read_frame : pts:%lld,dts:%lld,start:%lld,stop:%lld,idx:%d,start_dts:%lld", pkt.pts, pkt.dts, startPts, stopPts, stream_index, startDts);



            // todo  这里应该减去一个音视频的时间差，现在这么处理有可能会出现音视频不同步的现象
            if(startPts == AV_NOPTS_VALUE){
                startPts = pkt.pts;
                if( isAudioFrame){
                    firstAudioPts = startPts;
                }else{
                    firstVideoPts = startPts;
                }
            }

            if(pkt.dts != AV_NOPTS_VALUE && startDts == AV_NOPTS_VALUE){
                if(isAudioFrame){
                    startDts_a = pkt.dts;
                }else{
                    startDts_v = pkt.dts;
                }

                startDts = pkt.dts;
            }


            if(pkt.pts > stopPts){
                if(isAudioFrame){
                    audioEnded = true;
                }else{
                    videoEnded = true;
                }

                if(videoEnded && audioEnded){
                    LOGI("pts stoped : pkt pts:%lld,stopPts:%lld,a_start:%lld,a_stop:%lld,v_start:%lld,v_stop:%lld",
                         pkt.pts, stopPts, startPts_a, stopPts_a, startPts_v, stopPts_v);
                    break;
                }else{
                    continue;
                }
            }


            in_stream = iFmtCtx->streams[pkt.stream_index];
            if(isAudioFrame){
                if(isFirstAudioFrame){
                    av_dict_copy(&out_stream->metadata, in_stream->metadata, 0);
                    isFirstAudioFrame = false;
                }

//            LOGI("read_frame : pts:%lld,dts:%lld,start:%lld,stop:%lld,idx:%d,start_dts:%lld", pkt.pts, pkt.dts, startPts, stopPts, stream_index, startDts);

                if(pkt.pts != AV_NOPTS_VALUE && startPts > 0){
                    pkt.pts -= startPts;

                    if(pkt.dts != AV_NOPTS_VALUE && startDts > 0){
                        pkt.dts -= startDts;
                    }
                }

                if(lastDts != AV_NOPTS_VALUE && pkt.dts != AV_NOPTS_VALUE && pkt.dts <= lastDts){
                    if(pkt.pts == pkt.dts)
                    {
                        LOGW("add stream pts from %lld to %lld, stream_index=%d", pkt.pts, lastDts + 1, stream_index);
                        pkt.pts = lastDts + 1;
                    }

                    LOGW("add stream dts from %lld to %lld, stream_index=%d", pkt.dts, lastDts + 1, stream_index);
                    pkt.dts = lastDts + 1;
                }

                if(lastPts != AV_NOPTS_VALUE && pkt.pts != AV_NOPTS_VALUE && pkt.pts == lastPts){
                    LOGW("add stream pts from %lld to %lld, stream_index=%d", pkt.pts, lastPts + 1, stream_index);
                    pkt.pts = lastPts + 1;
                }

                if(isAudioFrame){
                    lastPts_a = pkt.pts;
                }else{
                    lastPts_v = pkt.pts;
                }

                if(pkt.dts != AV_NOPTS_VALUE){
                    if(isAudioFrame){
                        lastDts_a = pkt.dts;
                    }else{
                        lastDts_v = pkt.dts;
                    }
                }

                //Convert PTS/DTS
                pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,(AVRounding) (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
                pkt.pos = -1;
                pkt.stream_index = stream_index;

                if(pkt.dts != AV_NOPTS_VALUE)
                {
                    pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding) (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                }

                LOGI("Write 1 Packet. size:%5d,pts:%lld,dts:%lld,sindex:%d,duration:%lld,in_t:(%d/%d),out:(%d/%d)",pkt.size,pkt.pts, pkt.dts, stream_index, pkt.duration,
                     in_stream->time_base.num, in_stream->time_base.den, out_stream->time_base.num, out_stream->time_base.den);

                //Write
                // 这里用av_interleaved_write_frame回报错，用av_write_frame则没问题 在这里就用av_write_frame
                if (av_interleaved_write_frame(oFmtCtx, &pkt) < 0) {
//            if (av_write_frame(oFmtCtx, &pkt) < 0) {
                    char buf[1024];
                    av_strerror(errno, buf, 1024);
                    LOGE("av_interleaved_write_frame failed.err=%d,%s", errno, buf);
                    throw 1;
                }
                av_free_packet(&pkt);
            }
            else{

                if(isFirstVideoFrame){
                    isFirstVideoFrame = false;
                    av_dict_copy(&m_pVideoMetaData, in_stream->metadata, 0);
                }

                ret = avcodec_decode_video2(pCodecContext, pFrame, &got_picture, &pkt);
                if(ret < 0){
                    LOGE("decode video failed, ret=%d", ret);

                }

                LOGI("clip_encode: decode success got_picture:%d,pts:%lld", got_picture, pkt.pts);

                if(got_picture)
                {
//                    LOGI("before sws_scale");
                    sws_scale(pImgConvertCtx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, m_uVideoHeight, pFrameYUV->data, pFrameYUV->linesize);
//                    LOGI("after sws_scale");
//                    QueueManager::getInstance()->getLocalVideoEncodeQueue()->push(pFrameYUVData, yuvDataLen, AVUtil::getMillisecondsFromPts(iFmtCtx->streams[videoIdxIn]->time_base, pFrame->pkt_pts));
                }

                av_free_packet(&pkt);
            }

        }



        while (!m_bCanceled){
            ret = avcodec_decode_video2(pCodecContext, pFrame, &got_picture, &pkt);
            if (ret < 0 || !got_picture){
                break;
            }

            LOGI("clip_encode: decode success got_picture:%d,pts:%lld", got_picture, pkt.pts);
            sws_scale(pImgConvertCtx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, m_uVideoHeight, pFrameYUV->data, pFrameYUV->linesize);
//            QueueManager::getInstance()->getLocalVideoEncodeQueue()->push(pFrameYUVData, yuvDataLen, AVUtil::getMillisecondsFromPts( iFmtCtx->streams[videoIdxIn]->time_base, pFrame->pkt_pts));

            av_free_packet(&pkt);
        }


        //Write file trailer
        av_write_trailer(oFmtCtx);
    }
    catch(...)
    {
        if(pCodecContext)
        {
            avcodec_close(pCodecContext);
            pCodecContext = NULL;
        }

        if(pImgConvertCtx){
            sws_freeContext(pImgConvertCtx);
            pImgConvertCtx = NULL;
        }

        if(NULL != pAacbsfc){
            av_bitstream_filter_close(pAacbsfc);
            pAacbsfc = NULL;
        }

        avformat_close_input(&iFmtCtx);
        /* close output */
        if (oFmtCtx && !(ofmt->flags & AVFMT_NOFILE)){
            avio_close(oFmtCtx->pb);
        }

        avformat_free_context(oFmtCtx);

        m_bReEncodeDecodeEnded = true;

//        m_bMuxStopped = true;

//        CallbackManager::callback(LOCAL_VIDEO_UPLOAD_ERROR, LOCAL_VIDEO_DECODE_ERROR);
        m_bHasError = true;
        return;
    }

    if(pCodecContext)
    {
        avcodec_close(pCodecContext);
        pCodecContext = NULL;
    }

    if(pImgConvertCtx){
        sws_freeContext(pImgConvertCtx);
        pImgConvertCtx = NULL;
    }

    if(NULL != pAacbsfc){
        av_bitstream_filter_close(pAacbsfc);
        pAacbsfc = NULL;
    }

    avformat_close_input(&iFmtCtx);

    /* close output */
    if (oFmtCtx && !(ofmt->flags & AVFMT_NOFILE)){
        avio_close(oFmtCtx->pb);
    }
    avformat_free_context(oFmtCtx);

    m_bReEncodeDecodeEnded = true;

    pthread_join(m_encodeThreadId, NULL);

    if(m_bCanceled){
        clearTempFile();
        return;
    }

    // mux
    if(m_bNeedMuxAudioVideo){
//        LittleVideoEncoder::getInstance()->setMuxBlock(true);
//        int ret = LittleVideoEncoder::getInstance()->mux_video_audio(m_pTempAudioFilename, m_pTempVideoFilename, m_pOutFilename);
//        if(ret < 0){
//            CallbackManager::callback(LOCAL_VIDEO_UPLOAD_ERROR, LOCAL_VIDEO_MUX_ERROR);
//            m_bHasError = true;
//        }
    }

    clearTempFile();
    return;
}


void LocalVideo::encodeThread()
{
    AVFormatContext* pFmtContext = NULL;
    AVOutputFormat* pOutFmt = NULL;

    AVCodecContext* pCodecContext = NULL;
    AVCodec* pCodec = NULL;

    AVStream* pStream = NULL;

    AVFrame* pFrame = NULL;
    uint8_t* pFrameData = NULL;
    uint32_t pFrameDataLen = 0;


    AVPacket packet;

    int ret = 0;

    const char* filename = m_bNeedMuxAudioVideo ? m_pTempVideoFilename : m_pOutFilename;

    try {
        // 获取视频输出格式
        pOutFmt = av_guess_format(NULL, filename, NULL);
        if(NULL == pOutFmt){
            LOGE("LittleVideoEncoder : av_guess_format failed");
            throw 1;
        }

        // 申请内存
        pFmtContext = avformat_alloc_context();
        if(NULL == pFmtContext){
            LOGE("LittleVideoEncoder : avformat_alloc_context failed");
            throw 1;
        }

        // 为格式上下文设置属性
        pFmtContext->oformat = pOutFmt;
        snprintf(pFmtContext->filename, sizeof(pFmtContext->filename), "%s", filename);

        if(AV_CODEC_ID_NONE == pOutFmt->video_codec){
            LOGE("LittleVideoEncoder : fmt codec id none");
            throw 1;
        }

//    LOGI("LittleVideoEncoder : fmt->video_codec : %d", fmt->video_codec);

        // 设置视频流属性
        pStream = avformat_new_stream(pFmtContext, NULL);
        if(NULL == pStream){
            LOGE("LittleVideoEncoder : avformat_new_stream failed!");
            throw 1;
        }

        pCodecContext = pStream->codec;
        pCodecContext->codec_id = pOutFmt->video_codec;
        pCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;



        //H264
        AVDictionary* param = NULL;
        if(AV_CODEC_ID_H264 == pCodecContext->codec_id){
//	    	av_opt_set(pCodecContext->priv_data, "preset", "slow", 0);
//	    	av_opt_set(pCodecContext->priv_data, "tune","zerolatency", 0);
//        	av_dict_set(&param, "preset", "slow", 0);
            av_dict_set(&param, "preset", "ultrafast", 0);
            av_dict_set(&param, "tune","zerolatency", 0);
            av_dict_set(&param, "rc-lookahead", 0, 0);
            av_dict_set(&param, "profile", "baseline", 0);

        }

        // copy 房间的设置
        pCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
        pCodecContext->width = m_uEncodeVideoWidth;
        pCodecContext->height = m_uEncodeVideoHeight;
        pCodecContext->time_base.num = 1;
        pCodecContext->time_base.den = m_uEncodeVideoFPS;
        pCodecContext->level=40;

        pCodecContext->thread_count = 0;
        //Encoder parameters
        pCodecContext->refs = 1;
        pCodecContext->gop_size = m_uEncodeVideoFPS ;

        //码流
        pCodecContext->bit_rate = 1500*1000;
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

        //Optional Param
        if (!strcmp(pFmtContext->oformat->name, "mp4") || !strcmp(pFmtContext->oformat->name, "mov") || !strcmp(pFmtContext->oformat->name, "3gp"))
        {
            pCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }

        //将文件信息转储
        av_dump_format(pFmtContext, 0, filename, 1);

        // 查找编码器
        pCodec = avcodec_find_encoder(pCodecContext->codec_id);
        if (!pCodec)
        {
            LOGE("LittleVideoEncoder : avcodec_find_encoder error");
            throw 1;
        }

        // 打开编码器
//	if ((iRet = avcodec_open2(pCodecContext, pCodec, NULL)) < 0)
        if ((ret = avcodec_open2(pCodecContext, pCodec, &param)) < 0)
        {
            char buf[1024];
            av_strerror(ret, buf, 1024);
            LOGE("LittleVideoEncoder : avcodec_open failed : %d(%s)", ret, buf);
            throw 1;
        }

        // 申请yuv帧
        pFrame = av_frame_alloc();
        pFrameDataLen = avpicture_get_size(pCodecContext->pix_fmt, pCodecContext->width, pCodecContext->height);
        pFrameData = (uint8_t*)av_malloc(pFrameDataLen);
        if (NULL == pFrame || NULL == pFrameData)
        {
            LOGE("LittleVideoEncoder : malloc m_picture failed!");
            throw 1;
        }
        avpicture_fill((AVPicture*)pFrame, pFrameData, pCodecContext->pix_fmt, pCodecContext->width, pCodecContext->height);

        // 打开文件
        if ( (ret = avio_open(&pFmtContext->pb, filename, AVIO_FLAG_READ_WRITE)) < 0)
        {
            char buf[1024];
            av_strerror(ret, buf, 1024);
            LOGE("LittleVideoEncoder : url_fopen failed : %d(%s)", ret, buf);
            throw 1;
        }


        // 写视频头
        // 这里有一个蛋疼的事情，之前设置的帧率都不起作用，最后编码出来的视频帧率都
        // 是几千，通过查看ffmpeg源码，找到是在avformat_write_header中的mov_write_header中
        // 这个函数中如果没有设置video_track_timescale， 则他采用video stream 的 time_base的den
        // 在老版本的ffmpeg中，就是设置完这个值就完事了 但是在新版本中，加了一个循环，
        // 一直乘以2直到分母大于10000，不知道这个有何用意，但是从最后编码出来的视频结果来看，
        // 就是这个地方导致播放器不能正常的播放视频，所以我们在这里也设置一下video_track_timescale这个值
        // 不让他走另外一个分支。。。
        AVDictionary* opt = NULL;
//        av_dict_set_int(&opt, "video_track_timescale", m_uEncodeVideoFPS, 0);
        avformat_write_header(pFmtContext, &opt);

        av_new_packet(&packet, pFrameDataLen);


        int64_t pts;
        int64_t firstPts = -1;
        int timeout = 100;
        int32_t popLength = 1;
        uint32_t y_len = m_uEncodeVideoWidth * m_uEncodeVideoHeight;

        bool isFirstVideoFrame = true;

        // 编码
        while(popLength > 0 || !m_bReEncodeDecodeEnded)
        {
            if(m_bCanceled){
                break;
            }

            //
//            popLength = QueueManager::getInstance()->getLocalVideoEncodeQueue()->trypop(pFrameData, pFrameDataLen, timeout, pts);
            if(popLength <= 0 || popLength != pFrameDataLen){
                continue;
            }

            if(firstPts == -1 && pts != AV_NOPTS_VALUE){
                firstPts = pts;
            }

            if(isFirstVideoFrame){
                isFirstVideoFrame = false;
                if(m_pVideoMetaData){
                    av_dict_copy(&pStream->metadata, m_pVideoMetaData, 0);
                    av_dict_free(&m_pVideoMetaData);
                    m_pVideoMetaData = NULL;
                }
            }

            pFrame->data[0] = pFrameData;
            pFrame->data[1] = pFrameData + y_len;
            pFrame->data[2] = pFrameData + y_len + y_len / 4;
            pFrame->pts = AVUtil::getPtsFromMilliseconds(pStream->time_base, pts - firstPts);

            int got_picture = 0;
            //Encode
            packet.data = NULL;
            packet.size = 0;
            int ret = avcodec_encode_video2(pCodecContext, &packet, pFrame, &got_picture);
            if(ret < 0){
                LOGE("VideoEncoder : Failed to encode frame");
            }

            if (got_picture==1){
                packet.stream_index = pStream->index;
//                packet.dts = packet.pts;
                LOGI("encode frame, size = %d, pts = %lld, dts = %lld, getPts = %lld", packet.size, packet.pts, packet.dts, pts);
                ret = av_write_frame(pFmtContext, &packet);

                av_free_packet(&packet);
            }
            else {
                LOGI("VideoEncoder : encode success but not get picture!");
            }
        }

        // flush encoder
        int got_frame;
        AVPacket enc_pkt;
        if ((pCodecContext->codec->capabilities & CODEC_CAP_DELAY)){
            while (!m_bCanceled) {
                enc_pkt.data = NULL;
                enc_pkt.size = 0;
                av_init_packet(&enc_pkt);
                ret = avcodec_encode_video2(pCodecContext, &enc_pkt, NULL, &got_frame);
                av_frame_free(NULL);

                if (ret < 0){
                    break;
                }
                if (!got_frame) {
                    ret = 0;
                    break;
                }
//                enc_pkt.pts = enc_pkt.dts = ePts++;
                LOGI("LittleVideoEncoder : flush encode , size = %d, pts=%lld, dts = %lld", enc_pkt.size, enc_pkt.pts, enc_pkt.dts);
                /* mux encoded frame */
                ret = av_write_frame(pFmtContext, &enc_pkt);
                if (ret < 0){
                    break;
                }
            }
        }


        // write tailer

        av_write_trailer(pFmtContext);
    }
    catch (...){

        if (NULL != pStream)
        {
            avcodec_close(pStream->codec);
            pStream = NULL;
        }

        if(NULL != pFrame){
            av_free(pFrame);
            pFrame = NULL;
        }

        if(NULL != pFrameData){
            free(pFrameData);
            pFrameData = NULL;
        }

        if(NULL != pFmtContext){
            for (int i=0; i<pFmtContext->nb_streams; i++)
            {
                av_freep(&pFmtContext->streams[i]->codec);
                av_freep(&pFmtContext->streams[i]);
            }

            if(NULL != pFmtContext->oformat){
                if (!(pFmtContext->oformat->flags & AVFMT_NOFILE))
                {
                    if(NULL != pFmtContext->pb){
                        avio_close(pFmtContext->pb);
                    }
                }
            }
            av_free(pFmtContext);
            pFmtContext = NULL;
        }

//        CallbackManager::callback(LOCAL_VIDEO_UPLOAD_ERROR, LOCAL_VIDEO_ENCODE_ERROR);
        m_bHasError = true;
        return;
    }


    if (NULL != pStream)
    {
        avcodec_close(pStream->codec);
        pStream = NULL;
    }

    if(NULL != pFrame){
        av_free(pFrame);
        pFrame = NULL;
    }

    if(NULL != pFrameData){
        free(pFrameData);
        pFrameData = NULL;
    }

    if(NULL != pFmtContext) {
        for (int i = 0; i < pFmtContext->nb_streams; i++) {
            av_freep(&pFmtContext->streams[i]->codec);
            av_freep(&pFmtContext->streams[i]);
        }

        if (NULL != pFmtContext->oformat) {
            if (!(pFmtContext->oformat->flags & AVFMT_NOFILE)) {
                if (NULL != pFmtContext->pb) {
                    avio_close(pFmtContext->pb);
                }
            }
        }
        av_free(pFmtContext);
        pFmtContext = NULL;
    }

}


bool LocalVideo::isSupported(const char *filename)
{
    AVFormatContext* pFmtContext = NULL;
    AVCodecContext* pCodecContext = NULL;

    try {
        av_register_all();
        pFmtContext = avformat_alloc_context();
        if(!pFmtContext)
        {
            LOGI("avformat_alloc_context failed");
            throw 1;
        }

        if(avformat_open_input(&pFmtContext, filename, NULL, NULL) != 0)
        {
            LOGI("Couldn't open input stream. file:%s", filename);
            throw 1;
        }

        if(avformat_find_stream_info(pFmtContext, NULL) < 0){
            LOGI("Couldn't find stream information");
            throw 1;
        }

        int videoindex=-1;
        for(int i=0; i<pFmtContext->nb_streams; i++)
        {
            if(pFmtContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                videoindex=i;
                break;
            }
        }

        if(videoindex == -1){
            LOGI("Didn't find a video stream");
            throw 1;
        }

        pCodecContext = pFmtContext->streams[videoindex]->codec;
        AVCodec* pCodec = avcodec_find_decoder(pCodecContext->codec_id);

        if(pCodec == NULL){
            LOGI("Codec not found.");
            throw 1;
        }

        if(avcodec_open2(pCodecContext, pCodec, NULL) < 0){
            LOGI("Could not open codec.");
            throw 1;
        }

    }catch (...){

        if(pCodecContext)
        {
            avcodec_close(pCodecContext);
            pCodecContext = NULL;
        }

        if(pFmtContext)
        {
            avformat_close_input(&pFmtContext);
            pFmtContext = NULL;
        }

        return false;
    }

    if(pCodecContext)
    {
        avcodec_close(pCodecContext);
        pCodecContext = NULL;
    }

    if(pFmtContext)
    {
        avformat_close_input(&pFmtContext);
        pFmtContext = NULL;
    }

    return true;
}


