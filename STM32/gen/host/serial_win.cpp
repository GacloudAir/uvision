/* ============================================================================
 *  host/serial_win.cpp — Windows 串口封装（Win32 API）
 * ========================================================================== */
#include "serial_win.h"
#include <windows.h>
#include <cstring>

SerialPort::SerialPort() : h_((void*)INVALID_HANDLE_VALUE) {}
SerialPort::~SerialPort() { close(); }

bool SerialPort::open(const std::string &port, uint32_t baud)
{
    close();
    std::string path = "\\\\.\\" + port;   /* 如 "\\.\COM3"，支持 COM10+ */
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) { CloseHandle(h); return false; }
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity   = NOPARITY;
    if (!SetCommState(h, &dcb)) { CloseHandle(h); return false; }

    /* 读超时：字符间隔 1ms，总超时 0（读多少算多少，配合 read_byte 轮询）*/
    COMMTIMEOUTS to = {0};
    to.ReadIntervalTimeout = 1;
    to.ReadTotalTimeoutConstant = 0;
    to.ReadTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = 100;
    to.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(h, &to);

    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    h_ = (void*)h;
    return true;
}

void SerialPort::close()
{
    if (h_ != (void*)INVALID_HANDLE_VALUE)
    {
        CloseHandle((HANDLE)h_);
        h_ = (void*)INVALID_HANDLE_VALUE;
    }
}

size_t SerialPort::write(const uint8_t *buf, size_t len)
{
    if (!is_open()) return 0;
    DWORD n = 0;
    WriteFile((HANDLE)h_, buf, (DWORD)len, &n, NULL);
    return (size_t)n;
}

bool SerialPort::read_byte(uint8_t &b, uint32_t timeout_ms)
{
    if (!is_open()) return false;
    DWORD n = 0;
    /* 简易超时：每次 ReadFile 1 字节，外层轮询累计 timeout_ms */
    /* 更简单：依赖 SetCommTimeouts 的字符间隔超时 */
    if (!ReadFile((HANDLE)h_, &b, 1, &n, NULL) || n == 0)
        return false;
    (void)timeout_ms;
    return true;
}

size_t SerialPort::read_exact(uint8_t *buf, size_t n, uint32_t timeout_ms)
{
    size_t got = 0;
    /* 逐字节读，遇到超时即返回已读数（上层按帧结构判断够不够）*/
    while (got < n)
    {
        uint8_t b;
        if (!read_byte(b, timeout_ms)) break;
        buf[got++] = b;
    }
    return got;
}

std::vector<std::string> SerialPort::list_ports()
{
    std::vector<std::string> ports;
    /* best-effort：探测 COM1..COM16 是否能打开（仅供学习用）*/
    for (int i = 1; i <= 16; ++i)
    {
        std::string name = "COM" + std::to_string(i);
        std::string path = "\\\\.\\" + name;
        HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE)
        {
            ports.push_back(name);
            CloseHandle(h);
        }
    }
    return ports;
}
