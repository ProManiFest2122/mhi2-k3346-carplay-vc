/* QSA-shaped facade over the native NVIDIA PCM sink, included by the generated
 * K3346 hook (MHI2_SINK_NVIDIA) so the reference detached/cold logic runs
 * unchanged.  It never touches real QSA.  Semantics:
 *  - write: whole-period success returns bytes; a native failure returns the
 *    known-confirmed prefix (or -EIO) and records the uncertain frames, which the
 *    caller MUST drop (spa_nv_shim_take_uncertain_frames): at-most-once, no replay.
 *  - flush/prepare after a fault reopen the native connection (new generation).
 *    A reopen is NOT a proven downstream purge and not seamless.
 *  - six-channel use needs MHI2_NV_SIX_CHANNEL_QUALIFIED=1 at build time. */
#ifndef MHI2_NVIDIA_QSA_SHIM_H
#define MHI2_NVIDIA_QSA_SHIM_H
#include "nvidia_pcm_sink.h"
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef MHI2_NV_SIX_CHANNEL_QUALIFIED
#define MHI2_NV_SIX_CHANNEL_QUALIFIED 0
#endif
#define SPA_NV_SHIM_DEVICE_DEFAULT "MPL5"

typedef struct {
    struct mhi2_nv_sink sink;
    struct mhi2_nv_api api;
    int haveApi, inited, openFlag, sinkStarted, fd;
    unsigned generation, voices;
    int fragSize;
    size_t uncertainFrames;
    unsigned long long flushes, reopens, faults;
    char device[16];
} spa_nv_shim_t;
static spa_nv_shim_t spa_nv_shim = { .fd = -1 };
static char spa_nv_shim_handle_token;

/* Test seam: replace the native API before the first open. */
static void spa_nv_shim_set_api(const struct mhi2_nv_api *api)
{
    spa_nv_shim.api = *api;
    spa_nv_shim.haveApi = 1;
}

static int spa_nv_shim_bind_api(void)
{
    void *scope;
    if (spa_nv_shim.haveApi) return 0;
#ifdef MHI2_NV_STOCK_BRIDGE
    scope = RTLD_NEXT; /* this object exports nvaudio_*; skip our own definitions */
#else
    scope = RTLD_DEFAULT;
#endif
    spa_nv_shim.api.open = (int (*)(void **, const char *))dlsym(scope, "nvaudio_pcm_open");
    spa_nv_shim.api.configure = (int (*)(void *, const void *))dlsym(scope, "nvaudio_set_pcm_info");
    spa_nv_shim.api.metadata = (int (*)(void *, const void *, unsigned *))dlsym(scope, "nvaudio_create_metadata");
    spa_nv_shim.api.write = (int (*)(void *, const void *, unsigned, const unsigned *, unsigned))dlsym(scope, "nvaudio_pcm_write");
    spa_nv_shim.api.close = (int (*)(void *))dlsym(scope, "nvaudio_pcm_close");
#ifdef MHI2_NV_STOCK_BRIDGE
    if (!spa_nv_shim.api.open || !spa_nv_shim.api.configure || !spa_nv_shim.api.metadata ||
        !spa_nv_shim.api.write || !spa_nv_shim.api.close) { /* RTLD_NEXT incomplete: ask the library directly (as the fixture does) */
        void *lib = dlopen("libnvaudio.so", RTLD_NOW);
#ifdef SPA_NV_SHIM_DEBUG
        fprintf(stderr, "nv shim: RTLD_NEXT incomplete (open=%p write=%p), dlopen=%p\n", (void *)spa_nv_shim.api.open, (void *)spa_nv_shim.api.write, lib);
#endif
        if (lib) {
            spa_nv_shim.api.open = (int (*)(void **, const char *))dlsym(lib, "nvaudio_pcm_open");
            spa_nv_shim.api.configure = (int (*)(void *, const void *))dlsym(lib, "nvaudio_set_pcm_info");
            spa_nv_shim.api.metadata = (int (*)(void *, const void *, unsigned *))dlsym(lib, "nvaudio_create_metadata");
            spa_nv_shim.api.write = (int (*)(void *, const void *, unsigned, const unsigned *, unsigned))dlsym(lib, "nvaudio_pcm_write");
            spa_nv_shim.api.close = (int (*)(void *))dlsym(lib, "nvaudio_pcm_close");
        }
    }
#endif
    if (!spa_nv_shim.api.open || !spa_nv_shim.api.configure || !spa_nv_shim.api.metadata ||
        !spa_nv_shim.api.write || !spa_nv_shim.api.close) return -1;
    spa_nv_shim.haveApi = 1;
    return 0;
}

