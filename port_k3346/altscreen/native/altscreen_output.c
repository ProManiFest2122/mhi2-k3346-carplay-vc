#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <process.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "altscreen.h"
#include "framework/logging.h"

#define ALTSCREEN_OUTPUT_MODULE "AltScreenOutput"
#define ALTSCREEN_MAX_SESSIONS 4
#define NVSS_OUTPUT_CONFIG_OFFSET 10U
#define DISPLAY_TARGET_MAIN 59U
#define DISPLAY_TARGET_CLUSTER 58U
#define NVSS_DIAG_BYTES 20U
#define NVSS_ATTR_BYTES 64U
#define SCREEN_MASTER_KEY_LEN 16U

#define ALTSCREEN_FWD_PORT 19830
#define ALTSCREEN_FWD_BUFSZ (384U * 1024U)
#define ALTSCREEN_FWD_RETRY_MS 250ULL
#define ALTSCREEN_FWD_SEND_TIMEOUT_US 150000
#define ALTSCREEN_FWD_SEND_STALL_MAX 20
#define ALTSCREEN_AV11_MAGIC 0x31315641U

#define ALTSCREEN_ACTIVE_MARKER "/tmp/mib2high-lvds-active"
#define ALTSCREEN_REACQUIRE_MARKER "/tmp/mib2high-lvds-reacquire"
#define ALTSCREEN_CONTROL_SCRIPT "/mnt/app/root/mib2high-lvds-control.sh"

typedef struct {
    void *context;
    void *get_synchronized_ntp_time;
    void *get_up_ticks_near_synchronized_ntp_time;
} altscreen_time_sync;

typedef int (*screen_start_fn)(void *screen_session, void *delegate_context);
typedef void (*screen_delete_fn)(void *screen_session);
typedef void (*screen_time_sync_fn)(void *screen_session,
                                    const altscreen_time_sync *sync);
typedef int (*nvss_video_open_fn)(void *out_handle, void *config);
typedef int (*nvss_video_stream_configure_fn)(void *handle, void *config);
typedef int (*nvss_video_get_attribs_fn)(void *handle, void *attribs);
typedef int (*nvss_video_set_attribs_fn)(void *handle, void *attribs);
typedef int (*nvss_video_decode_fn)(void *handle, void *decode_args, int flags);
typedef int (*nvss_video_close_fn)(void *handle);
typedef void (*derive_screen_aes_fn)(const void *master_key,
                                     size_t master_key_len,
                                     uint64_t connection_id,
                                     uint8_t out_key[16],
                                     uint8_t out_iv[16]);
typedef int (*aes_cbc_frame_init_fn)(void *ctx,
                                     const uint8_t key[16],
                                     const uint8_t iv[16],
                                     unsigned char encrypt);

static pthread_mutex_t g_alt_lock = PTHREAD_MUTEX_INITIALIZER;
static void *g_alt_sessions[ALTSCREEN_MAX_SESSIONS];
static pthread_t g_patch_owner;
static int g_patch_owner_valid;
static unsigned int g_patch_depth;
static unsigned int g_output_patch_count;
static void *g_main_delegate_context;
static altscreen_time_sync g_main_time_sync;
static int g_main_time_sync_valid;
static uint8_t g_screen_master_key[SCREEN_MASTER_KEY_LEN];
static int g_screen_master_key_valid;

static screen_start_fn g_real_screen_start;
static screen_delete_fn g_real_screen_delete;
static screen_time_sync_fn g_real_time_sync;
static nvss_video_open_fn g_real_nvss_open;
static nvss_video_stream_configure_fn g_real_nvss_stream_configure;
static nvss_video_get_attribs_fn g_real_nvss_get_attribs;
static nvss_video_set_attribs_fn g_real_nvss_set_attribs;
static nvss_video_decode_fn g_real_nvss_decode;
static nvss_video_close_fn g_real_nvss_close;
static derive_screen_aes_fn g_real_derive_screen_aes;
static aes_cbc_frame_init_fn g_real_aes_cbc_frame_init;
static uintptr_t g_security_info_fn;

