/* K3346 port (MHI2_ER_AUG22_K3346 / MU1438): stock ipod-drvr-iap2.so
 * SHA-256 0ac13f43d3fb259eee328c272fa1211bb558b6e2cd97c6e38ef38e4e71469d51.
 * Offsets re-derived from the driver's own .symtab/.dynsym, relocations and
 * table contents (tools/derive_k3346_iap2_offsets.py); K2161 values in comments.
 * Runtime checks below remain fail-closed. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/uio.h>
#include <unistd.h>

#include "receive_probe.h"
#include "ident_hook.h"
#include "send_hook.h"
#include "message_table.h"
#include "rgi_bridge.h"
#include "diagnostics.h"

typedef void *(*real_dlopen_fn)(const char *, int);
typedef void *(*real_memcpy_fn)(void *, const void *, size_t);

enum {
    K2161_RAW_HEADER_LEN = 16,
    K2161_MAX_TRANSPORT_LEN = 65535
};

#define K2161_RAW_CAPTURE_LIMIT ((size_t)16U * 1024U * 1024U)

/* K3346 Etap 4: overridable so the mm-ipod-only RGI build (build_rgi_ipod.sh)
 * does not truncate the dio_manager copy's files. Defaults unchanged. */
#ifndef K2161_RX_BIN_PATH
#define K2161_RX_BIN_PATH "/dev/shmem/k2161-rgi-rx.bin"
#endif
#ifndef K2161_RX_LOG_PATH
#define K2161_RX_LOG_PATH "/dev/shmem/k2161-rgi-rx.log"
#endif

static real_dlopen_fn real_dlopen_ptr;
static real_memcpy_fn real_memcpy_ptr;
static int resolving_memcpy;
static uintptr_t driver_base;
static int raw_fd = -1;
static int text_fd = -1;
static int callsite_reported;
static int ident_install_attempted;
static int send_install_attempted;
static int capture_limit_reported;
static int capture_error_reported;
static int bridge_ok_reported;
static int bridge_error_reported;
static size_t raw_bytes_written;
static uint32_t raw_record_sequence;

volatile unsigned int k2161_rx_callsite_count;
volatile unsigned int k2161_rx_rgi_count;
volatile unsigned int k2161_rx_partial_rgi_count;
volatile unsigned int k2161_rx_5201;
volatile unsigned int k2161_rx_5202;
volatile unsigned int k2161_rx_5204;
volatile unsigned int k2161_rx_bridge_ok;
volatile unsigned int k2161_rx_bridge_fail;

static void *fallback_memcpy(void *dst, const void *src, size_t n)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    const volatile uint8_t *s = (const volatile uint8_t *)src;
    size_t i;
    for (i = 0; i < n; ++i)
        d[i] = s[i];
    return dst;
}

static void write_marker(const char *text, size_t len)
{
    if (text_fd >= 0)
        (void)write(text_fd, text, len);
}

static real_memcpy_fn resolve_memcpy(void)
{
    if (real_memcpy_ptr != NULL)
        return real_memcpy_ptr;
    if (resolving_memcpy)
        return NULL;
    resolving_memcpy = 1;
    real_memcpy_ptr = (real_memcpy_fn)dlsym(RTLD_NEXT, "memcpy");
    resolving_memcpy = 0;
    return real_memcpy_ptr;
}

static void resolve_driver_base(void *handle)
{
    void *ipod_module;
    void *blob_symbol;
    uintptr_t base_from_module;
    uintptr_t base_from_blob;
    void *scope = handle != NULL ? handle : RTLD_DEFAULT;

    if (driver_base != 0)
        return;

    ipod_module = dlsym(scope, "ipod_module");
    blob_symbol = dlsym(scope, "packet_addparam_blob");

    if (ipod_module == NULL || blob_symbol == NULL) {
        write_marker("K2161_RGI_RX base_symbols_missing\n",
                     sizeof("K2161_RGI_RX base_symbols_missing\n") - 1U);
        return;
    }

    base_from_module =
        (uintptr_t)ipod_module - (uintptr_t)0x27e7c; /* K2161: 0x2614c */

    base_from_blob =
        (uintptr_t)blob_symbol - (uintptr_t)0x1e2c4; /* K2161: 0x1d0d8 */

    if (base_from_module != base_from_blob) {
        write_marker("K2161_RGI_RX base_crosscheck_failed\n",
                     sizeof("K2161_RGI_RX base_crosscheck_failed\n") - 1U);
        return;
    }

    driver_base = base_from_module;

    write_marker("K2161_RGI_RX driver_base_ready\n",
                 sizeof("K2161_RGI_RX driver_base_ready\n") - 1U);
}

