#include "stm32f10x.h"

/* ============================================================================
 *  Lab2 + LED 跟踪
 *    用 TIM2_CH1 在 PA0 输出方波，并把 PA0 的电平"复制"到板载 LED(PC13)，
 *    方便没有示波器时观察波形是否产生。
 *
 *  板载 LED 极性：多数 STM32 蓝板是"低电平点亮"（3.3V→电阻→LED→PC13）。
 *  若你的板子相反，把下面 1U 改成 0U。
 * ========================================================================== */
#define LED_ACTIVE_LOW   1U

#if LED_ACTIVE_LOW
    #define LED_ON()    do { GPIOC->BRR  = (1U << 13); } while (0)
    #define LED_OFF()   do { GPIOC->BSRR = (1U << 13); } while (0)
#else
    #define LED_ON()    do { GPIOC->BSRR = (1U << 13); } while (0)
    #define LED_OFF()   do { GPIOC->BRR  = (1U << 13); } while (0)
#endif

/* PC13 配成通用推挽输出 */
static void led_init(void)
{
    RCC->APB2ENR |= (1U << 4);          /* 开 GPIOC 时钟 */
    GPIOC->CRH &= ~(0xF << 20);         /* 清 PC13 的 4 个配置位 */
    GPIOC->CRH |=  (0x3 << 20);         /* PC13 = 通用推挽输出 50MHz */
    LED_OFF();                          /* 初始熄灭 */
}

/* TIM2_CH1 在 PA0 输出 PWM */
static void timer_pwm_init(uint16_t psc, uint16_t arr, uint16_t ccr)
{
    RCC->APB1ENR |= (1U << 0);          /* 开 TIM2 时钟（APB1 bit0 = TIM2EN） */
    RCC->APB2ENR |= (1U << 2);          /* ★ 开 GPIOA 时钟（APB2 bit2 = IOPAEN）
                                         *   —— 之前漏了这一步！GPIOA 在 APB2，
                                         *   不开时钟的话下面两行 CRL 写入全部无效，
                                         *   PA0 就停在默认的浮空输入，不会有任何波形。 */
    GPIOA->CRL &= ~0xF;                 /* 清 PA0 的 4 个配置位 */
    GPIOA->CRL |=  0xB;                 /* PA0 = 复用推挽输出 50MHz */

    TIM2->PSC = psc;
    TIM2->ARR = arr;
    TIM2->CCMR1 &= ~((0x7 << 4) | (0x1 << 3));
    TIM2->CCMR1 |=  ((0x6 << 4) | (0x1 << 3));   /* PWM 模式1 + 预装载 */
    TIM2->CCR1 = ccr;

    TIM2->CCER |= (1U << 0);            /* 使能通道 1 输出（CC1E） */
    TIM2->CR1  |= (1U << 0);            /* 启动计数器（CEN） */
}

int main(void)
{
    led_init();
    timer_pwm_init(5999, 5999, 3000);   /* 2Hz、50% —— 低频，肉眼可见 */

    while (1)
    {
        /* 把 PA0 的实际电平复制到 LED：PA0 高 → 亮，PA0 低 → 灭 */
        if (GPIOA->IDR & (1U << 0))
            LED_ON();
        else
            LED_OFF();
    }
}