static pthread_mutex_t g_fwd_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_stub_token;
static int g_stub_active;
static int g_fwd_fd = -1;
static unsigned long long g_fwd_retry_ms;
static unsigned int g_fwd_sent;
static unsigned int g_fwd_dropped;
static uint8_t g_fwd_buf[ALTSCREEN_FWD_BUFSZ];
static uint8_t g_fwd_parameter_sets[512];
static uint32_t g_fwd_parameter_sets_len;
static int g_fwd_need_idr;
static unsigned long long g_fwd_last_keyframe_request_ms;
static uint32_t g_stub_width;
static uint32_t g_stub_height;
static float g_stub_fps;

static void resolve_symbols(void)
{
    if (g_real_screen_start == NULL) {
        g_real_screen_start = (screen_start_fn)dlsym(
            RTLD_NEXT, "AirPlayReceiverSessionScreen_StartSession");
    }
    if (g_real_screen_delete == NULL) {
        g_real_screen_delete = (screen_delete_fn)dlsym(
            RTLD_NEXT, "AirPlayReceiverSessionScreen_Delete");
    }
    if (g_real_time_sync == NULL) {
        g_real_time_sync = (screen_time_sync_fn)dlsym(
            RTLD_NEXT, "AirPlayReceiverSessionScreen_SetTimeSynchronizer");
    }
    if (g_real_nvss_open == NULL) {
        g_real_nvss_open = (nvss_video_open_fn)dlsym(
            RTLD_NEXT, "NvSSVideoOpen");
    }
    if (g_real_nvss_stream_configure == NULL) {
        g_real_nvss_stream_configure = (nvss_video_stream_configure_fn)dlsym(
            RTLD_NEXT, "NvSSVideoStreamConfigure");
    }
    if (g_real_nvss_get_attribs == NULL) {
        g_real_nvss_get_attribs = (nvss_video_get_attribs_fn)dlsym(
            RTLD_NEXT, "NvSSVideoGetAttribs");
    }
    if (g_real_nvss_set_attribs == NULL) {
        g_real_nvss_set_attribs = (nvss_video_set_attribs_fn)dlsym(
            RTLD_NEXT, "NvSSVideoSetAttribs");
    }
    if (g_real_nvss_decode == NULL) {
        g_real_nvss_decode = (nvss_video_decode_fn)dlsym(
            RTLD_NEXT, "NvSSVideoDecode");
    }
    if (g_real_nvss_close == NULL) {
        g_real_nvss_close = (nvss_video_close_fn)dlsym(
            RTLD_NEXT, "NvSSVideoClose");
    }
    if (g_real_derive_screen_aes == NULL) {
        g_real_derive_screen_aes = (derive_screen_aes_fn)dlsym(
            RTLD_NEXT, "AirPlay_DeriveAESKeySHA512ForScreen");
    }
    if (g_real_aes_cbc_frame_init == NULL) {
        g_real_aes_cbc_frame_init = (aes_cbc_frame_init_fn)dlsym(
            RTLD_NEXT, "AES_CBCFrame_Init");
    }
    if (g_security_info_fn == 0) {
        g_security_info_fn = (uintptr_t)dlsym(
            RTLD_NEXT, "AirPlayReceiverSessionSetSecurityInfo");
    }
}

static int session_index_locked(void *screen_session)
{
    int i;

    for (i = 0; i < ALTSCREEN_MAX_SESSIONS; ++i) {
        if (g_alt_sessions[i] == screen_session)
            return i;
    }
    return -1;
}

int altscreen_register_session(void *screen_session)
{
    int i;

    if (screen_session == NULL)
        return 0;

    pthread_mutex_lock(&g_alt_lock);

    if (session_index_locked(screen_session) >= 0) {
        pthread_mutex_unlock(&g_alt_lock);
        return 1;
    }

    for (i = 0; i < ALTSCREEN_MAX_SESSIONS; ++i) {
        if (g_alt_sessions[i] == NULL) {
            g_alt_sessions[i] = screen_session;
            pthread_mutex_unlock(&g_alt_lock);
            LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                     "session register screen=%p slot=%d primary_untouched=1",
                     screen_session, i);
            return 1;
        }
    }

    pthread_mutex_unlock(&g_alt_lock);
    LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
              "session register refused screen=%p reason=table_full primary_untouched=1",
              screen_session);
    return 0;
}

void altscreen_unregister_session(void *screen_session)
{
    int i;

    if (screen_session == NULL)
        return;

    pthread_mutex_lock(&g_alt_lock);
    i = session_index_locked(screen_session);
    if (i >= 0)
        g_alt_sessions[i] = NULL;
    pthread_mutex_unlock(&g_alt_lock);

    if (i >= 0) {
        LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                 "session unregister screen=%p slot=%d",
                 screen_session, i);
    }
}

