/**
 * @file    task_watchdog.c
 * @brief   任务心跳监控 + IWDG 硬件看门狗实现
 * @note    设计亮点:
 *          1. 独立看门狗 (IWDG) 4 秒超时复位
 *          2. 每个业务任务周期性 Ping 看门狗
 *          3. 监控任务检查各任务心跳间隔
 *          4. 异常时打印错误信息后让 IWDG 复位系统
 *          5. 临界区保护看门狗操作防止竞态
 */

#include "task_watchdog.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* IWDG 配置: LSI = 40kHz, 4 秒超时 */
#define IWDG_PRESCALER      IWDG_Prescaler_64    /* 40kHz / 64 = 625Hz */
#define IWDG_RELOAD_VALUE   2500                 /* 2500 / 625 = 4 秒 */

/* 心跳检查周期 (Ticks) */
#define WATCHDOG_CHECK_INTERVAL_MS   1000

/* 默认最大心跳间隔 (ms) */
#define DEFAULT_MAX_HEARTBEAT_MS     3000

/* 连续丢失次数上限 */
#define MAX_MISSED_BEFORE_RESET      3

/* 全局心跳表 */
TaskHeartbeat_t g_atTaskHeartbeats[WDT_TASK_COUNT];

/**
 * @brief  初始化 IWDG 和心跳表
 */
void Watchdog_Init(void)
{
    /* 初始化心跳表 */
    for (uint8_t i = 0; i < WDT_TASK_COUNT; i++)
    {
        g_atTaskHeartbeats[i].ulLastHeartbeat     = 0;
        g_atTaskHeartbeats[i].ulMissedCount       = 0;
        g_atTaskHeartbeats[i].ucEnabled           = 0;
        g_atTaskHeartbeats[i].ulMaxIntervalTicks  = pdMS_TO_TICKS(DEFAULT_MAX_HEARTBEAT_MS);
        g_atTaskHeartbeats[i].pcTaskName          = "UNREG";
    }

    /* 配置并启动 IWDG */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_PRESCALER);
    IWDG_SetReload(IWDG_RELOAD_VALUE);
    IWDG_ReloadCounter();
    IWDG_Enable();   /* 一旦使能无法关闭, 必须不断喂狗 */
}

/**
 * @brief  注册任务到看门狗监控表
 * @param  eTaskID        任务 ID
 * @param  pcName         任务名称
 * @param  ulMaxIntervalMS  最大允许间隔 (ms)
 */
void Watchdog_TaskRegister(WatchdogTaskID_t eTaskID, const char *pcName, uint32_t ulMaxIntervalMS)
{
    if (eTaskID < WDT_TASK_COUNT)
    {
        taskENTER_CRITICAL();
        g_atTaskHeartbeats[eTaskID].pcTaskName         = pcName;
        g_atTaskHeartbeats[eTaskID].ulMaxIntervalTicks = pdMS_TO_TICKS(ulMaxIntervalMS);
        g_atTaskHeartbeats[eTaskID].ulLastHeartbeat    = xTaskGetTickCount();
        g_atTaskHeartbeats[eTaskID].ulMissedCount      = 0;
        g_atTaskHeartbeats[eTaskID].ucEnabled          = 1;
        taskEXIT_CRITICAL();
    }
}

/**
 * @brief  任务心跳: 由各任务周期调用
 */
void Watchdog_Ping(WatchdogTaskID_t eTaskID)
{
    if (eTaskID < WDT_TASK_COUNT)
    {
        taskENTER_CRITICAL();
        g_atTaskHeartbeats[eTaskID].ulLastHeartbeat = xTaskGetTickCount();
        g_atTaskHeartbeats[eTaskID].ulMissedCount   = 0;
        taskEXIT_CRITICAL();
    }
}

/**
 * @brief  检查所有任务心跳 (由监控任务调用)
 * @note   如果发现某个任务心跳超时且超过最大允许次数,
 *         则打印错误信息并停止喂 IWDG, 让系统复位
 */
void Watchdog_Check(void)
{
    TickType_t xNow = xTaskGetTickCount();
    uint8_t ucSystemHealthy = 1;

    for (uint8_t i = 0; i < WDT_TASK_COUNT; i++)
    {
        if (g_atTaskHeartbeats[i].ucEnabled == 0)
        {
            continue;
        }

        TickType_t xElapsed = xNow - g_atTaskHeartbeats[i].ulLastHeartbeat;

        if (xElapsed > g_atTaskHeartbeats[i].ulMaxIntervalTicks)
        {
            g_atTaskHeartbeats[i].ulMissedCount++;

            printf("[WDT] WARNING: Task '%s' missed heartbeat %lu/%u, elapsed=%lu ms\r\n",
                   g_atTaskHeartbeats[i].pcTaskName,
                   (unsigned long)g_atTaskHeartbeats[i].ulMissedCount,
                   MAX_MISSED_BEFORE_RESET,
                   (unsigned long)pdTICKS_TO_MS(xElapsed));

            if (g_atTaskHeartbeats[i].ulMissedCount >= MAX_MISSED_BEFORE_RESET)
            {
                ucSystemHealthy = 0;
            }
        }
    }

    if (ucSystemHealthy == 0)
    {
        /* 任务卡死: 打印最后的错误信息, 停止喂狗 */
        printf("[WDT] SYSTEM RESET due to task heartbeat timeout!\r\n");

        /* 等待一段时间让串口输出完成, 然后 IWDG 复位 */
        for (volatile uint32_t i = 0; i < 100000; i++) { }

        /* 不再喂 IWDG, 4秒后系统复位 */
        while (1)
        {
            /* IWDG will reset the system */
        }
    }

    /* 系统健康: 喂硬件看门狗 */
    IWDG_ReloadCounter();
}

/**
 * @brief  打印所有任务心跳状态 (用于调试)
 */
void Watchdog_PrintStatus(void)
{
    TickType_t xNow = xTaskGetTickCount();

    printf("[WDT] === Task Heartbeat Status ===\r\n");
    for (uint8_t i = 0; i < WDT_TASK_COUNT; i++)
    {
        if (g_atTaskHeartbeats[i].ucEnabled)
        {
            TickType_t xElapsed = xNow - g_atTaskHeartbeats[i].ulLastHeartbeat;
            printf("[WDT] %-16s | Last: %lu ms ago | Missed: %lu\r\n",
                   g_atTaskHeartbeats[i].pcTaskName,
                   (unsigned long)pdTICKS_TO_MS(xElapsed),
                   (unsigned long)g_atTaskHeartbeats[i].ulMissedCount);
        }
    }
    printf("[WDT] =============================\r\n");
}

/**
 * @brief  系统是否健康
 */
uint8_t Watchdog_IsSystemHealthy(void)
{
    for (uint8_t i = 0; i < WDT_TASK_COUNT; i++)
    {
        if (g_atTaskHeartbeats[i].ucEnabled &&
            g_atTaskHeartbeats[i].ulMissedCount >= MAX_MISSED_BEFORE_RESET)
        {
            return 0;
        }
    }
    return 1;
}
