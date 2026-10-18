# S4 — SysTick、精确延时与按键

## 为什么

从 8051 带过来的 `for(i=0;i<100000;i++);` 在 STM32 上不可靠：

| 问题 | 说明 |
|---|---|
| 编译优化 | 空循环可能被**整个优化掉**，延时变成 0 |
| 中断干扰 | 中断一来，延时被拉长，且不可预测 |
| `int` 位宽 | 8051 的 `int` 是 16 位，STM32 是 32 位 —— 以前算的循环次数全不对 |
| 主频变化 | 换了时钟频率，延时就变了，代码不可移植 |

## 两套可靠延时

### ① `delay_ms` —— SysTick

SysTick 是 **Cortex-M3 内核自带**的 24 位递减计数器（不占用 STM32 的外设定时器）。

```c
SysTick_Config(SystemCoreClock / 1000);   /* 72e6/1000 = 72000 → 1ms 中断 */
```
它会自动：设 `LOAD`、清 `VAL`、选内核时钟、开中断、启动。
你只要在 `SysTick_Handler` 里 `g_ms_ticks++` 即可。

★ 好处：与优化无关、可被中断打断也不丢节拍。

### ② `delay_us` —— DWT->CYCCNT

内核里有个**周期计数器**，每 CPU 周期 +1（72MHz 下 1µs = 72 个计数）。

```c
CoreDebug->DEMCR |= (1UL << 24);    /* TRCENA：用 DWT 的前提 */
DWT->CYCCNT = 0;
DWT->CTRL  |= (1UL << 0);           /* CYCCNTENA */

uint32_t start = DWT->CYCCNT;
uint32_t need  = us * (SystemCoreClock / 1000000UL);   /* 72MHz → us * 72 */
while ((DWT->CYCCNT - start) < need) { }               /* ★ 减法：天然处理回绕 */
```

★ 精度到 1 个周期（13.9ns），且不占用任何定时器。

> ⚠ **回绕要用减法**：`(now - start) < need` 在 32 位无符号下天然正确；
> 写成 `while (DWT->CYCCNT < start + need)` 会出错。

## 任务清单

| 编号 | 任务 | 验收 |
|---|---|---|
| T1 | SysTick 1ms + `delay_ms()` | LED 按秒闪 |
| T2 | DWT `delay_us()` | 示波器测脉宽 |
| T3 | 用翻转引脚"夹住"延时：`PA1=1; delay_us(10); PA1=0; delay_ms(1);` | PA1 上应看到 **10µs 宽**、周期约 **1.01ms** 的脉冲 |
| T4 | EXTI 按键（PA0 下降沿）触发动作 | 按一次 LED 闪一下 |

T3 是最有价值的验收 —— **用示波器证明你的延时真的是 10µs**。

## 按键消抖（进阶思考）

ISR 里直接 `delay_ms(20)` 消抖是不好的（阻塞其它中断）。
更好的做法：**ISR 里只记录时间戳**，主循环里判断"距上次触发是否 >20ms"再响应。

## 寄存器速查

| 项目 | 说明 |
|---|---|
| `SysTick->LOAD / VAL / CTRL` | 重装载 / 当前值 / 控制(CLKSOURCE\|TICKINT\|ENABLE) |
| `SysTick_Config(ticks)` | CMSIS：一步配好并开中断 |
| `CoreDebug->DEMCR` bit24 | TRCENA，用 DWT 前**必须**置 1 |
| `DWT->CYCCNT` | 周期计数（写 0 清零） |
| `DWT->CTRL` bit0 | CYCCNTENA |
| EXTI / AFIO | 同 S2（★ AFIO 时钟别忘了开） |

## 思考题

1. 为什么 `for` 空循环在 STM32 上不可靠？（至少两个原因）
2. `delay_us` 用 DWT 和用定时器实现，各有什么优劣？
3. SysTick 中断里做很重的事，`delay_ms` 会怎样？
4. 按键用中断 vs 主循环轮询，各适合什么场景？
