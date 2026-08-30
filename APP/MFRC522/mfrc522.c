/**
 * @file    mfrc522.c
 * @brief   MFRC522 RFID 读写模块驱动实现
 * @note    SPI2 接口，标准 MFRC522 寄存器操作
 *          集成防碰撞、选卡、认证、读写块等完整功能
 */

#include "mfrc522.h"
#include "spi_driver.h"
#include "FreeRTOS.h"
#include "task.h"

volatile uint8_t g_ucCardDetected = 0;

/* 内部函数声明 */
static void     prvWriteReg(uint8_t ucReg, uint8_t ucData);
static uint8_t  prvReadReg(uint8_t ucReg);
static void     prvSetBitMask(uint8_t ucReg, uint8_t ucMask);
static void     prvClearBitMask(uint8_t ucReg, uint8_t ucMask);
static void     prvWriteFIFO(uint8_t *pucBuf, uint8_t ucLen);
static void     prvClearFIFO(void);
static uint16_t prvCRC16(uint8_t *pucBuf, uint8_t ucLen);

/**
 * @brief  写 MFRC522 寄存器
 */
static void prvWriteReg(uint8_t ucReg, uint8_t ucData)
{
    SPI_CS_LOW();
    SPI_ReadWriteByte((ucReg << 1) & 0x7E);   /* 地址+写标志 */
    SPI_ReadWriteByte(ucData);
    SPI_CS_HIGH();
}

/**
 * @brief  读 MFRC522 寄存器
 */
static uint8_t prvReadReg(uint8_t ucReg)
{
    uint8_t ucVal;
    SPI_CS_LOW();
    SPI_ReadWriteByte(((ucReg << 1) & 0x7E) | 0x80); /* 地址+读标志 */
    ucVal = SPI_ReadWriteByte(0xFF);
    SPI_CS_HIGH();
    return ucVal;
}

/**
 * @brief  置位寄存器的特定位
 */
static void prvSetBitMask(uint8_t ucReg, uint8_t ucMask)
{
    uint8_t ucTmp = prvReadReg(ucReg);
    prvWriteReg(ucReg, ucTmp | ucMask);
}

/**
 * @brief  清除寄存器的特定位
 */
static void prvClearBitMask(uint8_t ucReg, uint8_t ucMask)
{
    uint8_t ucTmp = prvReadReg(ucReg);
    prvWriteReg(ucReg, ucTmp & (~ucMask));
}

/**
 * @brief  向 FIFO 写入数据
 */
static void prvWriteFIFO(uint8_t *pucBuf, uint8_t ucLen)
{
    uint8_t i;
    for (i = 0; i < ucLen; i++)
    {
        prvWriteReg(MFRC522_FIFODataReg, pucBuf[i]);
    }
}

/**
 * @brief  清空 FIFO
 */
static void prvClearFIFO(void)
{
    prvSetBitMask(MFRC522_FIFOLevelReg, 0x80);
}

/**
 * @brief  初始化 MFRC522
 * @retval 1: 成功  0: 失败
 */
uint8_t MFRC522_Init(void)
{
    /* 初始化 SPI 总线 */
    SPI_Driver_Init();

    /* 延时等待 MFRC522 上电稳定 */
    for (volatile uint32_t i = 0; i < 100000; i++) { }

    /* 软件复位 */
    prvWriteReg(MFRC522_CommandReg, MFRC522_IDLE | 0x10);
    for (volatile uint32_t i = 0; i < 10000; i++) { }
    prvWriteReg(MFRC522_CommandReg, MFRC522_IDLE);
    for (volatile uint32_t i = 0; i < 10000; i++) { }

    /* 读取版本号验证通信 */
    uint8_t ucVersion = prvReadReg(MFRC522_VersionReg);

    /* MFRC522 版本号: 0x91(v2.0) 或 0x92(v2.1) */
    if ((ucVersion != 0x91) && (ucVersion != 0x92))
    {
        return 0;  /* 通信失败 */
    }

    /* 配置定时器 */
    prvWriteReg(MFRC522_TModeReg, 0x8D);    /* 定时器使能, 自动开始 */
    prvWriteReg(MFRC522_TPrescalerReg, 0x3E);
    prvWriteReg(MFRC522_TReloadRegL, 30);
    prvWriteReg(MFRC522_TReloadRegH, 0);

    /* 配置 14443A 帧格式 */
    prvWriteReg(MFRC522_TxASKReg, 0x40);    /* 强制 100% ASK 调制 */
    prvWriteReg(MFRC522_ModeReg, 0x3D);     /* CRC 初始值 0x6363 */

    /* 开启天线 */
    MFRC522_AntennaOn();

    return 1;
}

