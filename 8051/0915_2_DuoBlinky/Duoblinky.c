//DuoBlinky.c控制了开发板自带的两只小灯交替亮灭
//完整代码如下:
//-----------------------------------------------------------------------------
// F35x_Blinky_Left.c
//-----------------------------------------------------------------------------
#include <c8051f350.h>//若使用vscode等打开,请无视这里的报错

sfr16 TMR2RL = 0xca;                    // Timer2 reload value
sfr16 TMR2   = 0xcc;                    // Timer2 counter

#define SYSCLK 24500000 / 8             // 宏定义,将SYSCLK等价于24500000/8=3,062,500 Hz 

sbit LED_LEFT = P0^6;                   // 左灯接在 P0.6，LED='1' 表示亮,更换为P0.7即可接右灯
sbit LED_RIGHT = P0^7;                  // 右灯,每盏灯点亮前都需要声明
int count=0;
void SYSCLK_Init (void);
void PORT_Init (void);
void Timer2_Init (int counts);
void Timer2_ISR (void);

void main (void) {
   PCA0MD &= ~0x40;                     // 关看门狗
   SYSCLK_Init ();
   PORT_Init ();
   Timer2_Init (SYSCLK / 12 / 10);      // (24500000/8)/12/10=25,520.833
                                        /*系统计数器的计时特点:
                                        内部振荡器频率24,500,000 Hz,系统八分频SYSCLK=3,062,500 Hz,计时器再12分频得到SYSCLK/12=25,5208.33Hz(此处会保留整数)
                                        即每秒钟大约计数25,5208次
                                        而SYSCLK/12再除以10得到的是0.1秒的计时量,在下面使用计数器和中断即可起到每0.1秒触发一次中断的效果
                                        */
   EA = 1;                              // 开总中断,即允许包括计时器2中断的所有中断开启.
   while (1) { }
}

void SYSCLK_Init (void)
{
   OSCICN = 0x80;                       // 内部振荡器 24.5MHz / 8
   RSTSRC = 0x04;                       // 使能时钟丢失检测器
}

void PORT_Init (void)
{
   XBR0 = 0x00;                         // 不选数字外设
   XBR1 = 0x40;                         // 使能交叉开关和弱上拉
   P0MDOUT |= 0xC0;                     // P0.6、P0.7 都推挽输出
   LED_LEFT  = 0;                       // 左灯灭
   LED_RIGHT = 1;                       // 右灯亮

}

void Timer2_Init (int counts)
{
   TMR2CN = 0x00;                       // 选用时钟源的来源,此处选择了CKCON提供的分频时钟
   CKCON &= ~0x60;                      // 选用SYSCLK/12的12分频时钟
   TMR2RL = -counts;                    // 表述为计时器起数的值(int只能存0~65535,所以遇上负数是会变为65535-counts)
   TMR2 = 0xffff;                       // 表述为计时器数到的值,在此触发重载(0xFFFF=65535),实际原理为65535-counts数到65535,等效为0~counts
   ET2 = 1;                             // 允许 Timer2 中断
   TR2 = 1;                             // 启动 Timer2
}
/*
void Timer2_ISR (void) interrupt 5  //该定时器的最大间隔只有0.257秒左右,
                                    //可以使用下方代码用多段短时间合成长时间
{
   TF2H = 0;                            // 清 Timer2 中断标志
   LED = ~LED;                          // 翻转 P0.6，左灯亮灭
}
*/


void Timer2_ISR (void) interrupt 5 //计时器1溢出标志,添加后发生中断会使单片机跳到此处运行代码,具体见文件末尾
{
   TF2H = 0;        // 清 Timer2 中断标志
   count++;
   if (count >= 5) {     // 5 × 0.1s = 0.5s,修改计数延长间隔时间
      count = 0;
      LED_LEFT  = ~LED_LEFT;//使灯亮灭反转一次
      LED_RIGHT = ~LED_RIGHT;
   }
}

//中断号及其源和对应地址
/*中断号	    中断源	        向量地址
0	        外部中断0 (INT0)	0x0003
1	        定时器 0 溢出	    0x000B
2	        外部中断 1 (INT1)	0x0013
3	        定时器 1 溢出	    0x001B
4	        UART0	           0x0023
5	        定时器 2 溢出	    0x002B
6	        SPI0	           0x0033
7	        SMBus	           0x003B
10	        ADC0	           0x0053
11	        PCA	               0x005B
12	        比较器 0	        0x0063
14	        定时器 3 溢出	    0x0073
*/