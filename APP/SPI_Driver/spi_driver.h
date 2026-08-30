/**
 * @file    spi_driver.h
 * @brief   SPI 总线驱动 (用于 MFRC522 RFID 模块)
 * @note    SPI2: PB13(SCK) PB14(MISO) PB15(MOSI)
 *          软件片选: PB12 (NSS)
 */

#ifndef __SPI_DRIVER_H
#define __SPI_DRIVER_H

#include "stm32f10x.h"
#include <stdint.h>

/* SPI 引脚定义 */
#define SPI_CS_PORT             GPIOB
#define SPI_CS_PIN              GPIO_Pin_12
#define SPI_CS_RCC              RCC_APB2Periph_GPIOB

#define SPI_DEVICE              SPI2
#define SPI_RCC                 RCC_APB1Periph_SPI2
#define SPI_GPIO_RCC            RCC_APB2Periph_GPIOB
#define SPI_SCK_PIN             GPIO_Pin_13
#define SPI_MISO_PIN            GPIO_Pin_14
#define SPI_MOSI_PIN            GPIO_Pin_15
#define SPI_GPIO_PORT           GPIOB

/* 片选宏 */
#define SPI_CS_LOW()            GPIO_ResetBits(SPI_CS_PORT, SPI_CS_PIN)
#define SPI_CS_HIGH()           GPIO_SetBits(SPI_CS_PORT, SPI_CS_PIN)

/* 函数原型 */
void    SPI_Driver_Init(void);
uint8_t SPI_ReadWriteByte(uint8_t ucData);
void    SPI_ReadBuf(uint8_t *pucBuf, uint16_t usLen);
void    SPI_WriteBuf(uint8_t *pucBuf, uint16_t usLen);

#endif /* __SPI_DRIVER_H */
