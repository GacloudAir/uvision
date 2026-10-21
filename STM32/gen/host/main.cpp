/* ============================================================================
 *  host/main.cpp — C++ 上位机控制台（计划层③ v1）
 * ============================================================================
 *  功能：串口枚举 → 下发命令(SET/GET/START/STOP/STATUS/SELF_TEST) → 回读 →
 *        打印 + 写 CSV。可与下位机 L2 协议帧对接。
 *
 *  编译（二选一）：
 *    MinGW : g++ main.cpp protocol.cpp serial_win.cpp -o host.exe -lsetupapi
 *    MSVC  : cl main.cpp protocol.cpp serial_win.cpp  (需 Windows SDK)
 *
 *  运行：  host.exe COM3
 *          （不给端口则打印可用 COM 列表）
 * ========================================================================== */
#include "protocol.h"
#include "serial_win.h"

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

/* 读一帧（同步、遇错重同步）；返回帧长度，0 表示超时/失败 */
static uint8_t read_frame(SerialPort &sp, uint8_t *buf, uint32_t timeout_ms)
{
    /* 状态：0 等 MAGIC0, 1 等 MAGIC1, 2 等 LEN, 3 等余下(LEN+2) */
    uint8_t state = 0;
    uint8_t len = 0;
    uint32_t waited = 0;
    while (waited < timeout_ms)
    {
        uint8_t b;
        if (!sp.read_byte(b, 5)) { waited += 5; continue; }
        switch (state)
        {
            case 0: if (b == MAGIC0) { buf[0]=b; state=1; } break;
            case 1: if (b == MAGIC1) { buf[1]=b; state=2; }
                    else state = (b==MAGIC0)?1:0;          /* 重同步 */
                    break;
            case 2: len = b; buf[2]=b; state=3; buf[3]=0;   /* 先占位 CMD */
                    /* 余下需读 = LEN+2（PAYLOAD 含 CMD+数据 + CRC2）*/
                    /* 这里用循环继续读，下面直接把 CMD 也读进来 */
                    break;
            case 3:
            {
                static uint8_t idx = 4;
                buf[idx++] = b;
                uint8_t need = (uint8_t)(4 + len + 2);       /* 总字节 */
                if (idx >= need) { uint8_t got = idx; idx = 4; state = 0; return got; }
                break;
            }
        }
    }
    return 0;
}

static bool send_command(SerialPort &sp, uint8_t cmd,
                         const uint8_t *payload, uint8_t payload_len,
                         uint8_t *out_result, std::vector<uint8_t> *out_data)
{
    uint8_t frame[FRAME_MAX];
    uint8_t flen = build_command(cmd, payload, payload_len, frame);
    sp.write(frame, flen);

    uint8_t rbuf[FRAME_MAX];
    uint8_t rlen = read_frame(sp, rbuf, 1000);
    if (rlen == 0) { printf("  [超时] 未收到响应\n"); return false; }

    uint8_t rc, res; const uint8_t *data; uint8_t dlen;
    if (!parse_response(rbuf, rlen, &rc, &res, &data, &dlen))
    {
        printf("  [坏帧] CRC 错误或帧结构不对\n"); return false;
    }
    if (rc != (cmd | CMD_RESP_BIT)) { printf("  [错帧] 响应命令码不匹配\n"); return false; }
    *out_result = res;
    if (out_data) out_data->assign(data, data + dlen);
    return true;
}

