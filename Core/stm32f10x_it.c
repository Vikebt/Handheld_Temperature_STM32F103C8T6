/**
  ******************************************************************************
  * @file    stm32f10x_it.c
  * @brief   Interrupt Service Routines for FreeRTOS-based system.
  *          整合 FreeRTOS 必需的 PendSV/SysTick 中断处理
  *          同时包含 USART, EXTI, TIM 等外设中断
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"
#include "esp8266_drive.h"
#include "led.h"
#include <stdio.h>
#include <string.h>

/* 外部 FreeRTOS 移植层函数 */
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler(void);

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

void NMI_Handler(void) { }

void HardFault_Handler(void)
{
    printf("[FATAL] Hard Fault! System halted.\r\n");
    while (1);
}

void MemManage_Handler(void)
{
    printf("[FATAL] Memory Manage Fault!\r\n");
    while (1);
}

void BusFault_Handler(void)
{
    printf("[FATAL] Bus Fault!\r\n");
    while (1);
}

void UsageFault_Handler(void)
{
    printf("[FATAL] Usage Fault!\r\n");
    while (1);
}

/**
  * @brief  SVC_Handler — FreeRTOS starts the first task through SVC 0.
  * @note   An empty handler links successfully but never restores the first
  *         task context on the Cortex-M3 port.
  */
void SVC_Handler(void)
{
    vPortSVCHandler();
}

void DebugMon_Handler(void) { }

/**
  * @brief  PendSV_Handler — FreeRTOS 上下文切换
  * @note   委托给移植层的 xPortPendSVHandler
  */
void PendSV_Handler(void)
{
    xPortPendSVHandler();
}

/**
  * @brief  SysTick_Handler — FreeRTOS 系统节拍
  * @note   委托给移植层的 xPortSysTickHandler
  */
void SysTick_Handler(void)
{
    xPortSysTickHandler();
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/******************************************************************************/

/**
  * @brief  USART1 中断 — 调试串口 (printf)
  */
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        volatile uint8_t ucData = USART_ReceiveData(USART1);
        (void)ucData;
    }
}

/**
  * @brief  USART2 中断 — ESP8266 Wi-Fi 模块
  */
void USART2_IRQHandler(void)
{
    uint8_t ucCh;

    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        ucCh = (uint8_t)USART_ReceiveData(USART2);

        extern struct STRUCT_USART_Fram ESP8266_Fram_Record_Struct;
        if (ESP8266_Fram_Record_Struct.InfBit.FramLength < (RX_BUF_MAX_LEN - 1))
        {
            ESP8266_Fram_Record_Struct.Data_RX_BUF[
                ESP8266_Fram_Record_Struct.InfBit.FramLength++] = ucCh;
        }
    }

    if (USART_GetITStatus(USART2, USART_IT_IDLE) == SET)
    {
        extern struct STRUCT_USART_Fram ESP8266_Fram_Record_Struct;
        ESP8266_Fram_Record_Struct.InfBit.FramFinishFlag = 1;

        volatile uint8_t ucDummy = USART_ReceiveData(USART2);
        (void)ucDummy;

        extern volatile uint8_t TcpClosedFlag;
        TcpClosedFlag = (strstr(ESP8266_Fram_Record_Struct.Data_RX_BUF, "CLOSED\r\n") != NULL) ? 1 : 0;
    }
}

/**
  * @brief  USART3 中断 — 传感器数据接收 (备用)
  */
void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        volatile uint8_t ucData = (uint8_t)USART_ReceiveData(USART3);
        (void)ucData;
    }
}

/**
  * @brief  TIM4 中断 — 系统辅助定时器
  */
void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) == SET)
    {
        led2 = !led2;
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    }
}

void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) == SET)
    {
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

void EXTI2_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line2) == SET)
    {
        EXTI_ClearITPendingBit(EXTI_Line2);
    }
}

void EXTI3_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line3) == SET)
    {
        EXTI_ClearITPendingBit(EXTI_Line3);
    }
}

void EXTI4_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line4) == SET)
    {
        EXTI_ClearITPendingBit(EXTI_Line4);
    }
}

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
