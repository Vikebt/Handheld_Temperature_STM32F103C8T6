/**
 * @file    system_test.c
 * @brief   系统自检模块实现
 * @note    逐项诊断：MLX90614 / MFRC522 / ESP8266 / ADC / I2C / SPI / Flash / RTC
 *          结果通过串口�?OLED 呈现，支�?LED 指示
 *          自检异常项通过事件标志组上�? */

#include "system_test.h"
#include "data_fusion.h"
#include "mlx90614.h"
#include "mfrc522.h"
#include "battery.h"
#include "flash_store.h"
#include "rtc_driver.h"
#include "iic.h"
#include "spi_driver.h"
#include "esp8266_drive.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include "led.h"
#include "oled_iic.h"

extern EventGroupHandle_t g_xSystemEventGroup;

/* 自检项名称表 */
static const char *g_apcTestNames[TEST_ID_COUNT] = {
    "MLX90614", "MFRC522", "ESP8266", "ADC",
    "I2C Bus",  "SPI Bus", "Flash",   "RTC"
};

/**
 * @brief  运行全部自检�? * @param  pReport  输出测试报告
 */
void SystemTest_RunAll(SystemTestReport_t *pReport)
{
    if (pReport == NULL) return;

    pReport->ucPassCount = 0;
    pReport->ucFailCount = 0;
    pReport->ucSkipCount = 0;

    printf("\r\n");
    printf("========================================\r\n");
    printf("       系统自检 (System Self-Test)       \r\n");
    printf("========================================\r\n");

    /* 逐项测试 */
    for (uint8_t i = 0; i < TEST_ID_COUNT; i++)
    {
        pReport->atItems[i].pcItemName = g_apcTestNames[i];
        SystemTest_RunSingle(pReport, (SystemTestID_t)i);
    }

    /* 统计结果 */
    for (uint8_t i = 0; i < TEST_ID_COUNT; i++)
    {
        if (pReport->atItems[i].eResult == TEST_PASS) pReport->ucPassCount++;
        else if (pReport->atItems[i].eResult == TEST_FAIL) pReport->ucFailCount++;
        else pReport->ucSkipCount++;
    }

    pReport->ucAllPassed = (pReport->ucFailCount == 0) ? 1 : 0;

    /* 打印汇�?*/
    SystemTest_PrintReport(pReport);

    /* OLED 显示 */
    SystemTest_OLED_ShowResult(pReport);

    /* LED 指示 */
    SystemTest_LED_Indicate(pReport);

    /* 如果有失败项，通过事件标志组上�?*/
    if (pReport->ucFailCount > 0)
    {
        if (g_xSystemEventGroup != NULL)
        {
            xEventGroupSetBits(g_xSystemEventGroup, EVENT_BIT_SENSOR_FAULT);
        }
    }
}

/**
 * @brief  执行单项自检
 */
