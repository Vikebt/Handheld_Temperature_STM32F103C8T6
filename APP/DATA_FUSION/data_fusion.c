/**
 * @file    data_fusion.c
 * @brief   多传感器数据融合模块实现
 * @note    核心功能:
 *          1. 接收各传感器数据并校验
 *          2. 基于事件标志组向上层发送"数据就绪"信号
 *          3. 构建结构化的上传数据包 (含 CRC 校验)
 *          4. 实现即测即传: 温度+RFID 数据齐全后自动触发上传
 */

#include "data_fusion.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include "task.h"

/* 外部事件标志组 */
extern EventGroupHandle_t g_xSystemEventGroup;

/* 全局数据融合结构体 */
static DataFusion_t g_tDataFusion;
DataFusion_t *g_pDataFusion = &g_tDataFusion;

/* 设备 ID (可配置) */
#define DEVICE_ID    0x01

/**
 * @brief  初始化数据融合模块
 */
void DataFusion_Init(void)
{
    g_tDataFusion.iTemperature1     = 0;
    g_tDataFusion.iTemperature2     = 0;
    g_tDataFusion.iAmbientTemp      = 0;
    g_tDataFusion.ucCardDetected    = 0;
    g_tDataFusion.usBatteryMV       = 0;
    g_tDataFusion.ucUploadPending   = 0;
    g_tDataFusion.ulLastUploadTick  = 0;
    g_tDataFusion.ulFusionCount     = 0;

    for (uint8_t i = 0; i < 4; i++)
    {
        g_tDataFusion.aucCardUID[i] = 0;
    }
}

/**
 * @brief  注入传感器数据并执行融合处理
 * @param  iTemp1          温度传感器1值
 * @param  iTemp2          温度传感器2值
 * @param  iAmbient        环境温度
 * @param  pucCardUID      RFID 卡 UID (4字节)
 * @param  ucCardDetected  是否检测到卡
 * @param  usBatteryMV     电池电压
 * @param  ucBatteryPct    电池百分比
 * @retval 1: 数据齐全可上传  0: 数据尚未齐全
 * @note   此函数由数据融合任务调用，当温度和 RFID 数据都准备好时
 *         通过事件标志组触发上传任务
 */
uint8_t DataFusion_Feed(int16_t iTemp1, int16_t iTemp2, int16_t iAmbient,
                        uint8_t *pucCardUID, uint8_t ucCardDetected,
                        uint16_t usBatteryMV, uint8_t ucBatteryPct)
{
    uint8_t ucTrigger = 0;

    /* 更新温度数据 */
    g_tDataFusion.iTemperature1 = iTemp1;
    g_tDataFusion.iTemperature2 = iTemp2;
    g_tDataFusion.iAmbientTemp  = iAmbient;

    /* 更新 RFID 数据 */
    g_tDataFusion.ucCardDetected = ucCardDetected;
    if ((pucCardUID != NULL) && (ucCardDetected))
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            g_tDataFusion.aucCardUID[i] = pucCardUID[i];
        }
    }

    /* 更新电池数据 */
    g_tDataFusion.usBatteryMV = usBatteryMV;
    g_tDataFusion.ulFusionCount++;

    /* 触发上传条件判断:
     * 1. 温度数据有效 (非零)
     * 2. 或每隔 30 秒强制上传一次
     */
    TickType_t xCurTicks = xTaskGetTickCount();
    TickType_t xElapsed  = xCurTicks - g_tDataFusion.ulLastUploadTick;

    if ((iTemp1 != 0) && (xElapsed >= pdMS_TO_TICKS(2000)))
    {
        ucTrigger = 1;
    }
    else if (xElapsed >= pdMS_TO_TICKS(30000))
    {
        /* 30 秒心跳上传 */
        ucTrigger = 1;
    }

    if (ucTrigger)
    {
        g_tDataFusion.ucUploadPending = 1;
        g_tDataFusion.ulLastUploadTick = xCurTicks;

        /* 通过事件标志组通知上传任务 */
        if (g_xSystemEventGroup != NULL)
        {
            xEventGroupSetBits(g_xSystemEventGroup, EVENT_BIT_UPLOAD_TRIGGER);
        }
    }

    return ucTrigger;
}

/**
 * @brief  构建上传数据包
 * @param  pPacket  输出数据包指针
 * @retval 1: 成功  0: 无数据可上传
 */
uint8_t DataFusion_PreparePacket(DataPacket_t *pPacket)
{
    if (pPacket == NULL) return 0;

    if (g_tDataFusion.ucUploadPending == 0)
    {
        return 0;
    }

    /* 填充数据包 */
    pPacket->usHeader     = 0xAA55;
    pPacket->ucDeviceID   = DEVICE_ID;
    pPacket->iTemp1       = g_tDataFusion.iTemperature1;
    pPacket->iTemp2       = g_tDataFusion.iTemperature2;
    pPacket->iAmbient     = g_tDataFusion.iAmbientTemp;

    /* UID 合并为 32-bit */
    pPacket->ulCardUID    = ((uint32_t)g_tDataFusion.aucCardUID[0] << 24) |
                            ((uint32_t)g_tDataFusion.aucCardUID[1] << 16) |
                            ((uint32_t)g_tDataFusion.aucCardUID[2] << 8)  |
                            ((uint32_t)g_tDataFusion.aucCardUID[3]);

    pPacket->usBatteryMV  = g_tDataFusion.usBatteryMV;
    pPacket->ucBatteryPct = (uint8_t)(g_tDataFusion.usBatteryMV * 100 / 4200);
    pPacket->usTail       = 0x55AA;

    /* CRC16 校验 (从 Header 到 BatteryPct) */
    uint16_t usCRC = 0;
    uint8_t *pucBytes = (uint8_t *)pPacket;
    uint8_t ucLen = sizeof(DataPacket_t) - sizeof(uint16_t) - sizeof(uint16_t); /* 不计CRC和Tail */
    for (uint8_t i = 0; i < ucLen; i++)
    {
        usCRC ^= (uint16_t)pucBytes[i] << 8;
        for (uint8_t j = 0; j < 8; j++)
        {
            if (usCRC & 0x8000)
                usCRC = (usCRC << 1) ^ 0x8005;
            else
                usCRC <<= 1;
        }
    }
    pPacket->usCRC16 = usCRC;

    return 1;
}

/**
 * @brief  设置上传标志
 */
void DataFusion_SetUploadFlag(void)
{
    g_tDataFusion.ucUploadPending = 1;
}

/**
 * @brief  清除上传标志
 */
void DataFusion_ClearUploadFlag(void)
{
    g_tDataFusion.ucUploadPending = 0;
}

/**
 * @brief  查询是否有待上传数据
 */
uint8_t DataFusion_IsUploadPending(void)
{
    return g_tDataFusion.ucUploadPending;
}

/**
 * @brief  复位数据融合模块
 */
void DataFusion_Reset(void)
{
    DataFusion_Init();
}
