/* ============================================================================
 *  随钻超声阵列测井仪 · 激励电路时序控制器  —— 目标代码 v2
 *  MCU : STM32F103C8T6 @ 72MHz
 *  外设: TIM2(主: SYNC/OE/PULSE) + TIM3(DAMP, 相位预置)
 * ============================================================================
 *  输出（复现论文图 6）：
 *      PA0 = SYNC / Trigger_out
 *      PA1 = OE
 *      PA2 = PULSE        (MD1813 的 INA、INB)
 *      PA6 = DAMP         (MD1813 的 INC、IND)
 *
 *  一个周期 = 16 节拍；250kHz 时 = 288 tick = 4µs：
 *      tick   0 : OE↑ , PULSE↑       ┐ 两组边沿同时
 *      tick  72 : PULSE↓ , DAMP↑     ┘ 图 6 里同一条虚线
 *      tick 144 : OE↓ , DAMP↓         两组边沿同时
 *      tick 270 : SYNC↑              提前 1 节拍，OE↑ 时回落
 *  占空比：OE 50% / PULSE 25% / DAMP 25%
 * ---------------------------------------------------------------------------
 *  【v2 相对 v1 的两处修改】
 *
 *  ① PA6 无波形 —— 根因是 v1 用"TIM3 从模式=复位 + TIM2 的 OC4REF 触发"
 *     做相位对齐。但 OC4REF 在一个周期里是 [72,288) 的**长电平**（高 75%），
 *     从模式复位会按触发电平把 TIM3 计数器**按住贴在 0**：
 *     CNT 恒为 0 < CCR(72) → OC1REF 恒高 → DAMP 是一条**恒高直线**，看不到方波。
 *
 *     v2 改成最朴素、最可靠的办法：**不用从模式**，直接把 TIM3 的计数器
 *     预置成 (周期 − 72)，再启动。两个定时器同频同源，启动后相位保持不变；
 *     残余误差只有"两条启动指令之间的几个周期"（≈0.1µs），用
 *     DAMP_PHASE_TRIM 可微调。
 *
 *  ② 翻转时其它通道出现"逐渐削减的振铃" —— 首要推手是 GPIO 边沿太快。
 *     50MHz 档边沿仅 2~3ns，驱动长杜邦线时 di/dt 极大；降到 2MHz 后边沿
 *     ≈30~50ns，振铃明显减小，而 4µs 的周期完全不受影响（见 GPIO_SPEED_AF）。
 *
 *  ---------------------------------------------------------------------------
 *  为什么 DAMP 必须用第二个定时器：
 *     一个定时器只有一条 CNT，PWM 窗口只能贴"起点"(PWM1) 或"终点"(PWM2)；
 *     DAMP 的 [72,144) 夹在周期正中间，两种模式都做不出 → 只能换一个相位。
 * ========================================================================== */

#include "stm32f10x.h"

/* ========================= ★★ 改频率：只改这一个数 ★★ =========================
 *   F_OUT_HZ 可填 50000 ~ 300000（Hz）。改完重新编译，
 *   ARR / CCR / 相位偏移全部自动跟着算出来。
 * ========================================================================== */
#define F_OUT_HZ      250000UL
/* ============================================================================ */

/* DAMP 相位微调（单位 tick，1 tick = 13.9ns）：正数让 DAMP 提前，负数让它推后。
 * 正常情况下保持 0；若示波器上 DAMP↑ 与 PULSE↓ 差了一点，再 ±1~±5 微调。 */
#define DAMP_PHASE_TRIM   0

/* GPIO 输出速度档位：0x9=10MHz，0xA=2MHz，0xB=50MHz
 * ★ 振铃/串扰严重时优先用 2MHz(0xA)：边沿变缓，di/dt 变小。
 *   以后接线变短、或串了 22~100Ω 电阻，可以换回 10MHz(0x9) / 50MHz(0xB)。 */
#define GPIO_SPEED_AF    0xAU      /* 复用推挽：2MHz */
#define GPIO_SPEED_OUT   0x2U      /* 通用推挽：2MHz */


/* ---------- 由 F_OUT_HZ 自动推出的时间参数（编译期常量） ---------- */
#define TIM_CLK_HZ      72000000UL
#define PERIOD_TICK     (TIM_CLK_HZ / F_OUT_HZ)              /* 250k → 288 */
#define ARR_VAL         (PERIOD_TICK - 1U)                   /* 250k → 287 */
#define CCR_OE          (PERIOD_TICK / 2U)                   /* 144 (50%) */
#define CCR_PULSE       (PERIOD_TICK / 4U)                   /* 72  (25%) */
#define CCR_DAMP        (PERIOD_TICK / 4U)                   /* 72  (25%) */
#define CCR_SYNC        (PERIOD_TICK - PERIOD_TICK / 16U)    /* 270 (提前 1 节拍) */


/* ---------------------------------------------------------------------------
 *  1) 上电先把四个输出脚配成"通用推挽输出"并写 0 —— 保证启动前是关断电平
 * ------------------------------------------------------------------------- */
