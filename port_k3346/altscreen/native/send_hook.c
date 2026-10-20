/* K3346 port (MHI2_ER_AUG22_K3346 / MU1438): stock ipod-drvr-iap2.so
 * SHA-256 0ac13f43d3fb259eee328c272fa1211bb558b6e2cd97c6e38ef38e4e71469d51.
 * Offsets re-derived from the driver's own .symtab/.dynsym, relocations and
 * table contents (tools/derive_k3346_iap2_offsets.py); K2161 values in comments.
 * Runtime checks below remain fail-closed. */
/* K2161 Route Guidance 0x5200 sender.
 *
 * The sender preserves the proven ordering:
 *   0x2700 -> stock GPS handling -> real MsgReply -> custom 0x5200.
 *
 * Retry behavior is bounded and stops after the first real 0x5201/0x5202/0x5204
 * message confirms that Route Guidance delivery is active.
 */
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

#include "send_hook.h"
#include "message_table.h"
#include "status.h"
#include "diagnostics.h"

typedef int (*iap2_msg_fn)(void *, void *, void *, void *, void *);
typedef int (*iap2_close_fn)(void *, void *, void *, void *);
typedef int (*packet_addparam_nodata_fn)(void *, void *, uint16_t);
typedef int (*packet_addparam_num_fn)(void *, void *, uint16_t, const void *, size_t);
typedef void *(*packet_sendlist_pop_fn)(void *);
typedef int (*packet_sendlist_push_fn)(void *, void *);
typedef int (*link_send_message_fn)(void *, void *, uint16_t);
typedef int (*msgreply_fn)(int, int, const void *, int);

enum {
    IAP2_MSG_SLOT_OFFSET = 0x000a9bd4, /* iap2_drvr slot; K2161: 0x000a7dac */
    IAP2_CLOSE_SLOT_OFFSET = 0x000a9be8, /* iap2_drvr slot; K2161: 0x000a7dc0 */
    IAP2_MSG_OFFSET = 0x0000f4c8, /* iap2_msg; K2161: 0x0000e484 */
    IAP2_CLOSE_OFFSET = 0x000107b4, /* iap2_ocb_close; K2161: 0x0000f760 */
    ADDPARAM_NODATA_OFFSET = 0x0001e13c, /* K2161: 0x0001cf50 */
    ADDPARAM_NUM_OFFSET = 0x0001e3fc, /* K2161: 0x0001d210 */
    SENDLIST_POP_OFFSET = 0x0001f198, /* K2161: 0x0001dfac */
    SENDLIST_PUSH_OFFSET = 0x0001ee00, /* K2161: 0x0001dc14 */
    LINK_SEND_MESSAGE_OFFSET = 0x0001165c /* link_send_message; K2161: 0x00010530 */
};

volatile unsigned int k2161_rgi_start_sent;

static iap2_msg_fn original_iap2_msg;
static iap2_close_fn original_iap2_close;
static iap2_msg_fn *iap2_msg_slot;
static iap2_close_fn *iap2_close_slot;
static packet_addparam_nodata_fn addparam_nodata;
static packet_addparam_num_fn addparam_num;
static packet_sendlist_pop_fn sendlist_pop;
static packet_sendlist_push_fn sendlist_push;
static link_send_message_fn send_message;
static msgreply_fn real_MsgReply;
struct pending_gps_reply { int active; int rcvid; void *ctx; unsigned int generation; };
static struct pending_gps_reply pending_replies[8];
static pthread_mutex_t pending_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int send_hook_installed;
static void *installed_driver_base;

enum {
    RGI_START_FAST_RETRY_MS = 2000,
    RGI_START_SLOW_RETRY_MS = 30000,
    RGI_START_FAST_ATTEMPTS = 5,
    RGI_STALE_REARM_MS = 10000
};

static pthread_mutex_t retry_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t send_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int start_5200_attempts;
static uint64_t last_5200_ms;
static uint64_t last_rgi_rx_ms;
static int got_520x;
static void *retry_ctx;
static int need_5200;
static int retry_enabled;
static int retry_thread_running;
static unsigned int retry_generation = 1;
static unsigned int soft_rearm_generation;

