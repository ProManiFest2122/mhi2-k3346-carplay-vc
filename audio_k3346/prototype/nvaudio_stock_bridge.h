/* Stock-facing nvaudio_* interposers (build with -DMHI2_NV_STOCK_BRIDGE, LD_PRELOAD into dio_manager).
 * Translates the stock libairplay CAudioDriverIF calls (open / set_pcm_info / create_metadata / write / close)
 * into the hook's QSA-shaped logic (snd_pcm_* above), so the reference type-100 upmix, ownership handoff with the
 * type-103 receiver, route claim and recovery run unchanged on top of the NVIDIA shim.
 *  - Only the configured stock media device (default "MPL5") and only 48k/S16/stereo is claimed.
 *  - Everything else (other devices, 16k mono navigation, phone) is passed to the real libnvaudio untouched.
 *  - Without MHI2_NV_SIX_CHANNEL_QUALIFIED=1 NOTHING is claimed: stock audio stays exactly as shipped. */
#ifdef MHI2_NV_STOCK_BRIDGE
#ifndef SPA_NV_STOCK_DEVICE
#define SPA_NV_STOCK_DEVICE "MPL5"
#endif
/* MHI2_NV_BRIDGE_LOCAL_MEDIA: separate build for a process without libairplay (mm-renderer: Bluetooth/USB/SD on
 * MPL1). The claimed stream itself is declared as the media main stream (no AudioStreamPrepare there), and
 * 44.1 kHz stereo is also claimed and resampled to the sink's 48 kHz. The CarPlay build does not define it. */
#ifdef MHI2_NV_BRIDGE_LOCAL_MEDIA
#include "resample_44k_to_48k.h"
#endif
#define SPA_NV_TOKEN_MAGIC 0x4e565442u
#define SPA_NV_TOKENS 4
typedef struct {
    unsigned magic;
    int used, mode; /* 1 = claimed by the hook (snd), 2 = real passthrough */
    snd_pcm_t *snd;
    void *real;
    char name[16];
    unsigned meta[2];
#ifdef MHI2_NV_BRIDGE_LOCAL_MEDIA
    unsigned rate;   /* claimed input rate: 48000 or 44100 */
    int localMedia;  /* this token declared the media main stream */
    struct rs44_state rs;
    int16_t pend[(4 * SPA_PCM_SINK_PERIOD_FRAMES + RS_MAX_OUT) * 2]; /* resampled frames waiting for a full stock period */
    size_t pendFrames;
    unsigned long long droppedFrames; /* stalled-sink drops (diagnostic) */
    unsigned char info[64]; /* last real nvaudio_pcm_info (54 bytes used): served after the claim closed the real handle */
    int haveInfo;
#endif
} spa_nv_token_t;
static spa_nv_token_t spa_nv_tokens[SPA_NV_TOKENS];
static pthread_mutex_t spa_nv_token_lock = PTHREAD_MUTEX_INITIALIZER;

static spa_nv_token_t *spa_nv_token_of(void *h)
{
    unsigned i;
    for (i = 0; i < SPA_NV_TOKENS; i++)
        if (h == (void *)&spa_nv_tokens[i] && spa_nv_tokens[i].used) return &spa_nv_tokens[i];
    return 0;
}

static void spa_nv_token_drop_snd(spa_nv_token_t *t)
{
    if (t->snd) { (void)snd_pcm_close(t->snd); t->snd = 0; }
#ifdef MHI2_NV_BRIDGE_LOCAL_MEDIA
    if (t->localMedia) { spa_stock_main_audio_end(t, "bridge_local_end"); t->localMedia = 0; }
#endif
}

static int spa_nv_claim_format(const unsigned *c)
{
    if (!c || c[1] != 2 || c[2] != 2) return 0;
#ifdef MHI2_NV_BRIDGE_LOCAL_MEDIA
    if (c[0] == 44100) return 1;
#endif
    return c[0] == 48000;
}

static int spa_nv_token_use_real(spa_nv_token_t *t)
{
    if (t->real) return 0;
    return spa_nv_shim.api.open(&t->real, t->name);
}

HOOK_EXPORT int nvaudio_pcm_open(void **h, const char *name)
{
    spa_nv_token_t *t = 0;
    unsigned i;
    if (spa_nv_shim_bind_api()) return 1;
    if (!MHI2_NV_SIX_CHANNEL_QUALIFIED || !h || !name || strcmp(name, SPA_NV_STOCK_DEVICE))
        return spa_nv_shim.api.open(h, name);
    pthread_mutex_lock(&spa_nv_token_lock);
    for (i = 0; i < SPA_NV_TOKENS; i++)
        if (!spa_nv_tokens[i].used) { t = &spa_nv_tokens[i]; break; }
    if (t) {
        memset(t, 0, sizeof(*t));
        t->magic = SPA_NV_TOKEN_MAGIC; t->used = 1;
        strncpy(t->name, name, sizeof(t->name) - 1);
    }
    pthread_mutex_unlock(&spa_nv_token_lock);
    if (!t) return spa_nv_shim.api.open(h, name);
    *h = t;
    return 0;
}

