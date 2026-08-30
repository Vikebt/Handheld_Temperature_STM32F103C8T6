/**
 * @file    anomaly_detect.c
 * @brief   数据异常检测模块实现
 * @note    实现四种边缘检测算法:
 *          1. 突变检测 (Spike): 两次采集间温度变化超过阈值
 *          2. 漂移检测 (Drift): 窗口内温度缓慢偏离正常范围
 *          3. 超限检测 (Over Range): 温度超出设定上下限
 *          4. 卡死检测 (Stuck): 连续多次温度值完全不变化 (传感器故障)
 */

#include "anomaly_detect.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* 默认异常检测配置 */
const AnomalyConfig_t g_kDefaultAnomalyConfig = {
    .iSpikeThreshold       = 300,     /* 3.00°C 视为突变 */
    .iMaxTemp              = 4200,    /* 42.00°C 上限 */
    .iMinTemp              = 1000,    /* 10.00°C 下限 (测额头不会低于室温太多) */
    .ucStuckCountThreshold = 20,      /* 连续20次不变判定卡死 */
    .ucDriftWindowSize     = 5        /* 5次窗口漂移检测 */
};

/**
 * @brief  初始化异常检测器
 * @param  pDetector  检测器实例
 * @param  pConfig    配置 (NULL=使用默认)
 */
void Anomaly_Init(AnomalyDetector_t *pDetector, const AnomalyConfig_t *pConfig)
{
    if (pDetector == NULL) return;

    if (pConfig == NULL)
    {
        pConfig = &g_kDefaultAnomalyConfig;
    }

    pDetector->ucHistoryIndex    = 0;
    pDetector->ucStuckCounter    = 0;
    pDetector->ucTotalAnomalies  = 0;
    pDetector->ulLastAnomalyTick = 0;
    pDetector->ucAnomalyFlags    = ANOMALY_NONE;

    for (uint8_t i = 0; i < 10; i++)
    {
        pDetector->aiHistory[i] = 0;
    }
}

/**
 * @brief  注入新数据并执行异常检测
 * @param  pDetector  检测器实例
 * @param  iValue     最新温度值 (*100)
 * @retval 当前异常标志 (位域组合)
 */
uint8_t Anomaly_Feed(AnomalyDetector_t *pDetector, int16_t iValue)
{
    if (pDetector == NULL) return ANOMALY_NONE;

    uint8_t ucNewFlags = ANOMALY_NONE;
    int16_t iPrevValue;

    /* 获取上一次的值 (环形缓冲区) */
    uint8_t ucPrevIdx = (pDetector->ucHistoryIndex == 0) ?
                        9 : (pDetector->ucHistoryIndex - 1);
    iPrevValue = pDetector->aiHistory[ucPrevIdx];

    /* ====== 1. 突变检测 (Spike) ====== */
    if (pDetector->ucHistoryIndex > 0)
    {
        int16_t iDelta = (iValue > iPrevValue) ?
                         (iValue - iPrevValue) : (iPrevValue - iValue);

        if (iDelta > g_kDefaultAnomalyConfig.iSpikeThreshold)
        {
            ucNewFlags |= ANOMALY_SPIKE;
            printf("[ANOMALY] SPIKE: %d.%d -> %d.%d (Δ=%d.%d)\r\n",
                   iPrevValue / 100, (iPrevValue % 100) / 10,
                   iValue / 100, (iValue % 100) / 10,
                   iDelta / 100, (iDelta % 100) / 10);
        }
    }

    /* ====== 2. 超限检测 (Over Range) ====== */
    if (iValue > g_kDefaultAnomalyConfig.iMaxTemp ||
        iValue < g_kDefaultAnomalyConfig.iMinTemp)
    {
        ucNewFlags |= ANOMALY_OVER_RANGE;
        printf("[ANOMALY] OVER-RANGE: %d.%d (limit: %d.%d - %d.%d)\r\n",
               iValue / 100, (iValue % 100) / 10,
               g_kDefaultAnomalyConfig.iMinTemp / 100,
               (g_kDefaultAnomalyConfig.iMinTemp % 100) / 10,
               g_kDefaultAnomalyConfig.iMaxTemp / 100,
               (g_kDefaultAnomalyConfig.iMaxTemp % 100) / 10);
    }

    /* ====== 3. 卡死检测 (Stuck) ====== */
    if (pDetector->ucHistoryIndex > 0 && iValue == iPrevValue)
    {
        pDetector->ucStuckCounter++;
        if (pDetector->ucStuckCounter >= g_kDefaultAnomalyConfig.ucStuckCountThreshold)
        {
            ucNewFlags |= ANOMALY_STUCK;
            printf("[ANOMALY] STUCK: Value unchanged at %d.%d for %u samples\r\n",
                   iValue / 100, (iValue % 100) / 10,
                   pDetector->ucStuckCounter);

            if (pDetector->ucStuckCounter >= g_kDefaultAnomalyConfig.ucStuckCountThreshold + 10)
            {
                ucNewFlags |= ANOMALY_SENSOR_FAIL;
                printf("[ANOMALY] SENSOR FAIL: Stuck detected, likely hardware fault!\r\n");
            }
        }
    }
    else if (iValue != iPrevValue)
    {
        /* 温度变化时减少卡死计数 (允许传感器恢复) */
        if (pDetector->ucStuckCounter > 0)
        {
            pDetector->ucStuckCounter--;
        }
    }

    /* ====== 4. 漂移检测 (Drift) ====== */
    if (pDetector->ucHistoryIndex >= g_kDefaultAnomalyConfig.ucDriftWindowSize)
    {
        /* 检查窗口内最小和最大值 */
        int16_t iMin = iValue, iMax = iValue;
        for (uint8_t i = 0; i < g_kDefaultAnomalyConfig.ucDriftWindowSize; i++)
        {
            uint8_t idx = (pDetector->ucHistoryIndex - 1 - i) % 10;
            if (pDetector->aiHistory[idx] < iMin) iMin = pDetector->aiHistory[idx];
            if (pDetector->aiHistory[idx] > iMax) iMax = pDetector->aiHistory[idx];
        }

        /* 窗口内持续单向变化超过阈值视为漂移 */
        int16_t iDrift = (iMax - iMin);
        if (iDrift > g_kDefaultAnomalyConfig.iSpikeThreshold * 2)
        {
            ucNewFlags |= ANOMALY_DRIFT;
        }
    }

    /* 保存到历史 */
    pDetector->aiHistory[pDetector->ucHistoryIndex] = iValue;
    pDetector->ucHistoryIndex = (pDetector->ucHistoryIndex + 1) % 10;

    /* 更新异常标志和计数 */
    if (ucNewFlags != ANOMALY_NONE)
    {
        pDetector->ucAnomalyFlags |= ucNewFlags;
        pDetector->ucTotalAnomalies++;
        pDetector->ulLastAnomalyTick = xTaskGetTickCount();
    }

    return ucNewFlags;
}

