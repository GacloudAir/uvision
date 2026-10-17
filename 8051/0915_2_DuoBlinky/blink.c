#include <C8051F350.h>

void delay(unsigned int count) { while(count--); }

void main(void) {
    PCA0MD &= ~0x40;   // 关看门狗
    XBR1 = 0x40;       // 使能交叉开关

    // 只把 P0.0 设为推挽输出
    P0MDOUT |= 0x02;
    P0SKIP  |= 0x02;   // 跳过 P0.0，避免被外设占用
    // 其他引脚保持默认（模拟输入，高阻）

    while(1) {
        P0 &= ~0x02;   // P0.0 输出低
        delay(50000);
        P0 |= 0x02;    // P0.0 输出高
        delay(50000);
    }
}