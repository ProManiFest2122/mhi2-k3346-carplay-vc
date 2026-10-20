#ifndef K2161_SEND_HOOK_H
#define K2161_SEND_HOOK_H

#include <stdint.h>

extern volatile unsigned int k2161_rgi_start_sent;

int k2161_send_hook_try_install_base(void *driver_base);
int k2161_send_start_route_guidance(void *ctx);
void k2161_send_note_rgi_rx(uint16_t msgid);

#endif
