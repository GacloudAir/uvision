# 多通道精密时序发生器 · 阶梯例程（gen）

> 本目录是「项目方案_多通道精密时序发生器」的**学习用例程骨架**。
> 风格与 `firmware/labs/` 完全一致：结构搭好、参数留白、关键处写 `TODO` 和思考题，
> 你自己查手册、算数值、把空白填上，代码才会真正工作。
>
> **能不能跑通不重要，想明白为什么才重要。**

---

## 一、这套例程对应计划的哪一块

计划把项目拆成**四层**、**四周里程碑**。本目录的例程 1:1 覆盖：

| 例程 | 计划里的位置 | 主题 | 里程碑 |
|---|---|---|---|
| **L1_timing**  | 层①固件 + 层①核心 | 多通道时序：单通道可变脉宽 → 多通道同相/移相 | W1 → W2 |
| **L2_protocol**| 层②通信协议 | 自定义帧（0xAA55+CRC16）+ 6 命令 + 收帧状态机 | W2 |
| **L3_flash**   | 层①固件（持久化）| 参数存 Flash、上电恢复、IWDG 看门狗、合法性校验 | W3 |
| **L4_dma**     | 层①固件（低抖动）| TIM+DMA 搬 BSRR，绕开中断延迟 | 进阶 |
| **L5_selftest**| 层④自检 | 输入捕获测脉宽 + ADC+DMA 采样 + 误差表 | W4 |
| **host/**      | 层③上位机（v1）| C++ 控制台：串口枚举/下发/回读/CSV | W3 |

> 计划里的「4 通道、脉宽 1µs–10ms、误差 <1%、1000 帧压测零误帧、往返 <1s」
> 就是本目录各例程合起来要达成的**验收标准**。

---

## 二、怎么用（复用现有 Keil 工程）

推荐复用 `firmware/1014_blinky`，**不要为每个 Lab 新建工程**：

1. 打开 `firmware/1014_blinky/blinky.uvprojx`
2. 把要做的 Lab 的 `.c` 添加进工程：右键 `Source Group 1` → `Add Existing Files...`
3. 让 `blinky.c` 不参与编译：右键 `blinky.c` → `Options for File...` → 取消勾选 `Include in Target Build`
4. **F7 编译 → F8 下载**
5. 做完一个换下一个；**同一时间只能启用一个 `.c`（每个都有 `main()`）**

> `host/` 是 PC 端 C++，不在 Keil 里跑。用 `g++` 或 Visual Studio 编译（见 `host/README.md`）。

---

## 三、学习契约（请认真对待）

1. **先自己查，再问人**：寄存器去查《RM0008 参考手册》。
2. **先算，再写**：PSC/ARR/CCR、相位偏移、Flash 页地址、CRC 多项式，先在本子算好。
3. **先观察，再判断**：每个 Lab 都给了验证方法（示波器/串口助手/LED）。
4. **想不清楚就停**：`TODO` 下有提示，不够再回 `docs/学习路线图.md`。

---

## 四、目录索引

```
firmware/gen/
├── README.md                 ← 本文件
├── L1_timing/                多通道时序核心（W1→W2）
│   ├── lab1_timing.c
│   └── README.md
├── L2_protocol/               串口协议帧 + CRC16（层②）
│   ├── lab2_protocol.c
│   └── README.md
├── L3_flash/                  参数存 Flash + 看门狗（层①持久化）
│   ├── lab3_flash.c
│   └── README.md
├── L4_dma/                    DMA 搬 BSRR 低抖动（层①进阶）
│   ├── lab4_dma.c
│   └── README.md
├── L5_selftest/               输入捕获 + ADC 自检（层④）
│   ├── lab5_selftest.c
│   └── README.md
└── host/                      C++ 上位机控制台（层③ v1）
    ├── main.cpp
    ├── protocol.h
    ├── serial_win.cpp / .h
    └── README.md
```

---

## 五、Keil 常见报错速查（遇到先查这里）

| 报错 | 原因 / 解决 |
|---|---|
| `FCARM - Output Name not specified` | 文件 `File Type` 被设成 Image。右键文件 → `Options for File...` → 改回 `C Source File` |
| `identifier "xxx" is undefined` | 寄存器/位域拼错，对照 RM0008 |
| `multiply defined` / `undefined symbol main` | 同时有多个或零个 `main`，确保只启用一个 Lab |
| 编译通过但引脚无反应 | **忘了开时钟**（见下方头号大坑）或引脚没配模式 |
| `#include "stm32f10x.h"` 找不到 | RTE 缺 CMSIS Core + Device Startup |

---

### ★ 头号大坑：忘了开时钟

**症状**：0 错误 0 警告、程序也下了，但引脚毫无反应、串口没数据。

**原因**：每个外设都有独立时钟开关，且 **GPIO 在 APB2、TIM2/3 在 APB1、USART1 在 APB2**。

```c
RCC->APB1ENR |= (1U << 0);   /* TIM2  */
RCC->APB1ENR |= (1U << 1);   /* TIM3  */
RCC->APB2ENR |= (1U << 2);   /* GPIOA */
RCC->APB2ENR |= (1U << 14);  /* USART1 */
```

> 调试时在 Watch 里加 `RCC->APB2ENR` / `RCC->APB1ENR`，确认对应位是 1。

---

## 六、参考资源

- 项目方案：`项目方案_多通道精密时序发生器.md`
- 已有基础例程：`firmware/labs/`（Lab1–6）、`firmware/study/`（S1–S4）、`firmware/target/timing_target.c`（已验证时序）
- 芯片手册：《RM0008 参考手册》《STM32F103x8 数据手册》
