/*
 * TimeUtil.h
 *
 *  Created on: 2015年11月19日
 *      Author: mj
 */

#ifndef OFFSCREEN_TIMEUTIL_H_
#define OFFSCREEN_TIMEUTIL_H_

#include <stdint.h>

class TimeUtil{
public:

	static uint32_t GetTickCount();

	// 线程sleep
	// @param milliseconds 休眠毫秒数
	static void sleep(int milliseconds);

	static uint64_t GetTickCount64();
};



#endif /* OFFSCREEN_TIMEUTIL_H_ */
