/**
 * @file    main.c
 * @brief   基于 STM32F103C8T6 的智能体温检测仪 — 主程序 v2.0
 * @note    系统架构:
 *          - FreeRTOS V10.5.1 多任务内核
 *          主要外设: MLX90614 (I2C) + MFRC522 (SPI) + ESP8266 (UART)
 *          特色模块: 系统自检 / Flash参数存储 / RTC时间戳 / 异常检测 / 呼吸灯
 *
 * @attention 任务优先级映射:
 *   Pri 4: [Timer Service]    — FreeRTOS 软件定时器
 *   Pri 3: [Temperature]      — 温度采集 (关键传感器)
 *   Pri 3: [RFID]             — RFID 读卡
 *   Pri 3: [DataFusion]       — 数据融合 (核心处理)
 *   Pri 2: [Display]          — OLED 显示 + 呼吸灯
 *   Pri 2: [WiFi Upload]      — 数据上传
 *   Pri 2: [Battery Monitor]  — 电池监控
 *   Pri 1: [Watchdog]         — 心跳监控
 *   Pri 0: [IDLE]             — 空闲任务
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "semphr.h"

#include "systick.h"
#include "led.h"
#include "key.h"
#include "usart.h"
#include "oled_iic.h"
#include "esp8266_drive.h"
#include "esp8266_public.h"
#include "sta_tcpclent_test.h"

#include "mlx90614.h"
#include "mfrc522.h"
#include "battery.h"
#include "data_fusion.h"
#include "task_watchdog.h"
#include "system_test.h"
#include "flash_store.h"
#include "rtc_driver.h"
#include "anomaly_detect.h"

/* ======================== 系统参数配置 ======================== */
#define SYSTEM_CLOCK_MHZ                72U
#define WIFI_USART_BAUD                 115200U

/* 任务堆栈大小 (单位: 4字节) */
#define STACK_SIZE_TEMPERATURE          256
#define STACK_SIZE_RFID                 256
#define STACK_SIZE_DISPLAY              256
#define STACK_SIZE_WIFI                 384
#define STACK_SIZE_BATTERY              192
#define STACK_SIZE_FUSION               288
#define STACK_SIZE_WATCHDOG             192

/* 任务优先级 */
#define PRIO_TEMPERATURE                3
#define PRIO_RFID                       3
#define PRIO_FUSION                     3
#define PRIO_DISPLAY                    2
#define PRIO_WIFI                       2
#define PRIO_BATTERY                    2
#define PRIO_WATCHDOG                   1

/* System event bits shared by producer, fusion and consumer tasks. */
#define EVENT_BIT_TEMPERATURE_READY     (1U << 0)
#define EVENT_BIT_RFID_READY            (1U << 1)
#define EVENT_BIT_UPLOAD_TRIGGER        (1U << 2)
#define EVENT_BIT_LOW_BATTERY           (1U << 3)
#define EVENT_BIT_SENSOR_FAULT          (1U << 4)

/* ======================== 全局 IPC 对象 ======================== */
SemaphoreHandle_t   g_xI2CMutex          = NULL;
EventGroupHandle_t  g_xSystemEventGroup  = NULL;

static QueueHandle_t xTempDataQueue      = NULL;
static QueueHandle_t xRFIDDataQueue      = NULL;

/* 温度数据队列元素 */
typedef struct
{
    int16_t iObject1;
    int16_t iObject2;
    int16_t iAmbient;
} TempData_t;

/* RFID 数据队列元素 */
typedef struct
{
    uint8_t aucUID[4];
    uint8_t ucDetected;
} RFIDData_t;

/* 异常检测器实例 (温度任务内) */
static AnomalyDetector_t g_xAnomalyDetector;

/* ======================== 任务函数声明 ======================== */
static void vTaskTemperature(void *pvParameters);
static void vTaskRFID(void *pvParameters);
static void vTaskDisplay(void *pvParameters);
static void vTaskWiFiUpload(void *pvParameters);
static void vTaskBatteryMonitor(void *pvParameters);
static void vTaskDataFusion(void *pvParameters);
static void vTaskWatchdog(void *pvParameters);

/* ======================== 系统初始化 ======================== */