/* Frames the last failed write left in an unknown state; the caller drops them. */
static size_t spa_nv_shim_take_uncertain_frames(void)
{
    size_t n = spa_nv_shim.uncertainFrames;
    spa_nv_shim.uncertainFrames = 0;
    return n;
}

static int spa_nv_shim_faulted(void) { return spa_nv_shim.sink.initialized && spa_nv_shim.sink.fault != 0; }

static int spa_nv_shim_open_name(snd_pcm_t **handle, char *name, int mode)
{
    const char *device;
    (void)mode;
    if (!handle || !name || strcmp(name, "mpl5_dio_ent") || spa_nv_shim.openFlag) return -EBUSY;
    if (spa_nv_shim_bind_api()) return -ENOSYS;
    device = getenv("MHI2_NV_DEVICE");
    strncpy(spa_nv_shim.device, device && *device ? device : SPA_NV_SHIM_DEVICE_DEFAULT,
            sizeof(spa_nv_shim.device) - 1);
    if (mhi2_nv_sink_init(&spa_nv_shim.sink, &spa_nv_shim.api, spa_nv_shim.device,
                          MHI2_NV_SIX_CHANNEL_QUALIFIED)) return -EACCES;
    spa_nv_shim.inited = 1;
    spa_nv_shim.openFlag = 1;
    spa_nv_shim.sinkStarted = 0;
    spa_nv_shim.uncertainFrames = 0;
    if (spa_nv_shim.fd < 0) { /* empty pipe: write end is always POLLOUT; native write paces itself */
        int pfd[2];
        if (pipe(pfd) == 0) spa_nv_shim.fd = pfd[1]; /* read end stays open: closing it gives POLLERR */
        if (spa_nv_shim.fd < 0) return -EMFILE;
    }
    *handle = (snd_pcm_t *)&spa_nv_shim_handle_token;
    return 0;
}

static int spa_nv_shim_ok(snd_pcm_t *h) { return h == (snd_pcm_t *)&spa_nv_shim_handle_token && spa_nv_shim.openFlag; }

static unsigned int spa_nv_shim_set_disable(snd_pcm_t *h, unsigned int d) { (void)h; (void)d; return 0; }

static int spa_nv_shim_channel_info(snd_pcm_t *h, snd_pcm_channel_info_t *info)
{
    if (!spa_nv_shim_ok(h) || !info) return -EINVAL;
    info->min_voices = 6; info->max_voices = 6;
    info->min_rate = 48000; info->max_rate = 48000;
    info->min_fragment_size = 1; info->max_fragment_size = 1024 * 6 * 2;
    info->formats = 1u << 1;
    return 0;
}

static int spa_nv_shim_params(snd_pcm_t *h, snd_pcm_channel_params_t *p)
{
    if (!spa_nv_shim_ok(h) || !p) return -EINVAL;
    if (p->format.voices != 6 || p->format.rate != 48000 || p->format.format != SND_PCM_SFMT_S16_LE) {
        p->why_failed = 1;
        return -EINVAL;
    }
    if (p->buf.block.frag_size <= 0 || p->buf.block.frag_size % (6 * (int)sizeof(int16_t))) return -EINVAL;
    spa_nv_shim.voices = (unsigned)p->format.voices;
    spa_nv_shim.fragSize = p->buf.block.frag_size;
    return 0;
}

static int spa_nv_shim_setup(snd_pcm_t *h, snd_pcm_channel_setup_t *s)
{
    if (!spa_nv_shim_ok(h) || !s) return -EINVAL;
    s->mode = SND_PCM_MODE_BLOCK;
    s->format.format = SND_PCM_SFMT_S16_LE;
    s->format.rate = 48000;
    s->format.voices = (int)spa_nv_shim.voices;
    s->buf.block.frag_size = spa_nv_shim.fragSize;
    return 0;
}

/* First prepare starts the native connection; after a fault it reopens. */
static int spa_nv_shim_prepare(snd_pcm_t *h, int channel)
{
    int r;
    (void)channel;
    if (!spa_nv_shim_ok(h)) return -EINVAL;
    if (!spa_nv_shim.sinkStarted) {
        r = mhi2_nv_sink_start(&spa_nv_shim.sink);
        if (r) { spa_nv_shim.faults++; return r < 0 ? r : -EIO; }
        spa_nv_shim.sinkStarted = 1;
        spa_nv_shim.generation = mhi2_nv_sink_generation(&spa_nv_shim.sink);
        return 0;
    }
    if (spa_nv_shim.sink.fault || !spa_nv_shim.sink.ready) {
        r = mhi2_nv_sink_flush(&spa_nv_shim.sink);
        if (r) { spa_nv_shim.faults++; return r < 0 ? r : -EIO; }
        spa_nv_shim.reopens++;
        spa_nv_shim.generation = mhi2_nv_sink_generation(&spa_nv_shim.sink);
        spa_nv_shim.uncertainFrames = 0;
    }
    return 0;
}

