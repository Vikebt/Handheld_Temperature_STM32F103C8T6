#ifndef _led_H
#define _led_H

#include "system.h"
#include "stm32f10x.h"   

#define LED_PORT_RCC       RCC_APB2Periph_GPIOC
#define LED_PIN            GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7
#define LED_PORT           GPIOC
#define led1 PCout(0)
#define led2 PCout(1)
#define led3 PCout(2)
#define led4 PCout(3)
#define led5 PCout(4)
#define led6 PCout(5)
#define led7 PCout(6)
#define led8 PCout(7)

/* LED 开关宏 (主动电平控制) */
#define LED1_ON()    GPIO_ResetBits(GPIOC, GPIO_Pin_0)
#define LED1_OFF()   GPIO_SetBits(GPIOC, GPIO_Pin_0)
#define LED2_ON()    GPIO_ResetBits(GPIOC, GPIO_Pin_1)
#define LED2_OFF()   GPIO_SetBits(GPIOC, GPIO_Pin_1)
#define LED3_ON()    GPIO_ResetBits(GPIOC, GPIO_Pin_2)
#define LED3_OFF()   GPIO_SetBits(GPIOC, GPIO_Pin_2)
#define LED_ALL_ON()  GPIO_ResetBits(GPIOC, GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7)
#define LED_ALL_OFF() GPIO_SetBits(GPIOC, GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7)

void LED_Init(void);
void LED_Breathing_Start(void);
void LED_Breathing_Stop(void);
void LED_Breathing_Tick(void);




#endif