/**
 * @brief  初始化所有硬件外设 + 运行系统自检
 * @note   在创建 FreeRTOS 任务前调用，顺序执行
 */
static void prvSystemHardwareInit(void)
{
    SystemTestReport_t xTestReport;

    printf("\r\n");
    printf("========================================\r\n");
    printf("  智能体温检测仪 v2.0 (FreeRTOS)\r\n");
    printf("  STM32F103C8T6 + MLX90614 + MFRC522\r\n");
    printf("========================================\r\n\r\n");

    /* === Step 1: 基础系统时钟 === */
    SysTick_Init(SYSTEM_CLOCK_MHZ);
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* === Step 2: 基础外设初始化 (最小系统) === */
    LED_Init();
    USART1_Init();                  /* 调试串口 (115200) */
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(0, 0, (uint8_t *)"System Init...", 12);
    OLED_Display_On();

    /* === Step 3: Flash 参数加载 (WiFi/设备配置) === */
    FlashStore_Init();
    FlashStore_PrintConfig();

    /* === Step 4: 运行系统自检 === */
    printf("\r\n[SYS] Running system self-test...\r\n");
    SystemTest_RunAll(&xTestReport);

    /* === Step 5: RTC 初始化 (提供时间戳) === */
    if (RTC_Init())
    {
        RTCTime_t tNow;
        RTC_GetTime(&tNow);
        char cTimeBuf[32];
        RTC_TimeToString(&tNow, cTimeBuf, sizeof(cTimeBuf));
        printf("[RTC] System time: %s\r\n", cTimeBuf);
    }

    /* === Step 6: 看门狗初始化 === */
    Watchdog_Init();
    printf("[SYS] IWDG initialized (4s timeout).\r\n");
}

/* ======================== 任务实现 ======================== */

/**
 * @brief  温度采集任务
 * @note   周期 500ms, I2C Mutex 保护
 *         集成异常检测 (突变/超限/卡死/漂移)
 *         集成 RTC 时间戳
 */
static void vTaskTemperature(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TempData_t xTempData;
    int16_t iObj1, iObj2, iAmb;
    uint8_t ucErrorCount = 0;
    RTCTime_t xRTCNow;

    Watchdog_TaskRegister(WDT_TASK_TEMPERATURE, "Temperature", 2000);

    /* 初始化 MLX90614 */
    if (MLX90614_Init())
    {
        printf("[TEMP] MLX90614 initialized OK\r\n");
    }
    else
    {
        printf("[TEMP] MLX90614 init FAILED!\r\n");
        xEventGroupSetBits(g_xSystemEventGroup, EVENT_BIT_SENSOR_FAULT);
    }

    /* 初始化异常检测器 (温度突变/超限/卡死监测) */
    Anomaly_Init(&g_xAnomalyDetector, NULL);

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));

        iObj1 = 0; iObj2 = 0; iAmb = 0;

        if (MLX90614_ReadObject1(&iObj1) && MLX90614_ReadObject2(&iObj2))
        {
            MLX90614_ReadAmbient(&iAmb);
            ucErrorCount = 0;

            /* ===== 异常检测: 分析温度突变/超限/卡死 ===== */
            uint8_t ucAnomaly = Anomaly_Feed(&g_xAnomalyDetector, iObj1);
            if (ucAnomaly & ANOMALY_SENSOR_FAIL)
            {
                xEventGroupSetBits(g_xSystemEventGroup, EVENT_BIT_SENSOR_FAULT);
            }

            /* 获取 RTC 时间戳 (用于数据标记) */
            RTC_GetTime(&xRTCNow);

            /* 发送到数据队列 */
            xTempData.iObject1 = iObj1;
            xTempData.iObject2 = iObj2;
            xTempData.iAmbient = iAmb;
            xQueueSend(xTempDataQueue, &xTempData, 0);
        }
        else
        {
            ucErrorCount++;
            if (ucErrorCount > 5)
            {
                printf("[TEMP] Sensor read failed %u times\r\n", ucErrorCount);
                xEventGroupSetBits(g_xSystemEventGroup, EVENT_BIT_SENSOR_FAULT);
            }
        }

        Watchdog_Ping(WDT_TASK_TEMPERATURE);
    }
}

