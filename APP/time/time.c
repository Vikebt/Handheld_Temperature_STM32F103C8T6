#include "time.h"
#include "led.h"
void TIM4_Init(u16 pre,u16 psc){
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;//定时器配置结构体
	NVIC_InitTypeDef NVIC_InitStructure;//优先级配置结构体
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4,ENABLE);//时钟初始化
	//***********************************************************************
	TIM_TimeBaseInitStructure.TIM_Period=pre;//定时器周期（自动重载定时器的值）
	TIM_TimeBaseInitStructure.TIM_Prescaler=psc;//用来作为TIMx时钟频率出书的预分频值
  TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;//时钟分频因子
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;//向上计数模式
	TIM_TimeBaseInit(TIM4,&TIM_TimeBaseInitStructure);
	//*************************************************************************
	TIM_ITConfig(TIM4,TIM_IT_Update,ENABLE);//中断的配置与使能
	//*************************************************************************
	NVIC_InitStructure.NVIC_IRQChannel=TIM4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=3;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	//**************************************************************************
	TIM_Cmd(TIM4,ENABLE);//开启定时器
	TIM_ClearITPendingBit(TIM4,TIM_IT_Update);
}
