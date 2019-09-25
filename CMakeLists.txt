cmake_minimum_required(VERSION 3.10)
project(ffmpeg_test)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-reserved-user-defined-literal -Wno-deprecated-declarations")
include_directories(/usr/local/include)



add_executable(ffmpeg_test
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