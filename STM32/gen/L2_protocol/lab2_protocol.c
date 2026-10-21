/* ============================================================================
 *  L2 — 自定义串口协议帧（0xAA55 + CRC16）+ 命令分发  ★ 计划层② ★
 * ============================================================================
 *  文件：firmware/gen/L2_protocol/lab2_protocol.c
 *
 *  目标：
 *    让 STM32 通过 USART1（PA9=TX, PA10=RX）与上位机通信。
 *    收到一帧命令后执行（改参数 / 启动 / 停止 / 回状态 / 自检），并回一帧应答。
 *    本 Lab 只做"协议与命令分发"；具体动作（改时序、触发自检）留到 L3/L5 接上。
 *
 *  ★★ 帧格式（MCU 与 上位机 host/ 必须逐字节一致）★★
 *    字节序：多字节字段一律 大端（高字节先发）。
 *    ┌──────┬──────┬─────┬─────┬─────────────┬──────────┐
 *    │ 0xAA │ 0x55 │ LEN │ CMD │  PAYLOAD    │ CRC16    │
 *    └──────┴──────┴─────┴─────┴─────────────┴──────────┘
 *    - MAGIC : 2 字节 0xAA,0x55（帧头，用来找回同步）
 *    - LEN   : 1 字节 = (CMD + PAYLOAD) 的总长度（不含 MAGIC 与 CRC）
 *    - CMD   : 1 字节，命令码（见 CMD_*）
 *    - PAYLOAD: LEN−1 字节（CMD 之后、CRC 之前；可为空）
 *    - CRC16 : 2 字节，Modbus CRC16，**作用于 [LEN .. PAYLOAD 末尾]**
 *              （即覆盖 LEN、CMD、PAYLOAD；不含 MAGIC 与 CRC 本身）
 *
 *    响应帧：结构相同，但 CMD 最高位置 1（CMD | 0x80）；
 *            PAYLOAD 第 1 字节固定为 RESULT 码（见 RES_*），其后才是数据。
 *
 *  命令集：
 *    CMD_SET_PARAM  0x01  PAYLOAD: [param_id(1)] [value(2,大端)]      → 设置某参数
 *    CMD_GET_PARAM  0x02  PAYLOAD: [param_id(1)]                       → 回 value(2)
 *    CMD_START      0x03  PAYLOAD: 可选 [freq_hz(3,大端)] 或空         → 启动输出
 *    CMD_STOP       0x04  PAYLOAD: 空                                 → 停止输出
 *    CMD_GET_STATUS 0x05  PAYLOAD: 空                                 → 回 [run(1)][freq(3)][err(1)]
 *    CMD_SELF_TEST  0x06  PAYLOAD: 空                                 → 触发自检，回 [err_permille(2)]
 *
 *  应答 RESULT 码：
 *    RES_OK=0x00  RES_ERR_CRC=0x01  RES_ERR_UNK=0x02  RES_ERR_RANGE=0x03
 *
 *  机制（计划要求）：应答码 + 超时重发（上位机侧）+ 非法命令拒绝。
 *
 *  涉及知识（见 docs/学习路线图.md 模块 8）：
 *    USART 波特率/BRR、中断接收、环形缓冲、状态机解析、CRC16、查表分发。
 *
 *  你会用到的寄存器（USART1 在 APB2）：
 *    RCC->APB2ENR   USART1EN(bit14) + IOPAEN(bit2)
 *    USART1->BRR    波特率分频
 *    USART1->CR1    UE/TE/RE/RXNEIE
 *    USART1->SR     RXNE / TXE
 *    USART1->DR     数据寄存器
 *    NVIC           USART1_IRQn
 *
 *  ===== 思考题 =====
 *  Q1：115200 波特率、72MHz 主频，BRR 应填 0x0271。验证这个数是怎么来的。
 *  Q2：为什么 CRC 只覆盖 [LEN..PAYLOAD]，不覆盖 MAGIC？（提示：MAGIC 用于"找帧头"）
 *  Q3：一帧数据被噪声打断、RXNE 收到错字节，状态机怎么复位且不死锁？
 *  Q4：非法命令（比如 CMD=0x99）该回什么？直接忽略还是回 RES_ERR_UNK？
 * ========================================================================== */

#include "stm32f10x.h"
#include <string.h>

/* ---------- 帧常量 ---------- */
#define MAGIC0      0xAAU
#define MAGIC1      0x55U
#define FRAME_MAX   64U      /* 一帧最大字节数（含头与 CRC）*/

#define CMD_SET_PARAM   0x01U
#define CMD_GET_PARAM   0x02U
#define CMD_START       0x03U
#define CMD_STOP        0x04U
#define CMD_GET_STATUS  0x05U
#define CMD_SELF_TEST   0x06U
#define CMD_RESP_BIT    0x80U   /* 响应帧 CMD |= 此位 */

#define RES_OK        0x00U
#define RES_ERR_CRC   0x01U
#define RES_ERR_UNK   0x02U
#define RES_ERR_RANGE 0x03U

#define UART_BRR_115200  0x0271U

/* ---------- 全局：收到的完整帧缓存（由状态机填充） ---------- */
static uint8_t g_rxbuf[FRAME_MAX];
static uint8_t g_rxcnt = 0;          /* 已收字节数 */
static volatile uint8_t g_frame_ready = 0;  /* 收到一完整合法帧置 1 */

