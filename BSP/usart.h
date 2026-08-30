#ifndef _usart_H
#define _usart_H

#include "system.h"
#include "stdio.h"

extern volatile u8 RxCounter;
extern volatile u8 tem[7];
extern volatile u8 tem_frame_ready;

int tem_trans(void);
int tem2_trans(void);

void USART1_Init(void);
void USART2_Init(u32 bound);
void USART3_Init(u32 bound);
void USART3_ResetFrameBuffer(void);

int fputc(int ch, FILE *p);
int fgetc(FILE *f);

#endif

