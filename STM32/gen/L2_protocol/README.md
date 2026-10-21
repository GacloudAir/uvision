# L2 — 自定义串口协议帧（0xAA55 + CRC16）

## 目标

STM32 经 USART1 与上位机通信：收帧 → 执行命令（设参/启停/回状态/自检）→ 回应答帧。仅做协议与分发，动作留到 L3/L5 接。

## 帧格式（与 host/ 必须逐字节一致）

```
[0xAA][0x55][LEN][CMD][PAYLOAD ...][CRC16lo][CRC16hi]
```
- 多字节**大端**传输
- LEN = CMD+PAYLOAD 字节数（不含 MAGIC/CRC）
- CRC16 = Modbus，作用于 `[LEN .. PAYLOAD末]`
- 响应帧：CMD |= 0x80；PAYLOAD[0] = RESULT

## 命令集

| CMD | 值 | PAYLOAD | 响应 |
|---|---|---|---|
| SET_PARAM | 0x01 | param_id(1)+value(2) | RES_OK / RES_ERR_RANGE |
| GET_PARAM | 0x02 | param_id(1) | value(2) |
| START | 0x03 | 可选 freq_hz(3) | RES_OK |
| STOP | 0x04 | 空 | RES_OK |
| GET_STATUS | 0x05 | 空 | [run(1)][freq(3)][err(1)] |
| SELF_TEST | 0x06 | 空 | [err_permille(2)] |

RESULT：`RES_OK=0` `RES_ERR_CRC=1` `RES_ERR_UNK=2` `RES_ERR_RANGE=3`

## 任务清单

| 编号 | 任务 |
|---|---|
| 1 | `uart_init`（115200/8N1/收中断） |
| 2 | `crc16_modbus` |
| 3 | `send_byte` |
| 4 | `send_response`（自动拼 LEN+CRC） |
| 5 | `dispatch_frame`（命令分发+范围校验） |
| 6 | `protocol_feed`（收帧状态机，5 状态） |
| 7 | main 里喂狗/处理 g_frame_ready |

## 寄存器速查

| 寄存器 | 位 |
|---|---|
| `RCC->APB2ENR` | USART1EN(bit14), IOPAEN(bit2) |
| `USART1->BRR` | 0x0271 @115200 |
| `USART1->CR1` | UE/TE/RE/RXNEIE |
| `USART1->SR` | RXNE(bit5)/TXE(bit7) |

## 关键领悟

> 串口是**无边界字节流**。"帧"是你自己用 MAGIC + LEN + 状态机造出来的。
> CRC16（不是累加和）能发现"两位同时翻转"这类累加和漏检的错误——这正是计划要求"1000 帧压测零误帧"的底气。

## 验证方法

| 验证点 | 方法 | 期望 |
|---|---|---|
| 回环 | 串口助手发 0xAA55... 带正确 CRC | 收到结构正确的响应帧 |
| 坏 CRC | 改一字节 CRC 重发 | 回 RES_ERR_CRC 或不回（不误执行） |
| 非法 CMD | 发 CMD=0x99 | 回 RES_ERR_UNK |
| 帧错位 | 先发半帧垃圾再发真帧 | 状态机重新同步，不卡死 |