int altscreen_is_session(void *screen_session)
{
    int found;

    if (screen_session == NULL)
        return 0;

    pthread_mutex_lock(&g_alt_lock);
    found = session_index_locked(screen_session) >= 0;
    pthread_mutex_unlock(&g_alt_lock);
    return found;
}

static int begin_output_scope(void)
{
    pthread_t self = pthread_self();
    int ok = 0;

    pthread_mutex_lock(&g_alt_lock);

    if (!g_patch_owner_valid) {
        g_patch_owner = self;
        g_patch_owner_valid = 1;
        g_patch_depth = 1;
        ok = 1;
    } else if (pthread_equal(g_patch_owner, self)) {
        ++g_patch_depth;
        ok = 1;
    }

    pthread_mutex_unlock(&g_alt_lock);

    if (!ok) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "output scope refused reason=concurrent_owner primary_untouched=1");
    }
    return ok;
}

static void end_output_scope(void)
{
    pthread_t self = pthread_self();

    pthread_mutex_lock(&g_alt_lock);

    if (g_patch_owner_valid && pthread_equal(g_patch_owner, self)) {
        if (g_patch_depth > 1) {
            --g_patch_depth;
        } else {
            g_patch_depth = 0;
            g_patch_owner_valid = 0;
        }
    }

    pthread_mutex_unlock(&g_alt_lock);
}

static int output_scope_active_for_current_thread(void)
{
    pthread_t self = pthread_self();
    int active;

    pthread_mutex_lock(&g_alt_lock);
    active = g_patch_owner_valid &&
             g_patch_depth != 0 &&
             pthread_equal(g_patch_owner, self);
    pthread_mutex_unlock(&g_alt_lock);
    return active;
}

int altscreen_session_scope_active(void)
{
    return output_scope_active_for_current_thread();
}

void *altscreen_main_delegate_context(void)
{
    void *ctx;

    pthread_mutex_lock(&g_alt_lock);
    ctx = g_main_delegate_context;
    pthread_mutex_unlock(&g_alt_lock);
    return ctx;
}

int altscreen_time_sync_ready(void)
{
    int ready;

    resolve_symbols();
    pthread_mutex_lock(&g_alt_lock);
    ready = g_main_time_sync_valid && g_real_time_sync != NULL;
    pthread_mutex_unlock(&g_alt_lock);
    return ready;
}

int altscreen_apply_main_time_sync(void *screen_session)
{
    altscreen_time_sync sync;
    int ready;

    if (screen_session == NULL)
        return 0;
    resolve_symbols();

    pthread_mutex_lock(&g_alt_lock);
    ready = g_main_time_sync_valid && g_real_time_sync != NULL;
    if (ready)
        sync = g_main_time_sync;
    pthread_mutex_unlock(&g_alt_lock);

    if (!ready) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "time sync refused reason=main_sync_not_captured");
        return 0;
    }

    g_real_time_sync(screen_session, &sync);
    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "time sync applied screen=%p context=%p",
             screen_session, sync.context);
    return 1;
}

int altscreen_aes_ready(void)
{
    int ready;

    resolve_symbols();
    pthread_mutex_lock(&g_alt_lock);
    ready = g_screen_master_key_valid && g_real_derive_screen_aes != NULL;
    pthread_mutex_unlock(&g_alt_lock);
    return ready;
}

int altscreen_derive_aes(uint64_t stream_connection_id,
                         uint8_t out_key[16],
                         uint8_t out_iv[16])
{
    uint8_t master[SCREEN_MASTER_KEY_LEN];
    int ready;

    if (stream_connection_id == 0 || out_key == NULL || out_iv == NULL)
        return 0;

    resolve_symbols();

    pthread_mutex_lock(&g_alt_lock);
    ready = g_screen_master_key_valid && g_real_derive_screen_aes != NULL;
    if (ready)
        memcpy(master, g_screen_master_key, sizeof(master));
    pthread_mutex_unlock(&g_alt_lock);

    if (!ready) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "AES derive refused conn=%llu reason=master_key_not_captured",
                  (unsigned long long)stream_connection_id);
        return 0;
    }

    g_real_derive_screen_aes(master, sizeof(master), stream_connection_id,
                             out_key, out_iv);
    memset(master, 0, sizeof(master));

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "AES derived conn=%llu source=stock_screen_master",
             (unsigned long long)stream_connection_id);
    return 1;
}

