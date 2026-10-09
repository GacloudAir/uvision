#include <c8051F330.h>

// ===== 74HC595 控制引脚 =====
sbit HC595_CLK   = P1^5;   // 移位时钟 SRCLK
sbit HC595_DATA  = P1^6;   // 串行数据 SER
sbit HC595_LATCH = P1^7;   // 锁存时钟 RCLK

// ===== 蜂鸣器引脚 =====
sbit BEEP = P1^4;          // 蜂鸣器，低电平响（PNP 三极管驱动）

// ===== 共阳极数码管段码表（0~9，低电平点亮）=====
unsigned char code seg_table[] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90
};

// ===== 数码管片选表（SMG1~SMG4，低电平有效）=====
// U3 的 QA~QD 分别接 SMG1~SMG4，低电平使 PNP 三极管导通
// 对应 595 输出最低 4 位的编码：
//   SMG1 导通 → 0xFE (1111 1110)
//   SMG2 导通 → 0xFD (1111 1101)
//   SMG3 导通 → 0xFB (1111 1011)
//   SMG4 导通 → 0xF7 (1111 0111)
unsigned char code dig_table[] = {0xFE, 0xFD, 0xFB, 0xF7};

// ===== 显示缓冲区 =====
unsigned char disp_buf[4] = {0, 0, 0, 0};

// ===== 全局变量 =====
unsigned char scan_idx = 0;    // 当前扫描位
unsigned int  ms_count = 0;    // 毫秒计数
unsigned int  seconds  = 0;    // 秒数

// ===== 函数声明 =====
void Port_Init(void);
void Timer0_Init(void);
void HC595_SendWord(unsigned char dig, unsigned char seg);
void Update_Display_Buffer(void);
void Display_Scan(void);

// ===== 主函数 =====
void main(void) {
    PCA0MD &= ~0x40;           // 关看门狗
    Port_Init();               // 初始化端口
    Timer0_Init();             // 初始化 Timer0

    while(5) {
        // ===== 查询法：每 1ms 检查 Timer0 溢出 =====
        if (TF0 == 1) {
            TF0 = 0;           // 软件清零
            ms_count++;

            // 每 1ms 刷新一位数码管（动态扫描）
            Display_Scan();

            // 累计 1000ms = 1 秒
            if (ms_count >= 200) {
                ms_count = 0;
                seconds++;
                Update_Display_Buffer();
            }
        }
    }
}

// ===== 端口初始化 =====
void Port_Init(void) {
    XBR0 = 0x00;
    XBR1 = 0x40;               // 使能交叉开关

    // P1.4 (蜂鸣器), P1.5~P1.7 (595) 推挽输出
    P1MDOUT |= 0xF0;           // 1111 0000
    P1SKIP  |= 0xF0;           // 交叉开关跳过这些引脚

    BEEP = 1;                  // 初始关闭蜂鸣器（PNP：高电平截止）
    HC595_CLK   = 0;
    HC595_DATA  = 0;
    HC595_LATCH = 0;
}

// ===== Timer0 初始化（1ms 定时）=====
// SYSCLK = 24500000 / 8 = 3.0625 MHz
// Timer0 时钟源 = SYSCLK / 12 ≈ 255208 Hz
// 1ms 需要约 255 个计数
void Timer0_Init(void) {
    CKCON &= ~0x08;            // Timer0 时钟源 = SYSCLK / 12
    TMOD  = 0x01;              // Timer0 方式1：16 位定时器
    TH0 = (65536 - 255) / 256; // 高字节
    TL0 = (65536 - 255) % 256; // 低字节
    TR0 = 1;                   // 启动 Timer0
}

// ===== 向 74HC595 发送 16 位数据 =====
// 先发 dig（片选，进入 U3），再发 seg（段码，进入 U4）
void HC595_SendWord(unsigned char dig, unsigned char seg) {
    unsigned int data16 = ((unsigned int)dig << 8) | seg;
    unsigned char i;

    HC595_LATCH = 0;           // 拉低锁存，准备移位
    for(i = 0; i < 16; i++) {
        HC595_CLK = 0;
        if(data16 & 0x8000) {
            HC595_DATA = 1;
        } else {
            HC595_DATA = 0;
        }
        HC595_CLK = 1;         // 上升沿移位
        data16 <<= 1;
    }
    HC595_LATCH = 1;           // 上升沿锁存
    HC595_LATCH = 0;
}

// ===== 动态扫描一位数码管 =====
void Display_Scan(void) {
    HC595_SendWord(dig_table[scan_idx], seg_table[disp_buf[scan_idx]]);
    scan_idx++;
    if(scan_idx >= 4) scan_idx = 0;
}

// ===== 更新显示缓冲区 =====
void Update_Display_Buffer(void) {
    unsigned int sec = seconds;
    disp_buf[3] = sec / 1000;          // 千位
    disp_buf[2] = (sec % 1000) / 100;  // 百位
    disp_buf[1] = (sec % 100) / 10;    // 十位
    disp_buf[0] = sec % 10;            // 个位
}