static uint64_t k2161_monotonic_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000ULL +
           (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void retry_tick_sleep(void)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100000000L;
    (void)nanosleep(&ts, NULL);
}

static void *retry_worker(void *arg)
{
    unsigned int my_generation = (unsigned int)(uintptr_t)arg;
    for (;;) {
        uint64_t now, retry_ms;
        const char *reason;
        unsigned int next_attempt = 0;
        int marked = 0;

        pthread_mutex_lock(&retry_lock);
        if (my_generation != retry_generation) {
            pthread_mutex_unlock(&retry_lock);
            return NULL;
        }
        if (!retry_enabled || retry_ctx == NULL) {
            retry_thread_running = 0;
            pthread_mutex_unlock(&retry_lock);
            return NULL;
        }
        if (got_520x) {
            retry_enabled = 0;
            need_5200 = 0;
            retry_ctx = NULL;
            retry_thread_running = 0;
            pthread_mutex_unlock(&retry_lock);
            return NULL;
        }
        now = k2161_monotonic_ms();
        retry_ms = start_5200_attempts < RGI_START_FAST_ATTEMPTS
                 ? RGI_START_FAST_RETRY_MS : RGI_START_SLOW_RETRY_MS;
        reason = start_5200_attempts < RGI_START_FAST_ATTEMPTS
               ? "fast_retry" : "slow_retry";
        if (!need_5200 && now != 0 && last_5200_ms != 0 &&
            now >= last_5200_ms && now - last_5200_ms >= retry_ms) {
            need_5200 = 1;
            next_attempt = start_5200_attempts + 1;
            marked = 1;
        }
        pthread_mutex_unlock(&retry_lock);
        if (marked)
            rgi_diag_log("5200", "retry_due next_attempt=%u reason=%s action=deferred generation=%u",
                         next_attempt, reason, my_generation);
        retry_tick_sleep();
    }
}


static int retry_needs_trigger(unsigned int *generation)
{
    int result;
    pthread_mutex_lock(&retry_lock);
    result = !got_520x && !retry_enabled && start_5200_attempts == 0;
    if (generation) *generation = retry_generation;
    pthread_mutex_unlock(&retry_lock);
    return result;
}

static int retry_soft_rearm_if_stale(uint64_t now, uint64_t *silence_ms,
                                     unsigned int *generation)
{
    int rearmed = 0;
    uint64_t silence = 0;
    unsigned int new_generation = 0;
    pthread_mutex_lock(&retry_lock);
    if (got_520x && !retry_enabled && last_rgi_rx_ms != 0 && now != 0 &&
        now >= last_rgi_rx_ms && now - last_rgi_rx_ms >= RGI_STALE_REARM_MS) {
        silence = now - last_rgi_rx_ms;
        ++retry_generation;
        if (retry_generation == 0) retry_generation = 1;
        new_generation = retry_generation;
        start_5200_attempts = 0;
        last_5200_ms = 0;
        last_rgi_rx_ms = 0;
        got_520x = 0;
        need_5200 = 0;
        retry_ctx = NULL;
        retry_enabled = 0;
        retry_thread_running = 0;
        soft_rearm_generation = new_generation;
        rearmed = 1;
    }
    pthread_mutex_unlock(&retry_lock);
    if (rearmed) {
        if (silence_ms) *silence_ms = silence;
        if (generation) *generation = new_generation;
    }
    return rearmed;
}

static int retry_is_soft_rearm_generation(void)
{
    int result;
    pthread_mutex_lock(&retry_lock);
    result = soft_rearm_generation != 0 && soft_rearm_generation == retry_generation;
    pthread_mutex_unlock(&retry_lock);
    return result;
}