int altscreen_output_ready(void)
{
    resolve_symbols();
    return g_real_screen_start != NULL && g_real_nvss_open != NULL;
}

unsigned int altscreen_output_patch_count(void)
{
    unsigned int count;

    pthread_mutex_lock(&g_alt_lock);
    count = g_output_patch_count;
    pthread_mutex_unlock(&g_alt_lock);
    return count;
}

int altscreen_start_session(void *screen_session, void *delegate_context)
{
    int rc;

    resolve_symbols();

    if (g_real_screen_start == NULL) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "session start refused reason=stock_start_missing");
        return -1;
    }
    if (!altscreen_is_session(screen_session)) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "session start refused screen=%p reason=not_registered",
                  screen_session);
        return -1;
    }
    if (!altscreen_apply_main_time_sync(screen_session)) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "session start refused screen=%p reason=time_sync_not_ready",
                  screen_session);
        return -1;
    }
    if (!begin_output_scope())
        return -1;

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "session start screen=%p output_scope=1 delegate=%p",
             screen_session, delegate_context);

    rc = g_real_screen_start(screen_session, delegate_context);
    end_output_scope();

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "session start return screen=%p rc=%d patches=%u",
             screen_session, rc, altscreen_output_patch_count());
    return rc;
}

int AirPlayReceiverSessionScreen_StartSession(void *screen_session,
                                               void *delegate_context)
{
    int rc;
    int alt_session;

    resolve_symbols();
    if (g_real_screen_start == NULL)
        return -1;

    alt_session = altscreen_is_session(screen_session);
    if (!alt_session) {
        pthread_mutex_lock(&g_alt_lock);
        g_main_delegate_context = delegate_context;
        pthread_mutex_unlock(&g_alt_lock);
        LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                 "primary delegate captured context=%p",
                 delegate_context);
        return g_real_screen_start(screen_session, delegate_context);
    }

    if (!begin_output_scope())
        return -1;
    rc = g_real_screen_start(screen_session, delegate_context);
    end_output_scope();
    return rc;
}

void AirPlayReceiverSessionScreen_SetTimeSynchronizer(
    void *screen_session,
    const altscreen_time_sync *sync)
{
    int alt_session;

    resolve_symbols();
    if (g_real_time_sync == NULL)
        return;

    alt_session = altscreen_is_session(screen_session);
    if (!alt_session && sync != NULL) {
        pthread_mutex_lock(&g_alt_lock);
        g_main_time_sync = *sync;
        g_main_time_sync_valid = 1;
        pthread_mutex_unlock(&g_alt_lock);
        LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                 "primary time sync captured screen=%p context=%p",
                 screen_session, sync->context);
    }

    g_real_time_sync(screen_session, sync);
}

void AirPlayReceiverSessionScreen_Delete(void *screen_session)
{
    resolve_symbols();
    altscreen_unregister_session(screen_session);
    if (g_real_screen_delete != NULL)
        g_real_screen_delete(screen_session);
}

/*
 * K2161 early master-key capture. AirPlayReceiverSessionSetSecurityInfo()
 * calls AES_CBCFrame_Init through the PLT before Stream 111 SETUP can arrive.
 * Capture only that exact stock caller; all unrelated CBC users pass through.
 */
__attribute__((noinline))
int AES_CBCFrame_Init(void *ctx,
                      const uint8_t key[16],
                      const uint8_t iv[16],
                      unsigned char encrypt)
{
    uintptr_t ra = (uintptr_t)__builtin_return_address(0);
    int rc;

    resolve_symbols();
    if (g_real_aes_cbc_frame_init == NULL)
        return -1;

    rc = g_real_aes_cbc_frame_init(ctx, key, iv, encrypt);

    if (rc == 0 && !encrypt && key != NULL && g_security_info_fn != 0 &&
        ra >= g_security_info_fn && ra < g_security_info_fn + 0x80U) {
        pthread_mutex_lock(&g_alt_lock);
        memcpy(g_screen_master_key, key, SCREEN_MASTER_KEY_LEN);
        g_screen_master_key_valid = 1;
        pthread_mutex_unlock(&g_alt_lock);
        LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                 "K2161 early screen AES master captured source=SetSecurityInfo ra_off=0x%x",
                 (unsigned int)(ra - g_security_info_fn));
    }

    return rc;
}

