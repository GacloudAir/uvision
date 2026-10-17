#include <c8051F330.h>

// Timer2 的 16 位 SFR
sfr16 TMR2RL = 0xca;   // Timer2 重载值
sfr16 TMR2   = 0xcc;   // Timer2 计数器

#define SYSCLK 24500000 / 8   // 系统时钟 ≈ 3.0625 MHz

sbit sound = P1^0;            // 蜂鸣器 / LED 接在 P1.0

void SYSCLK_Init (void);
void PORT_Init (void);
void Timer2_Init (int counts);
void Timer2_ISR (void);

void main (void)
{
    PCA0MD &= ~0x40;          // 关看门狗
    SYSCLK_Init ();           // 初始化系统时钟
    PORT_Init ();             // 初始化端口
    Timer2_Init (SYSCLK / 12 / 10);  // 10Hz 中断 → 每 0.1 秒翻转一次
    EA = 1;                   // 开总中断
    while (1) { }             // 主循环空转，等待中断
}

void SYSCLK_Init (void)
{
    OSCICN = 0x80;            // 内部振荡器 24.5MHz / 8
    RSTSRC = 0x04;            // 使能时钟丢失检测器
}

void PORT_Init (void)
{
    XBR0 = 0x00;              // 不选数字外设
    XBR1 = 0x40;              // 使能交叉开关和弱上拉
    P1MDOUT |= 0x01;          // P1.0 推挽输出
    P1SKIP  |= 0x01;          // 交叉开关跳过 P1.0
    sound = 0;                // 初始输出低电平
}

void Timer2_Init (int counts)
{
    TMR2CN  = 0x00;           // 停止 Timer2，清 TF2，16 位自动重载
    CKCON  &= ~0x60;          // Timer2 时钟源 = SYSCLK / 12
    TMR2RL  = -counts;        // 重载值：从 65536-counts 数到 0xFFFF
    TMR2    = 0xffff;         // 立即重载，第一次中断很快到来
    ET2     = 1;              // 允许 Timer2 中断
    TR2     = 1;              // 启动 Timer2
}

void Timer2_ISR (void) interrupt 5
{
    TF2H = 0;                 // 清 Timer2 中断标志
    sound = ~sound;           // 翻转 P1.0
}