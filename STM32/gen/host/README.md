# 上位机（C++ 控制台）— 计划层③ v1

PC 端程序，经串口与下位机 L2 协议帧对接：枚举端口 → 下发命令 → 回读 → 打印 + 写 CSV。

## 文件

| 文件 | 作用 |
|---|---|
| `protocol.h/.cpp` | 帧定义、Modbus CRC16、组帧/解帧（与下位机 L2 逐字节一致） |
| `serial_win.h/.cpp` | Windows 串口封装（Win32 API，MinGW/MSVC 皆可） |
| `main.cpp` | 控制台主流程：SET/GET/START/STOP/STATUS/SELF_TEST + CSV |

## 编译

```bat
:: MinGW
g++ main.cpp protocol.cpp serial_win.cpp -o host.exe -lsetupapi

:: MSVC（需 Windows SDK）
cl main.cpp protocol.cpp serial_win.cpp
```

## 运行

```bat
host.exe COM3        :: 指定端口
host.exe             :: 不指定则打印可用 COM 列表
```

## 帧结构（与下位机 L2 一致）

```
[0xAA][0x55][LEN][CMD][PAYLOAD...][CRC16lo][CRC16hi]
```
- 多字节大端；CRC16(Modbus) 作用于 `[LEN..PAYLOAD末]`
- 响应帧：`CMD |= 0x80`，`PAYLOAD[0] = RESULT`

## 命令速查

| 命令 | 值 | 说明 |
|---|---|---|
| SET_PARAM | 0x01 | payload: [param_id][value] |
| GET_PARAM | 0x02 | payload: [param_id] → 回 value |
| START | 0x03 | 启动输出 |
| STOP | 0x04 | 停止 |
| GET_STATUS | 0x05 | 回 [run][freq(3)][err] |
| SELF_TEST | 0x06 | 回 [err_permille(2)] |

## 关键工程点（v1 → v2）

- **v1 控制台用阻塞读 + 超时**，简单够用；但 Qt 界面里串口必须放**独立线程/信号槽**，否则 UI 卡死——这是面试能讲的工程质量细节。
- 协议编解码（`build_command`/`parse_response`/`read_frame`）与 UI 解耦，v2 直接复用。
- v2 加"时序图预览"：按 ARR/CCR 在 QWidget 上画预期波形。

## 验证方法

| 验证点 | 方法 | 期望 |
|---|---|---|
| 下发 | `host.exe COM3` 跑 SET/GET | 回读频率 = 250000 |
| 压测 | 循环发 1000 帧 | 零误帧（CRC 校验拦截坏帧） |
| 往返 | SET→GET 计时 | < 1s |
| 自检 | SELF_TEST | 打印平均相对误差（目标 <1%） |
