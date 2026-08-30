/**
 * @file    spi_driver.c
 * @brief   SPI 总线驱动实现
 * @note    SPI2 主机模式: 8位数据, CPOL=0, CPHA=0 (模式0)
 *          速率: 4.5 MHz (PCLK1/8)
 */

#include "spi_driver.h"

/**
 * @brief  SPI2 主机模式初始化
 */
void SPI_Driver_Init(void)
{
    GPIO_InitTypeDef gpioInit;
    SPI_InitTypeDef  spiInit;

    /* 使能时钟 */
    RCC_APB1PeriphClockCmd(SPI_RCC, ENABLE);
    RCC_APB2PeriphClockCmd(SPI_GPIO_RCC, ENABLE);

    /* 配置 CS (PB12) 为推挽输出 */
    gpioInit.GPIO_Pin = SPI_CS_PIN;
    gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
    gpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SPI_CS_PORT, &gpioInit);
    SPI_CS_HIGH();  /* CS 默认拉高 */

    /* 配置 SCK (PB13) MOSI (PB15) 为复用推挽输出 */
    gpioInit.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    gpioInit.GPIO_Mode = GPIO_Mode_AF_PP;
    gpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SPI_GPIO_PORT, &gpioInit);

    /* 配置 MISO (PB14) 为浮空输入 */
    gpioInit.GPIO_Pin = GPIO_Pin_14;
    gpioInit.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SPI_GPIO_PORT, &gpioInit);

    /* SPI 配置 */
    SPI_I2S_DeInit(SPI_DEVICE);
    spiInit.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spiInit.SPI_Mode = SPI_Mode_Master;
    spiInit.SPI_DataSize = SPI_DataSize_8b;
    spiInit.SPI_CPOL = SPI_CPOL_Low;
    spiInit.SPI_CPHA = SPI_CPHA_1Edge;
    spiInit.SPI_NSS = SPI_NSS_Soft;
    spiInit.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;  /* 72MHz/8=9MHz */
    spiInit.SPI_FirstBit = SPI_FirstBit_MSB;
    spiInit.SPI_CRCPolynomial = 7;
    SPI_Init(SPI_DEVICE, &spiInit);
    SPI_Cmd(SPI_DEVICE, ENABLE);
}

/**
 * @brief  SPI 读写一个字节
 * @param  ucData  要发送的数据
 * @retval 接收到的数据
 */
uint8_t SPI_ReadWriteByte(uint8_t ucData)
{
    while (SPI_I2S_GetFlagStatus(SPI_DEVICE, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI_DEVICE, ucData);
    while (SPI_I2S_GetFlagStatus(SPI_DEVICE, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(SPI_DEVICE);
}

/**
 * @brief  连续读取多个字节
 */
void SPI_ReadBuf(uint8_t *pucBuf, uint16_t usLen)
{
    uint16_t i;
    for (i = 0; i < usLen; i++)
    {
        pucBuf[i] = SPI_ReadWriteByte(0xFF);
    }
}

/**
 * @brief  连续写入多个字节
 */
void SPI_WriteBuf(uint8_t *pucBuf, uint16_t usLen)
{
    uint16_t i;
    for (i = 0; i < usLen; i++)
    {
        SPI_ReadWriteByte(pucBuf[i]);
    }
}
