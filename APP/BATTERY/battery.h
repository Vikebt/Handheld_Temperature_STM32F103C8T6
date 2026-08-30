/**
 * @file    battery.h
 * @brief   电池电压监控驱动头文件
 * @note    使用 ADC1 通道 (PA1) 采集电池电压分压值
 *          通过电阻分压 + 软件换算得到真实电压
 *          实现低电量报警、百分比估算、电压历史记录功能
 */

#ifndef __BATTERY_H
#define __BATTERY_H

#include "stm32f10x.h"
#include <stdint.h>

/* 电池参数配置 */
#define BATTERY_ADC_CHANNEL         ADC_Channel_1         /* PA1 */
#define BATTERY_ADC_PORT            GPIOA
#define BATTERY_ADC_PIN             GPIO_Pin_1
#define BATTERY_ADC_RCC             RCC_APB2Periph_GPIOA

#define BATTERY_DIVIDER_RATIO       2.0f                  /* 分压比 (可根据实际硬件修改) */
#define BATTERY_REF_VOLTAGE         3.3f                  /* ADC 参考电压 */
#define BATTERY_ADC_RESOLUTION      4095                  /* 12-bit ADC */

/* 电池状态阈值 (单位: mV) */
#define BATTERY_FULL_MV             4200                  /* 满电电压 (4.2V 锂电池) */
#define BATTERY_WARNING_MV          3600                  /* 低电量警告阈值 */
#define BATTERY_CRITICAL_MV         3200                  /* 严重低电阈值 */
#define BATTERY_SHUTDOWN_MV         3000                  /* 自动关机阈值 */
#define BATTERY_EMPTY_MV            2800                  /* 空电电压 */

/* 电池状态枚举 */
typedef enum
{
    BATTERY_STATE_NORMAL = 0,        /* 正常 */
    BATTERY_STATE_WARNING,           /* 低电量警告 */
    BATTERY_STATE_CRITICAL,          /* 严重低电 */
    BATTERY_STATE_SHUTDOWN           /* 需要关机 */
} BatteryState_t;

/* 电池信息结构体 */
typedef struct
{
    volatile uint16_t      usVoltageMV;           /* 当前电压 (mV) */
    volatile uint8_t       ucPercentage;          /* 电量百分比 (0-100) */
    volatile BatteryState_t eState;               /* 电池状态 */
    volatile uint8_t       ucIsCharging;          /* 是否在充电 */
    volatile uint16_t      usHistory[10];         /* 最近10次电压历史 (环形) */
    volatile uint8_t       ucHistoryIndex;        /* 历史记录索引 */
    volatile uint32_t      ulSampleCount;         /* 采样计数 */
} Battery_Info_t;

/* 函数原型 */
void     Battery_Init(void);
void     Battery_Update(void);
uint16_t Battery_GetVoltageMV(void);
uint8_t  Battery_GetPercentage(void);
BatteryState_t Battery_GetState(void);
uint8_t  Battery_IsLow(void);
uint8_t  Battery_IsCritical(void);
void     Battery_PrintInfo(void);

/* 全局电池信息指针 */
extern Battery_Info_t *g_pBatteryInfo;

#endif /* __BATTERY_H */
