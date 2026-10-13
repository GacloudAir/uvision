# C8051F330 / C8051F350 单片机学习例程

Silicon Labs C8051F330/F350 单片机的 Keil C51 学习例程集，按练习日期编号整理，配套开发板为 C8051F330 学习板。

## 例程索引

| 目录 | 日期 | 内容 | 关键源文件 |
| --- | --- | --- | --- |
| [0915_Blink](./0915_Blink) | 09-15 | GPIO 入门：单灯闪烁（推挽输出 + 软件延时） | `blinker.c` |
| [0915_2_DuoBlinky](./0915_2_DuoBlinky) | 09-15 | 双灯交替闪烁（P0.6 / P0.7，Timer2 中断定时） | `Duoblinky.c` |
| [0918_2](./0918_2) | 09-18 | 蜂鸣器发声（Timer2 中断 10Hz 翻转 P1.0） | `sound.c` |
| [1009](./1009) | 10-09 | 74HC595 驱动四位数码管走时显示（含蜂鸣器） | `LED-7.c` |
| [1009_2](./1009_2) | 10-09 | 74HC595 四位数码管秒表（Timer2 1ms 扫描） | `LED.c` |
| [1012_UART](./1012_UART) | 10-12 | UART0 串口通信（printf 重定向，9600bps） | `UART.c`、`Repeater/Repeater.ino` |
| [1013_Palindrome](./1013_Palindrome) | 10-13 | 非阻塞串口通信（中断 + 环形缓冲区） | `Palindrome.c` |

> `1012_UART/Repeater/` 是 RP2040（Arduino）侧的 USB ↔ UART1 透传程序，用于电脑与学习板之间的串口通信。

## 开发环境

- 芯片：C8051F330 / C8051F350（Silicon Labs）
- IDE：Keil uVision（C51 工具链），调试器驱动（SiC8051F）已配置在工程中
- 串口波特率：9600 bps（1012 起的例程）

## 使用方法

1. 用 Keil uVision 打开例程目录下的 `*.uvproj` 工程文件
2. Build 编译 → 用 USB Debug Adapter 下载到开发板
3. 编译产物（`Listings/`、`Objects/`、`*.hex` 等）由本地编译自动生成，不会上传到仓库

## 仓库约定

- 每个例程一个目录，命名规则 `MMDD_主题`；同日多个练习加后缀 `_2`
- 编译产物、IDE 个人状态文件（`.uvopt`、`.uvgui.*`）已由 `.gitignore` 自动排除，无需手动清理
- `C8051F330单板资料/`：开发板配套资料（芯片手册、原理图、官方例程、视频等），**因版权原因且体积较大，不入库**，仅本地保留
- 日常提交：`git add . && git commit -m "说明" && git push`