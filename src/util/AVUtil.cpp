/**
 * Created by mj on 17-6-1.
 */

#include "AVUtil.h"


int64_t AVUtil::getMillisecondsFromPts(AVRational rational, int64_t pts)
{
    return av_q2d(rational) * pts * 1000;
}

int64_t AVUtil::getPtsFromMilliseconds(AVRational rational, int64_t milliseconds)
{
    return milliseconds / (av_q2d(rational) * 1000);
}