static int retry_begin(void *ctx, unsigned int expected_generation)
{
    pthread_t tid;
    uint64_t now;
    int spawn_worker = 0;
    unsigned int worker_generation;
    if (ctx == NULL) return 0;
    if (!k2161_msgtable_ready()) {
        rgi_diag_log("5200", "5200_blocked reason=msgtable_not_ready stage=retry_begin");
        return 0;
    }
    pthread_mutex_lock(&retry_lock);
    if (expected_generation != retry_generation) {
        pthread_mutex_unlock(&retry_lock);
        return 0;
    }
    if (got_520x || retry_enabled || start_5200_attempts != 0) {
        pthread_mutex_unlock(&retry_lock);
        return 0;
    }
    now = k2161_monotonic_ms();
    retry_ctx = ctx;
    retry_enabled = 1;
    need_5200 = 0;
    start_5200_attempts = 1;
    last_5200_ms = now;
    worker_generation = retry_generation;
    if (!retry_thread_running) {
        retry_thread_running = 1;
        spawn_worker = 1;
    }
    pthread_mutex_unlock(&retry_lock);
    rgi_diag_log("5200", "retry_begin attempt=1 cadence=2s_x5_then_30s generation=%u",
                 worker_generation);
    if (spawn_worker) {
        if (pthread_create(&tid, NULL, retry_worker,
                           (void *)(uintptr_t)worker_generation) != 0) {
            pthread_mutex_lock(&retry_lock);
            if (retry_generation == worker_generation) retry_thread_running = 0;
            pthread_mutex_unlock(&retry_lock);
        } else {
            (void)pthread_detach(tid);
        }
    }
    return 1;
}


static int retry_claim_pending(void *ctx, unsigned int *attempt)
{
    int claimed = 0;

    pthread_mutex_lock(&retry_lock);

    if (retry_enabled &&
        !got_520x &&
        need_5200 &&
        retry_ctx == ctx) {

        need_5200 = 0;
        ++start_5200_attempts;
        last_5200_ms = k2161_monotonic_ms();

        if (attempt)
            *attempt = start_5200_attempts;

        claimed = 1;
    }

    pthread_mutex_unlock(&retry_lock);
    return claimed;
}

void k2161_send_note_rgi_rx(uint16_t msgid)
{
    int first = 0, soft_first = 0;
    uint64_t now;
    unsigned int generation = 0;
    if (msgid != 0x5201 && msgid != 0x5202 && msgid != 0x5204) return;
    now = k2161_monotonic_ms();
    pthread_mutex_lock(&retry_lock);
    if (now != 0) last_rgi_rx_ms = now;
    if (!got_520x) {
        got_520x = 1;
        need_5200 = 0;
        retry_enabled = 0;
        retry_ctx = NULL;
        last_5200_ms = now;
        generation = retry_generation;
        if (soft_rearm_generation != 0 && soft_rearm_generation == retry_generation) {
            soft_first = 1;
            soft_rearm_generation = 0;
        }
        first = 1;
    }
    pthread_mutex_unlock(&retry_lock);
    if (first && soft_first)
        rgi_diag_log("5200", "soft_rearm_first_rgi msg=%04x generation=%u retries_cancelled=1",
                     (unsigned)msgid, generation);
    else if (first)
        rgi_diag_log("5200", "first_rgi msg=%04x retries_cancelled=1", (unsigned)msgid);
    else
        rgi_diag_log("5200", "rgi_rx msg=%04x", (unsigned)msgid);
}


static unsigned int retry_reset(const char *reason)
{
    unsigned int generation;
    pthread_mutex_lock(&retry_lock);
    ++retry_generation;
    if (retry_generation == 0) retry_generation = 1;
    generation = retry_generation;
    start_5200_attempts = 0;
    last_5200_ms = 0;
    last_rgi_rx_ms = 0;
    got_520x = 0;
    need_5200 = 0;
    retry_ctx = NULL;
    retry_enabled = 0;
    retry_thread_running = 0;
    soft_rearm_generation = 0;
    pthread_mutex_unlock(&retry_lock);
    rgi_diag_log("5200", "retry_reset reason=%s generation=%u",
                 reason ? reason : "unknown", generation);
    return generation;
}


static void reset_rgi_for_identification(void *ctx)
{
    unsigned int i;
    unsigned int generation;

    /* Match the proven v9 session-boundary behavior: serialize the full
     * Identification reset against 0x5200 sends, while retaining the v1.1
     * diagnostics and inactive-slot rcvid=-1 cleanup. */
    pthread_mutex_lock(&send_lock);

    generation = retry_reset("start_identification");
    pthread_mutex_lock(&pending_lock);
    for (i = 0; i < 8; ++i) {
        pending_replies[i].active = 0;
        pending_replies[i].rcvid = -1;
        pending_replies[i].ctx = NULL;
        pending_replies[i].generation = 0;
    }
    pthread_mutex_unlock(&pending_lock);
    k2161_rgi_start_sent = 0;
    k2161_status_reset(ctx);
    (void)rgi_diag_session_bump("start_identification");

    pthread_mutex_unlock(&send_lock);

    rgi_diag_log("5200", "session_boundary msg=1d00 ctx=%p generation=%u action=rearm",
                 ctx, generation);
}