static void outputs_safe_low(void)
{
    RCC->APB2ENR |= (1U << 2);                  /* 开 GPIOA 时钟（APB2） */

    GPIOA->CRL &= ~(0xFU << 0);  GPIOA->CRL |= (GPIO_SPEED_OUT << 0);   /* PA0 */
    GPIOA->CRL &= ~(0xFU << 4);  GPIOA->CRL |= (GPIO_SPEED_OUT << 4);   /* PA1 */
    GPIOA->CRL &= ~(0xFU << 8);  GPIOA->CRL |= (GPIO_SPEED_OUT << 8);   /* PA2 */
    GPIOA->CRL &= ~(0xFU << 24); GPIOA->CRL |= (GPIO_SPEED_OUT << 24);  /* PA6 */

    GPIOA->BRR = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 6);         /* 全部拉低 */
}

/* ---------------------------------------------------------------------------
 *  2) 时基、通道都配好之后，再把这四个脚切成"复用推挽"，交给定时器
 * ------------------------------------------------------------------------- */
static void outputs_to_af(void)
{
    GPIOA->CRL &= ~(0xFU << 0);  GPIOA->CRL |= (GPIO_SPEED_AF << 0);    /* PA0 = SYNC  */
    GPIOA->CRL &= ~(0xFU << 4);  GPIOA->CRL |= (GPIO_SPEED_AF << 4);    /* PA1 = OE    */
    GPIOA->CRL &= ~(0xFU << 8);  GPIOA->CRL |= (GPIO_SPEED_AF << 8);    /* PA2 = PULSE */
    GPIOA->CRL &= ~(0xFU << 24); GPIOA->CRL |= (GPIO_SPEED_AF << 24);   /* PA6 = DAMP  */
}

/* ---------------------------------------------------------------------------
 *  TIM2：SYNC / OE / PULSE（三个都贴周期两端，PWM 直接生成）
 * ------------------------------------------------------------------------- */
static void tim2_init(void)
{
    RCC->APB1ENR |= (1U << 0);                  /* TIM2 时钟 */

    TIM2->PSC = 0;                              /* 72MHz 不分频 */
    TIM2->ARR = ARR_VAL;                        /* 250k → 287 */

    /* CH1 = SYNC：PWM2（CNT ≥ CCR 有效）→ 高电平 [CCR1, 周期末) = tick 270~288 */
    TIM2->CCMR1 &= ~(0x7U << 4);  TIM2->CCMR1 |= (0x7U << 4);   /* OC1M = 111 (PWM2) */
    TIM2->CCR1   = CCR_SYNC;

    /* CH2 = OE：PWM1（CNT < CCR 有效）→ 高电平 [0, CCR2) */
    TIM2->CCMR1 &= ~(0x7U << 12); TIM2->CCMR1 |= (0x6U << 12);  /* OC2M = 110 (PWM1) */
    TIM2->CCR2   = CCR_OE;

    /* CH3 = PULSE：PWM1 → 高电平 [0, CCR3) */
    TIM2->CCMR2 &= ~(0x7U << 4);  TIM2->CCMR2 |= (0x6U << 4);   /* OC3M = 110 (PWM1) */
    TIM2->CCR3   = CCR_PULSE;

    /* 只让 CH1/CH2/CH3 输出到引脚 */
    TIM2->CCER |= (1U << 0) | (1U << 4) | (1U << 8);            /* CC1E, CC2E, CC3E */
}

/* ---------------------------------------------------------------------------
 *  TIM3：DAMP（相位比 TIM2 落后 72 tick —— 靠"预置计数器"实现，不用从模式）
 * ------------------------------------------------------------------------- */
static void tim3_init(void)
{
    RCC->APB1ENR |= (1U << 1);                  /* TIM3 时钟 */

    TIM3->PSC = 0;
    TIM3->ARR = ARR_VAL;                        /* 与 TIM2 完全相同的周期 */

    /* CH1 = DAMP：PWM1 → 高电平 [0, CCR1)（相对 TIM3 自己的计数器） */
    TIM3->CCMR1 &= ~(0x7U << 4);  TIM3->CCMR1 |= (0x6U << 4);   /* OC1M = 110 (PWM1) */
    TIM3->CCR1   = CCR_DAMP;

    /* ★ 相位对齐：让 TIM3 的 CNT "落后" TIM2 72 tick。
     *   TIM3 预置成 (周期 − 72) = 216 → 它只要再数 72 个 tick 就归零，
     *   而归零那一刻正是 TIM2 的 tick 72 = PULSE↓ / DAMP↑。
     *   之后两个定时器同频同源，相位永久保持。 */
    TIM3->CNT = (uint16_t)(PERIOD_TICK - CCR_DAMP + DAMP_PHASE_TRIM);

    TIM3->CCER |= (1U << 0);                    /* CC1E：CH1 输出到 PA6 */
}


int main(void)
{
    outputs_safe_low();     /* ① 先给"关断"电平 */
    tim2_init();            /* ② 配好 TIM2（SYNC/OE/PULSE） */
    tim3_init();            /*    配好 TIM3（DAMP），此时 CNT 已预置好相位 */
    outputs_to_af();        /* ③ 一切就绪，才把引脚交给定时器 */

    TIM2->CR1 |= (1U << 0); /* ④ 启动 TIM2 */
    TIM3->CR1 |= (1U << 0); /*    紧接着启动 TIM3（相隔几条指令，≈0.1µs） */

    while (1)
    {
        /* 波形完全由硬件产生，CPU 空闲。 */
    }
}