static char hex_digit(unsigned int value)
{
    static const char digits[] = "0123456789abcdef";
    return digits[value & 0x0fU];
}

static void write_text_event(const k2161_rgi_frame *frame)
{
    char line[] = "K2161_RGI_RX msg=0000 len=0000 complete=0\n";
    unsigned int i;

    for (i = 0; i < 4; ++i) {
        unsigned int shift = (3U - i) * 4U;
        line[17 + i] = hex_digit((unsigned int)frame->msgid >> shift);
        line[26 + i] = hex_digit((unsigned int)frame->frame_len >> shift);
    }
    line[40] = frame->complete ? '1' : '0';
    if (text_fd >= 0)
        (void)write(text_fd, line, sizeof(line) - 1U);
}

static void put_be32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value >> 24);
    dst[1] = (uint8_t)(value >> 16);
    dst[2] = (uint8_t)(value >> 8);
    dst[3] = (uint8_t)value;
}

static void write_raw_packet(const uint8_t *packet, size_t n,
                             const k2161_rgi_frame *frame)
{
    uint8_t header[K2161_RAW_HEADER_LEN];
    struct iovec vectors[2];
    size_t record_len;
    ssize_t written;
    unsigned int flags = 0;

    if (raw_fd < 0 || packet == NULL || n == 0)
        return;
    if (n > K2161_MAX_TRANSPORT_LEN) {
        if (!capture_error_reported) {
            capture_error_reported = 1;
            write_marker("K2161_RGI_RX invalid_transport_length\n",
                         sizeof("K2161_RGI_RX invalid_transport_length\n") - 1U);
        }
        return;
    }

    record_len = K2161_RAW_HEADER_LEN + n;
    if (raw_bytes_written > K2161_RAW_CAPTURE_LIMIT - record_len) {
        if (!capture_limit_reported) {
            capture_limit_reported = 1;
            write_marker("K2161_RGI_RX capture_limit_reached\n",
                         sizeof("K2161_RGI_RX capture_limit_reached\n") - 1U);
        }
        return;
    }

    if (n >= 9 && packet[0] == 0xff && packet[1] == 0x5a)
        flags |= 1U;
    if (frame != NULL) {
        flags |= 2U;
        if (frame->complete)
            flags |= 4U;
        if (k2161_is_expected_rgi_rx(frame->msgid))
            flags |= 8U;
    }

    header[0] = 'K'; header[1] = '2'; header[2] = 'R'; header[3] = 'X';
    header[4] = 2;
    header[5] = (uint8_t)flags;
    header[6] = (flags & 1U) != 0 ? packet[7] : 0xff;
    header[7] = (flags & 1U) != 0 ? packet[4] : 0;
    put_be32(header + 8, (uint32_t)n);
    put_be32(header + 12, raw_record_sequence++);

    vectors[0].iov_base = header;
    vectors[0].iov_len = sizeof(header);
    vectors[1].iov_base = (void *)packet;
    vectors[1].iov_len = n;
    written = writev(raw_fd, vectors, 2);
    if (written == (ssize_t)record_len) {
        raw_bytes_written += record_len;
        return;
    }

    if (!capture_error_reported) {
        capture_error_reported = 1;
        write_marker("K2161_RGI_RX raw_write_failed\n",
                     sizeof("K2161_RGI_RX raw_write_failed\n") - 1U);
    }
}

