/**
 * @file    led.c
 * @brief   LED 驱动 — 基础GPIO控制 + PWM呼吸灯效果
 * @note    PC0~PC7 共8个LED
 *          呼吸灯使用 TIM3_CH1 (PC6) 实现 PWM 正弦调制
 */

#include "led.h"
#include "pwm.h"

/* 呼吸灯参数 */
#define BREATHING_CYCLE_MS      2000    /* 完整呼吸周期 2秒 */
#define BREATHING_STEPS         200     /* 周期内步数 */
#define PWM_MAX_DUTY            899     /* PWM 最大占空比 (ARR=899) */

static volatile uint8_t  g_ucBreathingEnabled = 0;
static volatile uint16_t g_usBreathingStep = 0;

/* 预计算正弦表 (256点, 0~255 → 0~PWM_MAX_DUTY) */
static const uint16_t g_auSinTable[256] = {
    0, 5, 11, 17, 22, 28, 33, 39, 44, 49, 55, 60, 65, 70, 75, 80,
    85, 90, 95, 100, 104, 109, 113, 118, 122, 126, 130, 134, 138, 142, 146, 149,
    153, 157, 160, 163, 167, 170, 173, 176, 179, 182, 184, 187, 190, 192, 195, 197,
    199, 201, 203, 205, 207, 209, 211, 212, 214, 215, 217, 218, 219, 220, 221, 222,
    223, 224, 225, 225, 226, 226, 227, 227, 227, 228, 228, 228, 228, 228, 228, 228,
    228, 227, 227, 227, 226, 226, 225, 225, 224, 223, 223, 222, 221, 220, 219, 218,
    217, 216, 215, 213, 212, 211, 209, 208, 206, 205, 203, 201, 200, 198, 196, 194,
    192, 190, 188, 186, 184, 181, 179, 177, 174, 172, 169, 167, 164, 161, 159, 156,
    153, 150, 147, 145, 142, 139, 136, 133, 130, 126, 123, 120, 117, 114, 111, 108,
    105, 102, 99, 96, 93, 90, 87, 84, 81, 78, 75, 72, 69, 66, 64, 61,
    58, 55, 53, 50, 48, 45, 43, 40, 38, 36, 33, 31, 29, 27, 25, 23,
    21, 19, 17, 15, 14, 12, 10, 9, 7, 6, 5, 3, 2, 1, 0, 0,
    0, 1, 2, 3, 5, 6, 7, 9, 10, 12, 14, 15, 17, 19, 21, 23,
    25, 27, 29, 31, 33, 36, 38, 40, 43, 45, 48, 50, 53, 55, 58, 61,
    64, 66, 69, 72, 75, 78, 81, 84, 87, 90, 93, 96, 99, 102, 105, 108,
    111, 114, 117, 120, 123, 126, 130, 133, 136, 139, 142, 145, 147, 150, 153, 156
};

void LED_Init(void)
{
    GPIO_InitTypeDef gpioInit;

    RCC_APB2PeriphClockCmd(LED_PORT_RCC, ENABLE);

    gpioInit.GPIO_Pin = LED_PIN;
    gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
    gpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &gpioInit);

    LED_ALL_OFF();

    /* 初始化 PWM (呼吸灯用) */
    TIM3_CH1_PWM_Init(PWM_MAX_DUTY, 0);
}

/**
 * @brief  启动呼吸灯效果
 * @note   需要周期性调用 LED_Breathing_Tick() (如每10ms)
 */
void LED_Breathing_Start(void)
{
    g_ucBreathingEnabled = 1;
    g_usBreathingStep = 0;

    /* 使能 PWM 输出 */
    TIM_Cmd(TIM3, ENABLE);
}

/**
 * @brief  停止呼吸灯
 */
void LED_Breathing_Stop(void)
{
    g_ucBreathingEnabled = 0;

    /* 关闭 PWM */
    TIM_Cmd(TIM3, DISABLE);
    GPIO_ResetBits(GPIOC, GPIO_Pin_6);  /* 确保LED灭 */
}

/**
 * @brief  呼吸灯步进 (由定时器或任务周期调用)
 * @note   每步更新一次 PWM 占空比
 *         建议调用间隔 = BREATHING_CYCLE_MS / BREATHING_STEPS = 10ms
 */
void LED_Breathing_Tick(void)
{
    if (g_ucBreathingEnabled == 0) return;

    /* 查表获取当前亮度值 */
    uint8_t ucTableIndex = (uint8_t)((uint32_t)g_usBreathingStep * 256 / BREATHING_STEPS);
    uint16_t usDuty = g_auSinTable[ucTableIndex];

    /* 设置 PWM 比较值 */
    TIM_SetCompare2(TIM3, usDuty);

    g_usBreathingStep++;
    if (g_usBreathingStep >= BREATHING_STEPS)
    {
        g_usBreathingStep = 0;
    }
}