void AirPlay_DeriveAESKeySHA512ForScreen(const void *master_key,
                                         size_t master_key_len,
                                         uint64_t connection_id,
                                         uint8_t out_key[16],
                                         uint8_t out_iv[16])
{
    resolve_symbols();

    if (master_key != NULL && master_key_len == SCREEN_MASTER_KEY_LEN) {
        pthread_mutex_lock(&g_alt_lock);
        memcpy(g_screen_master_key, master_key, SCREEN_MASTER_KEY_LEN);
        g_screen_master_key_valid = 1;
        pthread_mutex_unlock(&g_alt_lock);
        LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                 "primary screen AES master captured conn=%llu len=%u",
                 (unsigned long long)connection_id,
                 (unsigned int)master_key_len);
    }

    if (g_real_derive_screen_aes != NULL) {
        g_real_derive_screen_aes(master_key, master_key_len, connection_id,
                                 out_key, out_iv);
    }
}

static int is_stub_handle(void *handle)
{
    return handle == (void *)&g_stub_token;
}

static unsigned long long fwd_now_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (unsigned long long)ts.tv_sec * 1000ULL +
           (unsigned long long)ts.tv_nsec / 1000000ULL;
}

static void fwd_close_locked(void)
{
    if (g_fwd_fd >= 0) {
        close(g_fwd_fd);
        g_fwd_fd = -1;
        LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                 "renderer connection closed sent=%u dropped=%u",
                 g_fwd_sent, g_fwd_dropped);
    }
}

static void fwd_reset_locked(void)
{
    fwd_close_locked();
    g_fwd_retry_ms = 0;
    g_fwd_sent = 0;
    g_fwd_dropped = 0;
    g_fwd_need_idr = 0;
    g_fwd_last_keyframe_request_ms = 0;
    g_fwd_parameter_sets_len = 0;
    g_stub_width = 0;
    g_stub_height = 0;
    g_stub_fps = 0.0f;
}

static int fwd_connect_locked(void)
{
    struct sockaddr_in addr;
    struct timeval timeout;
    unsigned long long now;
    int fd;
    int send_buf;
    int no_delay;

    if (g_fwd_fd >= 0)
        return 1;

    now = fwd_now_ms();
    if (now != 0 && now < g_fwd_retry_ms)
        return 0;
    if (now != 0)
        g_fwd_retry_ms = now + ALTSCREEN_FWD_RETRY_MS;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return 0;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ALTSCREEN_FWD_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    timeout.tv_sec = 0;
    timeout.tv_usec = ALTSCREEN_FWD_SEND_TIMEOUT_US;
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
                     &timeout, sizeof(timeout));

    send_buf = 256 * 1024;
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                     &send_buf, sizeof(send_buf));

    no_delay = 1;
    (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                     &no_delay, sizeof(no_delay));

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return 0;
    }

    g_fwd_fd = fd;
    g_fwd_need_idr = 1;
    g_fwd_last_keyframe_request_ms = 0;
    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "renderer connected 127.0.0.1:%u",
             (unsigned int)ALTSCREEN_FWD_PORT);
    return 1;
}

static int fwd_write_locked(const uint8_t *data,
                            uint32_t len,
                            uint64_t timestamp,
                            uint32_t flag)
{
    uint32_t header[5];
    size_t total;
    size_t offset = 0;
    int stalls = 0;

    if (g_fwd_fd < 0 || data == NULL || len == 0)
        return 0;

    total = sizeof(header) + (size_t)len;
    if (total > sizeof(g_fwd_buf)) {
        ++g_fwd_dropped;
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "renderer frame dropped reason=oversize len=%u max=%u",
                  (unsigned int)len,
                  (unsigned int)(sizeof(g_fwd_buf) - sizeof(header)));
        return 0;
    }

    header[0] = ALTSCREEN_AV11_MAGIC;
    header[1] = len;
    header[2] = (uint32_t)(timestamp & 0xffffffffULL);
    header[3] = (uint32_t)(timestamp >> 32);
    header[4] = flag;

    memcpy(g_fwd_buf, header, sizeof(header));
    memcpy(g_fwd_buf + sizeof(header), data, len);

    while (offset < total) {
        ssize_t written = send(g_fwd_fd,
                               g_fwd_buf + offset,
                               total - offset,
                               0);
        if (written > 0) {
            offset += (size_t)written;
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        if (written < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) &&
            ++stalls <= ALTSCREEN_FWD_SEND_STALL_MAX) {
            continue;
        }

        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "renderer send failed errno=%d sent=%u/%u stalls=%d",
                  errno, (unsigned int)offset, (unsigned int)total, stalls);
        fwd_close_locked();
        ++g_fwd_dropped;
        return 0;
    }

    ++g_fwd_sent;
    return 1;
}

