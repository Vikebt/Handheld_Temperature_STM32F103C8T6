#include "pwm.h"

void  TIM3_CH1_PWM_Init(u16 pre,u16 psc){
		GPIO_InitTypeDef  GPIO_InitStructure;
		TIM_TimeBaseInitTypeDef  TIM_TimeBaseInitStructure;
	  TIM_OCInitTypeDef TIM_OCInitStructure;

	
	
	  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);//时钟初始化
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);
	//*************************************************（映射AFIO 时钟开启）
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
	//*************************************************
	  GPIO_InitStructure.GPIO_Pin=GPIO_Pin_6;
	  GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	  GPIO_Init(GPIOC,&GPIO_InitStructure);
	//**************************************************(GPIO管脚映射)
	
	  GPIO_PinRemapConfig(GPIO_FullRemap_TIM3,ENABLE);//完全复用映射
	
	//**************************************************(初始化定时器参数)
	
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_Period=pre;
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Prescaler=psc;
	TIM_TimeBaseInit(TIM3,&TIM_TimeBaseInitStructure);
	
	//**************************************************PWM配置
	
	TIM_OCInitStructure.TIM_OCMode=TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OCPolarity=TIM_OCPolarity_Low;
	TIM_OCInitStructure.TIM_OutputState=TIM_OutputState_Enable;
	TIM_OC2Init(TIM3,&TIM_OCInitStructure);
	
	
	 	TIM_Cmd(TIM3,ENABLE);//开启定时器
		
		
		TIM_OC2PreloadConfig(TIM3,TIM_OCPreload_Enable);//使能CCRx   
//		TIM_ARRPreloadConfig(TIM3,ENABLE);//使能ARR




}
