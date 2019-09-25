/*
 * const.h
 *
 *  Created on: 2015年11月12日
 *      Author: mj
 */

#ifndef OFFSCREEN_CONST_H_
#define OFFSCREEN_CONST_H_

#define POP_TIMEOUT								100
#define FLOAT_PRECISION             0.00001

const int MAX_VIDEO_TIMESTAMPS = 1024;

#define TIME_OVER_FLOW 1000000000

const int VV_FILENAME_MAX = 1024;

const int VV_LOCAL_VIDEO_UPLOAD_MAX_TIME = 120*1000;
//const int VV_LOCAL_VIDEO_UPLOAD_MAX_TIME = 30*1000;

const int VV_LOCAL_VIDEO_MAX_WH = 640;
const char FILE_DELIMITER = '.';
const int VV_LOCAL_VIDEO_DEFAULT_FPS = 25;

// 滤镜相关

// 错误码
enum JNI_MSG_TYPE{
    // 视频相关
    MSG_CHANGE_BITRATE = 0,

    // 音频相关
    MSG_RECORD_INIT_ERROR = 100,

    // 音频播放相关
    AUDIO_PLAYER_ONERROR = 200,
    AUDIO_PLAYER_ONSTART = 201,
    AUDIO_PLAYER_ONPASUE = 202,
    AUDIO_PLAYER_ONSTOP = 203,
    AUDIO_PLAYER_ONREFRESH = 204,
    AUDIO_PLAYER_ONPREPARE = 205,

    //音效相关
    AUDIO_EFFECT_PROCESS_SLOW = 400,

    //小视频
    LITTLE_VIDEO_ERROR = 2000,
    LITTLE_VIDEO_ENCODE_FINISHED,
    LITTLE_VIDEO_MUX_FINISHED,
    LITTLE_VIDEO_PICTURE_FINISHED,
    LITTLE_VIDEO_CANCELED,
    LITTLE_VIDEO_PERCENT,

    // 本地视频上传
    LOCAL_VIDEO_UPLOAD_FINISHED = 2200,
    LOCAL_VIDEO_UPLOAD_ERROR,
    LOCAL_VIDEO_UPLOAD_CANCELED,
};

enum {
    ASPECT_RATIO_UNKOWN = -1,
    ASPECT_RATIO_9_16 = 0,
    ASPECT_RATIO_16_9 = 1,
    ASPECT_RATIO_4_3 = 2,
    ASPECT_RATIO_3_4 = 3,
    ASPECT_RATIO_1_1 = 4,
};

enum{
    ROTATE_DEGREE_0 = 0,
    ROTATE_DEGREE_90 = 90,
    ROTATE_DEGREE_180 = 180,
    ROTATE_DEGREE_270 = 270,
};

enum {
    LVL_VERBOSE,
    LVL_DEBUG,
    LVL_INFO,
    LVL_WARN,
    LVL_ERROR,
    LVL_FATAL
};


enum LookUpTextureType{
    LOOK_UP_AMATORKA = 1,
    LOOK_UP_MISS_ETIKATE = 2,
    LOOK_UP_SOFT_ELEGANCE_1 = 3,
    LOOK_UP_SOFT_ELEGANCE_2 = 4,

    // vv直播使用
    LOOK_UP_ORIGIN = 100,       // 原图
    LOOK_UP_ABAOSE,             // 阿宝色
    LOOK_UP_GAOLENGFAN,         // 高冷范
    LOOK_UP_HONGRUN,            // 红润
    LOOK_UP_JIAOPIAN,           // 胶片
    LOOK_UP_MUQIU,              // 暮秋
    LOOK_UP_QINGXIN,            // 清新
    LOOK_UP_XIAOSENLIN,         // 小森林
    LOOK_UP_ZIRAN,              // 自然
    LOOK_UP_ZHONGXIA,           // 仲夏


};

enum FilterType {
    FILTER_NONE = -1,
    FILTER_ORIGIN = LOOK_UP_ORIGIN,         // 原图
    FILTER_ABAOSE,                          // 阿宝色
    FILTER_GAOLENGFAN,                      // 高冷范
    FILTER_HONGRUN,                         // 红润
    FILTER_JIAOPIAN,                        // 胶片
    FILTER_MUQIU,                           // 暮秋
    FILTER_QINGXIN,                         // 清新
    FILTER_XIAOSENLIN,                      // 小森林
    FILTER_ZIRAN,                           // 自然
    FILTER_ZHONGXIA,                        // 仲夏

};

enum AVType{
    AVTYPE_LIVE,
    AVTYPE_LITTLEVIDEO,
    AVTYPE_TAKEPICTURE,
};

enum{
    SUCCESS = 0,
    RECORD_ERROR = 1,
    AUDIO_ENCODE_ERROR = 2,
    VIDEO_ENCODE_ERROR = 3,
    OFFSCREEN_ERROR = 4,
    MUX_AV_ERROR = 5,
    LOCAL_VIDEO_DEMUX_ERROR = 6,
    LOCAL_VIDEO_DECODE_ERROR = 7,
    LOCAL_VIDEO_ENCODE_ERROR = 8,
    LOCAL_VIDEO_MUX_ERROR = 9,
};

enum NALU_TYPE{
    NALU_NON_IDR = 1,
    NALU_IDR = 5,
    NALU_SEI = 6,
    NALU_SPS = 7,
    NALU_PPS = 8,
};


#endif /* OFFSCREEN_CONST_H_ */