/**
 * @brief  搜索并检测卡片
 * @param  pucCardType  输出卡片类型
 * @retval 1: 检测到卡片  0: 无卡片
 */
uint8_t MFRC522_CheckCard(uint8_t *pucCardType)
{
    uint8_t ucATQA[2];
    uint8_t ucCount = 0;

    /* Request 命令 (寻找 14443A 卡片) */
    prvClearBitMask(MFRC522_Status2Reg, 0x08);  /* 清除上次数据 */

    prvWriteReg(MFRC522_BitFramingReg, 0x07);   /* TxLastBits = 7 */
    prvWriteReg(MFRC522_CommandReg, MFRC522_IDLE);

    prvClearFIFO();
    uint8_t ucReqCmd = 0x52;                     /* Request mode */
    prvWriteFIFO(&ucReqCmd, 1);
    prvWriteReg(MFRC522_CommandReg, MFRC522_TRANSCEIVE);
    prvSetBitMask(MFRC522_BitFramingReg, 0x80); /* 开始发送 */

    /* 等待命令完成 */
    uint32_t ulTimeout = 10000;
    do {
        ucCount = prvReadReg(MFRC522_ComIrqReg);
        ulTimeout--;
    } while ((ulTimeout > 0) && ((ucCount & 0x01) == 0) && ((ucCount & 0x20) == 0));

    prvClearBitMask(MFRC522_BitFramingReg, 0x80);

    if ((ulTimeout != 0) && ((ucCount & 0x20) == 0))
    {
        if (prvReadReg(MFRC522_ErrorReg) & 0x1F)
        {
            return 0;
        }

        uint8_t ucLen = prvReadReg(MFRC522_FIFOLevelReg);
        if (ucLen >= 2)
        {
            ucATQA[0] = prvReadReg(MFRC522_FIFODataReg);
            ucATQA[1] = prvReadReg(MFRC522_FIFODataReg);

            if (pucCardType != NULL)
            {
                /* 根据 ATQA 判断卡片类型 */
                if ((ucATQA[0] == 0x04) && (ucATQA[1] == 0x00))
                    *pucCardType = MFRC522_CARD_MIFARE;
                else if ((ucATQA[0] == 0x44) && (ucATQA[1] == 0x00))
                    *pucCardType = MFRC522_CARD_ULTRALIGHT;
                else if ((ucATQA[0] == 0x08) && (ucATQA[1] == 0x00))
                    *pucCardType = MFRC522_CARD_TNP3XXX;
                else
                    *pucCardType = MFRC522_CARD_MIFARE; /* 默认 */
            }

            g_ucCardDetected = 1;
            return 1;
        }
    }

    g_ucCardDetected = 0;
    return 0;
}

/**
 * @brief  防碰撞: 获取卡片的 UID
 * @param  pucUID  输出 UID (4字节)
 * @retval 1: 成功  0: 失败
 */
uint8_t MFRC522_Anticoll(uint8_t *pucUID)
{
    uint8_t ucCount, ucBackBits, ucLen;
    uint8_t ucStatus = 0;

    /* 防碰撞命令 0x93 (CL1) */
    prvClearFIFO();
    prvWriteReg(MFRC522_BitFramingReg, 0x00);

    uint8_t ucCmdBuf[2] = {0x93, 0x20};
    prvWriteFIFO(ucCmdBuf, 2);
    prvWriteReg(MFRC522_CommandReg, MFRC522_TRANSCEIVE);
    prvSetBitMask(MFRC522_BitFramingReg, 0x80);

    ucCount = 0;
    uint32_t ulTimeout = 10000;
    do {
        ucBackBits = prvReadReg(MFRC522_ComIrqReg);
        ulTimeout--;
    } while ((ulTimeout > 0) && ((ucBackBits & 0x01) == 0) && ((ucBackBits & 0x20) == 0));

    ucCount = prvReadReg(MFRC522_ErrorReg);

    if ((ulTimeout != 0) && ((ucCount & 0x1F) == 0x00) && ((ucBackBits & 0x20) == 0))
    {
        ucLen = prvReadReg(MFRC522_FIFOLevelReg);
        if (ucLen == 5)  /* 4字节 UID + 1字节 UID校验 */
        {
            for (uint8_t i = 0; i < 4; i++)
            {
                pucUID[i] = prvReadReg(MFRC522_FIFODataReg);
            }
            ucStatus = 1;
        }
    }

    prvClearBitMask(MFRC522_BitFramingReg, 0x80);
    return ucStatus;
}

