/**
 * @file    flash_store.c
 * @brief   Flash 参数存储模块实现
 * @note    使用 STM32F103C8T6 内置 Flash 最后 1KB 存储
 *          写入前先擦除整页, 写入后 CRC32 校验
 *          启动时自动加载, 校验失败则恢复出厂设置
 */

#include "flash_store.h"
#include <stdio.h>
#include <string.h>

/* 全局系统配置 (RAM 镜像) */
SystemConfig_t g_tSystemConfig;

/**
 * @brief  计算 CRC32
 */
static uint32_t prvCRC32(const uint8_t *pucData, uint32_t ulLen)
{
    uint32_t ulCRC = 0xFFFFFFFFUL;
    uint32_t ulIndex;
    uint8_t ucBit;

    for (ulIndex = 0; ulIndex < ulLen; ulIndex++)
    {
        ulCRC ^= ((uint32_t)pucData[ulIndex] << 24);
        for (ucBit = 0; ucBit < 8; ucBit++)
        {
            ulCRC = (ulCRC & 0x80000000UL) ?
                    ((ulCRC << 1) ^ 0x04C11DB7UL) : (ulCRC << 1);
        }
    }

    return ~ulCRC;
}

/** Calculate the record CRC with the stored CRC field normalized to zero. */
static uint32_t prvConfigCRC(const SystemConfig_t *pConfig)
{
    SystemConfig_t tCopy;

    if (pConfig == NULL)
    {
        return 0;
    }

    memcpy(&tCopy, pConfig, sizeof(tCopy));
    tCopy.ulCRC32 = 0;
    return prvCRC32((const uint8_t *)&tCopy, sizeof(tCopy));
}

/**
 * @brief  设置默认出厂配置
 */
void FlashStore_SetDefaults(void)
{
    memset(&g_tSystemConfig, 0, sizeof(g_tSystemConfig));
    g_tSystemConfig.ulMagic       = FLASH_STORE_MAGIC;
    g_tSystemConfig.ulVersion     = FLASH_STORE_VERSION;

    /* Credentials must be provisioned after deployment. */
    g_tSystemConfig.cWiFiSSID[0] = '\0';
    g_tSystemConfig.cWiFiPassword[0] = '\0';

    /* Networking remains disabled until a valid endpoint is configured. */
    g_tSystemConfig.cServerIP[0] = '\0';
    g_tSystemConfig.usServerPort = 0;

    /* 设备信息 */
    g_tSystemConfig.ucDeviceID = 0x01;

    /* 校准参数 */
    g_tSystemConfig.iTempCalibOffset   = 0;
    g_tSystemConfig.iTempCalibGain     = 10000;  /* 1.0000 */
    g_tSystemConfig.usBatteryCalibRef  = 4095;

    /* 系统配置 */
    g_tSystemConfig.ucUploadIntervalSec    = 10;
    g_tSystemConfig.ucLowBatteryThreshold  = 20;
    g_tSystemConfig.ucTempAlarmHigh        = 38;   /* 38°C */
    g_tSystemConfig.ucTempAlarmLow         = 35;   /* 35°C */

    g_tSystemConfig.ulTail = FLASH_STORE_TAIL;

    /* 计算 CRC */
    g_tSystemConfig.ulCRC32 = prvConfigCRC(&g_tSystemConfig);
}

/**
 * @brief  从 Flash 加载配置到 RAM
 * @retval 1: 成功  0: 失败
 */
uint8_t FlashStore_Load(void)
{
    const SystemConfig_t *pFlash = (const SystemConfig_t *)FLASH_STORE_PAGE_START;
    SystemConfig_t tCandidate;

    /* Reject unknown layouts before copying their payload into RAM. */
    if ((pFlash->ulMagic != FLASH_STORE_MAGIC) ||
        (pFlash->ulVersion != FLASH_STORE_VERSION) ||
        (pFlash->ulTail != FLASH_STORE_TAIL))
    {
        return 0;
    }

    /* Validate before publishing the record to the live RAM configuration. */
    memcpy(&tCandidate, pFlash, sizeof(tCandidate));
    if (tCandidate.ulCRC32 != prvConfigCRC(&tCandidate))
    {
        return 0;
    }
    memcpy(&g_tSystemConfig, &tCandidate, sizeof(tCandidate));
    return 1;
}

/**
 * @brief  将 RAM 配置保存到 Flash
 * @retval 1: 成功  0: 失败
 */
uint8_t FlashStore_Save(void)
{
    uint32_t ulIdx;
    uint16_t *pucSrc;
    FLASH_Status eStatus;

    /* 更新 CRC */
    g_tSystemConfig.ulCRC32 = prvConfigCRC(&g_tSystemConfig);

    /* 解锁 Flash */
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    /* 擦除页 */
    eStatus = FLASH_ErasePage(FLASH_STORE_PAGE_START);
    if (eStatus != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return 0;
    }

    /* 半字写入 */
    pucSrc = (uint16_t *)&g_tSystemConfig;
    for (ulIdx = 0; ulIdx < sizeof(SystemConfig_t) / 2; ulIdx++)
    {
        eStatus = FLASH_ProgramHalfWord(FLASH_STORE_PAGE_START + ulIdx * 2, pucSrc[ulIdx]);
        if (eStatus != FLASH_COMPLETE)
        {
            FLASH_Lock();
            return 0;
        }
    }

    FLASH_Lock();

    /* 验证写入 */
    return FlashStore_Load();
}

/**
 * @brief  CRC32 校验
 */
uint8_t FlashStore_CRC_Verify(void)
{
    uint32_t ulExpected = prvConfigCRC(&g_tSystemConfig);
    return (g_tSystemConfig.ulCRC32 == ulExpected) ? 1 : 0;
}

/**
 * @brief  初始化配置 (加载或设置默认)
 */
uint8_t FlashStore_Init(void)
{
    if (FlashStore_Load())
    {
        return 1;
    }
    else
    {
        FlashStore_SetDefaults();
        return FlashStore_Save();
    }
}

/**
 * @brief  打印当前配置
 */
void FlashStore_PrintConfig(void)
{
    printf("\r\n[FLASH] System Configuration:\r\n");
    printf("  WiFi SSID: %s (%s)\r\n",
           g_tSystemConfig.cWiFiSSID,
           g_tSystemConfig.cWiFiSSID[0] ? "provisioned" : "not provisioned");
    printf("  Server:    %s:%u\r\n", g_tSystemConfig.cServerIP, g_tSystemConfig.usServerPort);
    printf("  Device ID: %u\r\n", g_tSystemConfig.ucDeviceID);
    printf("  Calib:     Offset=%d, Gain=%d\r\n",
           g_tSystemConfig.iTempCalibOffset, g_tSystemConfig.iTempCalibGain);
    printf("  Upload:    %us interval\r\n", g_tSystemConfig.ucUploadIntervalSec);
    printf("  Alarms:    High=%dC, Low=%dC\r\n",
           g_tSystemConfig.ucTempAlarmHigh, g_tSystemConfig.ucTempAlarmLow);
    printf("  CRC:       %s\r\n", FlashStore_CRC_Verify() ? "OK" : "FAIL");
}