HOOK_EXPORT int nvaudio_set_pcm_info(void *h, const void *info)
{
    spa_nv_token_t *t = spa_nv_token_of(h);
    const unsigned *c = (const unsigned *)info;
    snd_pcm_channel_params_t params;
    snd_pcm_channel_setup_t setup;
    int r;
    if (spa_nv_shim_bind_api()) return 1;
    if (!t) return spa_nv_shim.api.configure(h, info);
    if (spa_nv_claim_format(c)) { /* stock music: claim */
        if (t->real) { (void)spa_nv_shim.api.close(t->real); t->real = 0; }
#ifdef MHI2_NV_BRIDGE_LOCAL_MEDIA
        if (t->mode == 1 && t->snd && t->rate == c[0]) return 0;
        t->rate = c[0];
        rs44_reset(&t->rs);
        t->pendFrames = 0;
        if (!t->localMedia && spa_stock_main_audio_begin(t, SPA_DIO_AUDIO_TYPE_MEDIA, "prepare") == 1) t->localMedia = 1;
#endif
        if (t->mode == 1 && t->snd) return 0; /* same format again: keep the live handle */
        r = snd_pcm_open_name(&t->snd, (char *)SPA_PCM_SINK_NAME, SND_PCM_OPEN_PLAYBACK);
        if (r || !t->snd) { t->snd = 0; return r ? r : 1; }
        memset(&params, 0, sizeof(params));
        params.channel = SND_PCM_CHANNEL_PLAYBACK;
        params.mode = SND_PCM_MODE_BLOCK;
        params.start_mode = SND_PCM_START_FULL;
        params.stop_mode = SND_PCM_STOP_STOP;
        params.format.interleave = 1;
        params.format.format = SND_PCM_SFMT_S16_LE;
        params.format.rate = 48000;
        params.format.voices = 2;
        params.buf.block.frag_size = (int32_t)(SPA_PCM_SINK_PERIOD_FRAMES * 2 * sizeof(int16_t));
        params.buf.block.frags_min = 1;
        params.buf.block.frags_max = SPA_PCM_SINK_MIN_FRAGS;
        r = snd_pcm_plugin_params(t->snd, &params);
        if (!r) { memset(&setup, 0, sizeof(setup)); setup.channel = SND_PCM_CHANNEL_PLAYBACK; r = snd_pcm_plugin_setup(t->snd, &setup); }
        if (!r) r = snd_pcm_plugin_prepare(t->snd, SND_PCM_CHANNEL_PLAYBACK);
        if (r) { spa_nv_token_drop_snd(t); return r; }
        t->mode = 1;
        return 0;
    }
    /* any other format (16 kHz mono navigation, ...): real device, untouched */
    spa_nv_token_drop_snd(t);
    r = spa_nv_token_use_real(t);
    if (r) return r;
    t->mode = 2;
    return spa_nv_shim.api.configure(t->real, info);
}

HOOK_EXPORT int nvaudio_create_metadata(void *h, const void *sad, unsigned *meta)
{
    spa_nv_token_t *t = spa_nv_token_of(h);
    if (spa_nv_shim_bind_api()) return 1;
    if (!t) return spa_nv_shim.api.metadata(h, sad, meta);
    if (t->mode == 2 && t->real) {
        int r = spa_nv_shim.api.metadata(t->real, sad, t->meta);
        if (!r && meta) { meta[0] = t->meta[0]; meta[1] = t->meta[1]; }
        return r;
    }
    if (meta) { meta[0] = 1; meta[1] = 2; } /* placeholder: the hook's sink builds its own PCM6 metadata */
    return 0;
}

