/* ============================================================================
 *  S4 — SysTick、精确延时与按键：告别 8051 的 for 循环
 * ============================================================================
 *  文件：firmware/study/S4_SYSTICK_EXTI/systick_exti.c
 *
 *  为什么要做这个：
 *    你从 8051 过来，最容易带过来的习惯是 `for(i=0;i<100000;i++);` 延时。
 *    在 STM32 上这招不灵了：
 *      · 编译优化一开，空循环可能被**整个删掉**
 *      · 中断一来，延时就被拉长
 *      · `int` 从 16 位变成 32 位，你以前算的循环次数完全不对
 *    本模块给你两套**可靠**的延时，并加一个按键做交互。
 *
 *  ---------------------------------------------------------------------------
 *  【两套延时】
 *
 *    ① delay_ms —— 用 SysTick
 *       SysTick 是 Cortex-M3 内核自带的 24 位递减计数器，
 *       配成 1ms 中断，ISR 里给一个全局计数 +1；delay_ms 就是"等计数涨够"。
 *       ★ 好处：与 CPU 频率无关、不受编译优化影响、可被中断打断也不丢节拍。
 *
 *    ② delay_us —— 用 DWT->CYCCNT
 *       Cortex-M3 内核里有个"周期计数器"(CYCCNT)，每 CPU 周期 +1，
 *       72MHz 下 1µs = 72 个计数。读两次的差值就是精确的 µs 数。
 *       ★ 好处：精度到 1 个周期（13.9ns），不用占用任何定时器。
 *       ★ 代价：占一点内核调试资源（不影响下载/调试）。
 *
 *  ---------------------------------------------------------------------------
 *  ===== 任务 =====
 *  T1：SysTick 配成 1ms 中断，实现 delay_ms()
 *  T2：打开 DWT->CYCCNT，实现 delay_us()
 *  T3：用示波器验收——把延时"夹"在两次翻转之间：
 *          PA1 = 1; delay_us(10); PA1 = 0; delay_ms(1);
 *       → PA1 上应看到 10µs 宽的脉冲，周期约 1.01ms
 *  T4：EXTI 按键（PA0 下降沿）→ 按一次，让 PC13 的 LED 闪一下
 *      （进阶：按一次发"一组"图 6 的时序，把 Lab4 的东西接进来）
 *
 *  ===== 涉及寄存器 / 函数 =====
 *    SysTick->LOAD / VAL / CTRL   重装载值 / 当前值 / 控制(CLKSOURCE|TICKINT|ENABLE)
 *    CMSIS：SysTick_Config(ticks) 一步配好并开中断
 *    CoreDebug->DEMCR             bit24 (TRCENA) —— 用 DWT 前必须置 1
 *    DWT->CYCCNT                  周期计数（先写 0 清零）
 *    DWT->CTRL                    bit0 (CYCCNTENA) 使能计数
 *    EXTI / AFIO                  同 S2（★ AFIO 时钟别忘了）
 *
 *  ===== 思考题 =====
 *  Q1：为什么 `for` 空循环在 STM32 上不可靠？（至少说出两个原因）
 *  Q2：delay_us() 用 DWT 和"用定时器"实现，各有什么优劣？
 *  Q3：SysTick 中断里如果做很重的事，delay_ms 会怎样？
 *  Q4：把按键接到中断和"主循环轮询"，各适合什么场景？按键抖动怎么消？
 * ========================================================================== */

#include "stm32f10x.h"

#define LED_PIN     13U    /* PC13 */
#define KEY_PIN     0U     /* PA0 按键（内部上拉 → 下降沿） */
#define SCOPE_PIN   1U     /* PA1 示波器观察 */

/* SysTick 的毫秒计数（ISR 里 +1） */
static volatile uint32_t g_ms_ticks = 0;

/* ---------------------------------------------------------------------------
 *  TODO T2：打开 DWT->CYCCNT，实现 delay_us
 *    ① CoreDebug->DEMCR |= (1UL << 24);     // TRCENA：使能跟踪（DWT 的前提）
 *    ② DWT->CYCCNT = 0;
 *    ③ DWT->CTRL  |= (1UL << 0);            // CYCCNTENA
 *    delay_us(uint32_t us)：
 *       uint32_t start = DWT->CYCCNT;
 *       uint32_t need  = us * (SystemCoreClock / 1000000UL);   // 72MHz → us*72
 *       while ((DWT->CYCCNT - start) < need) { }               // ★ 用减法，天然处理回绕
 *    ⚠ 单次延时上限：32 位 / 72 ≈ 59.6 秒，足够；但要延时更长就分段或改用 ms
 * ------------------------------------------------------------------------- */
static void dwt_init(void)
{
    /* 你的代码： */
}

static void delay_us(uint32_t us)
{
    /* 你的代码： */
}

/* ---------------------------------------------------------------------------
 *  TODO T1：SysTick 1ms 中断
 *    SysTick_Config(SystemCoreClock / 1000);   // 72e6/1000 = 72000 → 1ms
 *    它会自动：设 LOAD、清 VAL、选内核时钟、开 TICKINT、启动
 *    你只要在 SysTick_Handler 里给 g_ms_ticks 加 1 即可
 * ------------------------------------------------------------------------- */
static void systick_init(void)
{
    /* 你的代码： */
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = g_ms_ticks;
    while ((g_ms_ticks - start) < ms)      /* ★ 用减法，回绕也安全 */
    {
    }
}

/* ---------------------------------------------------------------------------
 *  TODO T4：按键 GPIO + EXTI（PA0 下降沿）
 *    同 S2：GPIOA 输入上拉、AFIO 时钟、EXTICR1、FTSR、IMR、NVIC
 * ------------------------------------------------------------------------- */
static void key_init(void)
{
    RCC->APB2ENR |= (1U << 2);            /* GPIOA 时钟 */

    /* 你的代码： */
}

int main(void)
{
    systick_init();
    dwt_init();

    /* 示波器观察引脚 PA1 + LED PC13 */
    RCC->APB2ENR |= (1U << 2) | (1U << 4);
    GPIOA->CRL &= ~(0xFU << (SCOPE_PIN * 4));
    GPIOA->CRL |=  (0x3U << (SCOPE_PIN * 4));           /* PA1 通用推挽 */
    GPIOC->CRH &= ~(0xFU << ((LED_PIN - 8) * 4));
    GPIOC->CRH |=  (0x3U << ((LED_PIN - 8) * 4));
    GPIOC->BSRR = (1U << LED_PIN);                      /* 先灭 */

    key_init();

    while (1)
    {
        /* TODO T3：验收延时精度 —— 应看到 10µs 宽、约 1.01ms 周期的脉冲 */
        GPIOA->BSRR = (1U << SCOPE_PIN);
        delay_us(10);
        GPIOA->BRR  = (1U << SCOPE_PIN);
        delay_ms(1);
    }
}

/* ---------------------------------------------------------------------------
 *  SysTick 中断：毫秒节拍
 * ------------------------------------------------------------------------- */
void SysTick_Handler(void)
{
    /* 你的代码：g_ms_ticks++ */
}

/* ---------------------------------------------------------------------------
 *  按键中断：按一次，LED 闪一下（进阶：发一组图 6 时序）
 *    记得消抖：进 ISR 后可以先 delay_ms(20) 再判一次引脚电平，
 *    ★ 但 ISR 里延时不好 —— 想一想怎么用"记录时间戳"的方式在主循环里消抖
 * ------------------------------------------------------------------------- */
void EXTI0_IRQHandler(void)
{
    /* 你的代码：清 EXTI->PR（写 1）+ 触发动作 */
}