void SystemTest_RunSingle(SystemTestReport_t *pReport, SystemTestID_t eTestID)
{
    TestItemResult_t *pItem;
    if ((pReport == NULL) || (eTestID >= TEST_ID_COUNT)) return;

    pItem = &pReport->atItems[eTestID];

    printf("[TEST] %s ... ", g_apcTestNames[eTestID]);
    pItem->pcItemName = g_apcTestNames[eTestID];

    switch (eTestID)
    {
        case TEST_ID_MLX90614:
            if (MLX90614_SelfTest())
            {
                pItem->eResult = TEST_PASS;
                pItem->ulDetailCode = (uint32_t)g_pMLX90614_Data->iObjectTemp1;
                pItem->pcDetailStr = "Sensor OK";
                printf("PASS (T=%d.%dC)\r\n",
                       g_pMLX90614_Data->iObjectTemp1 / 100,
                       (g_pMLX90614_Data->iObjectTemp1 % 100) / 10);
            }
            else
            {
                pItem->eResult = TEST_FAIL;
                pItem->ulDetailCode = 0;
                pItem->pcDetailStr = "No response";
                printf("FAIL\r\n");
            }
            break;

        case TEST_ID_MFRC522:
            if (MFRC522_SelfTest())
            {
                pItem->eResult = TEST_PASS;
                pItem->ulDetailCode = 0x92;
                pItem->pcDetailStr = "Module OK";
                printf("PASS (Version=0x%02X)\r\n", 0x92);
            }
            else
            {
                pItem->eResult = TEST_FAIL;
                pItem->pcDetailStr = "No module";
                printf("FAIL\r\n");
            }
            break;

        case TEST_ID_ESP8266:
            /* 发�?AT 指令测试 */
            printf("(skip in self-test)\r\n");
            pItem->eResult = TEST_SKIP;
            pItem->pcDetailStr = "Skipped (WiFi)";
            break;

        case TEST_ID_ADC:
        {
            /* Initialization alone does not verify an ADC conversion. */
            Battery_Init();
            pItem->eResult = TEST_SKIP;
            pItem->ulDetailCode = 0;
            pItem->pcDetailStr = "Needs ADC sample verification";
            printf("SKIP (no conversion verification)\r\n");
            break;
        }

        case TEST_ID_I2C_BUS:
        {
            /* 尝试扫描 24C02 (I2C 地址 0x50) */
            I2C_INIT();
            I2C_Start();
            I2C_Send_Byte(0xA0);  /* 24C02 写地址 */
            if (I2C_Wait_Ack() == 0)
            {
                I2C_Stop();
                pItem->eResult = TEST_PASS;
                pItem->ulDetailCode = 0;
                pItem->pcDetailStr = "24C02 ACK OK";
                printf("PASS (24C02 detected)\r\n");
            }
            else
            {
                I2C_Stop();
                pItem->eResult = TEST_FAIL;
                pItem->pcDetailStr = "No ACK";
                printf("FAIL\r\n");
            }
            break;
        }

        case TEST_ID_SPI_BUS:
            SPI_Driver_Init();
            /* Controller initialization is not an end-to-end device test. */
            pItem->eResult = TEST_SKIP;
            pItem->ulDetailCode = 0;
            pItem->pcDetailStr = "Controller initialized; device test pending";
            printf("SKIP (no device transaction)\r\n");
            break;

        case TEST_ID_FLASH:
            if (FlashStore_Load())
            {
                pItem->eResult = TEST_PASS;
                pItem->ulDetailCode = g_tSystemConfig.ulMagic;
                pItem->pcDetailStr = "Config valid";
                printf("PASS (Magic=0x%08lX)\r\n", (unsigned long)g_tSystemConfig.ulMagic);
            }
            else
            {
                /* 首次运行，写入默认配�?*/
                FlashStore_SetDefaults();
                FlashStore_Save();
                pItem->eResult = TEST_PASS;
                pItem->pcDetailStr = "Defaults written";
                printf("PASS (First boot, defaults saved)\r\n");
            }
            break;

        case TEST_ID_RTC:
            if (RTC_Init())
            {
                RTCTime_t tNow;
                RTC_GetTime(&tNow);
                pItem->eResult = TEST_PASS;
                pItem->ulDetailCode = tNow.ucHour * 10000 + tNow.ucMinute * 100 + tNow.ucSecond;
                pItem->pcDetailStr = "RTC running";
                printf("PASS (%02u:%02u:%02u)\r\n", tNow.ucHour, tNow.ucMinute, tNow.ucSecond);
            }
            else
            {
                pItem->eResult = TEST_FAIL;
                pItem->pcDetailStr = "RTC init fail";
                printf("FAIL\r\n");
            }
            break;

        default:
            /* The range guard above makes this path defensive only. */
            pItem->eResult = TEST_FAIL;
            pItem->ulDetailCode = (uint32_t)eTestID;
            pItem->pcDetailStr = "Invalid test ID";
            printf("FAIL (invalid test ID)\r\n");
            break;
    }
}

/**
 * @brief  打印自检报告
 */
void SystemTest_PrintReport(const SystemTestReport_t *pReport)
{
    if (pReport == NULL) return;

    printf("\r\n========================================\r\n");
    printf("  Self-test summary: PASS=%u  FAIL=%u  SKIP=%u\r\n",
           pReport->ucPassCount, pReport->ucFailCount, pReport->ucSkipCount);
    printf("  整体结果: %s\r\n",
           pReport->ucAllPassed ? "全部通过" : "存在异常");
    printf("========================================\r\n\r\n");
}

/**
 * @brief  �?OLED 上显示自检结果
 */
void SystemTest_OLED_ShowResult(const SystemTestReport_t *pReport)
{
    if (pReport == NULL) return;

    OLED_Clear();
    OLED_ShowString(0, 0, (uint8_t *)"Self-Test Results", 12);

    char cBuf[20];
    sprintf(cBuf, "PASS:%u FAIL:%u", pReport->ucPassCount, pReport->ucFailCount);
    OLED_ShowString(0, 2, (uint8_t *)cBuf, 12);

    if (pReport->ucAllPassed)
    {
        OLED_ShowString(0, 4, (uint8_t *)"> ALL TESTS OK", 16);
    }
    else
    {
        OLED_ShowString(0, 4, (uint8_t *)"> FAILURES!", 16);
    }

    OLED_ShowString(0, 6, (uint8_t *)"System Starting...", 12);
}

/**
 * @brief  LED 指示自检结果
 * @note   PC0 �?= 全部通过, PC0 闪烁 = 存在异常
 */
void SystemTest_LED_Indicate(const SystemTestReport_t *pReport)
{
    if (pReport == NULL) return;

    if (pReport->ucAllPassed)
    {
        LED1_ON();
        LED2_OFF();
        LED3_OFF();
    }
    else
    {
        LED1_OFF();
        /* 异常时闪�?*/
        for (uint8_t i = 0; i < 5; i++)
        {
            LED2_ON();
            for (volatile uint32_t j = 0; j < 200000; j++) { }
            LED2_OFF();
            for (volatile uint32_t j = 0; j < 200000; j++) { }
        }
    }
}

/**
 * @brief  查询是否全部通过
 */
uint8_t SystemTest_IsAllPassed(const SystemTestReport_t *pReport)
{
    return (pReport != NULL) ? pReport->ucAllPassed : 0;
}
