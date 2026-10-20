#ifndef K2161_RECEIVE_PROBE_H
#define K2161_RECEIVE_PROBE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t msgid;
    uint16_t frame_len;
    size_t frame_offset;
    size_t available_len;
    const uint8_t *frame;
    int complete;
} k2161_rgi_frame;

/* Detect the start of an RGI frame in one K2161 link packet.  A successful
 * result may be incomplete; continuation data is preserved by the raw packet
 * recorder and reconstructed offline. */
int k2161_probe_rgi_start(const uint8_t *transport,
                          size_t transport_len,
                          k2161_rgi_frame *out);
int k2161_is_expected_rgi_rx(uint16_t msgid);

#endif
