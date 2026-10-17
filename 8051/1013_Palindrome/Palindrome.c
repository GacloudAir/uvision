//-----------------------------------------------------------------------------
// F350_UART_Interrupt.c
//-----------------------------------------------------------------------------
// 非阻塞 UART0 通信示例，使用中断 + 环形缓冲区
// 目标：C8051F350，系统时钟 24.5MHz，波特率 9600
//-----------------------------------------------------------------------------

#include <c8051f350.h>
#include <stdio.h>

//-----------------------------------------------------------------------------
// 常量
//-----------------------------------------------------------------------------
#define SYSCLK      24500000           // 系统时钟频率 Hz
#define BAUDRATE    9600               // 波特率

#define RX_BUF_SIZE 64                 // 接收环形缓冲区大小（2的幂更佳）
#define TX_BUF_SIZE 64                 // 发送环形缓冲区大小

//-----------------------------------------------------------------------------
// 全局变量（环形缓冲区）
//-----------------------------------------------------------------------------
volatile unsigned char xdata rxBuf[RX_BUF_SIZE];//使用尺寸更大的xRAM(512字节)
volatile unsigned char rxHead = 0;     // 中断中写入
volatile unsigned char rxTail = 0;     // 主循环中读取

volatile unsigned char xdata txBuf[TX_BUF_SIZE];
volatile unsigned char txHead = 0;     // 主循环中写入
volatile unsigned char txTail = 0;     // 中断中读取

//-----------------------------------------------------------------------------
// 函数原型
//-----------------------------------------------------------------------------
void SYSCLK_Init (void);
void PORT_Init (void);
void UART0_Init (void);

unsigned char uart_available (void);
unsigned char uart_read_byte (void);
unsigned char uart_send_byte (unsigned char c);
void uart_send_string (char *s);

char putchar (char c);
char _getkey (void);

//-----------------------------------------------------------------------------
// 主函数
//-----------------------------------------------------------------------------
void main (void)
{
    unsigned char cmd;
    unsigned int  workCounter = 0;

    PCA0MD &= ~0x40;                   // 关闭看门狗
    PORT_Init();
    SYSCLK_Init();
    UART0_Init();

    uart_send_string("System Ready\r\n");

    while (1)
    {
        //---------------------------------------------------------------------
        // 这里可以执行其他任务，例如记录数据、采集传感器等
        //---------------------------------------------------------------------
        workCounter++;                 // 模拟后台工作

        //---------------------------------------------------------------------
        // 处理串口接收（非阻塞）
        //---------------------------------------------------------------------
        if (uart_available())
        {
            cmd = uart_read_byte();
            switch (cmd)
            {
                case 'R':
                    uart_send_string("Version 1.0\r\n");
                    break;
                case 'C':
                    uart_send_string("Counter running\r\n");
                    break;
                default:
                    uart_send_byte(cmd);   // 回显
                    break;
            }
        }
    }
}

//-----------------------------------------------------------------------------
// PORT_Init
//-----------------------------------------------------------------------------
void PORT_Init (void)
{
    P0MDOUT |= 0x10;                   // P0.4 推挽输出（TX）
    XBR0    = 0x01;                    // UART0 映射到 P0.4(TX) / P0.5(RX)
    XBR1    = 0x40;                    // 使能交叉开关和弱上拉
}

//-----------------------------------------------------------------------------
// SYSCLK_Init
//-----------------------------------------------------------------------------
void SYSCLK_Init (void)
{
    OSCICN |= 0x03;                    // 内部振荡器最高频率 24.5MHz
    RSTSRC  = 0x04;                    // 使能缺失时钟检测
}

//-----------------------------------------------------------------------------
// UART0_Init
//-----------------------------------------------------------------------------
void UART0_Init (void)
{
    SCON0 = 0x50;                      // 模式1：8位UART，可变波特率，使能接收

    // 波特率配置（与原始例程相同）
    if (SYSCLK/BAUDRATE/2/256 < 1) {
        TH1 = -(SYSCLK/BAUDRATE/2);
        CKCON &= ~0x0B;
        CKCON |=  0x08;
    } else if (SYSCLK/BAUDRATE/2/256 < 4) {
        TH1 = -(SYSCLK/BAUDRATE/2/4);
        CKCON &= ~0x0B;
        CKCON |=  0x01;
    } else if (SYSCLK/BAUDRATE/2/256 < 12) {
        TH1 = -(SYSCLK/BAUDRATE/2/12);
        CKCON &= ~0x0B;
    } else {
        TH1 = -(SYSCLK/BAUDRATE/2/48);
        CKCON &= ~0x0B;
        CKCON |=  0x02;
    }

    TL1 = TH1;                         // 初始化 Timer1
    TMOD &= ~0xf0;                     // Timer1 8位自动重载
    TMOD |=  0x20;
    TR1 = 1;                           // 启动 Timer1

    TI0 = 0;                           // 发送空闲
    RI0 = 0;                           // 清除接收标志

    ES0 = 1;                           // 使能 UART0 中断
    EA  = 1;                           // 使能全局中断
}

//-----------------------------------------------------------------------------
// UART0 中断服务函数
//-----------------------------------------------------------------------------
void UART0_ISR (void) interrupt 4//UART0中断向量
{
    unsigned char next;

    // ---- 接收中断 ----
    if (RI0)
    {
        RI0 = 0;                       // 清除接收标志
        next = (rxHead + 1) % RX_BUF_SIZE;
        if (next != rxTail)            // 缓冲区未满
        {
            rxBuf[rxHead] = SBUF0;     // 存入接收缓冲区
            rxHead = next;
        }
        // 若缓冲区满，则丢弃该字节
    }

    // ---- 发送中断 ----
    if (TI0)
    {
        TI0 = 0;                       // 清除发送完成标志
        if (txTail != txHead)          // 发送缓冲区还有数据
        {
            SBUF0 = txBuf[txTail];     // 发送下一个字节
            txTail = (txTail + 1) % TX_BUF_SIZE;
        }
    }
}

//-----------------------------------------------------------------------------
// 非阻塞 API
//-----------------------------------------------------------------------------
unsigned char uart_available (void)
{
    return (rxHead != rxTail);
}

unsigned char uart_read_byte (void)
{
    unsigned char c;
    if (rxHead == rxTail) return 0;    // 无数据
    c = rxBuf[rxTail];
    rxTail = (rxTail + 1) % RX_BUF_SIZE;
    return c;
}

unsigned char uart_send_byte (unsigned char c)
{
    unsigned char next;
    unsigned char was_empty;

    next = (txHead + 1) % TX_BUF_SIZE;
    if (next == txTail)                // 发送缓冲区满
        return 0;

    was_empty = (txHead == txTail);    // 入队前是否为空
    txBuf[txHead] = c;
    txHead = next;

    if (was_empty)                     // 之前空闲，立即启动发送
    {
        SBUF0 = txBuf[txTail];
        txTail = (txTail + 1) % TX_BUF_SIZE;
    }
    return 1;
}

void uart_send_string (char *s)
{
    while (*s)
    {
        while (!uart_send_byte(*s));   // 等待缓冲区有空位
        s++;
    }
}

//-----------------------------------------------------------------------------
// 重定向 putchar / _getkey，以便继续使用 printf / getchar
//-----------------------------------------------------------------------------
char putchar (char c)
{
    while (!uart_send_byte(c));        // 仅当发送缓冲区满时才等待
    return c;
}

char _getkey (void)
{
    while (!uart_available());         // 仅当没有数据时才等待
    return uart_read_byte();
}