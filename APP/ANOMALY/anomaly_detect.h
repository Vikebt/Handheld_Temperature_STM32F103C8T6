/**
 * @file    anomaly_detect.h
 * @brief   数据异常检测模块
 * @note    实现边缘智能: 在 MCU 端实时检测温度异常趋势
 *          无需等待云端分析，本地即可触发告警
 *          检测算法: 突变检测 + 漂移检测 + 范围检测 + 卡死检测
 */

#ifndef __ANOMALY_DETECT_H
#define __ANOMALY_DETECT_H

#include "stm32f10x.h"
#include <stdint.h>

/* 异常类型枚举 */
typedef enum
{
    ANOMALY_NONE         = 0,         /* 无异常 */
    ANOMALY_SPIKE        = (1 << 0),  /* 温度突变 */
    ANOMALY_DRIFT        = (1 << 1),  /* 传感器漂移 */
    ANOMALY_OVER_RANGE   = (1 << 2),  /* 超限 */
    ANOMALY_STUCK        = (1 << 3),  /* 数据卡死 */
    ANOMALY_SENSOR_FAIL  = (1 << 4),  /* 传感器失效 */
} AnomalyType_t;

/* 异常检测配置 */
typedef struct
{
    int16_t  iSpikeThreshold;          /* 突变阈值 (*100/次) 例如: 200 = 2.0°C */
    int16_t  iMaxTemp;                 /* 温度上限 (*100) */
    int16_t  iMinTemp;                 /* 温度下限 (*100) */
    uint8_t  ucStuckCountThreshold;    /* 卡死判定: 连续多少值不变 */
    uint8_t  ucDriftWindowSize;        /* 漂移检测窗口大小 */
} AnomalyConfig_t;

/* 异常检测器状态 */
typedef struct
{
    int16_t  aiHistory[10];            /* 最近10次温度值历史 */
    uint8_t  ucHistoryIndex;           /* 历史索引 */
    uint8_t  ucStuckCounter;           /* 卡死计数器 */
    uint8_t  ucTotalAnomalies;         /* 总异常次数 */
    uint32_t ulLastAnomalyTick;        /* 上次异常时间 */
    uint8_t  ucAnomalyFlags;           /* 当前异常标志 (位域) */
} AnomalyDetector_t;

/* 函数原型 */
void     Anomaly_Init(AnomalyDetector_t *pDetector, const AnomalyConfig_t *pConfig);
uint8_t  Anomaly_Feed(AnomalyDetector_t *pDetector, int16_t iValue);
void     Anomaly_PrintStatus(const AnomalyDetector_t *pDetector);
uint8_t  Anomaly_GetFlags(const AnomalyDetector_t *pDetector);
void     Anomaly_ClearFlags(AnomalyDetector_t *pDetector);
const char* Anomaly_TypeToString(uint8_t ucFlag);

/* 默认配置 */
extern const AnomalyConfig_t g_kDefaultAnomalyConfig;

#endif /* __ANOMALY_DETECT_H */
