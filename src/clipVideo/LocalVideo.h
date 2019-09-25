/**
 * Created by mj on 17-4-18.
 */

#ifndef BRANCH_LOCAL_VIDEO_LOCALVIDEO_H
#define BRANCH_LOCAL_VIDEO_LOCALVIDEO_H


#include <thread>
#include "../util/const.h"

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/frame.h"
#include "libswscale/swscale.h"
#include "libavutil/log.h"
};

class LocalVideo {
public:
    static LocalVideo* getInstance();

public:
    bool isSupported(const char* filename);

    bool prepare(const char* filename);

    void start();
    void stop();
    void cancel();

    void release();

    uint32_t getVideoWidth();
    uint32_t getVideoHeight();

    void clipVideo(int64_t startTime, int64_t stopTime, const char* outFilename);

    void seek(int64_t time);

private:
    LocalVideo();
    virtual ~LocalVideo();


public:
    void run();
    bool initDecoder();

    void onGotYuvData();

    bool isKeyFrame(uint8_t* data, int size, int startPos);
    bool isKeyFrame(AVPacket packet);

    int findNALUStartCode(uint8_t* data, int size, int startPos);

    void internalClipVideo();

    void internalClipAndEncodeVideo();

    void savePicture();
    void savePicture(int64_t ms);

    void setDecodeStartTime(int64_t milliseconds);

    void onSeekChanged();

    void encodeThread();

    void initTempFile();
    void clearTempFile();
private:
    static LocalVideo* ms_instance;

private:
    char m_pFilename[VV_FILENAME_MAX];

private:
    AVFormatContext* m_pFmtContext;
    AVCodecContext* m_pCodecContext;
    AVFrame* m_pAVFrame;
    AVPacket* m_pAVPacket;
    AVPacket* m_pAVNALUPacket;
    AVFrame* m_pYuvFrame;
    SwsContext* m_pImgConvertCtx;

    AVBitStreamFilterContext* m_pH264Bsfc;

    int m_iVideoIndex;
    int m_iAudioIndex;

    uint8_t *m_pYuvData;
    uint8_t *m_pRGBAData;
    uint32_t m_uVideoWidth;
    uint32_t m_uVideoHeight;
    uint32_t m_uYLen;

    uint32_t m_uTextureWidth;
    uint32_t m_uTextureHeight;
    uint32_t m_uTextureYLen;

    bool m_bFirstFrame;

private:
    std::thread* mThread;
    std::thread* mEncodeThread;

    bool m_bStopped;
    bool m_bWantStop;
    bool m_bCanceled;

private:
    int64_t m_iStartTimeMs;
    int64_t m_iStopTimeMs;
    char m_pOutFilename[VV_FILENAME_MAX];
    bool m_bStartClipVideo;

private:
    bool m_bNeedReEncode;
    bool m_bNeedMuxAudioVideo;
    bool m_bHasError;
    bool m_bReEncodeDecodeEnded;
    char m_pTempAudioFilename[VV_FILENAME_MAX];
    char m_pTempVideoFilename[VV_FILENAME_MAX];

    uint32_t m_uEncodeVideoWidth;
    uint32_t m_uEncodeVideoHeight;
    uint32_t m_uEncodeVideoFPS;

    AVDictionary* m_pVideoMetaData;

private:
    int64_t m_iDecodeStartTimeMs;
    int64_t m_iDecodeStopTimeMs;
    bool m_bDecodeEnded;

    int64_t m_iSeekedTime;
    bool m_bSeekedChanged;
};


#endif //BRANCH_LOCAL_VIDEO_LOCALVIDEO_H