HOOK_EXPORT int nvaudio_pcm_write(void *h, const void *p, unsigned bytes, const unsigned *meta, unsigned timeout)
{
    spa_nv_token_t *t = spa_nv_token_of(h);
    ssize_t r;
    if (spa_nv_shim_bind_api()) return 1;
    if (!t) return spa_nv_shim.api.write(h, p, bytes, meta, timeout);
    if (t->mode == 2 && t->real) return spa_nv_shim.api.write(t->real, p, bytes, t->meta, timeout);
    if (t->mode != 1 || !t->snd) return 7;
#ifdef MHI2_NV_BRIDGE_LOCAL_MEDIA
    if (t->rate == 44100) { /* resample 44.1 -> 48 kHz, then feed the claim in whole stock periods (the realtime
                               * upmix only runs on full periods; odd sizes fall back to a plain 2 -> 6 expand) */
        const int16_t *in = (const int16_t *)p;
        const size_t period = SPA_PCM_SINK_PERIOD_FRAMES, periodBytes = period * 2 * sizeof(int16_t);
        size_t frames = bytes / (2 * sizeof(int16_t)), done = 0, n;
        const size_t cap = sizeof(t->pend) / (2 * sizeof(int16_t));
        while (done < frames) {
            n = frames - done > RS_MAX_IN ? RS_MAX_IN : frames - done;
            if (t->pendFrames + RS_MAX_OUT > cap) {
                /* the sink is stalled beyond the per-write budget: never fail the player (mm-renderer stops after
                 * one error), drop the oldest pending audio instead -- an audible glitch, counted */
                size_t drop = t->pendFrames + RS_MAX_OUT - cap;
                t->pendFrames -= drop;
                memmove(t->pend, t->pend + drop * 2, t->pendFrames * 2 * sizeof(int16_t));
                t->droppedFrames += drop;
            }
            t->pendFrames += rs44_process(&t->rs, in + done * 2, n, t->pend + t->pendFrames * 2);
            done += n;
            while (t->pendFrames >= period) {
                size_t took, want = period;
                unsigned waited = 0;
                /* like the real nvaudio_pcm_write, block up to the caller's timeout while the sink is full; whatever
                 * is still not taken stays here (input is already consumed, so the caller must not retry) */
                for (;;) {
                    errno = 0;
                    r = snd_pcm_plugin_write(t->snd, t->pend, want * 2 * sizeof(int16_t));
                    took = r > 0 ? (size_t)r / (2 * sizeof(int16_t)) : 0;
                    if (took > t->pendFrames) took = t->pendFrames;
                    t->pendFrames -= took;
                    memmove(t->pend, t->pend + took * 2, t->pendFrames * 2 * sizeof(int16_t));
                    want -= took < want ? took : want;
                    if (!want || waited >= (timeout ? timeout : 20)) break;
                    usleep(2000); waited += 2;
                }
                if (want) break;
            }
        }
        return 0;
    }
#endif
    errno = 0;
    r = snd_pcm_plugin_write(t->snd, p, bytes);
    if (r == (ssize_t)bytes) return 0;
    return errno ? errno : 11; /* short write: stock retries like it does for QSA */
}

#ifdef MHI2_NV_BRIDGE_LOCAL_MEDIA
/* Players (nvaudio_play, possibly mm-renderer) size their writes from nvaudio_pcm_info before set_pcm_info.
 * Before the claim the real device answers; afterwards the copy taken from it is returned. */
HOOK_EXPORT int nvaudio_pcm_info(void *h, void *info)
{
    static int (*realInfo)(void *, void *);
    spa_nv_token_t *t = spa_nv_token_of(h);
    int r;
    if (spa_nv_shim_bind_api()) return 1;
    if (!realInfo) realInfo = (int (*)(void *, void *))dlsym(RTLD_NEXT, "nvaudio_pcm_info");
    if (!realInfo) { void *lib = dlopen("libnvaudio.so", RTLD_NOW); if (lib) realInfo = (int (*)(void *, void *))dlsym(lib, "nvaudio_pcm_info"); }
    if (!realInfo) return 1;
    if (!t) return realInfo(h, info);
    if (!info) return 1;
    if (!t->real && !t->haveInfo && spa_nv_token_use_real(t)) return 1;
    if (t->real) {
        r = realInfo(t->real, t->info);
        if (r) return r;
        t->haveInfo = 1;
    }
    memcpy(info, t->info, 54);
    return 0;
}
#endif

HOOK_EXPORT int nvaudio_pcm_close(void *h)
{
    spa_nv_token_t *t = spa_nv_token_of(h);
    int r = 0;
    if (spa_nv_shim_bind_api()) return 1;
    if (!t) return spa_nv_shim.api.close(h);
    spa_nv_token_drop_snd(t);
    if (t->real) r = spa_nv_shim.api.close(t->real);
    pthread_mutex_lock(&spa_nv_token_lock);
    memset(t, 0, sizeof(*t));
    pthread_mutex_unlock(&spa_nv_token_lock);
    return r;
}
#endif /* MHI2_NV_STOCK_BRIDGE */
