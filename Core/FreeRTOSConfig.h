#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* STM32F103C8T6: 72 MHz, Cortex-M3, 64KB Flash / 20KB SRAM. */
#define configCPU_CLOCK_HZ                  ((unsigned long)72000000)
#define configTICK_RATE_HZ                  ((TickType_t)1000)
#define pdTICKS_TO_MS(xTicks)               ((TickType_t)(((xTicks) * 1000UL) / configTICK_RATE_HZ))
#define configMAX_PRIORITIES                5
#define configMINIMAL_STACK_SIZE            ((unsigned short)128)
#define configTOTAL_HEAP_SIZE               ((size_t)(12 * 1024))
#define configMAX_TASK_NAME_LEN             (12)
#define configUSE_PREEMPTION                1
#define configUSE_IDLE_HOOK                 1
#define configUSE_TICK_HOOK                 0
#define configUSE_MALLOC_FAILED_HOOK        1
#define configCHECK_FOR_STACK_OVERFLOW      2
#define configUSE_16_BIT_TICKS              0
#define configIDLE_SHOULD_YIELD             1
#define configUSE_TASK_NOTIFICATIONS        1
#define configUSE_MUTEXES                   1
#define configUSE_RECURSIVE_MUTEXES         0
#define configUSE_COUNTING_SEMAPHORES       0
#define configUSE_TRACE_FACILITY            0
#define configUSE_QUEUE_REGISTRY            0
#define configQUEUE_REGISTRY_MAX_NAME_LEN   0
#define configUSE_TIMERS                    1
#define configTIMER_TASK_PRIORITY           (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH            5
#define configTIMER_TASK_STACK_DEPTH        configMINIMAL_STACK_SIZE
#define configUSE_TASK_HEARTBEAT            1

/* Interrupt nesting behavior configuration. */
#define configKERNEL_INTERRUPT_PRIORITY     255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 5

/* Set the following to 1 to include the function in the build. */
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskDelayUntil             1
#define INCLUDE_xTaskGetSchedulerState      0
#define INCLUDE_xTaskGetTickCount           1
#define INCLUDE_xTaskResumeFromISR          1
#define INCLUDE_xTimerPendFunctionCall      0

/* Assert. */
#define configASSERT(x)                     if ((x) == 0) { taskDISABLE_INTERRUPTS(); for (;;); }

#endif /* FREERTOS_CONFIG_H */
