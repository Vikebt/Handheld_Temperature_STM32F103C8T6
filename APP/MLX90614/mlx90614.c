/**
 * @file    mlx90614.c
 * @brief   MLX90614 红外温度传感器驱动实现
 * @note    基于软件 I2C (PB6-SCL, PB7-SDA)，与 24C02 共享总线
 *          通过 FreeRTOS Mutex 保护 I2C 总线，避免多任务并发冲突
 */

#include "mlx90614.h"
#include "iic.h"
#include "FreeRTOS.h"
#include "semphr.h"

/* 全局互斥量句柄 (由 main 中创建) */
extern SemaphoreHandle_t g_xI2CMutex;

/* 全局温度数据 */
static MLX90614_Data_t g_tMLX90614Data;
MLX90614_Data_t *g_pMLX90614_Data = &g_tMLX90614Data;

/* 内部函数声明 */
static uint8_t prvMLX90614_ReadReg(uint8_t ucRegAddr, uint16_t *pusData);
static uint8_t prvMLX90614_WriteCmd(uint8_t ucCmd);
static uint8_t prvMLX90614_CRC_Check(uint8_t *pucData, uint8_t ucLen);

/**
 * @brief  初始化 MLX90614 传感器
 * @retval 1: 成功  0: 失败
 * @note   上电后需至少 500ms 等待传感器稳定
 *         使用 Mutex 保护 I2C 总线访问
 */
uint8_t MLX90614_Init(void)
{
    uint16_t usTmp;
    uint8_t ucRet = 0;

    /* 初始化传感器数据结构 */
    g_tMLX90614Data.iAmbientTemp  = 0;
    g_tMLX90614Data.iObjectTemp1  = 0;
    g_tMLX90614Data.iObjectTemp2  = 0;
    g_tMLX90614Data.ucSensorFault = 0;
    g_tMLX90614Data.ulSampleCount = 0;

    /* 取 Mutex 保护 I2C (等待时间 100ms) */
    if (g_xI2CMutex != NULL)
    {
        xSemaphoreTake(g_xI2CMutex, pdMS_TO_TICKS(100));
    }

    /* 初始化底层 I2C (PB6/PB7) */
    I2C_INIT();

    /* 上电延时 (等待传感器稳定) */
    for (volatile uint32_t i = 0; i < 500000; i++) { }

    /* 读取传感器 ID 以验证通信 (0x1E EEPROM 地址存器件ID) */
    I2C_Start();
    I2C_Send_Byte((MLX90614_ADDR << 1) | 0);    /* 写地址 */
    if (I2C_Wait_Ack() == 0)
    {
        I2C_Send_Byte(0x1E);                     /* EEPROM ID 地址 */
        I2C_Wait_Ack();
        I2C_Start();
        I2C_Send_Byte((MLX90614_ADDR << 1) | 1); /* 读地址 */
        I2C_Wait_Ack();
        usTmp = (uint16_t)I2C_Read_Byte(1) << 8; /* 高字节 + ACK */
        usTmp |= I2C_Read_Byte(0);                /* 低字节 + NACK */
        I2C_Stop();

        ucRet = (usTmp == 0x1007) ? 1 : 0;        /* MLX90614 家族典型 ID */
    }
    else
    {
        I2C_Stop();
    }

    /* 释放 Mutex */
    if (g_xI2CMutex != NULL)
    {
        xSemaphoreGive(g_xI2CMutex);
    }

    if (ucRet == 0)
    {
        g_tMLX90614Data.ucSensorFault = 1;
    }

    return ucRet;
}

/**
 * @brief  读取 MLX90614 RAM 寄存器
 * @param  ucRegAddr  寄存器地址 (如 0x07=TOBJ1)
 * @param  pusData    输出数据指针 (16位原始数据)
 * @retval 1: 成功  0: 失败
 */
static uint8_t prvMLX90614_ReadReg(uint8_t ucRegAddr, uint16_t *pusData)
{
    uint8_t ucData[3];
    uint8_t ucRet = 0;

    if (pusData == NULL) return 0;

    /* 写寄存器地址 (发送 SMBus 读命令) */
    I2C_Start();
    I2C_Send_Byte((MLX90614_ADDR << 1) | 0);
    if (I2C_Wait_Ack() != 0)
    {
        I2C_Stop();
        return 0;
    }

    I2C_Send_Byte(ucRegAddr);
    if (I2C_Wait_Ack() != 0)
    {
        I2C_Stop();
        return 0;
    }

    /* 重新启动，读取数据 */
    I2C_Start();
    I2C_Send_Byte((MLX90614_ADDR << 1) | 1);
    if (I2C_Wait_Ack() != 0)
    {
        I2C_Stop();
        return 0;
    }

    ucData[0] = I2C_Read_Byte(1);   /* Data LSB + ACK */
    ucData[1] = I2C_Read_Byte(1);   /* Data MSB + ACK */
    ucData[2] = I2C_Read_Byte(0);   /* PEC (CRC) + NACK */
    I2C_Stop();

    /* CRC 校验 (SMBus PEC) */
    if (prvMLX90614_CRC_Check(ucData, 3))
    {
        *pusData = ((uint16_t)ucData[1] << 8) | ucData[0];
        ucRet = 1;
    }

    return ucRet;
}

/**
 * @brief  发送命令到 MLX90614
 * @param  ucCmd  命令字节
 * @retval 1: 成功  0: 失败
 */