static void fwd_scan_parameter_sets(const uint8_t *data,
                                    uint32_t len,
                                    int *has_idr,
                                    int *has_parameter_sets)
{
    uint32_t starts[64];
    unsigned int types[64];
    uint32_t i = 0;
    unsigned int count = 0;
    unsigned int j;
    unsigned int end_index;

    *has_idr = 0;
    *has_parameter_sets = 0;

    while (i + 4 <= len && count < 64) {
        uint32_t start_code = 0;

        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
            start_code = 3;
        else if (i + 5 <= len && data[i] == 0 && data[i + 1] == 0 &&
                 data[i + 2] == 0 && data[i + 3] == 1)
            start_code = 4;

        if (start_code != 0 && i + start_code < len) {
            starts[count] = i;
            types[count] = data[i + start_code] & 0x1fU;
            ++count;
            i += start_code;
        } else {
            ++i;
        }
    }

    for (j = 0; j < count; ++j) {
        if (types[j] == 5)
            *has_idr = 1;
        if (types[j] == 7 || types[j] == 8)
            *has_parameter_sets = 1;
    }

    for (j = 0; j < count && types[j] != 7; ++j)
        ;

    if (j < count) {
        int have_pps = 0;
        end_index = j;
        while (end_index < count &&
               (types[end_index] == 7 || types[end_index] == 8)) {
            if (types[end_index] == 8)
                have_pps = 1;
            ++end_index;
        }

        if (have_pps) {
            uint32_t end = (end_index < count) ? starts[end_index] : len;
            uint32_t ps_len = end - starts[j];

            if (ps_len > 0 && ps_len <= sizeof(g_fwd_parameter_sets)) {
                memcpy(g_fwd_parameter_sets, data + starts[j], ps_len);
                g_fwd_parameter_sets_len = ps_len;
            }
        }
    }
}

static void fwd_send_access_unit(const uint8_t *data,
                                 uint32_t len,
                                 uint64_t timestamp,
                                 uint32_t flag)
{
    int has_idr = 0;
    int has_parameter_sets = 0;
    int fresh_connection;
    int request_keyframe = 0;
    unsigned long long now;

    if (data == NULL || len == 0)
        return;

    pthread_mutex_lock(&g_fwd_lock);

    /* Always inspect/cache codec parameter sets even before vc_render is up.
     * In the car, Stream 111 begins ~1.5 s before the renderer connects. */
    fwd_scan_parameter_sets(data, len, &has_idr, &has_parameter_sets);

    fresh_connection = g_fwd_fd < 0;
    if (!fwd_connect_locked()) {
        ++g_fwd_dropped;
        pthread_mutex_unlock(&g_fwd_lock);
        return;
    }

    if (fresh_connection && !has_parameter_sets &&
        g_fwd_parameter_sets_len != 0) {
        (void)fwd_write_locked(g_fwd_parameter_sets,
                               g_fwd_parameter_sets_len,
                               timestamp, 0);
        if (g_fwd_fd < 0) {
            pthread_mutex_unlock(&g_fwd_lock);
            return;
        }
        LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                 "renderer replayed cached SPS/PPS bytes=%u",
                 (unsigned int)g_fwd_parameter_sets_len);
    }

    if (g_fwd_need_idr) {
        if (has_idr) {
            g_fwd_need_idr = 0;
            g_fwd_last_keyframe_request_ms = 0;
            LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                     "renderer IDR acquired after reconnect");
        } else {
            now = fwd_now_ms();
            if (g_fwd_last_keyframe_request_ms == 0 ||
                now == 0 ||
                now - g_fwd_last_keyframe_request_ms >= 750ULL) {
                g_fwd_last_keyframe_request_ms = now != 0 ? now : 1;
                request_keyframe = 1;
            }

            /* SPS/PPS may be sent while waiting so the renderer is primed,
             * but predictive frames are discarded until a fresh IDR lands. */
            if (!has_parameter_sets) {
                ++g_fwd_dropped;
                pthread_mutex_unlock(&g_fwd_lock);
                if (request_keyframe)
                    (void)altscreen_request_fresh_keyframe();
                return;
            }
        }
    }

    (void)fwd_write_locked(data, len, timestamp, flag);
    pthread_mutex_unlock(&g_fwd_lock);

    if (request_keyframe)
        (void)altscreen_request_fresh_keyframe();
}