int k2161_send_start_route_guidance(void *ctx)
{
    void *pkt;
    int send_rc = -1;
    int allowed;
    int p0=-1, p1=-1, p2=-1, p3=-1;
    uint16_t component_id = 0x0010;
    unsigned int attempt;
    int after_first;

    if (!k2161_msgtable_ready()) {
        rgi_diag_log("5200",
                    "5200_blocked reason=msgtable_not_ready stage=send");
        return -1;
    }

    if (!ctx || !addparam_nodata || !addparam_num ||
        !sendlist_pop || !sendlist_push || !send_message)
        return -1;

    pthread_mutex_lock(&send_lock);

    pthread_mutex_lock(&retry_lock);
    allowed = retry_enabled && retry_ctx == ctx;
    attempt = start_5200_attempts;
    after_first = got_520x;
    pthread_mutex_unlock(&retry_lock);

    if (!allowed) {
        pthread_mutex_unlock(&send_lock);
        rgi_diag_log("5200", "send_blocked reason=retry_state ctx=%p", ctx);
        return -1;
    }

    k2161_status_send_attempt(ctx);
    pkt = sendlist_pop(ctx);
    if (!pkt) {
        k2161_status_send_result(ctx, 0);
        pthread_mutex_unlock(&send_lock);
        rgi_diag_log("5200", "attempt=%u result=FAIL reason=packet_pool_empty", attempt);
        return -1;
    }

    p0 = addparam_num(ctx, pkt, 0x0000, &component_id, sizeof(component_id));
    if (p0 == 0) p1 = addparam_nodata(ctx, pkt, 0x0001);
    if (p0 == 0 && p1 == 0) p2 = addparam_nodata(ctx, pkt, 0x0002);
    if (p0 == 0 && p1 == 0 && p2 == 0) p3 = addparam_nodata(ctx, pkt, 0x0003);

    if (p0 == 0 && p1 == 0 && p2 == 0 && p3 == 0)
        send_rc = send_message(ctx, pkt, 0x5200);

    if (send_rc == 0)
        k2161_rgi_start_sent = 1;

    k2161_status_send_result(ctx, send_rc == 0);
    (void)sendlist_push(ctx, pkt);

    rgi_diag_log("5200",
                "attempt=%u after_first_rgi=%d shape=component_qualified component=0010 tlv0=%d tlv1=%d tlv2=%d tlv3=%d send_rc=%d",
                attempt, after_first, p0, p1, p2, p3, send_rc);

    pthread_mutex_unlock(&send_lock);
    return send_rc;
}

int MsgReply(int rcvid, int status, const void *msg, int size)
{
    int rc, soft_rearm = 0;
    void *ctx = NULL;
    unsigned int generation = 0;
    unsigned int i;
    if (!real_MsgReply) real_MsgReply = (msgreply_fn)dlsym(RTLD_NEXT, "MsgReply");
    if (!real_MsgReply) return -1;
    rc = real_MsgReply(rcvid, status, msg, size);
    pthread_mutex_lock(&pending_lock);
    for (i = 0; i < 8; ++i) {
        if (pending_replies[i].active && pending_replies[i].rcvid == rcvid) {
            ctx = pending_replies[i].ctx;
            generation = pending_replies[i].generation;
            pending_replies[i].active = 0;
            pending_replies[i].generation = 0;
            break;
        }
    }
    pthread_mutex_unlock(&pending_lock);
    if (ctx) k2161_status_ffff_success(ctx, rcvid);
    if (ctx) {
        pthread_mutex_lock(&retry_lock);
        soft_rearm = soft_rearm_generation != 0 && soft_rearm_generation == generation;
        pthread_mutex_unlock(&retry_lock);
    }
    if (ctx && retry_begin(ctx, generation)) {
        rgi_diag_log("5200", soft_rearm
            ? "soft_rearm_5200 attempt=1 context=MsgReply rcvid=%d generation=%u"
            : "attempt=1 reason=initial_after_real_MsgReply rcvid=%d generation=%u",
            rcvid, generation);
        (void)k2161_send_start_route_guidance(ctx);
    }
    return rc;
}


