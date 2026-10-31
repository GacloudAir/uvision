#include "stm32f10x.h"
//==========DMA声明部分==========
#define TX_DMA_LEN  128U
static uint8_t  g_tx_buf[TX_DMA_LEN];//定义发送缓冲区(SRAM, DMA不能读Flash)
static volatile uint8_t g_tx_busy = 0;//定义忙状态: 1=DMA正在搬运发送
//==============================

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
    // 注意: 这里保留了 RXNE 接收中断(bit5), 因为只开了 DMAT(发送DMA)、没开 DMAR(接收DMA),
    //       接收仍走每字节中断是安全的; 发送则一律走下面的 DMA。
    USART1->CR1 |=(1U<<5);//使能串口接收中断(5)
    // 串口中断优先级降到 1, 让发送DMA完成中断(优先级0)能抢占它, 避免"等busy"死锁
    NVIC_SetPriority((IRQn_Type)37,1);//设置串口中断优先级为1
    NVIC_EnableIRQ((IRQn_Type)37);//使能串口中断
    //==========DMA部分(发送: DMA1_Channel4)==========
    RCC->AHBENR |= (1U << 0);//启用DMA1时钟
    DMA1_Channel4->CCR  = 0;//先清空CCR寄存器
    DMA1_Channel4->CPAR = (uint32_t)&USART1->DR;//设置外设地址为USART1->DR
    DMA1_Channel4->CMAR = (uint32_t)g_tx_buf;//设置内存地址为g_tx_buf
    DMA1_Channel4->CCR  = (1U << 7)   // MINC：内存自增
                        | (1U << 4)   // DIR=1：内存→外设
                        | (1U << 1);  // TCIE：传输完成中断
    NVIC_SetPriority(DMA1_Channel4_IRQn, 0);//发送完成中断优先级0(高于串口)
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    USART1->CR3 |= (1U << 7);         // DMAT：允许 USART 触发 DMA 发送
}

//==========发送DMA启动函数(替换原阻塞发送)==========
void uart_send_dma(const uint8_t *s, uint16_t len)
{
    if (len == 0) return;
    while (g_tx_busy) {}              // 等上一帧发完(简单互斥)
    if (len > TX_DMA_LEN) len = TX_DMA_LEN;
    for (uint16_t i = 0; i < len; i++) g_tx_buf[i] = s[i]; // 拷到SRAM(DMA源必须在SRAM)

    DMA1_Channel4->CCR  &= ~(1U << 0);  // 关 EN 才能重载 CNDTR
    DMA1_Channel4->CNDTR = len;          // 设置传输长度
    DMA1_Channel4->CCR  |=  (1U << 0);   // EN 启动传输, USART在TXE时自动请求DMA
    g_tx_busy = 1;
}

void uart_send(uint8_t b)//发送一个字节(调试用: 启用DMAT后请勿与DMA混用)
{
    while (!(USART1->SR & (1U<<7)));  // 等 TXE(发送寄存器空)
    USART1->DR = b;//将数据写入发送寄存器
}
void uart_send_str(const char *s)//发送一个字符串(改为DMA非阻塞)
{
    uint16_t len = 0;
    while (s[len]) len++;            // 自己数长度, 不依赖 string.h
    uart_send_dma((const uint8_t *)s, len);
}

//==========DMA1_Channel4 发送完成中断==========
void DMA1_Channel4_IRQHandler(void)
{
    if (DMA1->ISR & (1U << 13))      // TCIF4: 通道4传输完成标志(bit13)
    {
        DMA1->IFCR = (1U << 13);     // 写1清标志(必须用IFCR, 不是直接清ISR)
        g_tx_busy = 0;               // 解锁, 允许下一帧发送
        // 严谨做法: 再等 USART1->SR 的 TC 位, 确保最后一字节移出移位寄存器
    }
}

void USART1_IRQHandler(void)//串口中断函数: 中断时接收字节并自动回文
{
    if (USART1->SR & (1U<<5))        // RXNE: 收到一字节
    {
        uint8_t b = (uint8_t)USART1->DR;
        uart_send_dma(&b, 1);        // 回显(echo): 走发送DMA(已开DMAT, 不能再用uart_send)
    }
}

int main(void)
{
    UART_Init();
    while (1) {}                    // 收到数据会在中断里自动回显, 不需要使用CPU
}
