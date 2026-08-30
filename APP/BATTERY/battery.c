/**
 * @file    battery.c
 * @brief   电池电压监控驱动实现
 * @note    ADC1 规则组单次转换模式, 使用 PA1 通道
 *          实现滑动平均滤波和电量百分比估算
 *          低电量时通过事件标志组向系统发送告警
 */

#include "battery.h"
#include "data_fusion.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include <stdio.h>

/* 外部事件标志组 (用于发送低电告警) */
extern EventGroupHandle_t g_xSystemEventGroup;

/* 全局电池信息 */
static Battery_Info_t g_tBatteryInfo;
Battery_Info_t *g_pBatteryInfo = &g_tBatteryInfo;

/* 内部函数 */
static uint16_t prvReadADC(void);
static uint8_t  prvConvertToPercent(uint16_t usVoltageMV);

/**
 * @brief  初始化电池监控
 * @note   配置 ADC1: 单次转换, 12-bit, PA1 通道
 */
void Battery_Init(void)
{
    GPIO_InitTypeDef gpioInit;
    ADC_InitTypeDef  adcInit;

    /* 初始化数据 */
    g_tBatteryInfo.usVoltageMV    = 0;
    g_tBatteryInfo.ucPercentage   = 0;
    g_tBatteryInfo.eState         = BATTERY_STATE_NORMAL;
    g_tBatteryInfo.ucIsCharging   = 0;
    g_tBatteryInfo.ucHistoryIndex = 0;
    g_tBatteryInfo.ulSampleCount  = 0;
    for (uint8_t i = 0; i < 10; i++)
    {
        g_tBatteryInfo.usHistory[i] = 0;
    }

    /* 使能时钟 */
    RCC_APB2PeriphClockCmd(BATTERY_ADC_RCC | RCC_APB2Periph_ADC1, ENABLE);

    /* 配置 PA1 为模拟输入 */
    gpioInit.GPIO_Pin = BATTERY_ADC_PIN;
    gpioInit.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(BATTERY_ADC_PORT, &gpioInit);

    /* ADC 配置 */
    ADC_DeInit(ADC1);
    adcInit.ADC_Mode = ADC_Mode_Independent;
    adcInit.ADC_ScanConvMode = DISABLE;
    adcInit.ADC_ContinuousConvMode = DISABLE;
    adcInit.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adcInit.ADC_DataAlign = ADC_DataAlign_Right;
    adcInit.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &adcInit);

    /* 配置 ADC 通道 (采样时间 55.5 周期) */
    ADC_RegularChannelConfig(ADC1, BATTERY_ADC_CHANNEL, 1, ADC_SampleTime_55Cycles5);

    /* 校准 ADC */
    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

/**
 * @brief  单次 ADC 转换
 * @retval 12-bit ADC 原始值 (0-4095)
 */
static uint16_t prvReadADC(void)
{
    ADC_RegularChannelConfig(ADC1, BATTERY_ADC_CHANNEL, 1, ADC_SampleTime_55Cycles5);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    return (uint16_t)ADC_GetConversionValue(ADC1);
}

/**
 * @brief  根据电压估算电量百分比 (锂电池近似曲线)
 * @param  usVoltageMV  当前电压 (mV)
 * @retval 0-100 百分比
 */
static uint8_t prvConvertToPercent(uint16_t usVoltageMV)
{
    /* 简单的线性映射 + 电压查表修正 */
    const uint16_t aucVoltageTable[] = {
        4200, 4100, 3950, 3850, 3750, 3650, 3550, 3450, 3300, 3100, 2800
    };
    const uint8_t  aucPercentTable[] = {
        100,   90,   80,   70,   60,   50,   40,   30,   20,   10,   0
    };

    if (usVoltageMV >= aucVoltageTable[0]) return 100;
    if (usVoltageMV <= aucVoltageTable[10]) return 0;

    for (uint8_t i = 0; i < 10; i++)
    {
        if (usVoltageMV >= aucVoltageTable[i + 1])
        {
            /* 线性插值 */
            uint16_t usRange = aucVoltageTable[i] - aucVoltageTable[i + 1];
            uint16_t usOffset = usVoltageMV - aucVoltageTable[i + 1];
            return (uint8_t)(aucPercentTable[i + 1] +
                   (uint16_t)(aucPercentTable[i] - aucPercentTable[i + 1]) * usOffset / usRange);
        }
    }

    return 0;
}

