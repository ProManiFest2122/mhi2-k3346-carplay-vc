#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "diagnostics.h"

static pthread_mutex_t g_rgi_diag_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_diag_session = 1;
static uint32_t g_diag_rx_seq = 0;
static uint32_t g_diag_current_rx_seq = 0;
static uint16_t g_diag_current_msgid = 0;

static unsigned long long diag_now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (unsigned long long)ts.tv_sec * 1000ULL +
           (unsigned long long)ts.tv_nsec / 1000000ULL;
}

static void diag_write_line(const char *line, size_t len, int truncate)
{
    int flags = O_WRONLY | O_CREAT | (truncate ? O_TRUNC : O_APPEND);
    int fd = open(MIB2HIGH_RGI_DIAGNOSTICS_PATH, flags, 0600);
    if (fd < 0)
        return;
    (void)write(fd, line, len);
    (void)close(fd);
}

void rgi_diag_reset_file(void)
{
    static const char header[] = "MIB2HIGH_CARPLAY_RGI_DIAG version=1\n";
    pthread_mutex_lock(&g_rgi_diag_lock);
    g_diag_session = 1;
    g_diag_rx_seq = 0;
    g_diag_current_rx_seq = 0;
    g_diag_current_msgid = 0;
    diag_write_line(header, sizeof(header)-1U, 1);
    pthread_mutex_unlock(&g_rgi_diag_lock);
}

void rgi_diag_log(const char *layer, const char *fmt, ...)
{
    char line[1024];
    int off;
    va_list ap;

    pthread_mutex_lock(&g_rgi_diag_lock);
    off = snprintf(line, sizeof(line),
                   "t=%llu session=%u rx=%u msg=%04x [%s] ",
                   diag_now_ms(), (unsigned)g_diag_session,
                   (unsigned)g_diag_current_rx_seq,
                   (unsigned)g_diag_current_msgid,
                   layer ? layer : "?");
    if (off < 0) off = 0;
    if (off >= (int)sizeof(line)) off = (int)sizeof(line)-1;

    va_start(ap, fmt);
    if (off < (int)sizeof(line)-1)
        off += vsnprintf(line + off, sizeof(line) - (size_t)off, fmt, ap);
    va_end(ap);

    if (off < 0) off = 0;
    if (off >= (int)sizeof(line)-2) off = (int)sizeof(line)-2;
    line[off++] = '\n';
    line[off] = '\0';
    diag_write_line(line, (size_t)off, 0);
    pthread_mutex_unlock(&g_rgi_diag_lock);
}

uint32_t rgi_diag_session_get(void)
{
    uint32_t v;
    pthread_mutex_lock(&g_rgi_diag_lock);
    v = g_diag_session;
    pthread_mutex_unlock(&g_rgi_diag_lock);
    return v;
}

uint32_t rgi_diag_session_bump(const char *reason)
{
    uint32_t v;
    pthread_mutex_lock(&g_rgi_diag_lock);
    ++g_diag_session;
    g_diag_rx_seq = 0;
    g_diag_current_rx_seq = 0;
    g_diag_current_msgid = 0;
    v = g_diag_session;
    pthread_mutex_unlock(&g_rgi_diag_lock);
    rgi_diag_log("SESSION", "bump reason=%s new_session=%u",
                reason ? reason : "unknown", (unsigned)v);
    return v;
}

uint32_t rgi_diag_rx_begin(uint16_t msgid, size_t len)
{
    uint32_t seq;
    pthread_mutex_lock(&g_rgi_diag_lock);
    seq = ++g_diag_rx_seq;
    g_diag_current_rx_seq = seq;
    g_diag_current_msgid = msgid;
    pthread_mutex_unlock(&g_rgi_diag_lock);
    rgi_diag_log("RX", "begin len=%u", (unsigned)len);
    return seq;
}

void rgi_diag_rx_end(uint32_t seq, int bridge_rc)
{
    rgi_diag_log("RX", "end seq=%u bridge_rc=%d", (unsigned)seq, bridge_rc);
    pthread_mutex_lock(&g_rgi_diag_lock);
    if (g_diag_current_rx_seq == seq) {
        g_diag_current_rx_seq = 0;
        g_diag_current_msgid = 0;
    }
    pthread_mutex_unlock(&g_rgi_diag_lock);
}

uint32_t rgi_diag_current_rx_seq(void)
{
    uint32_t v;
    pthread_mutex_lock(&g_rgi_diag_lock);
    v = g_diag_current_rx_seq;
    pthread_mutex_unlock(&g_rgi_diag_lock);
    return v;
}

uint16_t rgi_diag_current_msgid(void)
{
    uint16_t v;
    pthread_mutex_lock(&g_rgi_diag_lock);
    v = g_diag_current_msgid;
    pthread_mutex_unlock(&g_rgi_diag_lock);
    return v;
}
