#include <dlfcn.h>
#include <stdint.h>

extern volatile unsigned int k2161_rx_callsite_count;
extern volatile unsigned int k2161_rx_rgi_count;
extern volatile unsigned int k2161_rx_partial_rgi_count;
extern volatile unsigned int k2161_rx_5201;
extern volatile unsigned int k2161_rx_5202;
extern volatile unsigned int k2161_rx_5204;
#include "status.h"

typedef void (*ipod_log_fn)(void *, int, const char *, ...);
static ipod_log_fn stock_ipod_log;
static void *stock_logger;
static int initial_state_reported;

volatile unsigned int k2161_status_base_resolved;
volatile unsigned int k2161_status_ident_hook_installed;
volatile unsigned int k2161_status_ident_001e_attempted;
volatile unsigned int k2161_status_ident_001e_succeeded;
volatile unsigned int k2161_status_iap2_msg_hook_installed;
volatile unsigned int k2161_status_iap2_close_hook_installed;
volatile unsigned int k2161_status_cmd2700_observed;
volatile unsigned int k2161_status_cmd2700_tracked;
volatile unsigned int k2161_status_msgreply_matched;
volatile unsigned int k2161_status_send_5200_attempted;
volatile unsigned int k2161_status_send_5200_succeeded;
volatile unsigned int k2161_status_send_5200_failed;
volatile unsigned int k2161_status_connection_reset;

static int log_ready(void)
{
    if (!stock_ipod_log)
        stock_ipod_log = (ipod_log_fn)dlsym(RTLD_DEFAULT, "ipod_log");
    return stock_ipod_log != 0 && stock_logger != 0;
}

void k2161_status_note_context(void *ctx)
{
    if (ctx) stock_logger = *(void **)((uint8_t *)ctx + 20);
    if (!initial_state_reported && log_ready()) {
        stock_ipod_log(stock_logger, 0, "K2161_RGI base=%u ident_hook=%u\n",
                       k2161_status_base_resolved,
                       k2161_status_ident_hook_installed);
        initial_state_reported = 1;
    }
}

void k2161_status_base_ready(void) { k2161_status_base_resolved = 1; }
void k2161_status_ident_hook_ready(void) { k2161_status_ident_hook_installed = 1; }

void k2161_status_ident_attempt(void *ctx)
{
    k2161_status_note_context(ctx);
    k2161_status_ident_001e_attempted++;
}

void k2161_status_ident_success(void *ctx)
{
    k2161_status_note_context(ctx);
    k2161_status_ident_001e_succeeded++;
    if (log_ready()) stock_ipod_log(stock_logger, 0, "K2161_RGI ident_001e=ok\n");
}

void k2161_status_send_hooks_ready(int msg_ready, int close_ready)
{
    if (msg_ready) k2161_status_iap2_msg_hook_installed = 1;
    if (close_ready) k2161_status_iap2_close_hook_installed = 1;
}

void k2161_status_cmd2700(void *ctx, int rcvid, int tracked)
{
    k2161_status_note_context(ctx);
    k2161_status_cmd2700_observed++;
    if (tracked) k2161_status_cmd2700_tracked++;
    if (log_ready()) stock_ipod_log(stock_logger, 0,
        "K2161_RGI cmd2700 rcvid=%d tracked=%d\n", rcvid, tracked);
}

void k2161_status_ffff_success(void *ctx, int rcvid)
{
    k2161_status_note_context(ctx);
    k2161_status_msgreply_matched++;
    if (log_ready()) stock_ipod_log(stock_logger, 0,
        "K2161_RGI fffb_ok rcvid=%d\n", rcvid);
}

void k2161_status_send_attempt(void *ctx)
{
    k2161_status_note_context(ctx);
    k2161_status_send_5200_attempted++;
    if (log_ready()) stock_ipod_log(stock_logger, 0, "K2161_RGI 5200_try\n");
}

void k2161_status_send_result(void *ctx, int success)
{
    k2161_status_note_context(ctx);
    if (success) k2161_status_send_5200_succeeded++;
    else k2161_status_send_5200_failed++;
    if (log_ready()) stock_ipod_log(stock_logger, 0,
        success ? "K2161_RGI 5200_ok\n" : "K2161_RGI 5200_fail\n");
}

void k2161_status_reset(void *ctx)
{
    k2161_status_note_context(ctx);
    k2161_status_connection_reset++;
    if (log_ready()) stock_ipod_log(stock_logger, 0, "K2161_RGI reset\n");
}

void k2161_status_rx(uint16_t msgid)
{
    if (log_ready()) stock_ipod_log(stock_logger, 0, "K2161_RGI rx=%04x\n", msgid);
}

void k2161_rgi_dump_status(void)
{
    if (!log_ready()) return;
    stock_ipod_log(stock_logger, 0,
        "K2161_RGI status base=%u ident_hook=%u ident_try=%u ident_ok=%u "
        "msg_hook=%u close_hook=%u cmd2700=%u tracked=%u fffb_ok=%u "
        "5200_try=%u 5200_ok=%u 5200_fail=%u reset=%u "
        "rx5201=%u rx5202=%u rx5204=%u\n",
        k2161_status_base_resolved, k2161_status_ident_hook_installed,
        k2161_status_ident_001e_attempted, k2161_status_ident_001e_succeeded,
        k2161_status_iap2_msg_hook_installed,
        k2161_status_iap2_close_hook_installed,
        k2161_status_cmd2700_observed, k2161_status_cmd2700_tracked,
        k2161_status_msgreply_matched, k2161_status_send_5200_attempted,
        k2161_status_send_5200_succeeded, k2161_status_send_5200_failed,
        k2161_status_connection_reset, k2161_rx_5201,
        k2161_rx_5202, k2161_rx_5204);
}
