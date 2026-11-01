#include "stm32f10x.h"
#include <stdio.h>

/* ========== UART1 (PA9=TX, PA10=RX, 115200 8N1) ========== */
void uart1_set_baud(uint32_t baud)
{
    uint32_t pclk2 = 72000000U;
    uint32_t div   = (25U * pclk2) / (4U * baud);   // = USARTDIV×100
    uint32_t mant  = div / 100U;
    uint32_t frac  = ((div % 100U) * 16U + 50U) / 100U;
    USART1->BRR   = (mant << 4) | (frac & 0xFU);
}

void UART_Init(void)
{
    RCC->APB2ENR |= (1U << 2);                  // GPIOA 时钟
    GPIOA->CRH &= ~(0xF << 4);  GPIOA->CRH |= (0xB << 4);  // PA9 TX 复用推挽
    GPIOA->CRH &= ~(0xF << 8);  GPIOA->CRH |= (0x4 << 8);  // PA10 RX 浮空输入

    RCC->APB2ENR |= (1U << 14);                 // USART1 时钟
    uart1_set_baud(115200);
    USART1->CR1 |= (1U<<2)|(1U<<3)|(1U<<13);    // RE + TE + UE
    USART1->CR1 |= (1U << 5);                   // RXNEIE 接收中断(回显用)
    NVIC_SetPriority(USART1_IRQn, 0);
    NVIC_EnableIRQ(USART1_IRQn);
}

void uart_send(uint8_t b)
{
    while (!(USART1->SR & (1U << 7)));
    USART1->DR = b;
}
void uart_send_str(const char *s)
{
    while (*s) uart_send((uint8_t)*s++);
}

/* printf 重定向: 标准库(armlib)走 _write */
int _write(int fd, char *ptr, int len)
{
    for (int i = 0; i < len; i++) uart_send((uint8_t)ptr[i]);
    return len;
}
/* 若工程勾选了 MicroLIB, 走 fputc (两者都留着不冲突) */
int fputc(int ch, FILE *f)
{
    uart_send((uint8_t)ch);
    return ch;
}

void USART1_IRQHandler(void)
{
    if (USART1->SR & (1U << 5))                 // RXNE
    {
        uint8_t b = (uint8_t)USART1->DR;
        uart_send(b);                           // 回显
    }
}

/* ========== ADC1 (PA0, 12位) ========== */
void adc_init(void)
{
    RCC->APB2ENR |= (1U << 2) | (1U << 9);      // GPIOA + ADC1
    GPIOA->CRL  &= ~(0xF << 0);                 // PA0 模拟输入
    RCC->CFGR  &= ~(3U << 14);
    RCC->CFGR  |=  (2U << 14);                  // ADCPRE /6 -> 12MHz

    ADC1->SMPR2 |= (0x7 << 0);                  // CH0 采样 239.5 周期
    ADC1->SQR3   = 0;                           // 规则组第1次 = 通道0(PA0)
    ADC1->CR2  |= (1U << 0);                    // ADON 上电
    for (volatile int i = 0; i < 1000; i++);
    ADC1->CR2 |= (1U << 3);                     // RSTCAL
    while (ADC1->CR2 & (1U << 3));
    ADC1->CR2 |= (1U << 2);                     // CAL
    while (ADC1->CR2 & (1U << 2));
}

uint16_t adc_read(void)
{
    ADC1->SR  &= ~(1U << 1);                    // 清 EOC
    ADC1->CR2 |= (1U << 22);                    // SWSTART
    while (!(ADC1->SR & (1U << 1)));            // 等 EOC
    return (uint16_t)ADC1->DR;
}

uint32_t adc_mv(void)
{
    return ((uint32_t)adc_read() * 3300UL) / 4095UL;
}

/* ========== main ========== */
int main(void)
{
    UART_Init();
    adc_init();
    printf("ADC test start\r\n");
    while (1)
    {
        uint16_t raw = adc_read();
        uint32_t mv  = (raw * 3300UL) / 4095UL;
        printf("raw=%u  %u mV\r\n", raw, mv);
        for (volatile int i = 0; i < 800000; i++);
    }
}
