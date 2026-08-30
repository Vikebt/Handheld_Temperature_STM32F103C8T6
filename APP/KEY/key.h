#ifndef _key_H
#define _key_H

#include "system.h"
#include "systick.h"

void Key_Init(void);
u8 KEY_Scan(u8 mode);

#define KEY_UP_Pin      GPIO_Pin_0
#define KEY_UP_Port     GPIOA

#define KEY_LEFT_Pin    GPIO_Pin_2
#define KEY_DOWN_Pin    GPIO_Pin_3
#define KEY_RIGHT_Pin   GPIO_Pin_6
#define KEY_Port        GPIOE

#define K_UP            GPIO_ReadInputDataBit(KEY_UP_Port, KEY_UP_Pin)
#define K_DOWN          GPIO_ReadInputDataBit(KEY_Port, KEY_DOWN_Pin)
#define K_LEFT          GPIO_ReadInputDataBit(KEY_Port, KEY_LEFT_Pin)
#define K_RIGHT         GPIO_ReadInputDataBit(KEY_Port, KEY_RIGHT_Pin)

#define KEY_UP     1
#define KEY_DOWN   2
#define KEY_LEFT   3
#define KEY_RIGHT  4

#endif
