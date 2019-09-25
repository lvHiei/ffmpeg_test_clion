/**
 * Created by mj on 17-6-1.
 */

#ifndef SRC_AVUTIL_H
#define SRC_AVUTIL_H


#include <stdint.h>
extern "C"
{
    #include "libavutil/rational.h"
};

class AVUtil {
public:
    static int64_t getMillisecondsFromPts(AVRational rational, int64_t pts);
    static int64_t getPtsFromMilliseconds(AVRational rational, int64_t milliseconds);
};


#endif //SRC_AVUTIL_H
