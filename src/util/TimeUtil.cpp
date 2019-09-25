/*
 * TimeUtil.cpp
 *
 *  Created on: 2015年11月19日
 *      Author: mj
 */
extern "C"{

#include <unistd.h>
//#include <sys/timeb.h>
#include <sys/time.h>
}

#include "TimeUtil.h"


uint32_t TimeUtil::GetTickCount()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint32_t mt = ((uint32_t)tv.tv_sec)*1000+(uint32_t)tv.tv_usec/1000;

    return mt;
}

void TimeUtil::sleep(int milliseconds)
{
	::usleep(milliseconds * 1000);
}

uint64_t TimeUtil::GetTickCount64()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint64_t mt = ((uint64_t)tv.tv_sec)*1000+(uint64_t)tv.tv_usec/1000;

    return mt;
}