/**
 * @brief  RFID 读卡任务
 */
static void vTaskRFID(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    MFRC522_CardInfo_t xCardInfo;
    RFIDData_t xRFIDData;
    uint8_t ucPrevDetected = 0;

    Watchdog_TaskRegister(WDT_TASK_RFID, "RFID", 2000);

    if (MFRC522_Init())
        printf("[RFID] MFRC522 initialized OK\r\n");
    else
        printf("[RFID] MFRC522 init FAILED!\r\n");

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(300));

        if (MFRC522_ReadCardUID(&xCardInfo))
        {
            if (ucPrevDetected == 0)
            {
                printf("[RFID] Card detected! Type=%d, UID=", xCardInfo.ucCardType);
                xRFIDData.ucDetected = 1;
                for (uint8_t i = 0; i < 4; i++)
                {
                    xRFIDData.aucUID[i] = xCardInfo.aucUID[i];
                    printf("%02X ", xCardInfo.aucUID[i]);
                }
                printf("\r\n");
                xQueueSend(xRFIDDataQueue, &xRFIDData, 0);
            }
            ucPrevDetected = 1;
        }
        else
        {
            if (ucPrevDetected)
            {
                printf("[RFID] Card removed.\r\n");
                xRFIDData.ucDetected = 0;
                xQueueSend(xRFIDDataQueue, &xRFIDData, 0);
            }
            ucPrevDetected = 0;
        }

        Watchdog_Ping(WDT_TASK_RFID);
    }
}

/**
 * @brief  OLED 显示 + 呼吸灯任务
 * @note   周期 200ms, 显示温度/RFID/电池/时间/异常
 *         同时驱动呼吸灯 (每10ms步进, 由内部循环实现)
 */
static void vTaskDisplay(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int16_t iT1, iT2, iBat;
    uint8_t ucBatPct;
    char cBuf[32];
    RTCTime_t xTime;
    uint8_t ucTimeValid;

    Watchdog_TaskRegister(WDT_TASK_DISPLAY, "Display", 3000);

    /* 启动 PWM 呼吸灯 (PC6 输出呼吸效果, 表示系统运行) */
    LED_Breathing_Start();

    OLED_Clear();
    OLED_ShowString(0, 0, (uint8_t *)"Temp Detector v2", 16);
    OLED_ShowString(0, 2, (uint8_t *)"FreeRTOS Ready", 12);

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));

        /* 每200ms调用呼吸灯步进20次 (每10ms一步) */
        for (uint8_t i = 0; i < 20; i++)
        {
            LED_Breathing_Tick();
            /* 小延时模拟10ms间隔 */
            for (volatile uint32_t d = 0; d < 10000; d++) { }
        }

        /* 临界区读取共享数据 */
        taskENTER_CRITICAL();
        iT1     = g_pMLX90614_Data->iObjectTemp1;
        iT2     = g_pMLX90614_Data->iObjectTemp2;
        iBat    = g_pBatteryInfo->usVoltageMV;
        ucBatPct = g_pBatteryInfo->ucPercentage;
        ucTimeValid = RTC_GetTime(&xTime);
        taskEXIT_CRITICAL();

        /* 第0行: RTC 时间 (如果有效) */
        if (ucTimeValid)
        {
            snprintf(cBuf, sizeof(cBuf), "%02u:%02u:%02u",
                     xTime.ucHour, xTime.ucMinute, xTime.ucSecond);
            OLED_ShowString(72, 0, (uint8_t *)cBuf, 12);
        }

        /* 第2行: 温度1 */
        OLED_ShowString(0, 2, (uint8_t *)"T1:", 12);
        snprintf(cBuf, sizeof(cBuf), "%d.%dC", iT1 / 100, (iT1 % 100 >= 0) ? (iT1 % 100) / 10 : 0);
        OLED_ShowString(24, 2, (uint8_t *)cBuf, 12);

        /* 第4行: 温度2 */
        OLED_ShowString(0, 4, (uint8_t *)"T2:", 12);
        snprintf(cBuf, sizeof(cBuf), "%d.%dC", iT2 / 100, (iT2 % 100 >= 0) ? (iT2 % 100) / 10 : 0);
        OLED_ShowString(24, 4, (uint8_t *)cBuf, 12);

        /* 第6行: 电池 */
        if (Battery_IsLow())
        {
            snprintf(cBuf, sizeof(cBuf), "LOW BAT %umV", iBat);
        }
        else
        {
            snprintf(cBuf, sizeof(cBuf), "Bat:%umV %u%%", iBat, ucBatPct);
        }
        OLED_ShowString(0, 6, (uint8_t *)cBuf, 12);

        /* 异常指示 (闪烁 "!" 标记) */
        if (Anomaly_GetFlags(&g_xAnomalyDetector) != ANOMALY_NONE)
        {
            OLED_ShowString(112, 6, (uint8_t *)"!", 16);
        }

        Watchdog_Ping(WDT_TASK_DISPLAY);
    }
}

