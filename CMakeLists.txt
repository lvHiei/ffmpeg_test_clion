cmake_minimum_required(VERSION 3.10)
project(ffmpeg_test_clion)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-reserved-user-defined-literal -Wno-deprecated-declarations")
include_directories(/usr/local/include)

link_directories(/usr/local/lib)

#according https://debugly.cn/

link_libraries(
        avformat
        avcodec
        avfilter
        postproc
        swresample
        swscale
        avutil
        x264
        fdk-aac
        z
        bz2
        iconv
        lzma
        "-framework AudioToolbox"
        "-framework VideoToolbox"
        "-framework VideoDecodeAcceleration"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework Security"
        "-framework CoreFoundation"
)

add_executable(ffmpeg_test_clion
        src/avfilter/VVFilterUtil.cpp
        src/avfilter/VVFilterUtilPure.cpp

        src/avspeed/ASpeed.cpp
        src/avspeed/AVSpeed.cpp
        src/clipVideo/LocalVideo.cpp

        src/codec/VVAVDecoder.cpp
        src/codec/VVAVEncoder.cpp
        src/format/VVAVFormat.cpp
        src/format/Mp4Muxer.cpp

        src/util/AVUtil.cpp
        src/util/TimeUtil.cpp


        src/ffmpeg_test.cpp
        )

include_directories(test test/av)

add_executable(test_remux
        test/av/LhAVCodec.cpp
        test/av/LhAVFormat.cpp
        test/test.cpp
        )