static void set_marker(const char *path, int present)
{
    int fd;

    if (path == NULL)
        return;
    if (!present) {
        (void)unlink(path);
        return;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0)
        (void)close(fd);
}

__attribute__((constructor))
static void altscreen_output_ctor(void)
{
    /* Never inherit a stale display ownership marker across dio_manager starts. */
    set_marker(ALTSCREEN_ACTIVE_MARKER, 0);
    set_marker(ALTSCREEN_REACQUIRE_MARKER, 0);
}

static int spawn_control(void)
{
    const char *script = ALTSCREEN_CONTROL_SCRIPT;
    char *argv[2];
    static char *envp[] = {
        (char *)"PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/bin:/mnt/app/armle/usr/sbin",
        (char *)"IPL_CONFIG_DIR=/etc/eso/production",
        NULL
    };
    unsigned char was[1024];
    int fd;
    int flags;
    int pid;

    if (access(script, X_OK) != 0) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "control spawn refused script=%s errno=%d",
                  script, errno);
        return 0;
    }

    memset(was, 0, sizeof(was));
    for (fd = 3; fd < (int)sizeof(was); ++fd) {
        flags = fcntl(fd, F_GETFD);
        if (flags < 0) {
            was[fd] = 0;
            continue;
        }
        if (flags & FD_CLOEXEC) {
            was[fd] = 2;
            continue;
        }
        was[fd] = 1;
        (void)fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }

    argv[0] = (char *)script;
    argv[1] = NULL;
    pid = spawnve(P_NOWAITO, script, argv, envp);

    for (fd = 3; fd < (int)sizeof(was); ++fd) {
        if (was[fd] != 1)
            continue;
        flags = fcntl(fd, F_GETFD);
        if (flags >= 0)
            (void)fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
    }

    if (pid < 0) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "control spawn failed script=%s errno=%d",
                  script, errno);
        return 0;
    }

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "control spawned pid=%d script=%s", pid, script);
    return 1;
}

int NvSSVideoOpen(void *out_handle, void *config)
{
    uint8_t *cfg = (uint8_t *)config;

    resolve_symbols();
    if (g_real_nvss_open == NULL)
        return -1;

    if (!output_scope_active_for_current_thread() || cfg == NULL)
        return g_real_nvss_open(out_handle, config);

    LOG_HEXDUMP(ALTSCREEN_OUTPUT_MODULE, "NvSS open stock config",
                cfg, NVSS_DIAG_BYTES);

    if (cfg[NVSS_OUTPUT_CONFIG_OFFSET] != DISPLAY_TARGET_MAIN) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "stub open refused offset=0x%x expected=%u actual=%u primary_untouched=1",
                  (unsigned int)NVSS_OUTPUT_CONFIG_OFFSET,
                  (unsigned int)DISPLAY_TARGET_MAIN,
                  (unsigned int)cfg[NVSS_OUTPUT_CONFIG_OFFSET]);
        return g_real_nvss_open(out_handle, config);
    }

    if (out_handle == NULL)
        return -1;

    pthread_mutex_lock(&g_fwd_lock);
    if (g_stub_active) {
        pthread_mutex_unlock(&g_fwd_lock);
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "stub open refused reason=already_active primary_untouched=1");
        return -1;
    }

    fwd_reset_locked();
    g_stub_active = 1;
    *(void **)out_handle = (void *)&g_stub_token;
    pthread_mutex_unlock(&g_fwd_lock);

    pthread_mutex_lock(&g_alt_lock);
    ++g_output_patch_count;
    pthread_mutex_unlock(&g_alt_lock);

    /* The exact working car build only starts the external renderer/control
     * after the AltScreen Screen B reaches this NvSS open boundary. */
    set_marker(ALTSCREEN_REACQUIRE_MARKER, 0);
    set_marker(ALTSCREEN_ACTIVE_MARKER, 1);
    if (!spawn_control())
        set_marker(ALTSCREEN_ACTIVE_MARKER, 0);

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "NvSS stub open handle=%p stock_layer=%u forward=127.0.0.1:%u no_nvss_in_dio_manager=1 patches=%u",
             (void *)&g_stub_token,
             (unsigned int)cfg[NVSS_OUTPUT_CONFIG_OFFSET],
             (unsigned int)ALTSCREEN_FWD_PORT,
             altscreen_output_patch_count());
    return 0;
}

