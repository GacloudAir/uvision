# L4 — TIM+DMA 搬 BSRR：零抖动边沿（进阶）

## 目标

用 DMA 在定时器每个更新事件把预算的 BSRR 字搬进 GPIO，硬件自动翻转引脚，零软件抖动。

## 原理

把波形按最小时间量子（1 tick）拆成 `bsfr_buf[]`：每格 = 该 tick 应写进 `GPIOA->BSRR` 的值（置位 `1<<pin`，复位 `1<<(pin+16)`）。TIM 每数 1 tick 触发一次 DMA，循环模式自动重复 → CPU 全程不参与。

## 任务清单

| 编号 | 任务 |
|---|---|
| 1 | `build_wave_table`（预计算 BSRR 表） |
| 2 | `gpio_init`（通用推挽 2MHz 档 + 先拉低） |
| 3 | `tim_init`（ARR=1，开 UDE 不是 UIE） |
| 4 | `dma_init`（DMA1_Ch2 → BSRR，循环 32 位） |

## 寄存器速查

| 寄存器 | 作用 |
|---|---|
| `RCC->AHBENR` | DMA1EN(bit0) |
| `DMA1_Channel2->CPAR` | = &GPIOA->BSRR |
| `DMA1_Channel2->CMAR` | = bsfr_buf |
| `DMA1_Channel2->CNDTR` | 表长 |
| `DMA1_Channel2->CCR` | MINC/CIRC/MSIZE=2/PSIZE=2/EN |
| `TIM2->DIER` | UDE(bit8) 更新触发 DMA |

## 关键领悟

> "精密"的敌人是软件抖动。只要翻转动作还经过中断 ISR，就有 ≈0.3µs 不确定延迟。
> 把它下沉到「定时器触发 → DMA 直搬 BSRR」的纯硬件链路，抖动只剩时钟量化步长（≈13.9ns）。
> 代价：波形被"固化"成一张表，在线细调不如 L2 的协议方案灵活——两者互补。

## 验证方法

| 验证点 | 方法 | 期望 |
|---|---|---|
| 波形 | 示波器 PA0..PA3 | 与 L1 同相/相位一致，但边沿更"齐" |
| 抖动 | 示波器放大边沿/多次触发叠加 | 无明显抖动（对比 L1 中断版） |
| 停止 | 清 TIM2 CEN | 波形停，引脚回关断态 |
