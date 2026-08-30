/**
 * @file    data_fusion.h
 * @brief   多传感器数据融合模块头文件
 * @note    基于 FreeRTOS 事件标志组实现"即测即传"数据流
 *          协调 MLX90614 温度传感器 + MFRC522 RFID 的数据采集与 WiFi 上传
 *          包含数据校验、异常检测、看门狗更新功能
 */

#ifndef __DATA_FUSION_H
#define __DATA_FUSION_H

#include "stm32f10x.h"
#include <stdint.h>

/* Event group contract used by the application tasks. */
#define EVENT_BIT_TEMPERATURE_READY     (1U << 0)
#define EVENT_BIT_RFID_READY            (1U << 1)
#define EVENT_BIT_UPLOAD_TRIGGER        (1U << 2)
#define EVENT_BIT_LOW_BATTERY           (1U << 3)
#define EVENT_BIT_SENSOR_FAULT          (1U << 4)

/* 数据融合状态结构体 */
typedef struct
{
    volatile int16_t   iTemperature1;         /* 温度传感器1值 (*100) */
    volatile int16_t   iTemperature2;         /* 温度传感器2值 (*100) */
    volatile int16_t   iAmbientTemp;          /* 环境温度 (*100) */
    volatile uint8_t   aucCardUID[4];         /* RFID 卡 UID */
    volatile uint8_t   ucCardDetected;        /* 卡检测标志 */
    volatile uint16_t  usBatteryMV;           /* 电池电压 (mV) */
    volatile uint8_t   ucUploadPending;       /* 待上传标志 */
    volatile uint32_t  ulLastUploadTick;      /* 上次上传时间 (Ticks) */
    volatile uint32_t  ulFusionCount;         /* 融合处理计数 */
} DataFusion_t;

/* 上传数据包格式 */
#pragma pack(1)
typedef struct
{
    uint16_t  usHeader;          /* 帧头 0xAA55 */
    uint8_t   ucDeviceID;        /* 设备ID */
    int16_t   iTemp1;            /* 温度1 */
    int16_t   iTemp2;            /* 温度2 */
    int16_t   iAmbient;          /* 环境温度 */
    uint32_t  ulCardUID;         /* 卡片 UID (合并为32位) */
    uint16_t  usBatteryMV;       /* 电池电压 */
    uint8_t   ucBatteryPct;      /* 电池百分比 */
    uint16_t  usCRC16;           /* CRC16 校验 */
    uint16_t  usTail;            /* 帧尾 0x55AA */
} DataPacket_t;
#pragma pack()

/* 函数原型 */
void     DataFusion_Init(void);
uint8_t  DataFusion_Feed(int16_t iTemp1, int16_t iTemp2, int16_t iAmbient,
                         uint8_t *pucCardUID, uint8_t ucCardDetected,
                         uint16_t usBatteryMV, uint8_t ucBatteryPct);
uint8_t  DataFusion_PreparePacket(DataPacket_t *pPacket);
void     DataFusion_SetUploadFlag(void);
void     DataFusion_ClearUploadFlag(void);
uint8_t  DataFusion_IsUploadPending(void);
void     DataFusion_Reset(void);

/* 全局数据融合结构体指针 */
extern DataFusion_t *g_pDataFusion;

#endif /* __DATA_FUSION_H */
