/*
 * call_card — our call card on the VC map.
 *
 * The stock call window on the map goes away when the call is answered or the
 * tab changes (docs/16, the same with a Bluetooth phone).  This card lies in the
 * map picture, above the VC bar, so unlike the bar text in capsule_text.h it
 * keeps its full brightness.  It is a picture only: the call is answered and
 * ended with the handset key on the wheel and on the VC phone tab.
 *
 * No GL in this file, so overlay/tests/call_card_test.c runs all of it on the
 * host.  platform_harman.c owns the texture and the draw call.
 *
 *   /tmp/vc-call-card  Java -> overlay   "<state>\n<name>\n<label>\nEND\n"
 *
 * state is R ringing, D dialing, A active, H held, empty for no call.  The label
 * comes from Java, which knows the HMI language; the timer is counted here.  Same
 * format rule as capsule_text.h: no rename() on /tmp, so a file without the
 * marker is "not ready yet" and the previous card stays.
 */

#ifndef K3346_CALL_CARD_H
#define K3346_CALL_CARD_H

#include "capsule_text.h"

#define CALL_CARD_PATH "/tmp/vc-call-card"
#define CALL_CARD_NAME_MAX  128
#define CALL_CARD_LABEL_MAX 64
#define CALL_CARD_FILE_MAX  (CALL_CARD_NAME_MAX + CALL_CARD_LABEL_MAX + 16)

typedef struct {
    char state;   /* 0 = no card */
    char name[CALL_CARD_NAME_MAX];
    char label[CALL_CARD_LABEL_MAX];
} call_card_t;

static int call_card_copy(char *dst, size_t cap, const char *start, const char *end)
{
    size_t n = (size_t)(end - start);
    if (n >= cap) return 0;
    memcpy(dst, start, n);
    dst[n] = '\0';
    return 1;
}

/* Returns 1 for a complete file, 0 for anything else, which leaves *out alone. */
static int call_card_parse(const char *buf, size_t len, call_card_t *out)
{
    static const char mark[] = "END\n";
    call_card_t parsed;
    const char *end = buf + len;
    const char *nl1, *nl2, *nl3;

    if (memchr(buf, '\0', len)) return 0;
    nl1 = memchr(buf, '\n', len);
    if (!nl1) return 0;
    nl2 = memchr(nl1 + 1, '\n', (size_t)(end - (nl1 + 1)));
    if (!nl2) return 0;
    nl3 = memchr(nl2 + 1, '\n', (size_t)(end - (nl2 + 1)));
    if (!nl3) return 0;
    if ((size_t)(end - (nl3 + 1)) != sizeof(mark) - 1) return 0;
    if (memcmp(nl3 + 1, mark, sizeof(mark) - 1) != 0) return 0;

    memset(&parsed, 0, sizeof(parsed));
    if (nl1 - buf == 1) {
        if (buf[0] != 'R' && buf[0] != 'D' && buf[0] != 'A' && buf[0] != 'H') return 0;
        parsed.state = buf[0];
    } else if (nl1 != buf) {
        return 0;
    }
    if (!call_card_copy(parsed.name, sizeof(parsed.name), nl1 + 1, nl2)) return 0;
    if (!call_card_copy(parsed.label, sizeof(parsed.label), nl2 + 1, nl3)) return 0;
    *out = parsed;
    return 1;
}

/* Returns 1 when *out holds the current card (a missing file is no card). */
static int call_card_read_file(const char *path, call_card_t *out)
{
    char buf[CALL_CARD_FILE_MAX];
    size_t got;
    FILE *f = fopen(path, "r");

    if (!f) {
        if (errno != ENOENT) return 0;
        memset(out, 0, sizeof(*out));
        return 1;
    }
    /* Longer than a complete file can be: what fits here cannot parse. */
    got = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    return call_card_parse(buf, got, out);
}

static int call_card_timed(char state)
{
    return state == 'A' || state == 'H';
}

/* 0:00 past an hour keeps the seconds, like the phone does: 1:02:15. */
static void call_card_format_time(long seconds, char *out, size_t cap)
{
    long h, m, s;
    if (seconds < 0) seconds = 0;
    h = seconds / 3600;
    m = (seconds / 60) % 60;
    s = seconds % 60;
    if (h > 0) snprintf(out, cap, "%ld:%02ld:%02ld", h, m, s);
    else       snprintf(out, cap, "%02ld:%02ld", m, s);
}

