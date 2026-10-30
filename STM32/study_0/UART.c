#include "stm32f10x.h"

void uart1_set_baud(uint32_t baud)//串口1波特率计算,直接输入波特率即可得到正确值
{
    uint32_t pclk2 = 72000000U;                    // USART1 挂在 APB2 = g_pclk2
    uint32_t div   = (25U * pclk2) / (4U * baud);  // 等价于 USARTDIV×100
    uint32_t mant  = div / 100U;                    // 整数部分 -> BRR[15:4]
    uint32_t frac  = ((div % 100U) * 16U + 50U) / 100U; // 小数四舍五入 -> BRR[3:0]
    USART1->BRR   = (mant << 4) | (frac & 0xFU);//将正确波特率输入对应寄存器
}

void UART_Init(void)
{
    //开始之前,列个清单:需要哪些管脚？哪些计时器？要不要启用UART串口？使用RCC->启用GPIO/定时器的时钟,从而允许管理具体外设
    //=========引脚部分==========
    RCC->APB2ENR |=(1U<<2);//启用GPIOA时钟
    GPIOA->CRH &=~(0xF<<4);//先清空
    GPIOA->CRH |=(0xB<<4);//启用PA9为复用推挽输出(串口TX)
    GPIOA->CRH &=~(0xF<<8);//先清空
    GPIOA->CRH |=(0x4<<8);//启用PA10为浮空输入(串口RX)
    //=========串口部分==========
    RCC->APB2ENR |=(1U<<14);//启用USART1时钟
    uart1_set_baud(115200);//设置波特率
    USART1->CR1 |=(1U<<3)|(1U<<2)|(1U<<13);//使能串口发送(3)和接收(2),并对串口使能(13)
    //=========中断部分==========
    USART1->CR1 |=(1U<<5);//使能串口接收中断(5)
    NVIC_SetPriority((IRQn_Type)37,0);//设置串口中断优先级为0(串口中断值为37)
    NVIC_EnableIRQ((IRQn_Type)37);//使能串口中断
}

void uart_send(uint8_t b)//发送一个字节
{
    while (!(USART1->SR & (1U<<7)));  // 等 TXE(发送寄存器空)
    USART1->DR = b;//将数据写入发送寄存器
}
void uart_send_str(const char *s)//发送一个字符串
{
    while (*s) uart_send((uint8_t)*s++);//将字符串逐个发送
}

void USART1_IRQHandler(void)//串口中断函数:中断时接收字节并自动回文
{
    if (USART1->SR & (1U<<5))        // RXNE: 收到一字节
    {
        uint8_t b = (uint8_t)USART1->DR;
        uart_send(b);                // 回显(echo)，串口助手发啥回啥
    }
}

int main(void)
{
    UART_Init();
    while (1) {}                    // 收到数据会在中断里自动回显,不需要使用CPU
}