/* 大端读写辅助 */
static void put_be24(uint8_t *p, uint32_t v) { p[0]=(uint8_t)(v>>16); p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)v; }
static uint32_t get_be24(const uint8_t *p) { return ((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2]; }
static void put_be16(uint8_t *p, uint16_t v){ p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static uint16_t get_be16(const uint8_t *p){ return (uint16_t)((p[0]<<8)|p[1]); }

int main(int argc, char **argv)
{
    /* 端口选择 */
    std::string port;
    if (argc >= 2) port = argv[1];
    else
    {
        auto ports = SerialPort::list_ports();
        if (ports.empty()) { printf("未发现 COM 端口。用法: host.exe COM3\n"); return 1; }
        printf("可用端口: ");
        for (auto &p : ports) printf("%s ", p.c_str());
        printf("\n请指定，如: host.exe %s\n", ports[0].c_str());
        return 0;
    }

    SerialPort sp;
    if (!sp.open(port, 115200)) { printf("打开 %s 失败\n", port.c_str()); return 1; }
    printf("已打开 %s @115200\n", port.c_str());

    /* CSV 日志 */
    std::ofstream csv("selftest_log.csv");
    csv << "cmd,result,value\n";

    uint8_t res; std::vector<uint8_t> data;

    /* ---- 1) SET_PARAM: 频率 = 250000 Hz ---- */
    {
        uint8_t pl[4]; pl[0] = PID_FREQ_HZ; put_be24(pl+1, 250000UL);
        printf("[SET_PARAM] freq=250000 ... ");
        if (send_command(sp, CMD_SET_PARAM, pl, 4, &res, &data))
            printf("%s\n", res==RES_OK ? "OK" : (res==RES_ERR_RANGE?"RANGE_ERR":"FAIL"));
        csv << "SET_FREQ," << (int)res << ",250000\n";
    }

    /* ---- 2) GET_PARAM: 读回频率 ---- */
    {
        uint8_t pl[1] = { PID_FREQ_HZ };
        printf("[GET_PARAM] freq ... ");
        if (send_command(sp, CMD_GET_PARAM, pl, 1, &res, &data) && res==RES_OK && data.size()>=3)
        {
            uint32_t f = get_be24(data.data());
            printf("= %lu Hz\n", f); csv << "GET_FREQ," << (int)res << "," << f << "\n";
        }
        else printf("失败\n");
    }

    /* ---- 3) START ---- */
    { printf("[START] ... ");
      if (send_command(sp, CMD_START, nullptr, 0, &res, &data))
          printf("%s\n", res==RES_OK?"OK":"FAIL");
      csv << "START," << (int)res << ",\n"; }

    /* ---- 4) GET_STATUS ---- */
    {
        printf("[GET_STATUS] ... ");
        if (send_command(sp, CMD_GET_STATUS, nullptr, 0, &res, &data) && res==RES_OK && data.size()>=5)
        {
            uint8_t run = data[0];
            uint32_t f = get_be24(data.data()+1);
            uint8_t err = data[4];
            printf("run=%d freq=%lu err=%d\n", run, f, err);
            csv << "STATUS," << (int)res << "," << (int)run << "\n";
        } else printf("失败\n");
    }

    /* ---- 5) SELF_TEST（跑 100 组，回平均相对误差‰）---- */
    {
        printf("[SELF_TEST] ... ");
        if (send_command(sp, CMD_SELF_TEST, nullptr, 0, &res, &data) && res==RES_OK && data.size()>=2)
        {
            uint16_t permille = get_be16(data.data());
            printf("平均相对误差 = %u ‰ (%.2f%%)\n", permille, permille/10.0);
            csv << "SELFTEST," << (int)res << "," << permille << "\n";
        } else printf("失败\n");
    }

    /* ---- 6) STOP ---- */
    { printf("[STOP] ... ");
      if (send_command(sp, CMD_STOP, nullptr, 0, &res, &data))
          printf("%s\n", res==RES_OK?"OK":"FAIL");
      csv << "STOP," << (int)res << ",\n"; }

    csv.close();
    printf("日志已写入 selftest_log.csv\n");
    return 0;
}

/* ---------------------------------------------------------------------------
 *  从 v1 到 v2（Qt）的工程要点（写进简历/汇报）：
 *    - 串口读写必须放进独立线程（或用 QSerialPort 的信号槽），
 *      否则界面会卡——这正是"工程细节"可以展开讲的地方。
 *    - 把 build_command / parse_response / read_frame 直接复用（协议与 UI 解耦）。
 *    - 界面加"时序图预览"：按当前参数（ARR/CCR）在 QWidget 上画出预期波形。
 * ========================================================================== */
