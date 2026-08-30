/**
 * @file    rtc_driver.h
 * @brief   RTC 实时时钟驱动
 * @note    使用 STM32 内置 RTC (外部32.768kHz晶振或LSI)
 *          提供系统时间戳，用于数据日志标记
 *          支持日期时间设置和读取
 */

#ifndef __RTC_DRIVER_H
#define __RTC_DRIVER_H

#include "stm32f10x.h"
#include <stdint.h>

/* 时间结构体 */
typedef struct
{
    uint16_t usYear;        /* 年 (2025+) */
    uint8_t  ucMonth;       /* 月 (1-12) */
    uint8_t  ucDay;         /* 日 (1-31) */
    uint8_t  ucHour;        /* 时 (0-23) */
    uint8_t  ucMinute;      /* 分 (0-59) */
    uint8_t  ucSecond;      /* 秒 (0-59) */
    uint8_t  ucWeekDay;     /* 星期 (1-7, Mon=1) */
} RTCTime_t;

/* 函数原型 */
uint8_t  RTC_Init(void);
uint8_t  RTC_SetTime(const RTCTime_t *pTime);
uint8_t  RTC_GetTime(RTCTime_t *pTime);
uint32_t RTC_GetUnixTimestamp(void);
void     RTC_PrintTime(void);
uint8_t  RTC_IsInitialized(void);

/* 时间格式转换 */
void     RTC_TimeToString(const RTCTime_t *pTime, char *pBuf, uint8_t ucBufLen);
char    *RTC_WeekDayToString(uint8_t ucWeekDay);

#endif /* __RTC_DRIVER_H */
