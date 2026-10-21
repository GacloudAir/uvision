/* ============================================================================
 *  host/serial_win.h — Windows 串口封装（Win32 API，MinGW / MSVC 均可编译）
 * ========================================================================== */
#ifndef SERIAL_WIN_H
#define SERIAL_WIN_H

#include <string>
#include <vector>
#include <cstdint>
#include <windows.h>

class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    /* 打开端口，如 "COM3"，波特率默认 115200 */
    bool open(const std::string &port, uint32_t baud = 115200);
    void close();

    /* 写任意字节；返回实际写出数 */
    size_t write(const uint8_t *buf, size_t len);

    /* 读一字节，带超时(ms)；成功返回 true */
    bool read_byte(uint8_t &b, uint32_t timeout_ms = 200);

    /* 读 n 字节（凑够或超时）；返回实际读到数 */
    size_t read_exact(uint8_t *buf, size_t n, uint32_t timeout_ms = 500);

    /* 列出系统中存在的 COM 端口（best-effort：探测 COM1..COM16）*/
    static std::vector<std::string> list_ports();

    bool is_open() const { return h_ != INVALID_HANDLE_VALUE; }

private:
    void *h_;   /* HANDLE */
};

#endif /* SERIAL_WIN_H */