/**
 * @brief  打印检测器状态
 */
void Anomaly_PrintStatus(const AnomalyDetector_t *pDetector)
{
    if (pDetector == NULL) return;

    printf("[ANOMALY] Detector status:\r\n");
    printf("  Flags:    0x%02X (", pDetector->ucAnomalyFlags);
    uint8_t ucFlags = pDetector->ucAnomalyFlags;
    uint8_t ucFirst = 1;
    for (uint8_t i = 0; i < 8; i++)
    {
        if (ucFlags & (1 << i))
        {
            if (!ucFirst) printf(", ");
            printf("%s", Anomaly_TypeToString(1 << i));
            ucFirst = 0;
        }
    }
    if (ucFirst) printf("None");
    printf(")\r\n");

    printf("  Total:    %u\r\n", pDetector->ucTotalAnomalies);
    printf("  StuckCnt: %u\r\n", pDetector->ucStuckCounter);

    printf("  History:  ");
    for (uint8_t i = 0; i < 10; i++)
    {
        printf("%d.%d ", pDetector->aiHistory[i] / 100,
               (pDetector->aiHistory[i] % 100) / 10);
    }
    printf("\r\n");
}

/**
 * @brief  获取异常标志
 */
uint8_t Anomaly_GetFlags(const AnomalyDetector_t *pDetector)
{
    return (pDetector != NULL) ? pDetector->ucAnomalyFlags : ANOMALY_NONE;
}

/**
 * @brief  清除异常标志
 */
void Anomaly_ClearFlags(AnomalyDetector_t *pDetector)
{
    if (pDetector != NULL)
    {
        pDetector->ucAnomalyFlags = ANOMALY_NONE;
    }
}

/**
 * @brief  异常类型转字符串
 */
const char *Anomaly_TypeToString(uint8_t ucFlag)
{
    switch (ucFlag)
    {
        case ANOMALY_SPIKE:        return "SPIKE";
        case ANOMALY_DRIFT:        return "DRIFT";
        case ANOMALY_OVER_RANGE:   return "OVER-RANGE";
        case ANOMALY_STUCK:        return "STUCK";
        case ANOMALY_SENSOR_FAIL:  return "SENSOR-FAIL";
        default:                   return "UNKNOWN";
    }
}
