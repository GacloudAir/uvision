/* ============================================================================
 *  host/protocol.h — 上位机侧协议定义（与下位机 L2 逐字节一致）
 * ============================================================================
 *  字节序：多字节字段一律 大端（高字节先发）。
 *  帧： [0xAA][0x55][LEN][CMD][PAYLOAD...][CRC16lo][CRC16hi]
 *    LEN     = CMD+PAYLOAD 总字节数（不含 MAGIC/CRC）
 *    CRC16   = Modbus，作用于 [LEN .. PAYLOAD末]
 *    响应帧： CMD |= 0x80；PAYLOAD[0] = RESULT
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <vector>

/* 帧常量 */
static const uint8_t MAGIC0 = 0xAA;
static const uint8_t MAGIC1 = 0x55;
static const uint8_t FRAME_MAX = 64;

/* 命令 */
enum Cmd : uint8_t {
    CMD_SET_PARAM  = 0x01,
    CMD_GET_PARAM  = 0x02,
    CMD_START      = 0x03,
    CMD_STOP       = 0x04,
    CMD_GET_STATUS = 0x05,
    CMD_SELF_TEST  = 0x06,
    CMD_RESP_BIT   = 0x80
};

/* 应答结果 */
enum Result : uint8_t {
    RES_OK       = 0x00,
    RES_ERR_CRC  = 0x01,
    RES_ERR_UNK  = 0x02,
    RES_ERR_RANGE= 0x03
};

/* 参数 id（SET/GET 用） */
enum ParamId : uint8_t {
    PID_FREQ_HZ  = 0x01,   /* 频率 Hz（3 字节大端）*/
    PID_PW_CH1   = 0x02,   /* 通道1脉宽 tick（2 字节）*/
    PID_PW_CH2   = 0x03,
    PID_PW_CH3   = 0x04,
    PID_PW_CH4   = 0x05,
    PID_PHASE_CH4= 0x06
};

/* ---- Modbus CRC16（与下位机一致）---- */
uint16_t crc16_modbus(const uint8_t *buf, uint16_t len);

/* ---- 组命令帧：cmd + payload -> out_frame；返回帧总字节数 ---- */
uint8_t build_command(uint8_t cmd, const uint8_t *payload, uint8_t payload_len,
                      uint8_t *out_frame);

/* ---- 解析响应帧：成功返回 true，*out_cmd/*out_result 填命令与结果，
 *    data 指向 PAYLOAD 中 RESULT 之后的数据，*data_len 为其长度 ---- */
bool parse_response(const uint8_t *frame, uint8_t frame_len,
                    uint8_t *out_cmd, uint8_t *out_result,
                    const uint8_t **data, uint8_t *data_len);

#endif /* PROTOCOL_H */
