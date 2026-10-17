//-----------------------------------------------------------------------------
// F33x_UART_STDIO.c
//-----------------------------------------------------------------------------
#include <c8051f350.h>                 // SFR declarations
#include <stdio.h>

//-----------------------------------------------------------------------------
// Global CONSTANTS
//-----------------------------------------------------------------------------
#define SYSCLK      24500000           // SYSCLK frequency in Hz
#define BAUDRATE        9600           // Baud rate of UART in bps

//-----------------------------------------------------------------------------
// Function PROTOTYPES
//-----------------------------------------------------------------------------
void SYSCLK_Init (void);
void UART0_Init (void);
void PORT_Init (void);

//-----------------------------------------------------------------------------
// UART0 重定向函数
//-----------------------------------------------------------------------------
char putchar (char c)
{
    SBUF0 = c;          // 写入 UART0 发送缓冲
    while (!TI0);       // 等待发送完成
    TI0 = 0;            // 清除发送完成标志
    return c;
}

char _getkey (void)
{
    while (!RI0);       // 等待接收数据
    RI0 = 0;            // 清除接收标志
    return SBUF0;       // 返回接收到的字符
}

//-----------------------------------------------------------------------------
// MAIN Routine
//-----------------------------------------------------------------------------
void main (void) 
{
   unsigned char inputcharacter;       // 存储UART传入的数据

   PCA0MD &= ~0x40;
   PORT_Init();                        
   SYSCLK_Init ();                     
   UART0_Init();

   while (1)
   {
      printf ("\nEnter character: ");
      inputcharacter = getchar ();
      printf ("\nCharacter entered : %c", inputcharacter);
      printf ("\n      Value in Hex: %bx", inputcharacter);
   }
}

//-----------------------------------------------------------------------------
// Initialization Subroutines
//-----------------------------------------------------------------------------
void PORT_Init (void)
{
   P0MDOUT |= 0x10;                    // Enable UTX as push-pull output
   XBR0    = 0x01;                     //启用UART,将引脚设置为P0.4(TX)和P0.5(RX)                     
   XBR1    = 0x40;                     // Enable crossbar and weak pull-ups
}

void SYSCLK_Init (void)
{
   OSCICN = 0x83;                      // 使能内部振荡器，不分频，SYSCLK=24.5MHz
   RSTSRC = 0x04;                      // Enable missing clock detector
}

void UART0_Init (void)
{
   SCON0 = 0x10;                       // SCON0: 8-bit variable bit rate
                                       //        level of STOP bit is ignored
                                       //        RX enabled
                                       //        ninth bits are zeros
                                       //        clear RI0 and TI0 bits
   if (SYSCLK/BAUDRATE/2/256 < 1) {
      TH1 = -(SYSCLK/BAUDRATE/2);
      CKCON &= ~0x0B;                  // T1M = 1; SCA1:0 = xx
      CKCON |=  0x08;
   } else if (SYSCLK/BAUDRATE/2/256 < 4) {
      TH1 = -(SYSCLK/BAUDRATE/2/4);
      CKCON &= ~0x0B;                  // T1M = 0; SCA1:0 = 01                  
      CKCON |=  0x01;
   } else if (SYSCLK/BAUDRATE/2/256 < 12) {
      TH1 = -(SYSCLK/BAUDRATE/2/12);
      CKCON &= ~0x0B;                  // T1M = 0; SCA1:0 = 00
   } else {
      TH1 = -(SYSCLK/BAUDRATE/2/48);
      CKCON &= ~0x0B;                  // T1M = 0; SCA1:0 = 10
      CKCON |=  0x02;
   }

   TL1 = TH1;                          // Init Timer1
   TMOD &= ~0xf0;                      // TMOD: timer 1 in 8-bit autoreload
   TMOD |=  0x20;                       
   TR1 = 1;                            // START Timer1
   TI0 = 1;                            // Indicate TX0 ready
}
//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------