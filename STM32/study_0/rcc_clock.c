#include "stm32f10x.h"

/* 板载输入时钟：按你实际硬件修改（蓝板外部晶振一般是 8M） */
#ifndef HSE_CLK
#define HSE_CLK 8000000U
#endif
#define HSI_CLK 8000000U   // F1 内部 HSI 标称 8M

/* 供 Watch 窗口观察的计算结果（volatile 防止被优化掉） */
volatile uint32_t g_sysclk, g_hclk, g_pclk1, g_pclk2, g_tim2;

static void clock_report(void)
{
    uint32_t cfgr  = RCC->CFGR;

    /* ---- 提取配置字段 ---- */
    uint32_t sws   = (cfgr >> 2)  & 0x3U;   // SWS: 当前实际系统时钟源
    uint32_t hpre  = (cfgr >> 4)  & 0xFU;   // HPRE: AHB 预分频
    uint32_t ppre1 = (cfgr >> 8)  & 0x7U;   // PPRE1: APB1 预分频
    uint32_t ppre2 = (cfgr >> 11) & 0x7U;   // PPRE2: APB2 预分频

    /* ---- 1) 求 SYSCLK ---- */
    uint32_t sysclk;
    if (sws == 0)          // HSI
        sysclk = HSI_CLK;
    else if (sws == 1)     // HSE
        sysclk = HSE_CLK;
    else                   // PLL
    {
        uint32_t pllsrc;
        if (cfgr & (1U << 16))           // PLLSRC=1: 用 HSE
        {
            pllsrc = HSE_CLK;
            if (cfgr & (1U << 17))       // PLLXTPRE=1: HSE 再 /2
                pllsrc /= 2;
        }
        else                             // PLLSRC=0: 用 HSI/2
        {
            pllsrc = HSI_CLK / 2;
        }
        uint32_t mf = (cfgr >> 18) & 0xFU;                 // PLLMUL[3:0]
        uint32_t pllmul = (mf >= 0xEU) ? 16U : (mf + 2U);  // 0000=×2 … 0111=×9, 111x=×16
        sysclk = pllsrc * pllmul;
    }

    /* ---- 2) 解码分频系数 ---- */
    uint32_t ahb  = (hpre  < 0x8) ? 1U : (1U << (hpre  - 7)); // 0xxx=1, 1000=2 … 1111=512
    uint32_t apb1 = (ppre1 < 0x4) ? 1U : (1U << (ppre1 - 3)); // 0xx=1, 100=2 … 111=16
    uint32_t apb2 = (ppre2 < 0x4) ? 1U : (1U << (ppre2 - 3));

    /* ---- 3) 推导各路时钟（APB 上的定时器有 ×2 规则） ---- */
    g_sysclk = sysclk;
    g_hclk   = sysclk / ahb;
    g_pclk1  = g_hclk / apb1;
    g_pclk2  = g_hclk / apb2;
    g_tim2   = (apb1 == 1) ? g_pclk1 : (2U * g_pclk1);   // 关键：TIM2 = 2×PCLK1（当 APB1≠1）
}

static void gpio_timer_init(uint16_t psc,uint16_t arr)//初始化函数
{
    //开始之前,列个清单:需要哪些管脚？哪些计时器？要不要启用UART串口？使用RCC->启用GPIO/定时器的时钟,从而允许管理具体外设
    //=========引脚部分==========
    RCC->APB2ENR |= (1U << 4);//启用(GPIO)A-2/B-3/C-4/AFIO-0,由APB2管理,写入1U激活,别忘了|=
    GPIOC->CRH &= ~(0xF<<20);//先清空
    GPIOC->CRH |= (0x3<<20);//启用PC13(先判断高(7-15)低位(0-7),然后用(位数-8)*4得到填入位数),具体填入内容见函数下的表
    //=========计时器部分========
    RCC->APB1ENR |= (1U << 0);//启用定时器(TIM)2-0/3-1/4-2,由APB1管理;注:TIM1由APB2管理,在11位
    TIM2->PSC = psc; TIM2->ARR = arr;//定义决定TIM2运行的三个数字:分频,单次重置上限,翻转电平计数(CCR,PWM模式需要)
    TIM2->DIER |= (1U << 0);        // 开启更新中断 (UIE)
    NVIC_SetPriority((IRQn_Type)28, 0); // STM32F103中,TIM2的中断值即为28
    NVIC_EnableIRQ((IRQn_Type)28);     // 在 NVIC 中开启 TIM2 中断
    TIM2->CR1  |= (1U << 0);//启动计数器(CEN)
}
int main(void)
{
    clock_report();
    gpio_timer_init(5999, 5999);//若为36MHZ,则周期为1*2=2s,若为72MHZ,则周期为0.5*2=1s,高低电平时间均为周期一半
    while (1)
    {
    }
}

/* ---------------------------------------------------------------------------
 *  TIM2 更新中断：翻转 PC13
 * ------------------------------------------------------------------------- */
void TIM2_IRQHandler(void)
{
        if (TIM2->SR & (1U << 0)) //检查更新中断标志位
        {
            TIM2->SR &= ~(1U << 0); //清除标志位
            GPIOC->ODR ^= (1U << 13); //翻转PC13,使LED闪烁
        }
}