static void observe_transport_copy(const uint8_t *copied, size_t n)
{
    k2161_rgi_frame frame;
    k2161_rgi_frame *found = NULL;

    if (k2161_probe_rgi_start(copied, n, &frame))
        found = &frame;
    write_raw_packet(copied, n, found);
    if (found == NULL)
        return;

    ++k2161_rx_rgi_count;
    if (!frame.complete)
        ++k2161_rx_partial_rgi_count;
    if (frame.msgid == 0x5201)
        ++k2161_rx_5201;
    else if (frame.msgid == 0x5202)
        ++k2161_rx_5202;
    else if (frame.msgid == 0x5204)
        ++k2161_rx_5204;
    {
        uint32_t diag_seq = rgi_diag_rx_begin(frame.msgid, frame.frame_len);
        k2161_send_note_rgi_rx(frame.msgid);

    /*
     * Only complete control frames enter Luka's parser.
     * Incomplete frames remain captured exactly as before and are not
     * presented downstream as valid navigation state.
     */
    if (frame.complete) {
        int bridge_rc = k2161_rgi_bridge_feed(frame.frame, frame.frame_len);

        if (bridge_rc == 1) {
            ++k2161_rx_bridge_ok;

            if (!bridge_ok_reported) {
                bridge_ok_reported = 1;
                write_marker("K2161_RGI_RX bridge_first_ok\n",
                             sizeof("K2161_RGI_RX bridge_first_ok\n") - 1U);
            }
        } else if (bridge_rc < 0) {
            ++k2161_rx_bridge_fail;

            if (!bridge_error_reported) {
                bridge_error_reported = 1;
                write_marker("K2161_RGI_RX bridge_feed_failed\n",
                             sizeof("K2161_RGI_RX bridge_feed_failed\n") - 1U);
            }
        }
    }

        rgi_diag_rx_end(diag_seq, frame.complete ? 1 : 0);
    }
    write_text_event(&frame);
}

void *memcpy(void *dst, const void *src, size_t n)
{
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    real_memcpy_fn fn = resolve_memcpy();
    void *result = fn != NULL ? fn(dst, src, n) : fallback_memcpy(dst, src, n);

    if (driver_base != 0 &&
        caller == driver_base + (uintptr_t)0x125e8) { /* link_get_transport_data+0x3e0; K2161: 0x11428 (see docs/02) */

        ++k2161_rx_callsite_count;

        if (!ident_install_attempted) {
            int ident_rc;

            ident_install_attempted = 1;

            write_marker("K2161_RGI_RX ident_install_from_callsite\n",
                         sizeof("K2161_RGI_RX ident_install_from_callsite\n") - 1U);

            ident_rc =
                k2161_ident_try_install_base((void *)driver_base);

            /*
             * Do not arm 0x5200 unless the proven Identification hook is
             * actually installed on this same validated K2161 base.
             */
            if (ident_rc == 1 && !send_install_attempted) {
                int msgtable_rc;

                msgtable_rc =
                    k2161_msgtable_try_install_base((void *)driver_base);

                if (msgtable_rc == 1 &&
                    k2161_send_hook_try_install_base(
                        (void *)driver_base) == 1)
                    send_install_attempted = 1;
            }
        }

        if (!callsite_reported) {
            callsite_reported = 1;

            write_marker("K2161_RGI_RX callsite_ready\n",
                         sizeof("K2161_RGI_RX callsite_ready\n") - 1U);
        }

        observe_transport_copy((const uint8_t *)dst, n);
    }
    return result;
}

void *dlopen(const char *path, int mode)
{
    void *handle;

    if (real_dlopen_ptr == NULL)
        real_dlopen_ptr = (real_dlopen_fn)dlsym(RTLD_NEXT, "dlopen");
    if (real_dlopen_ptr == NULL)
        return NULL;
    handle = real_dlopen_ptr(path, mode);
    if (handle != NULL) {
        resolve_driver_base(handle);

        /*
         * Start the localhost route-guidance bus after the real K2161 driver
         * has loaded, before the first RGI receive callback where possible.
         */
        if (driver_base != 0)
            (void)k2161_rgi_bridge_init();
    }
    return handle;
}

__attribute__((constructor))
static void k2161_rx_observer_init(void)
{
    rgi_diag_reset_file();
    rgi_diag_log("INIT", "observer_constructor");
    (void)resolve_memcpy();
    real_dlopen_ptr = (real_dlopen_fn)dlsym(RTLD_NEXT, "dlopen");
    raw_fd = open(K2161_RX_BIN_PATH,
                  O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
    text_fd = open(K2161_RX_LOG_PATH,
                   O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
    write_marker("K2161_RGI_RX observer_loaded\n",
                 sizeof("K2161_RGI_RX observer_loaded\n") - 1U);
    resolve_driver_base(NULL);
}

__attribute__((destructor))
static void k2161_rx_observer_fini(void)
{
    k2161_rgi_bridge_shutdown();

    if (raw_fd >= 0)
        (void)close(raw_fd);
    if (text_fd >= 0)
        (void)close(text_fd);
}
