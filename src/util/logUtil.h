/*
 * logUtil.h
 *
 *  Created on: 2017年6月26日
 *      Author: mj
 */

#ifndef UTIL_LOGUTIL_H_
#define UTIL_LOGUTIL_H_


#define LOGD(FMT, ...) printf(FMT, ##__VA_ARGS__);printf("\n")
#define LOGI(FMT, ...) printf(FMT, ##__VA_ARGS__);printf("\n")
#define LOGW(FMT, ...) printf(FMT, ##__VA_ARGS__);printf("\n")
#define LOGE(FMT, ...) printf(FMT, ##__VA_ARGS__);printf("\n")


#endif /* UTIL_LOGUTIL_H_ */
