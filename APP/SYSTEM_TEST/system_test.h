/**
 * @file    system_test.h
 * @brief   系统自检模块 — 启动时逐项诊断外设状态
 * @note    对 MLX90614、MFRC522、ESP8266、ADC、I2C、SPI、Flash 等
 *          关键外设执行功能验证，结果输出到串口和 OLED
 *          自检结果可通过事件标志组上报
 */

#ifndef __SYSTEM_TEST_H
#define __SYSTEM_TEST_H

#include "stm32f10x.h"
#include <stdint.h>

/* 自检项 ID 枚举 */
typedef enum
{
    TEST_ID_MLX90614   = 0,     /* 温度传感器 */
    TEST_ID_MFRC522    = 1,     /* RFID 模块 */
    TEST_ID_ESP8266    = 2,     /* Wi-Fi 模块 */
    TEST_ID_ADC        = 3,     /* ADC (电池) */
    TEST_ID_I2C_BUS    = 4,     /* I2C 总线 (24C02) */
    TEST_ID_SPI_BUS    = 5,     /* SPI 总线 */
    TEST_ID_FLASH      = 6,     /* Flash 存储 */
    TEST_ID_RTC        = 7,     /* RTC 实时时钟 */
    TEST_ID_COUNT      = 8      /* 测试项总数 */
} SystemTestID_t;

/* 自检结果 */
typedef enum
{
    TEST_PASS = 0,               /* 通过 */
    TEST_FAIL,                   /* 失败 */
    TEST_SKIP                    /* 需要实物或外部环境 */
} TestResult_t;

/* 自检项结果结构体 */
typedef struct
{
    TestResult_t   eResult;
    const char    *pcItemName;
    uint32_t       ulDetailCode;     /* 详细信息代码 */
    const char    *pcDetailStr;      /* 详细信息字符串 */
} TestItemResult_t;

/* 整体自检结果结构体 */
typedef struct
{
    TestItemResult_t atItems[TEST_ID_COUNT];
    uint8_t          ucPassCount;
    uint8_t          ucFailCount;
    uint8_t          ucSkipCount;
    uint8_t          ucAllPassed;     /* 1=全部通过 */
} SystemTestReport_t;

/* 函数原型 */
void     SystemTest_RunAll(SystemTestReport_t *pReport);
void     SystemTest_RunSingle(SystemTestReport_t *pReport, SystemTestID_t eTestID);
void     SystemTest_PrintReport(const SystemTestReport_t *pReport);
uint8_t  SystemTest_IsAllPassed(const SystemTestReport_t *pReport);
void     SystemTest_OLED_ShowResult(const SystemTestReport_t *pReport);
void     SystemTest_LED_Indicate(const SystemTestReport_t *pReport);

#endif /* __SYSTEM_TEST_H */
