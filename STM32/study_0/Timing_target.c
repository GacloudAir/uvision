#include "stm32f10x.h"
//===========时序参数=============
#define TIM_CLK_HZ    72000000UL               
#define TICK_US       2UL                     //每 tick 2µs
#define CNT_PER_TICK  (TIM_CLK_HZ / 1000000UL * TICK_US)   //每tick 2us,即144cnt
#define ACTIVE_TICKS  3UL                     //有效时序 3 tick(6us)
#define IDLE_TICKS    0UL                     //关断间隔 tick 数,本次需求中没有,故置零表示不需要
                                              //需要时改这里，如 5 → 周期 16µs，
                                              //间隔内三路全低
#define PERIOD_CNT    (CNT_PER_TICK * (ACTIVE_TICKS + IDLE_TICKS))  //单次循环计数:432
#define ARR_VAL       (PERIOD_CNT - 1U)                             //ARR:431
#define CCR_OE        (CNT_PER_TICK * 2U)     // OE 高 2 tick = 288,然后翻转为低
#define CCR_PULSE     (CNT_PER_TICK * 1U)     // PULSE 高 1 tick = 144,然后翻转为低
#define CCR_DAMP      (CNT_PER_TICK * 2U)     // DAMP: PWM2,低2tick后翻转为高
//===========推挽设置=============
#define GPIO_SPEED_AF   0xAU                  /* 复用推挽 2MHz */
#define GPIO_SPEED_OUT  0x2U                  /* 通用推挽 2MHz */

static void outputs_safe_low(void)
{   
//==========上电安全态设置=========
    RCC->APB2ENR |= (1U << 2);                          /* GPIOA 时钟 (APB2) */
    GPIOA->CRL &= ~(0xFU << 4);   GPIOA->CRL |= (GPIO_SPEED_OUT << 4);   /* PA1,均配置为通用推挽2MHz */
    GPIOA->CRL &= ~(0xFU << 8);   GPIOA->CRL |= (GPIO_SPEED_OUT << 8);   /* PA2 */
    GPIOA->CRL &= ~(0xFU << 12);  GPIOA->CRL |= (GPIO_SPEED_OUT << 12);  /* PA3 */
    GPIOA->BRR = (1U << 1) | (1U << 2) | (1U << 3);     // 全部拉低,保持上电安全态
}

static void tim2_init(void)//TIM2初始化,配置三路输出
{
    RCC->APB1ENR |= (1U << 0);                          /* TIM2 时钟 (APB1) */

    TIM2->PSC = 0;                                      /* 72MHz 不分频 */
    TIM2->ARR = (uint16_t)ARR_VAL;                      /* 431 → 周期 6µs */

    /* CH2 = OE：PWM1（CNT < CCR 有效）→ 高 [0, 288) = 0~4µs */
    TIM2->CCMR1 &= ~(0x7U << 12);  TIM2->CCMR1 |= (0x6U << 12);   /* OC2M=110 */
    TIM2->CCR2  = (uint16_t)CCR_OE;

    /* CH3 = PULSE：PWM1 → 高 [0, 144) = 0~2µs */
    TIM2->CCMR2 &= ~(0x7U << 4);   TIM2->CCMR2 |= (0x6U << 4);    /* OC3M=110 */
    TIM2->CCR3  = (uint16_t)CCR_PULSE;

    /* CH4 = DAMP：PWM2（CNT ≥ CCR 有效）→ 高 [288, 432) = 4~6µs,贴周期末尾 */
    TIM2->CCMR2 &= ~(0x7U << 12);  TIM2->CCMR2 |= (0x7U << 12);   //OC4M=111
    TIM2->CCR4  = (uint16_t)CCR_DAMP;
    TIM2->CCER |= (1U << 4) | (1U << 8) | (1U << 12);//使能三路输出：CC2E(bit4) / CC3E(bit8) / CC4E(bit12)
}

static void outputs_to_af(void)//将三路输出配置为复用推挽输出,交给TIM2控制
{
    GPIOA->CRL &= ~(0xFU << 4);   GPIOA->CRL |= (GPIO_SPEED_AF << 4);   /* PA1 */
    GPIOA->CRL &= ~(0xFU << 8);   GPIOA->CRL |= (GPIO_SPEED_AF << 8);   /* PA2 */
    GPIOA->CRL &= ~(0xFU << 12);  GPIOA->CRL |= (GPIO_SPEED_AF << 12);  /* PA3 */
}

int main(void)
{
    outputs_safe_low();
    tim2_init();
    GPIOA->BSRR = (1U << 2);                            /* PULSE 预置为高 */
    outputs_to_af();
    TIM2->CR1 |= (1U << 0);                             /* CEN：开始发射 */

    while (1)
    {
        //波形完全由硬件产生,CPU保持空闲即可
    }
}