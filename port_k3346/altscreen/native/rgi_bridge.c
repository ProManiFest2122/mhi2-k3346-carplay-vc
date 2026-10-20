#include "rgi_bridge.h"
#include "diagnostics.h"

#include <pthread.h>
#include <string.h>

#include "framework/hook_framework.h"
#include "framework/iap2_protocol.h"
#include "framework/bus.h"
#include "routeguidance/rgd_hook.h"

/*
 * rgd_module_def is Luka's existing route-guidance module definition.
 * Its on_message callback is the real parser/state/bus path.
 */
extern const hook_module_def_t rgd_module_def;

static pthread_mutex_t g_init_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_feed_lock = PTHREAD_MUTEX_INITIALIZER;

static hook_context_t g_ctx;
static int g_bus_ready;
static int g_shutdown;

/*
 * Minimal compatibility surface required by Luka rgd_hook.c.
 *
 * IMPORTANT:
 * K2161 identification and send sequencing remain owned by ident_hook.c and send_hook.c.
 * Therefore Luka must NEVER inject its own 0x5200/0x5203 from this build.
 */
hook_context_t *hook_framework_get_context(void)
{
    return &g_ctx;
}

bool hook_is_ready(void)
{
    return false;
}

bool hook_is_active(void)
{
    return g_ctx.session_active;
}

uint16_t hook_get_component_id(void)
{
    return 0x0010;
}

hook_result_t hook_inject_frame(const uint8_t *frame, size_t len)
{
    (void)frame;
    (void)len;
    return HOOK_ERR_INIT;
}

hook_result_t hook_inject_message(uint16_t msgid,
                                  const uint8_t *payload,
                                  size_t payload_len)
{
    (void)msgid;
    (void)payload;
    (void)payload_len;
    return HOOK_ERR_INIT;
}

int k2161_rgi_bridge_init(void)
{
    hook_result_t rc;

    pthread_mutex_lock(&g_init_lock);

    if (g_shutdown) {
        pthread_mutex_unlock(&g_init_lock);
        return 0;
    }

    if (g_bus_ready) {
        pthread_mutex_unlock(&g_init_lock);
        return 1;
    }

    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.direction = MSG_DIR_INCOMING;
    g_ctx.rgd_component_id = 0x0010;
    g_ctx.rgd_component_valid = true;

    rc = bus_init();
    if (rc != HOOK_OK && rc != HOOK_ERR_BUSY) {
        pthread_mutex_unlock(&g_init_lock);
        return 0;
    }

    g_bus_ready = 1;

    pthread_mutex_unlock(&g_init_lock);
    return 1;
}

int k2161_rgi_bridge_feed(const uint8_t *bytes, size_t len)
{
    rgi_diag_log("BRIDGE", "feed len=%u", (unsigned)len);
    iap2_frame_t frame;

    if (bytes == NULL || len < IAP2_MIN_FRAME)
        return -1;

    if (!iap2_find_frame(bytes, len, &frame))
        return -2;

    /* We only accept one exact, complete control frame. */
    if (frame.offset != 0 || frame.frame_len != len)
        return -3;

    if (frame.msgid != IAP2_MSG_ROUTE_GUIDANCE_UPDATE &&
        frame.msgid != IAP2_MSG_ROUTE_GUIDANCE_MANEUVER &&
        frame.msgid != IAP2_MSG_ROUTE_GUIDANCE_LANE)
        return 0;

    if (!k2161_rgi_bridge_init())
        return -4;

    if (rgd_module_def.on_message == NULL)
        return -5;

    pthread_mutex_lock(&g_feed_lock);

    g_ctx.direction = MSG_DIR_INCOMING;
    g_ctx.msgid = frame.msgid;
    g_ctx.raw_buf = bytes;
    g_ctx.raw_len = len;

    (void)rgd_module_def.on_message(&g_ctx, &frame);

    pthread_mutex_unlock(&g_feed_lock);

    return 1;
}

void k2161_rgi_bridge_shutdown(void)
{
    pthread_mutex_lock(&g_init_lock);

    if (!g_shutdown) {
        g_shutdown = 1;

        if (g_bus_ready) {
            rgd_shutdown();
            bus_shutdown();
            g_bus_ready = 0;
        }
    }

    pthread_mutex_unlock(&g_init_lock);
}
