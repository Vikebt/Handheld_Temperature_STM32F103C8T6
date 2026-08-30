/**
 * @file    mlx90614.h
 * @brief   MLX90614 红外温度传感器驱动头文件
 * @note    I2C 接口，7位器件地址 0x5B (默认)
 *          使用软件 I2C 占用 PB6(SCL) PB7(SDA)，与 24C02 共享总线
 *          通过 Mutex 保护 I2C 总线，防止多任务并发访问冲突
 */

#ifndef __MLX90614_H
#define __MLX90614_H

#include "stm32f10x.h"
#include <stdint.h>

/* MLX90614 默认 I2C 地址 (7-bit) */
#define MLX90614_ADDR               0x5B

/* MLX90614 RAM 寄存器地址 */
#define MLX90614_TA                 0x06    /* 环境温度 */
#define MLX90614_TOBJ1              0x07    /* 物体温度 (传感器1) */
#define MLX90614_TOBJ2              0x08    /* 物体温度 (传感器2) */

/* MLX90614 EEPROM 寄存器地址 */
#define MLX90614_PWMCTRL            0x10    /* PWM 控制 */
#define MLX90614_MAX_ADDR           0xFF    /* 最大器件地址 */

/* 温度数据结构体 */
typedef struct
{
    volatile int16_t   iAmbientTemp;       /* 环境温度 * 100 (单位 0.01°C) */
    volatile int16_t   iObjectTemp1;       /* 物体温度1 * 100 */
    volatile int16_t   iObjectTemp2;       /* 物体温度2 * 100 */
    volatile uint8_t   ucSensorFault;      /* 传感器故障标志 (1=故障) */
    volatile uint32_t  ulSampleCount;      /* 采样计数 */
} MLX90614_Data_t;

/* 初始化与接口函数 */
uint8_t MLX90614_Init(void);
uint8_t MLX90614_ReadTemp(int16_t *piAmbient, int16_t *piObject);
uint8_t MLX90614_ReadObject1(int16_t *piTemp);
uint8_t MLX90614_ReadObject2(int16_t *piTemp);
uint8_t MLX90614_ReadAmbient(int16_t *piTemp);
uint8_t MLX90614_SelfTest(void);
void    MLX90614_Reset(void);

/* 全局数据指针 */
extern MLX90614_Data_t *g_pMLX90614_Data;

#endif /* __MLX90614_H */
