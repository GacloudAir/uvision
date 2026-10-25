#include "stm32f10x.h"//STM32蓝板库
static void gpio_timer_init(uint16_t psc,uint16_t arr,uint16_t ccr)//初始化函数
{
    //开始之前,列个清单:需要哪些管脚？哪些计时器？要不要启用UART串口？使用RCC->启用
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

void TIM2_IRQHandler(void)//中断函数
{
    if (TIM2->SR & (1U << 0))
    {
        TIM2->SR &= ~(1U << 0); //清除更新中断标志位,以陷入中断处理无法退出
        GPIOC->ODR ^= (1U << 13);//使用异或操作,稳定触发电平翻转
    }
}
int main(void)
{
    gpio_timer_init(7199,9999,0);
    while(1)
    {

    }
}
/*
| `0x0`     | **模拟输入**          | ADC                           |
| `0x4`     | **浮空输入**          | 串口 RX、外部信号输入           |
| `0x8`     | 上拉/下拉输入         | 按键                           |
| `0x3`     | **通用推挽输出 50MHz**| 普通 IO(LED、手动控制线)        |
| `0xB`     | **复用推挽输出 50MHz**| 外设输出(定时器、串口 TX)       |
| `0x7`     | 通用开漏 50MHz        | I2C 等                        |
| `0xF`     | 复用开漏 50MHz        | —                             |
*/
