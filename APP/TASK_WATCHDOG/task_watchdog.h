/**
 * @file    task_watchdog.h
 * @brief   任务心跳监控 + IWDG 硬件看门狗
 * @note    每个 FreeRTOS 任务周期性调用 TASK_HEARTBEAT_PING()
 *          监控任务定期检查所有任务心跳，超时则触发 IWDG 复位
 *          实现"异常自动复位"的系统级可靠性保障
 */

#ifndef __TASK_WATCHDOG_H
#define __TASK_WATCHDOG_H

#include "stm32f10x.h"
#include <stdint.h>

/* 任务 ID 枚举 */
typedef enum
{
    WDT_TASK_TEMPERATURE = 0,    /* 温度采集任务 */
    WDT_TASK_RFID,               /* RFID 读取任务 */
    WDT_TASK_DISPLAY,            /* 显示任务 */
    WDT_TASK_WIFI,               /* WiFi 上传任务 */
    WDT_TASK_BATTERY,            /* 电池监控任务 */
    WDT_TASK_DATA_FUSION,        /* 数据融合任务 */
    WDT_TASK_COUNT               /* 任务总数 (必须放在最后) */
} WatchdogTaskID_t;

/* 每个任务的心跳信息 */
typedef struct
{
    volatile uint32_t  ulLastHeartbeat;     /* 上次心跳时间 (Ticks) */
    volatile uint32_t  ulMissedCount;       /* 连续丢失心跳次数 */
    volatile uint8_t   ucEnabled;           /* 是否启用监控 */
    uint32_t           ulMaxIntervalTicks;  /* 最大允许间隔 (Ticks) */
    const char         *pcTaskName;         /* 任务名称 */
} TaskHeartbeat_t;

/* 函数原型 */
void Watchdog_Init(void);
void Watchdog_TaskRegister(WatchdogTaskID_t eTaskID, const char *pcName, uint32_t ulMaxIntervalMS);
void Watchdog_Ping(WatchdogTaskID_t eTaskID);
void Watchdog_Check(void);
void Watchdog_PrintStatus(void);
uint8_t Watchdog_IsSystemHealthy(void);

/* 全局看门狗句柄 */
extern TaskHeartbeat_t g_atTaskHeartbeats[WDT_TASK_COUNT];

#endif /* __TASK_WATCHDOG_H */