/**
 * @brief  WIFI 上传任务 (从 Flash 读取配置)
 */
static void vTaskWiFiUpload(void *pvParameters)
{
    (void)pvParameters;
    DataPacket_t xPacket;
    EventBits_t xBits;
    uint8_t ucWiFiConnected = 0;
    uint8_t ucRetryCount = 0;

    Watchdog_TaskRegister(WDT_TASK_WIFI, "WiFi Upload", 5000);
    ESP8266_CH_PD_Pin_SetH;

    for (;;)
    {
        xBits = xEventGroupWaitBits(g_xSystemEventGroup,
                                     EVENT_BIT_UPLOAD_TRIGGER,
                                     pdTRUE, pdFALSE,
                                     pdMS_TO_TICKS(10000));

        if ((xBits & EVENT_BIT_UPLOAD_TRIGGER) == 0)
        {
            Watchdog_Ping(WDT_TASK_WIFI);
            continue;
        }

        if (DataFusion_PreparePacket(&xPacket) == 0)
        {
            Watchdog_Ping(WDT_TASK_WIFI);
            continue;
        }

        if (ucWiFiConnected == 0)
        {
            printf("[WIFI] Connecting to %s...\r\n", g_tSystemConfig.cWiFiSSID);

            /* 使用 Flash 中存储的 WiFi 配置 */
            ESP8266_Init(WIFI_USART_BAUD);
            if (ESP8266_STA_TCPClient_Test())
            {
                ucWiFiConnected = 1;
                ucRetryCount = 0;
                printf("[WIFI] Connected!\r\n");
            }
            else
            {
                ucRetryCount++;
                if (ucRetryCount >= 3)
                {
                    ucWiFiConnected = 0;
                    ucRetryCount = 0;
                }
                Watchdog_Ping(WDT_TASK_WIFI);
                continue;
            }
        }

        ESP8266_SendString(ENABLE, (char *)&xPacket, sizeof(DataPacket_t), Single_ID_0);
        printf("[WIFI] Data sent (%d bytes)\r\n", sizeof(DataPacket_t));
        DataFusion_ClearUploadFlag();

        if (TcpClosedFlag)
        {
            printf("[WIFI] TCP closed, reconnecting.\r\n");
            ucWiFiConnected = 0;
            TcpClosedFlag = 0;
        }

        Watchdog_Ping(WDT_TASK_WIFI);
    }
}

/**
 * @brief  电池监控任务
 */
static void vTaskBatteryMonitor(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    Watchdog_TaskRegister(WDT_TASK_BATTERY, "Battery", 5000);

    Battery_Init();
    printf("[BATTERY] Monitor initialized.\r\n");

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));

        Battery_Update();

        if (Battery_IsCritical())
        {
            printf("[BATTERY] CRITICAL: %umV, %u%%\r\n",
                   Battery_GetVoltageMV(), Battery_GetPercentage());
        }

        Watchdog_Ping(WDT_TASK_BATTERY);
    }
}

/**
 * @brief  数据融合任务 (含异常标志传递)
 */
