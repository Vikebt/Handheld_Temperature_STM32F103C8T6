/**
 * @file    rtc_driver.c
 * @brief   RTC 实时时钟驱动实现
 * @note    使用 STM32F103 内置 RTC，LSI (40kHz) 或 LSE (32.768kHz)
 *          通过备份寄存器判断 RTC 是否已初始化
 *          提供 Unix 时间戳 (秒) 计数
 */

#include "rtc_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* RTC 时钟源选择 */
#define RTC_CLOCK_SOURCE_LSI    0   /* 使用 LSI (40kHz) */
#define RTC_CLOCK_SOURCE_LSE    1   /* 使用 LSE (32.768kHz) */
#define RTC_USE_CLOCK           RTC_CLOCK_SOURCE_LSI  /* 无外部晶振时用 LSI */

/* 备份寄存器 (BKP) 用于存储初始化标志 */
#define RTC_BKP_INIT_FLAG       BKP_DR1
#define RTC_INIT_MAGIC          0xA5A5

/* 闰年判断 */
#define IS_LEAP_YEAR(y)         (((y) % 4 == 0 && (y) % 100 != 0) || (y) % 400 == 0)

/* 每月天数 */
static const uint8_t g_aucMonthDays[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static uint8_t g_ucRTCInitialized = 0;

/**
 * @brief  初始化 RTC
 * @retval 1: 成功  0: 失败
 */
uint8_t RTC_Init(void)
{
    /* 使能 PWR 和 BKP 时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);

    /* 允许访问备份域 */
    PWR_BackupAccessCmd(ENABLE);

    /* 检查 RTC 是否已初始化 */
    if (BKP_ReadBackupRegister(RTC_BKP_INIT_FLAG) == RTC_INIT_MAGIC)
    {
        /* RTC 已配置，等待同步 */
        RTC_WaitForSynchro();
        g_ucRTCInitialized = 1;
        return 1;
    }

    /* ====== 首次配置 RTC ====== */

    /* 复位备份域 */
    BKP_DeInit();

#if (RTC_USE_CLOCK == RTC_CLOCK_SOURCE_LSE)
    /* 使用 LSE (32.768kHz) — 需要外部晶振 */
    RCC_LSEConfig(RCC_LSE_ON);
    uint32_t ulTimeout = 300000;
    while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
    {
        if (--ulTimeout == 0) break;
    }

    if (ulTimeout > 0)
    {
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
    }
    else
    {
        /* LSE 不可用, 回退到 LSI */
        RCC_LSICmd(ENABLE);
        ulTimeout = 300000;
        while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET && --ulTimeout);
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
    }
#else
    /* 使用 LSI (40kHz) */
    RCC_LSICmd(ENABLE);
    uint32_t ulTimeout = 300000;
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
    {
        if (--ulTimeout == 0) return 0;
    }
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
#endif

    /* 使能 RTC */
    RCC_RTCCLKCmd(ENABLE);
    RTC_WaitForSynchro();
    RTC_WaitForLastTask();

    /* 配置预分频: LSI=40kHz, 目标1秒计数 */
    /* RTC 计数器频率 = LSI / (Prescaler + 1) */
    uint16_t usPrescaler;
#if (RTC_USE_CLOCK == RTC_CLOCK_SOURCE_LSE)
    usPrescaler = 32767;    /* 32.768kHz → 1Hz */
#else
    usPrescaler = 39999;    /* 40kHz → 1Hz (39999 + 1) */
#endif

    RTC_SetPrescaler(usPrescaler);
    RTC_WaitForLastTask();

    /* 设置初始时间: 2026-01-01 00:00:00 */
    RTCTime_t tInit;
    tInit.usYear   = 2026;
    tInit.ucMonth  = 1;
    tInit.ucDay    = 1;
    tInit.ucHour   = 0;
    tInit.ucMinute = 0;
    tInit.ucSecond = 0;
    tInit.ucWeekDay = 4;  /* 2026-01-01 是周四 */

    RTC_SetTime(&tInit);
    RTC_WaitForLastTask();

    /* 写入初始化标志 */
    BKP_WriteBackupRegister(RTC_BKP_INIT_FLAG, RTC_INIT_MAGIC);

    g_ucRTCInitialized = 1;
    return 1;
}

/**
 * @brief  时间结构体 → 秒计数 (自 1970-01-01)
 */
