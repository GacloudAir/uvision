//-----------------------------------------------------------------------------
// 包含文件
#include <c8051f330.h>              // SFR 声明

//-----------------------------------------------------------------------------
// 函数原形定义
#define uchar unsigned char
sfr16 TMR2RL = 0xca;   // Timer2 重载值（低字节 0xCA，高字节 0xCB）
sfr16 TMR2   = 0xcc;   // Timer2 计数器（低字节 0xCC，高字节 0xCD）
void main (void);
void SYSCLK_Init (void);
void PORT_Init (void);
void Timer2_Init (int counts);
void Timer2_ISR (void);

void LED_OUT(uchar X);

unsigned char code LED_0F[];

//-----------------------------------------------------------------------------
// 全局常量
#define SYSCLK 24500000

sbit LED_DOUT  = P1^6;      // 串行数据输入
sbit LED_CLK   = P1^5;      // 时钟脉冲信号——上升沿有效
sbit LED_LATCH = P1^7;      // 打入信号——上升沿有效

//-----------------------------------------------------------------------------
// 全局变量
uchar LED[4];               // 显示缓存：LED[3]千位，LED[2]百位，LED[1]十位，LED[0]个位
unsigned int seconds = 0;   // 秒计数

//-----------------------------------------------------------------------------
// 主程序
void main (void)
{
    PCA0MD &= ~0x40;        // 禁止看门狗
    SYSCLK_Init ();         // 系统时钟初始化
    PORT_Init ();           // IO 口初始化
    Timer2_Init (255);      // 定时器 2 初始化，约 1ms 中断

    // 初始显示 0000
    LED[0] = 0;
    LED[1] = 0;
    LED[2] = 0;
    LED[3] = 0;

    EA = 1;                 // 开总中断

    while (1) { }           // 空转，等待中断
}

//-----------------------------------------------------------------------------
// SYSCLK_Init 系统时钟初始化
void SYSCLK_Init (void)
{
    OSCICN = 0x80;          // 内部振荡器 24.5MHz / 8
    RSTSRC = 0x04;          // 使能时钟丢失检测器
}

//-----------------------------------------------------------------------------
// PORT_Init IO 端口初始化
void PORT_Init (void)
{
    XBR0 = 0x00;            // 未选择任何数字外设
    XBR1 = 0x40;            // 使能交叉开关和弱上拉
    P1MDOUT |= 0xE0;        // P1.5、P1.6、P1.7 设为推挽输出
}

//-----------------------------------------------------------------------------
// Timer2_Init 定时器 2 初始化
// counts: 重载计数值，决定中断周期
void Timer2_Init (int counts)
{
    TMR2CN  = 0x00;         // 停止 Timer2，16 位自动重载
    CKCON  &= ~0x60;        // Timer2 时钟源 = SYSCLK / 12
    TMR2RL  = -counts;      // 重载值
    TMR2    = 0xffff;       // 立即重载
    ET2     = 1;            // 允许 Timer2 中断
    TR2     = 1;            // 启动 Timer2
}

//-----------------------------------------------------------------------------
// Timer2_ISR 定时器 2 中断服务程序
void Timer2_ISR (void) interrupt 5
{
    static uchar scan_idx = 0;      // 当前扫描位 0~3
    static unsigned int ms_count = 0;

    TF2H = 0;                       // 清中断标志

    // 动态扫描一位
    switch (scan_idx) {
        case 0:
            LED_OUT(LED_0F[LED[3]]); // 段码
            LED_OUT(0xfe);           // 位码，第 1 位
            break;
        case 1:
            LED_OUT(LED_0F[LED[2]]);
            LED_OUT(0xfd);           // 第 2 位
            break;
        case 2:
            LED_OUT(LED_0F[LED[1]]);
            LED_OUT(0xfb);           // 第 3 位
            break;
        case 3:
            LED_OUT(LED_0F[LED[0]]);
            LED_OUT(0xf7);           // 第 4 位
            break;
    }
    LED_LATCH = 0;                  // 锁存数据
    LED_LATCH = 1;

    scan_idx++;
    if (scan_idx >= 4) scan_idx = 0;

    // 秒计时
    ms_count++;
    if (ms_count >= 1000) {         // 约 1 秒
        ms_count = 0;
        seconds++;
        if (seconds > 9999) seconds = 0;

        // 更新显示缓存
        LED[3] = seconds % 10;              // 个位
        LED[2] = (seconds / 10) % 10;       // 十位
        LED[1] = (seconds / 100) % 10;      // 百位
        LED[0] = (seconds / 1000) % 10;     // 千位
    }
}

//-----------------------------------------------------------------------------
// LED 单字节串行移位函数
void LED_OUT(uchar X)
{
    uchar i;
    for (i = 8; i >= 1; i--) {
        if (X & 0x80) LED_DOUT = 1;
        else          LED_DOUT = 0;
        X <<= 1;
        LED_CLK = 0;
        LED_CLK = 1;
    }
}

//-----------------------------------------------------------------------------
// 字模表（共阳极）
unsigned char code LED_0F[] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8,
    0x80, 0x90, 0x8C, 0xBF, 0xC6, 0xA1, 0x86, 0xFF,
    0xBF
};