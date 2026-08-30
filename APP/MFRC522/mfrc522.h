/**
 * @file    mfrc522.h
 * @brief   MFRC522 RFID 读写模块驱动头文件
 * @note    SPI2 接口连接:
 *          - SCK  -> PB13
 *          - MOSI -> PB15
 *          - MISO -> PB14
 *          - CS   -> PB12
 *          - RST  -> PD2 (可选, 也可用软件复位)
 */

#ifndef __MFRC522_H
#define __MFRC522_H

#include "stm32f10x.h"
#include <stdint.h>

/* MFRC522 命令定义 */
#define MFRC522_IDLE              0x00
#define MFRC522_MEM               0x01
#define MFRC522_GENERATERANDOMID  0x02
#define MFRC522_CALCCRC           0x03
#define MFRC522_TRANSMIT          0x04
#define MFRC522_NOCMDCHANGE       0x07
#define MFRC522_RECEIVE           0x08
#define MFRC522_TRANSCEIVE        0x0C
#define MFRC522_AUTHENT1A         0x0E
#define MFRC522_AUTHENT1B         0x0F

/* MFRC522 寄存器地址 */
#define MFRC522_CommandReg        0x01
#define MFRC522_ComIEnReg         0x02
#define MFRC522_DivIEnReg         0x03
#define MFRC522_ComIrqReg         0x04
#define MFRC522_DivIrqReg         0x05
#define MFRC522_ErrorReg          0x06
#define MFRC522_Status1Reg        0x07
#define MFRC522_Status2Reg        0x08
#define MFRC522_FIFODataReg       0x09
#define MFRC522_FIFOLevelReg      0x0A
#define MFRC522_WaterLevelReg     0x0B
#define MFRC522_ControlReg        0x0C
#define MFRC522_BitFramingReg     0x0D
#define MFRC522_CollReg           0x0E
#define MFRC522_ModeReg           0x11
#define MFRC522_TxModeReg         0x12
#define MFRC522_RxModeReg         0x13
#define MFRC522_TxControlReg      0x14
#define MFRC522_TxASKReg          0x15
#define MFRC522_RxSelReg          0x16
#define MFRC522_RxThresholdReg    0x17
#define MFRC522_DemodReg          0x18
#define MFRC522_MfTxReg           0x1C
#define MFRC522_MfRxReg           0x1D
#define MFRC522_TypeBReg          0x1E
#define MFRC522_SerialSpeedReg    0x1F
#define MFRC522_CRCResultRegH     0x21
#define MFRC522_CRCResultRegL     0x22
#define MFRC522_ModWidthReg       0x24
#define MFRC522_RFCfgReg          0x26
#define MFRC522_GsNReg            0x27
#define MFRC522_CWGsPReg          0x28
#define MFRC522_ModGsPReg         0x29
#define MFRC522_TModeReg          0x2A
#define MFRC522_TPrescalerReg     0x2B
#define MFRC522_TReloadRegH       0x2C
#define MFRC522_TReloadRegL       0x2D
#define MFRC522_TCounterValueRegH 0x2E
#define MFRC522_TCounterValueRegL 0x2F
#define MFRC522_VersionReg        0x37

/* 卡片类型 */
#define MFRC522_CARD_NONE         0x00
#define MFRC522_CARD_MIFARE       0x01
#define MFRC522_CARD_ULTRALIGHT   0x02
#define MFRC522_CARD_TNP3XXX      0x03

/* 认证类型 */
#define MFRC522_AUTH_KEYA         0x60
#define MFRC522_AUTH_KEYB         0x61

/* UID 最大长度 */
#define MFRC522_UID_MAX_LEN       10

/* RFID 卡信息结构体 */
typedef struct
{
    uint8_t  ucCardType;           /* 卡片类型 */
    uint8_t  ucUIDLen;             /* UID 长度 (字节) */
    uint8_t  aucUID[MFRC522_UID_MAX_LEN]; /* UID 缓冲区 */
    uint8_t  ucSAK;                /* SAK (Select Acknowledge) */
    uint8_t  ucATQA[2];            /* ATQA (Answer To Request) */
} MFRC522_CardInfo_t;

/* 函数原型 */
uint8_t MFRC522_Init(void);
uint8_t MFRC522_CheckCard(uint8_t *pucCardType);
uint8_t MFRC522_ReadCardUID(MFRC522_CardInfo_t *pCardInfo);
uint8_t MFRC522_Anticoll(uint8_t *pucUID);
uint8_t MFRC522_SelectCard(uint8_t *pucUID, uint8_t *pucSAK);
uint8_t MFRC522_Halt(void);
uint8_t MFRC522_AuthCard(uint8_t ucAuthMode, uint8_t ucBlockAddr, uint8_t *pucKey, uint8_t *pucUID);
uint8_t MFRC522_ReadBlock(uint8_t ucBlockAddr, uint8_t *pucData);
uint8_t MFRC522_WriteBlock(uint8_t ucBlockAddr, uint8_t *pucData);
void    MFRC522_AntennaOn(void);
void    MFRC522_AntennaOff(void);
uint8_t MFRC522_SelfTest(void);

/* 全局卡片信息 */
extern volatile uint8_t  g_ucCardDetected;

#endif /* __MFRC522_H */