int NvSSVideoStreamConfigure(void *handle, void *config)
{
    uint8_t *cfg = (uint8_t *)config;
    uint32_t width = 0;
    uint32_t height = 0;
    float fps = 0.0f;

    resolve_symbols();
    if (!is_stub_handle(handle)) {
        if (g_real_nvss_stream_configure == NULL)
            return -1;
        return g_real_nvss_stream_configure(handle, config);
    }

    if (cfg == NULL)
        return -1;

    memcpy(&width, cfg + 0, sizeof(width));
    memcpy(&height, cfg + 4, sizeof(height));
    memcpy(&fps, cfg + 8, sizeof(fps));

    pthread_mutex_lock(&g_fwd_lock);
    g_stub_width = width;
    g_stub_height = height;
    g_stub_fps = fps;
    pthread_mutex_unlock(&g_fwd_lock);

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "NvSS stub configure width=%u height=%u fps=%d.%03d",
             (unsigned int)width,
             (unsigned int)height,
             (int)fps,
             (int)((fps - (float)((int)fps)) * 1000.0f));
    return 0;
}

int NvSSVideoGetAttribs(void *handle, void *attribs)
{
    resolve_symbols();
    if (!is_stub_handle(handle)) {
        if (g_real_nvss_get_attribs == NULL)
            return -1;
        return g_real_nvss_get_attribs(handle, attribs);
    }

    if (attribs == NULL)
        return -1;

    memset(attribs, 0, NVSS_ATTR_BYTES);
    return 0;
}

int NvSSVideoSetAttribs(void *handle, void *attribs)
{
    resolve_symbols();
    if (!is_stub_handle(handle)) {
        if (g_real_nvss_set_attribs == NULL)
            return -1;
        return g_real_nvss_set_attribs(handle, attribs);
    }

    (void)attribs;
    return 0;
}

int NvSSVideoDecode(void *handle, void *decode_args, int flags)
{
    uint8_t *args = (uint8_t *)decode_args;
    const uint8_t *data = NULL;
    uint32_t len = 0;
    uint64_t timestamp = 0;
    uint8_t frame_flag = 0;

    resolve_symbols();
    if (!is_stub_handle(handle)) {
        if (g_real_nvss_decode == NULL)
            return -1;
        return g_real_nvss_decode(handle, decode_args, flags);
    }

    if (args == NULL)
        return -1;

    memcpy(&data, args + 0, sizeof(data));
    memcpy(&len, args + 4, sizeof(len));
    memcpy(&timestamp, args + 8, sizeof(timestamp));
    memcpy(&frame_flag, args + 0x10, sizeof(frame_flag));

    fwd_send_access_unit(data, len, timestamp, (uint32_t)frame_flag);

    /* Forwarding is deliberately fail-soft: renderer availability must never
     * fail the CarPlay ScreenStream itself. */
    return 0;
}

int NvSSVideoClose(void *handle)
{
    resolve_symbols();
    if (!is_stub_handle(handle)) {
        if (g_real_nvss_close == NULL)
            return -1;
        return g_real_nvss_close(handle);
    }

    pthread_mutex_lock(&g_fwd_lock);
    fwd_close_locked();
    g_stub_active = 0;
    g_fwd_need_idr = 0;
    g_fwd_last_keyframe_request_ms = 0;
    pthread_mutex_unlock(&g_fwd_lock);

    set_marker(ALTSCREEN_ACTIVE_MARKER, 0);
    set_marker(ALTSCREEN_REACQUIRE_MARKER, 0);

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "NvSS stub close handle=%p no_real_nvss_close=1 lifecycle_stop=1",
             handle);
    return 0;
}
