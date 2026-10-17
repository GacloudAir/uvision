# STM32 时序控制器 · 阶梯例程（Labs）

> 这套例程的目标不是"给你能直接跑的代码"，而是**给你一副脚手架**：
> 结构搭好、参数留白、关键处写 `TODO` 和思考题。
> 你必须自己查手册、算数值、把空白填上，代码才会真正工作。
>
> **能不能跑通不重要，想明白为什么才重要。**

---

## 一、怎么用这套例程

### 在 Keil 里跑某个 Lab

推荐复用现有的 `firmware/1014_blinky` 工程，**不要为每个 Lab 新建工程**（省事）：

1. 打开 `firmware/1014_blinky/blinky.uvprojx`
2. 把要做的 Lab 的 `.c` 文件**添加进工程**：
   右键 `Source Group 1` → `Add Existing Files to Group...` → 选中例如 `labs/Lab1_GPIO/lab1_gpio.c`
3. **让 `blinky.c` 不参与编译**：右键 `blinky.c` → `Options for File...` → 取消勾选 `Include in Target Build` → OK
4. **F7 编译 → F8 下载**
5. 做完一个 Lab，就把它的 `.c` 从工程移除，换上下一个

> 每个 Lab 的 `.c` 都有自己的 `main()`，所以**同一时间只能启用一个**。

### 编码提醒
- 源文件是 **UTF-8**，若 Keil 里中文注释乱码：`Edit → Configuration → Editor → Encoding` 选 `UTF-8`
- 每个 Lab 完成后，**先回答文件末尾的思考题**，再看下一个

---

## 二、阶梯路线（由易到难，紧扣项目）

| Lab | 主题 | 对应项目能力 | 里程碑 |
|---|---|---|---|
| **Lab1** | GPIO 输出与上电安全态 | 让控制线默认关断 | M2 前置 |
| **Lab2** | 定时器时间基准 + PWM 方波 | 输出 250kHz 基准波 | M2 |
| **Lab3** | 输出比较**翻转**模式 | 精确控制每个"边沿时刻" | M2 |
| **Lab4** | 四通道**边沿表** | **复现论文图 6 完整时序** ★ | M2/M3 |
| **Lab5** | 中断与发射间隔 | 每 ~0.7ms 重复发射 | M3/M4 |
| **Lab6** | 串口参数下发 | 上位机改频率 50–300kHz | M4 |

> ★ Lab4 是本项目的**技术核心**。前面三个 Lab 都在为它铺路。

---

## 三、学习契约（请认真对待）

1. **先自己查，再问人**：寄存器含义去查《STM32F103 参考手册 RM0008》或芯片数据手册，不要求助于"抄答案"。
2. **先算，再写**：涉及 PSC/ARR/CCR 的地方，先在本子上算出数值，再写进代码。
3. **先观察，再判断**：每个 Lab 都给了验证方法（示波器/万用表/LED），用它来检验你的理解。
4. **想不清楚就停下来**：`TODO` 下面留了提示，提示不够时，回到 `docs/学习路线图.md` 对应模块。

---

## 四、需要准备的工具

- **Keil µVision**（已装好）
- **ST-Link V2**（已装好驱动）
- **示波器**（实验室有，Lab2 起要用；没有的话至少准备万用表和 LED）
- 串口助手软件（Lab6 用，如 SSCOM、串口调试助手）

---

## 五、目录索引

```
firmware/labs/
├── README.md                 ← 本文件
├── Lab1_GPIO/                GPIO 输出基础
│   ├── lab1_gpio.c
│   └── README.md
├── Lab2_TIMER_PWM/           定时器与方波
│   ├── lab2_pwm.c
│   └── README.md
├── Lab3_OC_TOGGLE/           输出比较翻转（核心概念）
│   ├── lab3_octoggle.c
│   └── README.md
├── Lab4_EDGE_TABLE/          四通道边沿表（复现图6）★
│   ├── lab4_edgetable.c
│   └── README.md
├── Lab5_INTERRUPT/           中断与发射间隔
│   ├── lab5_interrupt.c
│   └── README.md
└── Lab6_UART/                串口参数下发
    ├── lab6_uart.c
    └── README.md
```

---

## 六、Keil 常见报错速查（遇到先查这里）

| 报错信息 | 原因 | 解决 |
|---|---|---|
| `FCARM - Output Name not specified, please check 'Options for Target - Utilities'` | 源文件的 **File Type 被设成了 Image file**，Keil 用 FCARM 图像转换工具处理它 | 右键该文件 → `Options for File...` → `File Type` 改为 **C Source File** |
| `identifier "xxx" is undefined` | 寄存器名 / 位域名拼错 | 对照 RM0008 核对拼写 |
| `multiply defined` / `undefined symbol main` | 同一时间有多个或零个 `main` | 确保只有一个 Lab 的 `.c` 参与编译（其余取消 `Include in Target Build`） |
| `implicit conversion ... changes value`（警告） | 类型位宽不够，如把大数存进 `uint16_t` | 检查变量类型（该用 `uint32_t` 的别用 `uint16_t`） |
| 编译通过，但引脚毫无反应 | 忘开时钟 / 引脚没配模式 | 检查 `RCC->xENR` 与 `CRL/CRH` |
| `#include "stm32f10x.h"` 找不到 | RTE 组件缺失 | 魔术棒 → RTE → 勾选 CMSIS Core + Device Startup |
| `Target not created` | 上面有 error 没解决 | 先解决所有 error（warning 可暂时忽略） |

---

### ★ 头号大坑：忘了开 GPIO 时钟

**症状**：编译 0 错误 0 警告，程序也下载了，但**引脚毫无反应**（LED 不亮、示波器一条直线、串口没数据）。

**原因**：STM32 每个外设都有独立的时钟开关。**连 GPIO 端口本身都有！**

```c
/* 要用 GPIOA，必须先开它的时钟 —— 而且 GPIO 在 APB2，不在 APB1！ */
RCC->APB2ENR |= (1U << 2);    /* IOPAEN = GPIOA 时钟 */
RCC->APB2ENR |= (1U << 3);    /* IOPBEN = GPIOB 时钟 */
RCC->APB2ENR |= (1U << 4);    /* IOPCEN = GPIOC 时钟 */
```

**为什么容易漏**：8051 的 IO 是"常电"的，写 P1 就能用；STM32 必须先"上电"。而且**定时器时钟（APB1）和 GPIO 时钟（APB2）在不同寄存器里**，开了一个不等于开了另一个——Lab2 就是在这里卡住的。

**记住这个组合**（本项目常用）：
```c
RCC->APB1ENR |= (1U << 0);        /* TIM2  （APB1） */
RCC->APB2ENR |= (1U << 2);        /* GPIOA （APB2） */
RCC->APB2ENR |= (1U << 14);       /* USART1（APB2） */
```

> 用 Keil 调试时，Watch 里加 `RCC->APB2ENR`，确认对应位是 1 —— 这是排查"引脚没反应"的第一步。

---

## 七、参考资源

- 技术规格与项目背景：`docs/项目梳理与STM32开发计划.md`
- 知识模块详解：`docs/学习路线图.md`
- 芯片手册：ST 官网《RM0008 参考手册》《STM32F103x8 数据手册》
- 引脚复用表：数据手册 Table（TIM2_CH1→PA0 等）
