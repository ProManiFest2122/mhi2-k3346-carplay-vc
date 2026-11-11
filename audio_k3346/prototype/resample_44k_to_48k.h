/* Streaming 44.1 -> 48 kHz (ratio 160/147) polyphase resampler for the AAC-LC/44.1 fallback (index 22).
 * Windowed-sinc, 32 taps x 160 phases, per-phase unity DC gain, fractional phase and history kept across
 * access units (1114/1115 output frames per 1024-frame AU).  Own implementation: the reference's libspeex
 * is not a dependency here.  No allocation after init; state is plain data.  Decoder delay: 16 input frames. */
#ifndef MHI2_RESAMPLE_44K_TO_48K_H
#define MHI2_RESAMPLE_44K_TO_48K_H
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define RS_UP 160u
#define RS_DOWN 147u
#define RS_TAPS 32u
#define RS_HALF (RS_TAPS / 2u)
#define RS_MAX_IN 1024u
#define RS_CH 2u
#define RS_MAX_OUT 1120u /* ceil(1024 * 160 / 147) + 1 */

struct rs44_state {
    float history[RS_CH][RS_TAPS + RS_MAX_IN]; /* last RS_TAPS inputs + the current AU */
    size_t have;                               /* valid input frames in history */
    uint64_t pos160;                           /* next output position, units of 1/160 input frame, relative to history[0] */
};

static float rs44_table[RS_UP][RS_TAPS];
static int rs44_table_ready;

static void rs44_build_table(void)
{
    unsigned p, t;
    const double pi = 3.14159265358979323846;
    const double fc = 0.5 * 0.94; /* cutoff as a fraction of the input rate (just below 22.05 kHz) */
    if (rs44_table_ready) return;
    for (p = 0; p < RS_UP; p++) {
        double sum = 0.0, tmp[RS_TAPS];
        for (t = 0; t < RS_TAPS; t++) {
            double x = (double)t - (double)(RS_HALF - 1u) - (double)p / (double)RS_UP; /* distance from the output instant */
            double s = fabs(x) < 1e-12 ? 2.0 * fc : sin(2.0 * pi * fc * x) / (pi * x);
            double w = 0.5 * (1.0 + cos(pi * x / (double)RS_HALF)); /* Hann over +-16 taps */
            tmp[t] = s * (fabs(x) < (double)RS_HALF ? w : 0.0);
            sum += tmp[t];
        }
        for (t = 0; t < RS_TAPS; t++) rs44_table[p][t] = (float)(tmp[t] / sum);
    }
    rs44_table_ready = 1;
}

static void rs44_reset(struct rs44_state *s)
{
    memset(s, 0, sizeof(*s));
    s->have = RS_TAPS; /* zero history so the first outputs ramp in from silence */
    s->pos160 = (uint64_t)(RS_HALF - 1u + 1u) * RS_UP; /* first output centred on input frame 0 (after the zero history) */
    s->pos160 = (uint64_t)RS_TAPS * RS_UP; /* input frame 0 sits at history index RS_TAPS */
}

/* in: interleaved stereo S16, frames <= RS_MAX_IN.  out capacity >= RS_MAX_OUT frames.  Returns frames written. */
static size_t rs44_process(struct rs44_state *s, const int16_t *in, size_t frames, int16_t *out)
{
    size_t i, n = 0, ch;
    if (!rs44_table_ready) rs44_build_table();
    if (frames > RS_MAX_IN) frames = RS_MAX_IN;
    for (ch = 0; ch < RS_CH; ch++)
        for (i = 0; i < frames; i++) s->history[ch][s->have + i] = (float)in[i * RS_CH + ch] * (1.0f / 32768.0f);
    s->have += frames;
    for (;;) {
        size_t base = (size_t)(s->pos160 / RS_UP);
        unsigned phase = (unsigned)(s->pos160 % RS_UP);
        /* taps cover input indices base - (HALF - 1) .. base + HALF */
        if (base + RS_HALF >= s->have) break;
        for (ch = 0; ch < RS_CH; ch++) {
            const float *h = &s->history[ch][base - (RS_HALF - 1u)];
            const float *c = rs44_table[phase];
            float acc = 0.0f;
            unsigned t;
            for (t = 0; t < RS_TAPS; t++) acc += c[t] * h[t];
            acc *= 32768.0f;
            out[n * RS_CH + ch] = (int16_t)(acc > 32767.0f ? 32767 : acc < -32768.0f ? -32768 : (int16_t)(acc + (acc >= 0 ? 0.5f : -0.5f)));
        }
        n++;
        s->pos160 += RS_DOWN;
    }
    /* keep the newest RS_TAPS inputs; rebase positions */
    if (s->have > RS_TAPS) {
        size_t drop = s->have - RS_TAPS;
        for (ch = 0; ch < RS_CH; ch++) memmove(s->history[ch], s->history[ch] + drop, RS_TAPS * sizeof(float));
        s->have = RS_TAPS;
        s->pos160 -= (uint64_t)drop * RS_UP;
    }
    return n;
}
#endif