/**
 * @brief  选卡: 选择卡片并返回 SAK
 * @param  pucUID  UID 指针
 * @param  pucSAK  输出 SAK
 * @retval 1: 成功  0: 失败
 */
uint8_t MFRC522_SelectCard(uint8_t *pucUID, uint8_t *pucSAK)
{
    uint8_t ucBuf[9];
    uint8_t ucCount, ucLen;

    ucBuf[0] = 0x93;      /* SEL (CL1) */
    ucBuf[1] = 0x70;      /* NVB (全部 40位) */
    ucBuf[2] = pucUID[0];
    ucBuf[3] = pucUID[1];
    ucBuf[4] = pucUID[2];
    ucBuf[5] = pucUID[3];

    /* CRC 校验 */
    uint16_t usCRC = prvCRC16(ucBuf, 6);
    ucBuf[6] = (uint8_t)(usCRC >> 8);
    ucBuf[7] = (uint8_t)(usCRC);

    prvClearFIFO();
    prvWriteReg(MFRC522_CommandReg, MFRC522_IDLE);

    for (uint8_t i = 0; i < 8; i++)
    {
        prvWriteReg(MFRC522_FIFODataReg, ucBuf[i]);
    }

    prvWriteReg(MFRC522_CommandReg, MFRC522_TRANSCEIVE);
    prvSetBitMask(MFRC522_BitFramingReg, 0x80);

    uint32_t ulTimeout = 10000;
    do {
        ucCount = prvReadReg(MFRC522_ComIrqReg);
        ulTimeout--;
    } while ((ulTimeout > 0) && ((ucCount & 0x01) == 0) && ((ucCount & 0x20) == 0));

    prvClearBitMask(MFRC522_BitFramingReg, 0x80);

    if ((ulTimeout != 0) && ((ucCount & 0x20) == 0))
    {
        ucLen = prvReadReg(MFRC522_FIFOLevelReg);
        if (ucLen == 1)
        {
            *pucSAK = prvReadReg(MFRC522_FIFODataReg);
            return 1;
        }
    }

    return 0;
}

/**
 * @brief  读卡完整流程: 请求 -> 防碰撞 -> 选卡
 * @param  pCardInfo  输出卡片信息结构体
 * @retval 1: 成功  0: 失败
 */
uint8_t MFRC522_ReadCardUID(MFRC522_CardInfo_t *pCardInfo)
{
    uint8_t ucCardType;

    if (pCardInfo == NULL) return 0;

    /* Step 1: 检测卡片 */
    if (MFRC522_CheckCard(&ucCardType) == 0)
    {
        return 0;
    }

    pCardInfo->ucCardType = ucCardType;

    /* Step 2: 防碰撞 */
    uint8_t aucUID[4];
    if (MFRC522_Anticoll(aucUID) == 0)
    {
        return 0;
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        pCardInfo->aucUID[i] = aucUID[i];
    }
    pCardInfo->ucUIDLen = 4;

    /* Step 3: 选卡 */
    if (MFRC522_SelectCard(aucUID, &pCardInfo->ucSAK) == 0)
    {
        return 0;
    }

    g_ucCardDetected = 1;
    return 1;
}

/**
 * @brief  使卡片进入 HALT 状态
 */
uint8_t MFRC522_Halt(void)
{
    uint8_t ucBuf[4];
    uint16_t usCRC;

    ucBuf[0] = 0x50;   /* HLTA */
    ucBuf[1] = 0x00;

    usCRC = prvCRC16(ucBuf, 2);
    ucBuf[2] = (uint8_t)(usCRC >> 8);
    ucBuf[3] = (uint8_t)(usCRC);

    prvClearFIFO();
    prvWriteReg(MFRC522_CommandReg, MFRC522_IDLE);

    for (uint8_t i = 0; i < 4; i++)
    {
        prvWriteReg(MFRC522_FIFODataReg, ucBuf[i]);
    }

    prvWriteReg(MFRC522_CommandReg, MFRC522_TRANSCEIVE);
    prvSetBitMask(MFRC522_BitFramingReg, 0x80);

    uint32_t ulTimeout = 5000;
    while ((prvReadReg(MFRC522_ComIrqReg) & 0x01) == 0 && (--ulTimeout));

    prvClearBitMask(MFRC522_BitFramingReg, 0x80);
    g_ucCardDetected = 0;

    return 1;
}

