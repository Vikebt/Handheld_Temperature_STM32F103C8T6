/**
 * @file    flash_store.h
 * @brief   Flash 参数存储模块
 * @note    利用 STM32F103C8T6 最后一页 (64KB Flash, 最后1KB)
 *          存储系统配置参数，支持掉电保存、恢复出厂、CRC校验
 *          参数在系统启动时加载到 RAM 中
 */

#ifndef __FLASH_STORE_H
#define __FLASH_STORE_H

#include "stm32f10x.h"
#include <stdint.h>

/* Flash 存储区域 (STM32F103C8T6: 64KB, 最后1页 1KB) */
#define FLASH_STORE_PAGE_START     0x0800FC00
#define FLASH_STORE_PAGE_END       0x0800FFFF
#define FLASH_STORE_MAGIC          0xA55A5AA5   /* 魔数标识有效数据 */

/* 存储参数版本 (用于向前兼容) */
#define FLASH_STORE_VERSION        0x0002
#define FLASH_STORE_TAIL           0x5AA55AA5UL

/* ============== 系统配置参数结构体 ============== */
#pragma pack(1)
typedef struct
{
    /* 头部 */
    uint32_t  ulMagic;                        /* 魔数 (标识数据有效) */
    uint32_t  ulVersion;                      /* 版本号 */
    uint32_t  ulCRC32;                        /* CRC32: whole record with this field zeroed */

    /* WiFi 配置 */
    char      cWiFiSSID[32];                  /* WiFi 名称 */
    char      cWiFiPassword[32];              /* WiFi 密码 */

    /* 服务器配置 */
    char     cServerIP[16];                   /* 服务器 IP */
    uint16_t usServerPort;                    /* 服务器端口 */

    /* 设备信息 */
    uint8_t  ucDeviceID;                      /* 设备 ID */
    uint8_t  aucReserved1[3];                 /* 保留 */

    /* 校准参数 */
    int16_t  iTempCalibOffset;                /* 温度校准偏移 (*100) */
    int16_t  iTempCalibGain;                  /* 温度校准增益 (*10000) */
    uint16_t usBatteryCalibRef;               /* 电池 ADC 参考校准 */

    /* 系统配置 */
    uint8_t  ucUploadIntervalSec;             /* 上传间隔 (秒) */
    uint8_t  ucLowBatteryThreshold;           /* 低电量阈值 (百分比) */
    uint8_t  ucTempAlarmHigh;                 /* 温度上限告警 (°C) */
    uint8_t  ucTempAlarmLow;                  /* 温度下限告警 (°C) */

    /* 保留 */
    uint8_t  aucReserved2[96];                /* 保留填充 */

    /* 尾部 */
    uint32_t  ulTail;                         /* 尾部标志 0x5AA55AA5 */
} SystemConfig_t;
#pragma pack()

/* 函数原型 */
uint8_t  FlashStore_Init(void);               /* 从Flash加载配置 */
uint8_t  FlashStore_Save(void);               /* 保存配置到Flash */
uint8_t  FlashStore_Load(void);               /* 从Flash加载配置 */
void     FlashStore_SetDefaults(void);        /* 恢复出厂设置 */
uint8_t  FlashStore_CRC_Verify(void);         /* CRC 校验 */
void     FlashStore_PrintConfig(void);        /* 打印当前配置 */

/* 全局配置指针 (可直接访问) */
extern SystemConfig_t g_tSystemConfig;

#endif /* __FLASH_STORE_H */
