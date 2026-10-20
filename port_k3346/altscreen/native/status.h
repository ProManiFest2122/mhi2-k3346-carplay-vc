#ifndef K2161_STATUS_H
#define K2161_STATUS_H

#include <stdint.h>

extern volatile unsigned int k2161_status_base_resolved;
extern volatile unsigned int k2161_status_ident_hook_installed;
extern volatile unsigned int k2161_status_ident_001e_attempted;
extern volatile unsigned int k2161_status_ident_001e_succeeded;
extern volatile unsigned int k2161_status_iap2_msg_hook_installed;
extern volatile unsigned int k2161_status_iap2_close_hook_installed;
extern volatile unsigned int k2161_status_cmd2700_observed;
extern volatile unsigned int k2161_status_cmd2700_tracked;
extern volatile unsigned int k2161_status_msgreply_matched;
extern volatile unsigned int k2161_status_send_5200_attempted;
extern volatile unsigned int k2161_status_send_5200_succeeded;
extern volatile unsigned int k2161_status_send_5200_failed;
extern volatile unsigned int k2161_status_connection_reset;

void k2161_status_note_context(void *ctx);
void k2161_status_base_ready(void);
void k2161_status_ident_hook_ready(void);
void k2161_status_ident_attempt(void *ctx);
void k2161_status_ident_success(void *ctx);
void k2161_status_send_hooks_ready(int msg_ready, int close_ready);
void k2161_status_cmd2700(void *ctx, int rcvid, int tracked);
void k2161_status_ffff_success(void *ctx, int rcvid);
void k2161_status_send_attempt(void *ctx);
void k2161_status_send_result(void *ctx, int success);
void k2161_status_reset(void *ctx);
void k2161_status_rx(uint16_t msgid);
void k2161_rgi_dump_status(void);

#endif