/**
 * @brief  Mifare 卡片认证
 * @param  ucAuthMode 认证模式 (0x60=KeyA, 0x61=KeyB)
 * @param  ucBlockAddr 块地址
 * @param  pucKey 密钥 (6字节)
 * @param  pucUID 卡片 UID (4字节)
 * @retval 1: 认证通过  0: 认证失败
 */
uint8_t MFRC522_AuthCard(uint8_t ucAuthMode, uint8_t ucBlockAddr, uint8_t *pucKey, uint8_t *pucUID)
{
    uint8_t ucBuf[12];
    uint8_t ucCount;

    /* 准备认证数据 */
    ucBuf[0] = ucAuthMode;
    ucBuf[1] = ucBlockAddr;

    for (uint8_t i = 0; i < 6; i++)
    {
        ucBuf[2 + i] = pucKey[i];
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        ucBuf[8 + i] = pucUID[i];
    }

    /* 发送认证命令 */
    prvWriteReg(MFRC522_CommandReg, MFRC522_IDLE);
    prvWriteFIFO(ucBuf, 12);
    prvWriteReg(MFRC522_CommandReg, MFRC522_AUTHENT1A + (ucAuthMode == 0x61 ? 1 : 0));

    /* 等待认证完成 */
    uint32_t ulTimeout = 10000;
    do {
        ucCount = prvReadReg(MFRC522_Status2Reg);
        ulTimeout--;
    } while ((ulTimeout > 0) && ((ucCount & 0x08) == 0));

    if (ucCount & 0x08)
    {
        return 1;  /* 认证成功: Status2Reg 的 Bit3 置位 */
    }

    return 0;
}

/**
 * @brief  读取 Mifare 卡片数据块 (16字节)
 * @param  ucBlockAddr 块地址
 * @param  pucData 输出数据缓冲区 (至少16字节)
 * @retval 1: 成功  0: 失败
 */
uint8_t MFRC522_ReadBlock(uint8_t ucBlockAddr, uint8_t *pucData)
{
    uint8_t ucBuf[4];
    uint16_t usCRC;

    ucBuf[0] = 0x30;                     /* MIFARE_READ */
    ucBuf[1] = ucBlockAddr;

    usCRC = prvCRC16(ucBuf, 2);
    ucBuf[2] = (uint8_t)(usCRC >> 8);
    ucBuf[3] = (uint8_t)(usCRC);

    prvClearFIFO();
    prvWriteReg(MFRC522_CommandReg, MFRC522_IDLE);
    prvWriteFIFO(ucBuf, 4);
    prvWriteReg(MFRC522_CommandReg, MFRC522_TRANSCEIVE);
    prvSetBitMask(MFRC522_BitFramingReg, 0x80);

    uint32_t ulTimeout = 10000;
    uint8_t ucIrq;
    do {
        ucIrq = prvReadReg(MFRC522_ComIrqReg);
        ulTimeout--;
    } while ((ulTimeout > 0) && ((ucIrq & 0x01) == 0) && ((ucIrq & 0x20) == 0));

    prvClearBitMask(MFRC522_BitFramingReg, 0x80);

    if ((ulTimeout == 0) || (ucIrq & 0x20))
    {
        return 0;
    }

    uint8_t ucLen = prvReadReg(MFRC522_FIFOLevelReg);
    if (ucLen >= 16)
    {
        for (uint8_t i = 0; i < 16; i++)
        {
            pucData[i] = prvReadReg(MFRC522_FIFODataReg);
        }
        return 1;
    }

    return 0;
}

/**
 * @brief  写入 Mifare 卡片数据块 (16字节)
 */