/* Drops the native connection and starts a new generation (reopen). */
static int spa_nv_shim_flush(snd_pcm_t *h, int channel)
{
    int r;
    (void)channel;
    if (!spa_nv_shim_ok(h)) return -EINVAL;
    if (!spa_nv_shim.sinkStarted) return 0;
    r = mhi2_nv_sink_flush(&spa_nv_shim.sink);
    spa_nv_shim.flushes++;
    if (r) { spa_nv_shim.faults++; return r < 0 ? r : -EIO; }
    spa_nv_shim.generation = mhi2_nv_sink_generation(&spa_nv_shim.sink);
    spa_nv_shim.uncertainFrames = 0;
    return 0;
}

static int spa_nv_shim_status(snd_pcm_t *h, snd_pcm_channel_status_t *st)
{
    if (!spa_nv_shim_ok(h) || !st) return -EINVAL;
    st->status = spa_nv_shim_faulted() ? SND_PCM_STATUS_UNDERRUN : SND_PCM_STATUS_RUNNING;
    return 0;
}

static ssize_t spa_nv_shim_write(snd_pcm_t *h, const void *buf, size_t bytes)
{
    struct mhi2_nv_result r;
    const size_t frameBytes = 6 * sizeof(int16_t);
    if (!spa_nv_shim_ok(h) || !buf || bytes % frameBytes || !spa_nv_shim.sinkStarted) {
        errno = EINVAL;
        return -EINVAL;
    }
    r = mhi2_nv_sink_write(&spa_nv_shim.sink, spa_nv_shim.generation, (const int16_t *)buf, bytes / frameBytes);
#ifdef SPA_NV_SHIM_DEBUG
    fprintf(stderr, "shim write bytes=%zu gen=%u status=%d conf=%zu unc=%zu\n", bytes, spa_nv_shim.generation, r.status, r.confirmed_frames, r.uncertain_frames);
#endif
    if (!r.status && r.confirmed_frames == bytes / frameBytes) return (ssize_t)bytes;
    spa_nv_shim.uncertainFrames = r.uncertain_frames ? r.uncertain_frames : (bytes / frameBytes - r.confirmed_frames);
    spa_nv_shim.faults++;
    errno = EIO;
    return r.confirmed_frames ? (ssize_t)(r.confirmed_frames * frameBytes) : -EIO;
}

static int spa_nv_shim_nonblock(snd_pcm_t *h, int on) { (void)on; return spa_nv_shim_ok(h) ? 0 : -EINVAL; }
static int spa_nv_shim_fd(snd_pcm_t *h, int channel) { (void)channel; return spa_nv_shim_ok(h) ? spa_nv_shim.fd : -1; }

static int spa_nv_shim_close(snd_pcm_t *h)
{
    int r = 0;
    if (!spa_nv_shim_ok(h)) return -EINVAL;
    if (spa_nv_shim.sinkStarted) r = mhi2_nv_sink_close(&spa_nv_shim.sink);
    if (!r) {
        if (spa_nv_shim.inited) (void)mhi2_nv_sink_destroy(&spa_nv_shim.sink);
        spa_nv_shim.inited = 0;
        spa_nv_shim.openFlag = 0;
        spa_nv_shim.sinkStarted = 0;
    }
    return r ? -EIO : 0;
}

/* Replacement for dlsym(RTLD_NEXT, "snd_pcm_*") on the NVIDIA build. */
static void *spa_nv_shim_resolve(const char *name)
{
    static const struct { const char *name; void *fn; } table[] = {
        { "snd_pcm_open_name", (void *)spa_nv_shim_open_name },
        { "snd_pcm_plugin_set_disable", (void *)spa_nv_shim_set_disable },
        { "snd_pcm_channel_info", (void *)spa_nv_shim_channel_info },
        { "snd_pcm_plugin_params", (void *)spa_nv_shim_params },
        { "snd_pcm_plugin_setup", (void *)spa_nv_shim_setup },
        { "snd_pcm_plugin_prepare", (void *)spa_nv_shim_prepare },
        { "snd_pcm_plugin_flush", (void *)spa_nv_shim_flush },
        { "snd_pcm_plugin_status", (void *)spa_nv_shim_status },
        { "snd_pcm_plugin_write", (void *)spa_nv_shim_write },
        { "snd_pcm_write_internal", (void *)spa_nv_shim_write },
        { "snd_pcm_nonblock_mode", (void *)spa_nv_shim_nonblock },
        { "snd_pcm_file_descriptor", (void *)spa_nv_shim_fd },
        { "snd_pcm_close", (void *)spa_nv_shim_close },
    };
    size_t i;
    for (i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (!strcmp(name, table[i].name)) return table[i].fn;
    return 0;
}
#endif