static int wrapped_iap2_msg(void *ctx, void *ctp, void *msg,
                            void *handle, void *extra)
{
    uint16_t msgid = msg ? *(const uint16_t *)((const uint8_t *)msg + 6) : 0xffffU;
    int is_start_identification = msg && msgid == 0x1d00;
    int is_gps_info = msg && msgid == 0x2700;
    int rc;
    int rcvid = ctp ? *(const int *)ctp : -1;
    unsigned int i, trigger_generation = 0;
    int soft_rearmed = 0;
    uint64_t soft_silence_ms = 0;
    unsigned int soft_generation = 0;

    if (is_start_identification) reset_rgi_for_identification(ctx);
    if (is_gps_info && ctp) {
        uint64_t now = k2161_monotonic_ms();
        if (retry_soft_rearm_if_stale(now, &soft_silence_ms, &soft_generation)) {
            soft_rearmed = 1;
            rgi_diag_log("5200", "soft_rearm reason=rgi_silence silence_ms=%llu generation=%u",
                         (unsigned long long)soft_silence_ms, soft_generation);
        }
    }
    if (is_gps_info && ctp && retry_needs_trigger(&trigger_generation)) {
        int tracked = 0;
        pthread_mutex_lock(&pending_lock);
        for (i = 0; i < 8; ++i) {
            if (!pending_replies[i].active || pending_replies[i].rcvid == rcvid) {
                pending_replies[i].ctx = ctx;
                pending_replies[i].rcvid = rcvid;
                pending_replies[i].active = 1;
                pending_replies[i].generation = trigger_generation;
                tracked = 1;
                break;
            }
        }
        pthread_mutex_unlock(&pending_lock);
        k2161_status_cmd2700(ctx, rcvid, tracked);
        if (soft_rearmed && trigger_generation == soft_generation)
            rgi_diag_log("5200", "soft_rearm_2700 rcvid=%d tracked=%d generation=%u",
                         rcvid, tracked, trigger_generation);
        else
            rgi_diag_log("5200", "cmd2700 rcvid=%d tracked=%d generation=%u",
                         rcvid, tracked, trigger_generation);
    } else if (is_gps_info) {
        k2161_status_cmd2700(ctx, rcvid, 0);
    }

    rc = original_iap2_msg(ctx, ctp, msg, handle, extra);
    if (is_gps_info) {
        pthread_mutex_lock(&pending_lock);
        for (i = 0; i < 8; ++i)
            if (pending_replies[i].active && pending_replies[i].rcvid == rcvid) {
                pending_replies[i].active = 0;
                pending_replies[i].generation = 0;
            }
        pthread_mutex_unlock(&pending_lock);
        {
            unsigned int deferred_attempt = 0;
            if (retry_claim_pending(ctx, &deferred_attempt)) {
                rgi_diag_log("5200", retry_is_soft_rearm_generation()
                    ? "soft_rearm_5200 attempt=%u context=gps_iap2_msg_after_stock"
                    : "retry_claimed attempt=%u context=gps_iap2_msg_after_stock",
                    deferred_attempt);
                (void)k2161_send_start_route_guidance(ctx);
            }
        }
    }
    return rc;
}


static int wrapped_iap2_close(void *ctx, void *ctp, void *reserved, void *ocb)
{
    int rc;

    /*
     * iap2_ocb_close is a per-OCB cleanup callback, not a whole
     * iAP2/CarPlay session boundary. Keep send serialization here,
     * but do not destroy global RGI/retry state on an OCB close.
     */
    pthread_mutex_lock(&send_lock);
    rc = original_iap2_close(ctx, ctp, reserved, ocb);
    pthread_mutex_unlock(&send_lock);

    rgi_diag_log("OCB",
                "close_passthrough ctx=%p ocb=%p rc=%d",
                ctx, ocb, rc);

    return rc;
}

