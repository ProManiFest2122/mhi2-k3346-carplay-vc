#include "receive_probe.h"

enum {
    K2161_LINK_HEADER_LEN = 9,
    K2161_CONTROL_HEADER_LEN = 6
};

int k2161_is_expected_rgi_rx(uint16_t msgid)
{
    return msgid == 0x5201 || msgid == 0x5202 || msgid == 0x5204;
}

int k2161_probe_rgi_start(const uint8_t *transport,
                          size_t transport_len,
                          k2161_rgi_frame *out)
{
    const size_t off = K2161_LINK_HEADER_LEN;
    uint16_t frame_len;
    uint16_t msgid;
    size_t available;

    if (transport == NULL || out == NULL)
        return 0;
    if (transport_len < off + K2161_CONTROL_HEADER_LEN)
        return 0;
    if (transport[off] != 0x40 || transport[off + 1] != 0x40)
        return 0;

    frame_len = (uint16_t)(((uint16_t)transport[off + 2] << 8) |
                           (uint16_t)transport[off + 3]);
    if (frame_len < K2161_CONTROL_HEADER_LEN)
        return 0;

    msgid = (uint16_t)(((uint16_t)transport[off + 4] << 8) |
                       (uint16_t)transport[off + 5]);
    if (msgid < 0x5200 || msgid > 0x5204)
        return 0;

    available = transport_len - off;
    out->msgid = msgid;
    out->frame_len = frame_len;
    out->frame_offset = off;
    out->available_len = available;
    out->frame = transport + off;
    out->complete = (size_t)frame_len <= available;
    return 1;
}
