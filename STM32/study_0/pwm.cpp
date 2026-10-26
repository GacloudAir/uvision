#include "stm32f10x.h"//STM32蓝板库
static void gpio_timer_init(uint16_t psc,uint16_t arr,uint16_t ccr)//初始化函数
{
    //开始之前,列个清单:需要哪些管脚？哪些计时器？要不要启用UART串口？使用RCC->启用GPIO/定时器的时钟,从而允许管理具体外设
    //=========引脚部分==========
    RCC->APB2ENR |=(1U<<2);//启用GPIOA时钟,由APB2管理,写入1U激活,别忘了|=;
    GPIOA->CRL &= ~(0xF<<0);//先清空
    GPIOA->CRL |= (0xB<<0);//启用PA0为复用推挽输出(串口TX,定时器专用输出模式)
    //=========定时器部分========
    RCC->APB1ENR |=(1U<<0);//启用TIM2时钟
    TIM2->PSC = psc;TIM2->ARR=arr;TIM2->CCR1=ccr;//设置决定计时器运行的三项数据
    TIM2->CCMR1 &= ~((0x7 << 4) | (0x1 << 3));//3,4号位分别具有1位,3位,分别需写入~(0x1),~(0x7)到1号位以清零
    TIM2->CCMR1 |= (0x6 << 4) | (0x1 << 3);//设置CCR1为PWM输出模式,并设置占空比
    TIM2->CCER |= (1U << 0);//使能CCR1输出,打通计时器到针脚的通道
    TIM2->CR1 |= (1U << 0);//启动计数器
}

int main(void)
{
    gpio_timer_init(7199,9999,4999);//周期1秒,x.5秒时会触发ccr溢出翻转电平
    while(1){};
}