/* ===========================================================================
 *  TODO 1：UART 初始化（115200, 8N1，开接收中断）
 *    提示：USART1 在 APB2；PA9 复用推挽(TX)，PA10 浮空输入(RX)；
 *          BRR=UART_BRR_115200；CR1 置 UE/TE/RE/RXNEIE；NVIC 使能。
 * ========================================================================= */
static void uart_init(void)
{
    /* 你的代码： */
}

/* ===========================================================================
 *  TODO 2：CRC16（Modbus，多项式 0x8005，初值 0xFFFF）
 *    算法（伪代码）：
 *      crc = 0xFFFF
 *      for each byte b in [LEN .. PAYLOAD末尾]:
 *          crc ^= b
 *          for i in 0..15:
 *              if crc & 1: crc = (crc>>1) ^ 0xA001   // 0xA001 是 0x8005 的"反转"
 *              else:        crc =  crc>>1
 *      return crc            // 传输时先发低字节、再发高字节（小端存 crc）
 *    返回 uint16_t。
 * ========================================================================= */
static uint16_t crc16_modbus(const uint8_t *buf, uint16_t len)
{
    /* 你的代码： */
    return 0;   /* TODO：实现后删掉这行 */
}

/* ===========================================================================
 *  TODO 3：发送一字节（等 TXE）
 * ========================================================================= */
static void send_byte(uint8_t b)
{
    /* 你的代码： while(!(USART1->SR & (1U<<7))); USART1->DR = b; */
}

/* ===========================================================================
 *  TODO 4：发送一帧应答
 *    resp_cmd = 原 cmd | CMD_RESP_BIT
 *    payload[0] = result，其后 data...；自动算 LEN 与 CRC16 并发送。
 *    提示：先拼 [MAGIC][LEN][resp_cmd][payload...]，再追加 CRC16 两字节。
 * ========================================================================= */
static void send_response(uint8_t cmd, uint8_t result,
                          const uint8_t *data, uint8_t datalen)
{
    /* 你的代码： */
}

/* ===========================================================================
 *  TODO 5：命令分发（收到完整帧后调用）
 *    根据 g_rxbuf 里的 CMD 调对应处理；用 send_response() 回 RESULT。
 *    - SET_PARAM：解析 param_id + value（大端），做范围校验，
 *                  越界回 RES_ERR_RANGE，否则存全局参数并 RES_OK
 *    - GET_PARAM：回对应 value（2 字节大端）
 *    - START    ：置 g_running=1 并应用参数（具体动作 L3/L5 接）
 *    - STOP     ：g_running=0
 *    - GET_STATUS：回 [run][freq_hz(3)][err_count(1)]
 *    - SELF_TEST：触发自检（L5 实现），回 [err_permille(2)]
 *    - 未知 CMD ：回 RES_ERR_UNK
 * ========================================================================= */
static uint8_t g_running = 0;
static uint32_t g_freq_hz = 250000UL;
static uint8_t g_err_count = 0;

static void dispatch_frame(void)
{
    /* 你的代码：解析 g_rxbuf[3]=CMD，g_rxbuf[4..]=PAYLOAD，分发；
     *          非法/越界一律用 send_response() 回对应 RES_*。 */
}

/* ===========================================================================
 *  TODO 6：收帧状态机（逐字节喂入，取代裸轮询）
 *    状态：0=等 MAGIC0, 1=等 MAGIC1, 2=等 LEN,
 *          3=收 PAYLOAD（已收 cnt，还需 LEN 字节）, 4=收 CRC 低, 5=收 CRC 高
 *    - 任何状态收到错误字节（比如状态 0 不是 0xAA）→ 回到状态 0 重新同步
 *    - PAYLOAD 收满后，用 crc16_modbus() 校验 [LEN..PAYLOAD末尾]；
 *      通过 → 置 g_frame_ready=1；失败 → 可选回 RES_ERR_CRC，状态归 0
 *    把下面这个函数从 USART1 中断里调用（每收到一字节喂一次）。
 * ========================================================================= */
static void protocol_feed(uint8_t byte)
{
    /* 你的代码： */
}

/* ----- USART1 中断：收到一字节就喂状态机 ----- */
void USART1_IRQHandler(void)
{
    if (USART1->SR & (1U << 5))          /* RXNE */
    {
        uint8_t b = (uint8_t)USART1->DR; /* 读 DR 自动清 RXNE */
        protocol_feed(b);
    }
}

int main(void)
{
    uart_init();

    while (1)
    {
        if (g_frame_ready)
        {
            g_frame_ready = 0;
            dispatch_frame();
        }
        /* TODO 7：喂狗（若接了 L3 的 IWDG，这里要 IWDG->KR = 0xAAAA）*/
    }
}

/* ---------------------------------------------------------------------------
 *  完成本 Lab 后请回答：
 *   1) 你的 BRR 值与宏定义一致吗？怎么算的？
 *   2) 为什么用"状态机"而不是"一次 recv 整帧"？（提示：串口是字节流，无边界）
 *   3) 如果上位机发了半帧就断线，状态机应当如何回到初始态而不卡死？
 *   4) CRC 与「SUM 累加和」(Lab6 方案) 比，强在哪？（提示：能否发现"两个字节同时翻转"?）
 * ------------------------------------------------------------------------- */