static uint32_t prvTimeToSeconds(const RTCTime_t *pTime)
{
    /* 从 1970 年到当前年 */
    uint32_t ulSeconds = 0;

    for (uint16_t y = 1970; y < pTime->usYear; y++)
    {
        ulSeconds += IS_LEAP_YEAR(y) ? 366 * 86400 : 365 * 86400;
    }

    /* 当前年的月日 */
    uint8_t ucLeap = IS_LEAP_YEAR(pTime->usYear);
    for (uint8_t m = 1; m < pTime->ucMonth; m++)
    {
        ulSeconds += g_aucMonthDays[m - 1] * 86400;
        if (m == 2 && ucLeap) ulSeconds += 86400;
    }

    ulSeconds += (pTime->ucDay - 1) * 86400;
    ulSeconds += pTime->ucHour * 3600;
    ulSeconds += pTime->ucMinute * 60;
    ulSeconds += pTime->ucSecond;

    return ulSeconds;
}

/**
 * @brief  秒计数 → 时间结构体
 */
static void prvSecondsToTime(uint32_t ulSeconds, RTCTime_t *pTime)
{
    uint8_t ucLeap;
    uint32_t ulRemaining = ulSeconds;

    /* 年份 */
    for (pTime->usYear = 1970; ; pTime->usYear++)
    {
        ucLeap = IS_LEAP_YEAR(pTime->usYear);
        uint32_t ulYearSecs = ucLeap ? 366 * 86400 : 365 * 86400;
        if (ulRemaining < ulYearSecs) break;
        ulRemaining -= ulYearSecs;
    }

    /* 月份 */
    ucLeap = IS_LEAP_YEAR(pTime->usYear);
    for (pTime->ucMonth = 1; pTime->ucMonth <= 12; pTime->ucMonth++)
    {
        uint8_t ucDays = g_aucMonthDays[pTime->ucMonth - 1];
        if (pTime->ucMonth == 2 && ucLeap) ucDays = 29;

        uint32_t ulMonthSecs = ucDays * 86400;
        if (ulRemaining < ulMonthSecs) break;
        ulRemaining -= ulMonthSecs;
    }

    /* 日 */
    pTime->ucDay = (uint8_t)(ulRemaining / 86400) + 1;
    ulRemaining %= 86400;

    /* 时 */
    pTime->ucHour = (uint8_t)(ulRemaining / 3600);
    ulRemaining %= 3600;

    /* 分 */
    pTime->ucMinute = (uint8_t)(ulRemaining / 60);
    ulRemaining %= 60;

    /* 秒 */
    pTime->ucSecond = (uint8_t)ulRemaining;

    /* 星期 (2026-01-01 = Thursday = 4) */
    uint32_t ulDays = ulSeconds / 86400;
    pTime->ucWeekDay = (uint8_t)((ulDays + 4) % 7);
    if (pTime->ucWeekDay == 0) pTime->ucWeekDay = 7;
}

/**
 * @brief  设置 RTC 时间
 */
uint8_t RTC_SetTime(const RTCTime_t *pTime)
{
    if (pTime == NULL) return 0;

    uint32_t ulSeconds = prvTimeToSeconds(pTime);

    RTC_WaitForLastTask();
    RTC_SetCounter(ulSeconds);
    RTC_WaitForLastTask();

    return 1;
}

/**
 * @brief  读取 RTC 时间
 */
uint8_t RTC_GetTime(RTCTime_t *pTime)
{
    if (pTime == NULL || g_ucRTCInitialized == 0) return 0;

    uint32_t ulSeconds = RTC_GetCounter();
    prvSecondsToTime(ulSeconds, pTime);

    return 1;
}

/**
 * @brief  获取 Unix 时间戳
 */
uint32_t RTC_GetUnixTimestamp(void)
{
    return RTC_GetCounter();
}

/**
 * @brief  打印当前时间到串口
 */
void RTC_PrintTime(void)
{
    RTCTime_t tNow;
    if (RTC_GetTime(&tNow))
    {
        char cBuf[32];
        RTC_TimeToString(&tNow, cBuf, sizeof(cBuf));
        printf("[RTC] %s\n", cBuf);
    }
}

/**
 * @brief  查询 RTC 是否已初始化
 */
uint8_t RTC_IsInitialized(void)
{
    return g_ucRTCInitialized;
}

/**
 * @brief  时间格式化为字符串 "2026-05-30 14:30:00 Mon"
 */
void RTC_TimeToString(const RTCTime_t *pTime, char *pBuf, uint8_t ucBufLen)
{
    if (pTime == NULL || pBuf == NULL) return;

    snprintf(pBuf, ucBufLen, "%04u-%02u-%02u %02u:%02u:%02u %s",
             pTime->usYear, pTime->ucMonth, pTime->ucDay,
             pTime->ucHour, pTime->ucMinute, pTime->ucSecond,
             RTC_WeekDayToString(pTime->ucWeekDay));
}

/**
 * @brief  星期数字转字符串
 */
char *RTC_WeekDayToString(uint8_t ucWeekDay)
{
    static const char *apcDays[] = {"", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    if (ucWeekDay >= 1 && ucWeekDay <= 7)
        return (char *)apcDays[ucWeekDay];
    return "???";
}
