/* ============================================================================
 *  host/protocol.cpp — 协议编解码实现（与下位机 L2 逐字节一致）
 * ========================================================================== */
#include "protocol.h"

uint16_t crc16_modbus(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; ++i)
    {
        crc ^= buf[i];
        for (int b = 0; b < 8; ++b)
        {
            if (crc & 0x0001)
                crc = (uint16_t)((crc >> 1) ^ 0xA001);
            else
                crc = (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

uint8_t build_command(uint8_t cmd, const uint8_t *payload, uint8_t payload_len,
                      uint8_t *out_frame)
{
    out_frame[0] = MAGIC0;
    out_frame[1] = MAGIC1;
    uint8_t len = (uint8_t)(1 + payload_len);   /* CMD + PAYLOAD */
    out_frame[2] = len;
    out_frame[3] = cmd;
    for (uint8_t i = 0; i < payload_len; ++i)
        out_frame[4 + i] = payload[i];

    /* CRC 作用于 [LEN .. PAYLOAD末]，共 len+1 字节，从 out_frame[2] 起 */
    uint16_t crc = crc16_modbus(&out_frame[2], (uint16_t)(len + 1));
    out_frame[4 + payload_len]     = (uint8_t)(crc & 0xFF);        /* 低字节先发 */
    out_frame[4 + payload_len + 1] = (uint8_t)(crc >> 8);          /* 高字节 */

    return (uint8_t)(6 + payload_len);
}

bool parse_response(const uint8_t *frame, uint8_t frame_len,
                    uint8_t *out_cmd, uint8_t *out_result,
                    const uint8_t **data, uint8_t *data_len)
{
    if (frame_len < 6) return false;
    if (frame[0] != MAGIC0 || frame[1] != MAGIC1) return false;

    uint8_t len = frame[2];
    uint8_t need = (uint8_t)(4 + len + 2);       /* MAGIC2 + LEN + (CMD+PAYLOAD) + CRC2 */
    if (frame_len != need) return false;

    /* 校验 CRC（覆盖 [LEN .. PAYLOAD末]） */
    uint16_t crc_calc = crc16_modbus(&frame[2], (uint16_t)(len + 1));
    uint16_t crc_recv = (uint16_t)(frame[4 + (len - 1)] | (frame[5 + (len - 1)] << 8));
    if (crc_calc != crc_recv) return false;

    *out_cmd    = frame[3];
    *out_result = frame[4];
    *data       = &frame[5];
    *data_len   = (uint8_t)(len - 2);             /* LEN = CMD(1)+RESULT(1)+data */
    return true;
}
