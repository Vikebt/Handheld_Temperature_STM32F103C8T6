#ifndef _systick_H
#define _systick_H

#include "system.h"

void SysTick_Init(u8 SYSCLK);
void delay_us(u32 i);
void delay_ms(u32 i);


#endif