int k2161_send_hook_try_install_base(void *driver_base)
{
    uint8_t *base = (uint8_t *)driver_base;
    iap2_msg_fn expected_msg, current_msg;
    iap2_close_fn expected_close, current_close;

    if (base == NULL)
        return 0;

    if (!k2161_msgtable_ready()) {
        rgi_diag_log("5200",
                    "send_hook_install=FAIL reason=msgtable_not_ready");
        return -1;
    }

    if (send_hook_installed)
        return installed_driver_base == base ? 1 : -1;

    iap2_msg_slot = (iap2_msg_fn *)(base + IAP2_MSG_SLOT_OFFSET);
    iap2_close_slot = (iap2_close_fn *)(base + IAP2_CLOSE_SLOT_OFFSET);
    expected_msg = (iap2_msg_fn)(base + IAP2_MSG_OFFSET);
    expected_close = (iap2_close_fn)(base + IAP2_CLOSE_OFFSET);
    current_msg = *(volatile iap2_msg_fn *)iap2_msg_slot;
    current_close = *(volatile iap2_close_fn *)iap2_close_slot;

    if (current_msg == wrapped_iap2_msg && current_close == wrapped_iap2_close) {
        original_iap2_msg = expected_msg;
        original_iap2_close = expected_close;
        addparam_nodata = (packet_addparam_nodata_fn)(base + ADDPARAM_NODATA_OFFSET);
        addparam_num = (packet_addparam_num_fn)(base + ADDPARAM_NUM_OFFSET);
        sendlist_pop = (packet_sendlist_pop_fn)(base + SENDLIST_POP_OFFSET);
        sendlist_push = (packet_sendlist_push_fn)(base + SENDLIST_PUSH_OFFSET);
        send_message = (link_send_message_fn)(base + LINK_SEND_MESSAGE_OFFSET);
        installed_driver_base = base;
        send_hook_installed = 1;
        rgi_diag_log("5200", "send_hook_already_installed base=%p", base);
        return 1;
    }

    if (current_msg != expected_msg || current_close != expected_close) {
        rgi_diag_log("5200", "send_hook_install=FAIL reason=slot_mismatch base=%p", base);
        return -1;
    }

    addparam_nodata = (packet_addparam_nodata_fn)(base + ADDPARAM_NODATA_OFFSET);
    addparam_num = (packet_addparam_num_fn)(base + ADDPARAM_NUM_OFFSET);
    sendlist_pop = (packet_sendlist_pop_fn)(base + SENDLIST_POP_OFFSET);
    sendlist_push = (packet_sendlist_push_fn)(base + SENDLIST_PUSH_OFFSET);
    send_message = (link_send_message_fn)(base + LINK_SEND_MESSAGE_OFFSET);
    original_iap2_msg = expected_msg;
    original_iap2_close = expected_close;

    *(volatile iap2_msg_fn *)iap2_msg_slot = wrapped_iap2_msg;
    *(volatile iap2_close_fn *)iap2_close_slot = wrapped_iap2_close;

    if (*(volatile iap2_msg_fn *)iap2_msg_slot != wrapped_iap2_msg ||
        *(volatile iap2_close_fn *)iap2_close_slot != wrapped_iap2_close) {
        if (*(volatile iap2_msg_fn *)iap2_msg_slot == wrapped_iap2_msg)
            *(volatile iap2_msg_fn *)iap2_msg_slot = expected_msg;
        if (*(volatile iap2_close_fn *)iap2_close_slot == wrapped_iap2_close)
            *(volatile iap2_close_fn *)iap2_close_slot = expected_close;
        original_iap2_msg = NULL;
        original_iap2_close = NULL;
        addparam_nodata = NULL;
        addparam_num = NULL;
        sendlist_pop = NULL;
        sendlist_push = NULL;
        send_message = NULL;
        rgi_diag_log("5200", "send_hook_install=FAIL reason=write_verify");
        return -1;
    }

    installed_driver_base = base;
    send_hook_installed = 1;
    k2161_status_send_hooks_ready(1, 1);
    rgi_diag_log("5200", "send_hook_install=PASS base=%p shape=component_qualified", base);
    return 1;
}