/**
 * @brief  更新电池状态 (由任务定期调用)
 * @note   包含滑动平均滤波、电量估算、低电告警
 */
void Battery_Update(void)
{
    uint32_t ulSum = 0;
    uint16_t usRawADC;
    uint16_t usVoltageMV;
    BatteryState_t eNewState;

    /* 多次采样取平均 (防抖) */
    for (uint8_t i = 0; i < 5; i++)
    {
        usRawADC = prvReadADC();
        ulSum += usRawADC;
    }

    usRawADC = (uint16_t)(ulSum / 5);

    /* 换算为真实电压: ADC值 / 4096 * Vref * 分压比 * 1000 (转为mV) */
    usVoltageMV = (uint16_t)((float)usRawADC / BATTERY_ADC_RESOLUTION *
                  BATTERY_REF_VOLTAGE * BATTERY_DIVIDER_RATIO * 1000.0f);

    /* 更新当前电压 */
    g_tBatteryInfo.usVoltageMV = usVoltageMV;

    /* 更新历史记录 */
    g_tBatteryInfo.usHistory[g_tBatteryInfo.ucHistoryIndex] = usVoltageMV;
    g_tBatteryInfo.ucHistoryIndex = (g_tBatteryInfo.ucHistoryIndex + 1) % 10;

    /* 更新电量百分比 */
    g_tBatteryInfo.ucPercentage = prvConvertToPercent(usVoltageMV);

    /* 更新电池状态 */
    if (usVoltageMV <= BATTERY_SHUTDOWN_MV)
    {
        eNewState = BATTERY_STATE_SHUTDOWN;
    }
    else if (usVoltageMV <= BATTERY_CRITICAL_MV)
    {
        eNewState = BATTERY_STATE_CRITICAL;
    }
    else if (usVoltageMV <= BATTERY_WARNING_MV)
    {
        eNewState = BATTERY_STATE_WARNING;
    }
    else
    {
        eNewState = BATTERY_STATE_NORMAL;
    }

    /* 状态发生变化时发送事件标志 */
    if (eNewState != g_tBatteryInfo.eState)
    {
        g_tBatteryInfo.eState = eNewState;

        if (g_xSystemEventGroup != NULL)
        {
            if (eNewState >= BATTERY_STATE_WARNING)
            {
                xEventGroupSetBits(g_xSystemEventGroup, EVENT_BIT_LOW_BATTERY);
            }
            else
            {
                xEventGroupClearBits(g_xSystemEventGroup, EVENT_BIT_LOW_BATTERY);
            }
        }
    }

    g_tBatteryInfo.ulSampleCount++;
}

/**
 * @brief  获取当前电压 (mV)
 */
uint16_t Battery_GetVoltageMV(void)
{
    return g_tBatteryInfo.usVoltageMV;
}

/**
 * @brief  获取电量百分比
 */
uint8_t Battery_GetPercentage(void)
{
    return g_tBatteryInfo.ucPercentage;
}

/**
 * @brief  获取电池状态
 */
BatteryState_t Battery_GetState(void)
{
    return g_tBatteryInfo.eState;
}

/**
 * @brief  是否低电量
 */
uint8_t Battery_IsLow(void)
{
    return (g_tBatteryInfo.eState >= BATTERY_STATE_WARNING) ? 1 : 0;
}

/**
 * @brief  是否严重低电
 */
uint8_t Battery_IsCritical(void)
{
    return (g_tBatteryInfo.eState >= BATTERY_STATE_CRITICAL) ? 1 : 0;
}

/**
 * @brief  打印电池信息到串口
 */
void Battery_PrintInfo(void)
{
    printf("[BATTERY] Voltage: %umV, %u%%, State: %d\r\n",
           g_tBatteryInfo.usVoltageMV,
           g_tBatteryInfo.ucPercentage,
           (int)g_tBatteryInfo.eState);
}