static uint8_t prvMLX90614_WriteCmd(uint8_t ucCmd)
{
    I2C_Start();
    I2C_Send_Byte((MLX90614_ADDR << 1) | 0);
    if (I2C_Wait_Ack() != 0) { I2C_Stop(); return 0; }

    I2C_Send_Byte(ucCmd);
    if (I2C_Wait_Ack() != 0) { I2C_Stop(); return 0; }
    I2C_Stop();

    return 1;
}

/**
 * @brief  SMBus CRC-8 校验 (多项式 x^8 + x^2 + x + 1)
 */
static uint8_t prvMLX90614_CRC_Check(uint8_t *pucData, uint8_t ucLen)
{
    uint8_t ucCRC = 0;
    uint8_t ucByteIdx, ucBitIdx;

    for (ucByteIdx = 0; ucByteIdx < ucLen; ucByteIdx++)
    {
        ucCRC ^= pucData[ucByteIdx];
        for (ucBitIdx = 0; ucBitIdx < 8; ucBitIdx++)
        {
            if (ucCRC & 0x80)
                ucCRC = (uint8_t)((ucCRC << 1) ^ 0x07);
            else
                ucCRC <<= 1;
        }
    }

    return (ucCRC == 0);
}

/**
 * @brief  读取物体温度1 (TOBJ1)
 * @param  piTemp  输出温度值 (*100, 单位 0.01°C)
 * @retval 1: 成功  0: 失败
 */
uint8_t MLX90614_ReadObject1(int16_t *piTemp)
{
    uint16_t usRaw;
    uint8_t ucRet;

    if (g_xI2CMutex != NULL)
        xSemaphoreTake(g_xI2CMutex, portMAX_DELAY);

    ucRet = prvMLX90614_ReadReg(MLX90614_TOBJ1, &usRaw);

    if (g_xI2CMutex != NULL)
        xSemaphoreGive(g_xI2CMutex);

    if (ucRet)
    {
        /* 转换: 原始值 * 0.02 - 273.15 -> *100 便于整数运算 */
        int32_t lTempC = (int32_t)usRaw * 2 - 27315;
        *piTemp = (int16_t)lTempC;
        g_tMLX90614Data.iObjectTemp1 = *piTemp;
    }

    return ucRet;
}

/**
 * @brief  读取物体温度2 (TOBJ2)
 */
uint8_t MLX90614_ReadObject2(int16_t *piTemp)
{
    uint16_t usRaw;
    uint8_t ucRet;

    if (g_xI2CMutex != NULL)
        xSemaphoreTake(g_xI2CMutex, portMAX_DELAY);

    ucRet = prvMLX90614_ReadReg(MLX90614_TOBJ2, &usRaw);

    if (g_xI2CMutex != NULL)
        xSemaphoreGive(g_xI2CMutex);

    if (ucRet)
    {
        int32_t lTempC = (int32_t)usRaw * 2 - 27315;
        *piTemp = (int16_t)lTempC;
        g_tMLX90614Data.iObjectTemp2 = *piTemp;
    }

    return ucRet;
}

/**
 * @brief  读取环境温度 (TA)
 */
uint8_t MLX90614_ReadAmbient(int16_t *piTemp)
{
    uint16_t usRaw;
    uint8_t ucRet;

    if (g_xI2CMutex != NULL)
        xSemaphoreTake(g_xI2CMutex, portMAX_DELAY);

    ucRet = prvMLX90614_ReadReg(MLX90614_TA, &usRaw);

    if (g_xI2CMutex != NULL)
        xSemaphoreGive(g_xI2CMutex);

    if (ucRet)
    {
        int32_t lTempC = (int32_t)usRaw * 2 - 27315;
        *piTemp = (int16_t)lTempC;
        g_tMLX90614Data.iAmbientTemp = *piTemp;
    }

    return ucRet;
}

/**
 * @brief  一次性读取所有温度值
 * @param  piAmbient  环境温度 *100
 * @param  piObject   物体温度1 *100
 * @retval 1: 全部成功  0: 有失败
 */
uint8_t MLX90614_ReadTemp(int16_t *piAmbient, int16_t *piObject)
{
    uint8_t ucOk1, ucOk2;

    ucOk1 = MLX90614_ReadAmbient(piAmbient);
    ucOk2 = MLX90614_ReadObject1(piObject);

    g_tMLX90614Data.ulSampleCount++;

    return (ucOk1 && ucOk2) ? 1 : 0;
}

/**
 * @brief  传感器自检
 * @retval 1: 正常  0: 异常
 */
uint8_t MLX90614_SelfTest(void)
{
    int16_t iTmp;
    uint8_t ucOk;

    ucOk = MLX90614_ReadObject1(&iTmp);

    /* 温度范围检查 (-40°C ~ +125°C, 即 -4000 ~ 12500) */
    if (ucOk && (iTmp > -4000) && (iTmp < 12500))
    {
        g_tMLX90614Data.ucSensorFault = 0;
        return 1;
    }

    g_tMLX90614Data.ucSensorFault = 1;
    return 0;
}

/**
 * @brief  软件复位 MLX90614
 */
void MLX90614_Reset(void)
{
    if (g_xI2CMutex != NULL)
        xSemaphoreTake(g_xI2CMutex, portMAX_DELAY);

    prvMLX90614_WriteCmd(0x60);  /* SMBus 复位命令 */

    if (g_xI2CMutex != NULL)
        xSemaphoreGive(g_xI2CMutex);
}