uint8_t MFRC522_WriteBlock(uint8_t ucBlockAddr, uint8_t *pucData)
{
    uint8_t ucBuf[4];
    uint16_t usCRC;

    /* MIFARE_WRITE 命令 */
    ucBuf[0] = 0xA0;
    ucBuf[1] = ucBlockAddr;
    usCRC = prvCRC16(ucBuf, 2);
    ucBuf[2] = (uint8_t)(usCRC >> 8);
    ucBuf[3] = (uint8_t)(usCRC);

    prvClearFIFO();
    prvWriteReg(MFRC522_CommandReg, MFRC522_IDLE);
    prvWriteFIFO(ucBuf, 4);
    prvWriteReg(MFRC522_CommandReg, MFRC522_TRANSCEIVE);
    prvSetBitMask(MFRC522_BitFramingReg, 0x80);

    uint32_t ulTimeout = 10000;
    uint8_t ucIrq;
    do {
        ucIrq = prvReadReg(MFRC522_ComIrqReg);
        ulTimeout--;
    } while ((ulTimeout > 0) && ((ucIrq & 0x01) == 0) && ((ucIrq & 0x20) == 0));

    prvClearBitMask(MFRC522_BitFramingReg, 0x80);

    if ((ulTimeout == 0) || (ucIrq & 0x20))
    {
        return 0;
    }

    /* 等待卡片确认 */
    ulTimeout = 5000;
    do {
        ucIrq = prvReadReg(MFRC522_ComIrqReg);
        ulTimeout--;
    } while ((ulTimeout > 0) && ((ucIrq & 0x01) == 0));

    /* 发送 16 字节数据 */
    prvClearFIFO();
    for (uint8_t i = 0; i < 16; i++)
    {
        prvWriteReg(MFRC522_FIFODataReg, pucData[i]);
    }
    prvWriteReg(MFRC522_CommandReg, MFRC522_TRANSCEIVE);
    prvSetBitMask(MFRC522_BitFramingReg, 0x80);

    ulTimeout = 10000;
    do {
        ucIrq = prvReadReg(MFRC522_ComIrqReg);
        ulTimeout--;
    } while ((ulTimeout > 0) && ((ucIrq & 0x01) == 0) && ((ucIrq & 0x20) == 0));

    prvClearBitMask(MFRC522_BitFramingReg, 0x80);

    return ((ulTimeout > 0) && ((ucIrq & 0x20) == 0));
}

/**
 * @brief  开启天线 (使能射频发射)
 */
void MFRC522_AntennaOn(void)
{
    uint8_t ucVal = prvReadReg(MFRC522_TxControlReg);
    if ((ucVal & 0x03) != 0x03)
    {
        prvWriteReg(MFRC522_TxControlReg, ucVal | 0x03);
    }
}

/**
 * @brief  关闭天线
 */
void MFRC522_AntennaOff(void)
{
    prvClearBitMask(MFRC522_TxControlReg, 0x03);
}

/**
 * @brief  CRC-16 计算 (用于 MFRC522 通信)
 * @note   多项式: x^16 + x^14 + x^12 + x^11 + x^8 + x^5 + x^4 + x^2 + 1
 */
static uint16_t prvCRC16(uint8_t *pucBuf, uint8_t ucLen)
{
    /* 利用 MFRC522 硬件 CRC 计算 */
    prvClearFIFO();
    prvWriteReg(MFRC522_CommandReg, MFRC522_IDLE);
    prvWriteReg(MFRC522_DivIrqReg, 0x04);
    prvSetBitMask(MFRC522_FIFOLevelReg, 0x80);  /* 清空 FIFO */

    for (uint8_t i = 0; i < ucLen; i++)
    {
        prvWriteReg(MFRC522_FIFODataReg, pucBuf[i]);
    }

    prvWriteReg(MFRC522_CommandReg, MFRC522_CALCCRC);

    uint32_t ulTimeout = 10000;
    do {
        ulTimeout--;
    } while ((ulTimeout > 0) && ((prvReadReg(MFRC522_DivIrqReg) & 0x04) == 0));

    return ((uint16_t)prvReadReg(MFRC522_CRCResultRegH) << 8) | prvReadReg(MFRC522_CRCResultRegL);
}

/**
 * @brief  MFRC522 自检
 * @retval 1: 正常  0: 异常
 */
uint8_t MFRC522_SelfTest(void)
{
    uint8_t ucVersion = prvReadReg(MFRC522_VersionReg);
    return ((ucVersion == 0x91) || (ucVersion == 0x92)) ? 1 : 0;
}
