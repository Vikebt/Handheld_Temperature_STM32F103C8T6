#include "key.h"

void Key_Init(void)
{
	GPIO_InitTypeDef gpio_init;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOE, ENABLE);

	gpio_init.GPIO_Mode = GPIO_Mode_IPU;
	gpio_init.GPIO_Speed = GPIO_Speed_50MHz;

	gpio_init.GPIO_Pin = KEY_UP_Pin;
	GPIO_Init(KEY_UP_Port, &gpio_init);

	gpio_init.GPIO_Pin = KEY_LEFT_Pin | KEY_DOWN_Pin | KEY_RIGHT_Pin;
	GPIO_Init(KEY_Port, &gpio_init);
}

// mode==0: scan once
// mode==1: continuous scan
u8 KEY_Scan(u8 mode)
{
	static u8 key_released = 1U;

	if (mode == 1U)
	{
		key_released = 1U;
	}

	if ((key_released == 1U) && ((K_UP == 0) || (K_DOWN == 0) || (K_LEFT == 0) || (K_RIGHT == 0)))
	{
		delay_ms(10);
		key_released = 0U;

		if (K_UP == 0)
		{
			return KEY_UP;
		}
		if (K_DOWN == 0)
		{
			return KEY_DOWN;
		}
		if (K_LEFT == 0)
		{
			return KEY_LEFT;
		}
		if (K_RIGHT == 0)
		{
			return KEY_RIGHT;
		}
	}

	if ((K_UP == 1) && (K_DOWN == 1) && (K_LEFT == 1) && (K_RIGHT == 1))
	{
		key_released = 1U;
	}

	return 0;
}

