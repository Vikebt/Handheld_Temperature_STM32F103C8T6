#include "systick.h"
#include "stm32f10x_rcc.h"

/* Legacy API name: TIM2 is the delay clock; SysTick belongs to FreeRTOS. */
void SysTick_Init(u8 SYSCLK)
{
    RCC_ClocksTypeDef clocks;
    u32 timer_hz;

    (void)SYSCLK;
    RCC_GetClocksFreq(&clocks);
    timer_hz = clocks.PCLK1_Frequency;
    if (clocks.PCLK1_Frequency != clocks.HCLK_Frequency)
    {
        timer_hz *= 2U; /* APB1 timer clock doubles when the bus is prescaled. */
    }

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM2->CR1 = 0U;
    TIM2->PSC = (timer_hz / 1000000U) - 1U;
    TIM2->ARR = 0xFFFFU;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CNT = 0U;
    TIM2->CR1 = TIM_CR1_CEN;
}

void delay_us(u32 microseconds)
{
    while (microseconds != 0U)
    {
        u16 start = (u16)TIM2->CNT;
        u16 interval = (microseconds > 60000U) ? 60000U : (u16)microseconds;

        while ((u16)((u16)TIM2->CNT - start) < interval)
        {
            /* Free-running 1 MHz counter works before and after scheduler start. */
        }
        microseconds -= interval;
    }
}

void delay_ms(u32 milliseconds)
{
    while (milliseconds-- != 0U)
    {
        delay_us(1000U);
    }
}
