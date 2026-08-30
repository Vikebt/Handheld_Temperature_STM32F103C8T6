#include "usart.h"
#include "systick.h"

#ifndef USART3_ISR_ECHO
#define USART3_ISR_ECHO 0
#endif

#define TEMP_FRAME_LEN 7U

volatile u8 RxCounter = 0U;
volatile u8 tem[TEMP_FRAME_LEN] = {0U};
volatile u8 tem_frame_ready = 0U;

int fputc(int ch, FILE *p)
{
	(void)p;

	USART_SendData(USART1, (u8)ch);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
	{
	}

	return ch;
}

int fgetc(FILE *f)
{
	(void)f;

	while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET)
	{
	}

	return (int)USART_ReceiveData(USART1);
}

void USART1_Init(void)
{
	GPIO_InitTypeDef gpio_init;
	USART_InitTypeDef usart_init;
	NVIC_InitTypeDef nvic_init;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

	gpio_init.GPIO_Pin = GPIO_Pin_9;
	gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio_init);

	gpio_init.GPIO_Pin = GPIO_Pin_10;
	gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &gpio_init);

	usart_init.USART_BaudRate = 115200;
	usart_init.USART_WordLength = USART_WordLength_8b;
	usart_init.USART_StopBits = USART_StopBits_1;
	usart_init.USART_Parity = USART_Parity_No;
	usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart_init.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART1, &usart_init);

	USART_Cmd(USART1, ENABLE);
	USART_ClearFlag(USART1, USART_FLAG_TC);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	nvic_init.NVIC_IRQChannel = USART1_IRQn;
	nvic_init.NVIC_IRQChannelPreemptionPriority = 3;
	nvic_init.NVIC_IRQChannelSubPriority = 3;
	nvic_init.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic_init);
}

void USART3_Init(u32 bound)
{
	GPIO_InitTypeDef gpio_init;
	USART_InitTypeDef usart_init;
	NVIC_InitTypeDef nvic_init;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	gpio_init.GPIO_Pin = GPIO_Pin_10;
	gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
	gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOB, &gpio_init);

	gpio_init.GPIO_Pin = GPIO_Pin_11;
	gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &gpio_init);

	nvic_init.NVIC_IRQChannel = USART3_IRQn;
	nvic_init.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_init.NVIC_IRQChannelSubPriority = 0;
	nvic_init.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic_init);

	usart_init.USART_BaudRate = bound;
	usart_init.USART_WordLength = USART_WordLength_8b;
	usart_init.USART_StopBits = USART_StopBits_1;
	usart_init.USART_Parity = USART_Parity_No;
	usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart_init.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &usart_init);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART3, ENABLE);
}

void USART3_ResetFrameBuffer(void)
{
	u8 i;

	__disable_irq();
	RxCounter = 0U;
	tem_frame_ready = 0U;
	for (i = 0U; i < TEMP_FRAME_LEN; i++)
	{
		tem[i] = 0U;
	}
	__enable_irq();
}

void USART3_LegacyFrame_IRQHandler(void)
{
	u8 rx_data;

	if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
	{
		rx_data = (u8)USART_ReceiveData(USART3);
#if USART3_ISR_ECHO
		printf("%c", rx_data);
#endif

		tem[RxCounter] = rx_data;
		RxCounter++;

		if (RxCounter >= TEMP_FRAME_LEN)
		{
			RxCounter = 0U;
			tem_frame_ready = 1U;
		}
	}
}