static void vTaskDataFusion(void *pvParameters)
{
    (void)pvParameters;
    TempData_t xRecvTemp;
    RFIDData_t xRecvRFID;
    uint8_t ucTempReady = 0;
    int16_t iLastTemp1 = 0, iLastTemp2 = 0, iLastAmb = 0;
    uint8_t aucLastUID[4] = {0};
    uint8_t ucLastCardDetected = 0;

    Watchdog_TaskRegister(WDT_TASK_DATA_FUSION, "DataFusion", 3000);
    DataFusion_Init();
    printf("[FUSION] Data fusion engine started.\r\n");

    for (;;)
    {
        if (xQueueReceive(xTempDataQueue, &xRecvTemp, 0) == pdTRUE)
        {
            iLastTemp1 = xRecvTemp.iObject1;
            iLastTemp2 = xRecvTemp.iObject2;
            iLastAmb   = xRecvTemp.iAmbient;
            ucTempReady = 1;
            xEventGroupSetBits(g_xSystemEventGroup, EVENT_BIT_TEMPERATURE_READY);
        }

        if (xQueueReceive(xRFIDDataQueue, &xRecvRFID, 0) == pdTRUE)
        {
            for (uint8_t i = 0; i < 4; i++)
                aucLastUID[i] = xRecvRFID.aucUID[i];
            ucLastCardDetected = xRecvRFID.ucDetected;

            if (xRecvRFID.ucDetected)
                xEventGroupSetBits(g_xSystemEventGroup, EVENT_BIT_RFID_READY);
        }

        if (ucTempReady)
        {
            DataFusion_Feed(iLastTemp1, iLastTemp2, iLastAmb,
                           aucLastUID, ucLastCardDetected,
                           Battery_GetVoltageMV(), Battery_GetPercentage());
        }

        vTaskDelay(pdMS_TO_TICKS(100));
        Watchdog_Ping(WDT_TASK_DATA_FUSION);
    }
}

/**
 * @brief  看门狗监控任务
 */
static void vTaskWatchdog(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    printf("[WDT] Watchdog monitor started.\r\n");

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));

        Watchdog_Check();

        static uint32_t ulPrintCounter = 0;
        if (++ulPrintCounter >= 30)
        {
            ulPrintCounter = 0;
            Watchdog_PrintStatus();
        }
    }
}

/* ======================== 主函数 ======================== */

int main(void)
{
    /* 第一步: 初始化硬件 + 自检 + Flash + RTC */
    prvSystemHardwareInit();

    /* 第二步: 创建 FreeRTOS IPC 对象 */
    g_xI2CMutex = xSemaphoreCreateMutex();
    g_xSystemEventGroup = xEventGroupCreate();
    xTempDataQueue  = xQueueCreate(4, sizeof(TempData_t));
    xRFIDDataQueue  = xQueueCreate(2, sizeof(RFIDData_t));

    /* 第三步: 创建 FreeRTOS 任务 (7个) */
    xTaskCreate(vTaskTemperature,   "Temperature", STACK_SIZE_TEMPERATURE, NULL, PRIO_TEMPERATURE, NULL);
    xTaskCreate(vTaskRFID,          "RFID",        STACK_SIZE_RFID,        NULL, PRIO_RFID,        NULL);
    xTaskCreate(vTaskDisplay,       "Display",     STACK_SIZE_DISPLAY,     NULL, PRIO_DISPLAY,     NULL);
    xTaskCreate(vTaskWiFiUpload,    "WiFi Upload", STACK_SIZE_WIFI,        NULL, PRIO_WIFI,        NULL);
    xTaskCreate(vTaskBatteryMonitor,"Battery",     STACK_SIZE_BATTERY,     NULL, PRIO_BATTERY,     NULL);
    xTaskCreate(vTaskDataFusion,    "DataFusion",  STACK_SIZE_FUSION,      NULL, PRIO_FUSION,      NULL);
    xTaskCreate(vTaskWatchdog,      "Watchdog",    STACK_SIZE_WATCHDOG,    NULL, PRIO_WATCHDOG,    NULL);

    /* 第四步: 启动调度器 */
    printf("[SYS] Starting FreeRTOS scheduler (%u tasks)...\r\n", uxTaskGetNumberOfTasks());
    vTaskStartScheduler();

    /* 不应到达此处 */
    while (1) { }
}

void vApplicationIdleHook(void)
{
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