/* Cuts on a character boundary and marks the cut, so a name never ends mid-letter. */
static void call_card_fit(const capsule_font_t *f, const char *in, int max_width,
                          char *out, size_t cap)
{
    static const char dots[] = "\xE2\x80\xA6";
    size_t n = strlen(in);

    snprintf(out, cap, "%s", in);
    if (capsule_text_width(f, out) <= max_width) return;
    if (n > cap - sizeof(dots)) n = cap - sizeof(dots);
    while (n > 0) {
        n--;
        while (n > 0 && ((unsigned char)in[n] & 0xC0) == 0x80) n--;
        memcpy(out, in, n);
        while (n > 0 && out[n - 1] == ' ') n--;
        memcpy(out + n, dots, sizeof(dots));
        if (capsule_text_width(f, out) <= max_width) return;
    }
}

static float call_card_clamp(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* Premultiplied source-over, the form DisplayManager composes with. */
static void call_card_over(unsigned char *px, int r, int g, int b, float a)
{
    float keep = 1.0f - a;
    px[0] = (unsigned char)(r * a + px[0] * keep + 0.5f);
    px[1] = (unsigned char)(g * a + px[1] * keep + 0.5f);
    px[2] = (unsigned char)(b * a + px[2] * keep + 0.5f);
    px[3] = (unsigned char)(255.0f * a + px[3] * keep + 0.5f);
}

static void call_card_text(const capsule_font_t *f, const char *s, float x, int baseline,
                           int r, int g, int b, unsigned char *scratch,
                           unsigned char *rgba, int w, int h)
{
    int i, n = w * h;
    memset(scratch, 0, (size_t)n);
    capsule_walk(f, s, x, baseline, scratch, w, h);
    for (i = 0; i < n; i++) {
        if (scratch[i])
            call_card_over(rgba + i * 4, r, g, b, scratch[i] / 255.0f);
    }
}

/* The look the driver picked from the mock-ups, variant A: blue-black glass, a
 * state dot, the name in bold, the state below.  rgba top row first; scratch is
 * w*h bytes.  With the 320x78 strip the numbers are those of the mock-up. */
static void call_card_render(const capsule_font_t *name_font, const capsule_font_t *status_font,
                             const call_card_t *card, long seconds,
                             unsigned char *scratch, unsigned char *rgba, int w, int h)
{
    static const float RADIUS = 18.0f, GLASS = 0.84f, DOT_R = 7.0f;
    float half_w = w / 2.0f, half_h = h / 2.0f;
    float dot_x = 24.0f, dot_y = h * 25.0f / 78.0f;
    int text_x = 42;
    int r, g, b, x, y;
    char shown[CALL_CARD_NAME_MAX];
    char status[CALL_CARD_LABEL_MAX + 32];

    memset(rgba, 0, (size_t)w * (size_t)h * 4u);
    if (!card->state) return;

    if (card->state == 'A')      { r = 64;  g = 214; b = 110; }
    else if (card->state == 'H') { r = 150; g = 158; b = 166; }
    else                         { r = 255; g = 164; b = 40;  }

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            float dx = fabsf(x + 0.5f - half_w) - (half_w - RADIUS);
            float dy = fabsf(y + 0.5f - half_h) - (half_h - RADIUS);
            float ax = dx > 0.0f ? dx : 0.0f, ay = dy > 0.0f ? dy : 0.0f;
            float d = sqrtf(ax * ax + ay * ay) - RADIUS;
            float cover = call_card_clamp(0.5f - d);
            float dot = call_card_clamp(0.5f - (hypotf(x + 0.5f - dot_x, y + 0.5f - dot_y) - DOT_R));
            unsigned char *px = rgba + (y * w + x) * 4;
            if (cover > 0.0f) call_card_over(px, 20, 30, 42, GLASS * cover);
            if (dot > 0.0f) call_card_over(px, r, g, b, dot);
        }
    }

    call_card_fit(name_font, card->name, w - text_x - 20, shown, sizeof(shown));
    call_card_text(name_font, shown, (float)text_x, h * 35 / 78, 255, 255, 255, scratch, rgba, w, h);

    if (call_card_timed(card->state)) {
        char clock[24];
        call_card_format_time(seconds, clock, sizeof(clock));
        snprintf(status, sizeof(status), "%s \xC2\xB7 %s", card->label, clock);
    } else {
        snprintf(status, sizeof(status), "%s", card->label);
    }
    call_card_text(status_font, status, (float)text_x, h * 63 / 78, 214, 222, 230, scratch, rgba, w, h);
}

#endif /* K3346_CALL_CARD_H */
