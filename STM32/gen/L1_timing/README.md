# L1 — 多通道精密时序：单通道脉宽 → 多通道相位

## 目标

阶段 A（W1）：TIM2_CH1 在 PA0 输出占空比/周期可调方波。
阶段 B（W2）：扩成 4 路（PA0..PA3），可同相或设定相位偏移；其中一路是"夹在周期中间"的高电平窗口（最难）。

## 前置知识

- 定时器时基 PSC/ARR/CNT；PWM 模式 1/2；复用引脚（见 `docs/学习路线图.md` 模块 2/3）
- 多通道相位：同 TIM 多 CCR；跨定时器预置 CNT

## 任务清单

| 编号 | 任务 | 位置 |
|---|---|---|
| A-1 | 使能 TIM2(APB1)+GPIOA(APB2) | `stageA_init` |
| A-2 | PA0 复用推挽 | `stageA_init` |
| A-3 | 设 PSC/ARR | `stageA_init` |
| A-4 | CH1 PWM1 + CCR + CC1E | `stageA_init` |
| A-5 | 启动 CEN | `stageA_init` |
| B-1~B-8 | 4 路 + TIM3 预置 CNT 相位 | `stageB_init` |

## 寄存器速查

| 寄存器 | 含义 |
|---|---|
| `RCC->APB1ENR` | TIM2EN(bit0)/TIM3EN(bit1) |
| `RCC->APB2ENR` | IOPAEN(bit2) ★ |
| `TIMx->CCMR1/2` | OCxM[2:0]：110=PWM1, 111=PWM2 |
| `TIMx->CCER` | CCxE 输出使能 |
| `TIMx->CNT` | 计数器值（相位预置用） |

## 核心公式

```
f_cnt = 72MHz / (PSC+1)
f_out = f_cnt / (ARR+1)
占空比 = CCR / (ARR+1)        (PWM1：CNT<CCR 为高)
```

## 关键难点（务必想通）

一个 TIM 只有一条 CNT，PWM 窗口只能贴"起点"(PWM1) 或"终点"(PWM2)。
"贴起点"的窗口（占空比 <100%）PWM1 直接搞定；"夹在周期正中间"的窗口两种模式都做不出
→ 用第二个 TIM，预置它的 `CNT = 周期 − 相位偏移` 实现对齐。
**不要用主从触发复位(SMS=100) 对齐长电平窗口**：触发占空比高时会把从 TIM 的 CNT 按住贴 0，输出恒高直线。

## 验证方法

| 验证点 | 方法 | 期望 |
|---|---|---|
| 阶段 A | 示波器 PA0 | 周期 4µs、占空比随 A_CCR 变 |
| 阶段 B | 示波器 PA0..PA3 | 4 路同周期；CH4 窗口落在中间，与 CH2↓ 对齐 |
| 低频自检 | 把 ARR 改大，LED/万用表 | 肉眼可见翻转 |

## 完成后应回答

> 为什么"精密时序"必须用硬件 PWM / 预置 CNT，而不能靠主循环或中断翻转 GPIO？（提示：中断延迟 ≈0.3µs，相对 4µs 周期是 7.5% 的抖动）
