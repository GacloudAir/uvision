# L5 — 闭环自检：输入捕获 + ADC + 误差表

## 目标

自己测自己完成闭环验证：输入捕获测周期/脉宽（≈14ns 分辨率）+ ADC+DMA 看波形 + 跑 100 组出误差表，经 UART 上报。

## 接线（回环）

```
PA1 (TIM2_CH2 被测波形) ──杜邦线──▶ PA6 (TIM3_CH1 捕获输入)
共地；示波器可同时看 PA1/PA6 对照
```

## 任务清单

| 编号 | 任务 |
|---|---|
| 1 | `wave_gen_init`（TIM2_CH2 出 250kHz/50%） |
| 2 | `capture_init`（TIM3_CH1 输入捕获） |
| 3 | `TIM3_IRQHandler`（取周期/脉宽，处理回绕） |
| 4 | `adc_dma_init`（ADC1+DMA 连续采样） |
| 5 | `self_test_one_group`（算相对误差‰） |
| 6 | `self_test_run`（跑 100 组，返回均值） |

## 寄存器速查

| 模块 | 寄存器 |
|---|---|
| 捕获 | `TIM3->CCMR1` CC1S, `CCER` CC1E/CC1P, `DIER` CC1IE/UIE, `CCR1` |
| ADC | `ADC1->CR1/CR2/SQR3/SMPR`, `RCC->APB2ENR` ADC1EN(bit9) |
| ADC DMA | `DMA1_Channel1` → `&ADC1->DR`, 循环 16 位 |

## 关键领悟

> "看着差不多"不是证据。输入捕获把波形量化成 tick，误差表用数据说话——
> 这正是计划里"能讲的关键"：用闭环自检证明设定值与实测值吻合，而不是肉眼判断。

## 验证方法

| 验证点 | 方法 | 期望 |
|---|---|---|
| 周期 | 捕获 ISR 测 g_period_tick | ≈288（250kHz） |
| 自动验收 | PC13 LED 判 ≈288 | 接对线灯亮 |
| 误差表 | 经 L2 CMD_SELF_TEST 上报 | 平均相对误差 < 1%（计划标准） |
| 波形 | 读 g_adc_buf 找上升沿 | 上升沿斜率符合预期 |
