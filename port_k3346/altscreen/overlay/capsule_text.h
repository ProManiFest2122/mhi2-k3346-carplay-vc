/*
 * capsule_text — the text we draw at the two ends of the VC NavStatus bar.
 *
 * The VC draws that bar itself, over our picture, and its text field is one
 * line of about 359 px in the middle (RE luka-dev, VC_LAYOUT_AND_FONTS.md).
 * Arrival, driving time, street and distance do not fit there, and the elements
 * at the ends take a number and a unit only.  So with small clocks the street
 * stays in the VC field and we draw the other two parts on our own canvas.  They
 * lie UNDER the bar's translucent base, which makes them darker than VC text.
 * Nothing we can do from the MMI changes that.
 *
 * No GL in this file, so overlay/tests/capsule_text_test.c runs all of it on the
 * host.  platform_harman.c owns the texture and the draw call.
 *
 * THE FILES.  /tmp on QNX is /dev/shmem, which has no rename() (EXDEV, car run
 * of 2026-10-27, see overlay_ready.h).  So a reader can catch a file between
 * truncate and write.  Java's writer does one write() of the whole text, and
 * the format ends with a marker: without it the file is "not ready yet" and the
 * previous text stays on screen.
 *
 *   /tmp/vc-capsule-text  Java -> overlay   "<left>\n<right>\nEND\n"
 *   /tmp/vc-capsule-slot  overlay -> Java   "<slot> <draw>\n"
 *   /tmp/vc-capsule-off   script -> overlay exists = our layer is not on the VC
 *
 * A missing text file means nothing to draw.  A missing slot file tells Java
 * the overlay is not running, and it keeps all the text in the VC field.
 */

#ifndef K3346_CAPSULE_TEXT_H
#define K3346_CAPSULE_TEXT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* Public domain, pinned: see PROVENANCE.md.  Not our code, so its warnings stay
 * out of our -Werror host build. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#pragma GCC diagnostic pop

#define CAPSULE_TEXT_PATH "/tmp/vc-capsule-text"
#define CAPSULE_SLOT_PATH "/tmp/vc-capsule-slot"
#define CAPSULE_OFF_PATH  "/tmp/vc-capsule-off"

#define CAPSULE_LINE_MAX  96
#define CAPSULE_FILE_MAX  256
#define CAPSULE_GLYPH_MAX 128   /* one glyph's bitmap, per side */

typedef struct {
    char left[CAPSULE_LINE_MAX];
    char right[CAPSULE_LINE_MAX];
} capsule_lines_t;

typedef struct {
    unsigned char *data;
    stbtt_fontinfo info;
    float scale;
    int ascent, descent;
} capsule_font_t;

static int capsule_copy_line(char *dst, const char *start, const char *end)
{
    size_t n = (size_t)(end - start);
    if (n >= CAPSULE_LINE_MAX) return 0;
    memcpy(dst, start, n);
    dst[n] = '\0';
    return 1;
}

/* Returns 1 for a complete file, 0 for anything else.  On 0 *out is untouched,
 * because the caller keeps showing the previous text. */
static int capsule_parse(const char *buf, size_t len, capsule_lines_t *out)
{
    static const char mark[] = "END\n";
    capsule_lines_t parsed;
    const char *end = buf + len;
    const char *nl1, *nl2;

    if (memchr(buf, '\0', len)) return 0;
    nl1 = memchr(buf, '\n', len);
    if (!nl1) return 0;
    nl2 = memchr(nl1 + 1, '\n', (size_t)(end - (nl1 + 1)));
    if (!nl2) return 0;
    if ((size_t)(end - (nl2 + 1)) != sizeof(mark) - 1) return 0;
    if (memcmp(nl2 + 1, mark, sizeof(mark) - 1) != 0) return 0;
    if (!capsule_copy_line(parsed.left, buf, nl1)) return 0;
    if (!capsule_copy_line(parsed.right, nl1 + 1, nl2)) return 0;
    *out = parsed;
    return 1;
}

/* Returns 1 when *out holds the current text (a missing file is empty text). */
static int capsule_read_file(const char *path, capsule_lines_t *out)
{
    char buf[CAPSULE_FILE_MAX];
    size_t got;
    FILE *f = fopen(path, "r");

    if (!f) {
        if (errno != ENOENT) return 0;
        out->left[0] = '\0';
        out->right[0] = '\0';
        return 1;
    }
    /* Longer than a complete file can be: what fits here cannot parse. */
    got = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    return capsule_parse(buf, got, out);
}

/* One code point, -1 for a byte that does not start a valid sequence.  Overlong
 * forms and surrogates are invalid.  *p always moves forward. */
static long capsule_utf8_next(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    long cp;
    int extra, i;

    if (s[0] < 0x80)      { *p += 1; return s[0]; }
    if (s[0] >= 0xC2 && s[0] <= 0xDF) { extra = 1; cp = s[0] & 0x1F; }
    else if ((s[0] & 0xF0) == 0xE0)   { extra = 2; cp = s[0] & 0x0F; }
    else if (s[0] >= 0xF0 && s[0] <= 0xF4) { extra = 3; cp = s[0] & 0x07; }
    else { *p += 1; return -1; }

    for (i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) { *p += 1; return -1; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    if ((extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)
        || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        *p += 1;
        return -1;
    }
    *p += extra + 1;
    return cp;
}

static int capsule_font_load(capsule_font_t *f, const char *path, int px)
{
    FILE *fp;
    long size;
    int line_gap = 0;

    memset(f, 0, sizeof(*f));
    fp = fopen(path, "rb");
    if (!fp) return -1;
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) <= 0
        || size > 8L * 1024L * 1024L || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    f->data = (unsigned char *)malloc((size_t)size);
    if (!f->data || fread(f->data, 1, (size_t)size, fp) != (size_t)size) {
        fclose(fp);
        free(f->data);
        f->data = NULL;
        return -1;
    }
    fclose(fp);
    if (!stbtt_InitFont(&f->info, f->data, stbtt_GetFontOffsetForIndex(f->data, 0))) {
        free(f->data);
        f->data = NULL;
        return -1;
    }
    f->scale = stbtt_ScaleForMappingEmToPixels(&f->info, (float)px);
    stbtt_GetFontVMetrics(&f->info, &f->ascent, &f->descent, &line_gap);
    return 0;
}

static void capsule_font_free(capsule_font_t *f)
{
    free(f->data);
    memset(f, 0, sizeof(*f));
}

/* The one place that walks a line: width and drawing cannot disagree.  A code
 * point the font does not have takes no room.  alpha == NULL only measures. */
static float capsule_walk(const capsule_font_t *f, const char *s, float pen_x,
                          int baseline, unsigned char *alpha, int w, int h)
{
    static unsigned char glyph_buf[CAPSULE_GLYPH_MAX * CAPSULE_GLYPH_MAX];
    float pen = pen_x;
    int prev = 0;

    while (*s) {
        long cp = capsule_utf8_next(&s);
        int glyph, adv = 0, lsb = 0;

        if (cp < 0) continue;
        glyph = stbtt_FindGlyphIndex(&f->info, (int)cp);
        if (glyph == 0) continue;
        if (prev)
            pen += f->scale * (float)stbtt_GetGlyphKernAdvance(&f->info, prev, glyph);
        stbtt_GetGlyphHMetrics(&f->info, glyph, &adv, &lsb);

        if (alpha) {
            int whole = (int)pen;
            float shift;
            int x0, y0, x1, y1, gw, gh, row, col;

            if (pen < 0.0f && (float)whole != pen) whole--;
            shift = pen - (float)whole;
            stbtt_GetGlyphBitmapBoxSubpixel(&f->info, glyph, f->scale, f->scale,
                                            shift, 0.0f, &x0, &y0, &x1, &y1);
            gw = x1 - x0;
            gh = y1 - y0;
            if (gw > 0 && gh > 0 && gw <= CAPSULE_GLYPH_MAX && gh <= CAPSULE_GLYPH_MAX) {
                stbtt_MakeGlyphBitmapSubpixel(&f->info, glyph_buf, gw, gh,
                                              CAPSULE_GLYPH_MAX, f->scale, f->scale,
                                              shift, 0.0f, glyph);
                for (row = 0; row < gh; row++) {
                    int dy = baseline + y0 + row;
                    if (dy < 0 || dy >= h) continue;
                    for (col = 0; col < gw; col++) {
                        int dx = whole + x0 + col;
                        int v;
                        if (dx < 0 || dx >= w) continue;
                        v = alpha[dy * w + dx] + glyph_buf[row * CAPSULE_GLYPH_MAX + col];
                        alpha[dy * w + dx] = (unsigned char)(v > 255 ? 255 : v);
                    }
                }
            }
        }
        pen += f->scale * (float)adv;
        prev = glyph;
    }
    return pen;
}

static int capsule_text_width(const capsule_font_t *f, const char *s)
{
    return (int)(capsule_walk(f, s, 0.0f, 0, NULL, 0, 0) + 0.999f);
}

/* Left text starts at column 0, right text ends at column w-1, both centred on
 * the row of the strip.  Anything that does not fit is clipped by the walk. */
static void capsule_render(const capsule_font_t *f, const capsule_lines_t *lines,
                           unsigned char *alpha, int w, int h)
{
    int baseline = (h + (int)(f->scale * (float)(f->ascent + f->descent))) / 2;

    memset(alpha, 0, (size_t)w * (size_t)h);
    capsule_walk(f, lines->left, 0.0f, baseline, alpha, w, h);
    capsule_walk(f, lines->right, (float)(w - capsule_text_width(f, lines->right)),
                 baseline, alpha, w, h);
}

/* White text, premultiplied: DisplayManager composes with premultiplied alpha
 * (conf key "alpha", staircase measured 2026-10-27). */
static void capsule_alpha_to_rgba(const unsigned char *alpha, unsigned char *rgba,
                                  int w, int h)
{
    int i, n = w * h;
    for (i = 0; i < n; i++) {
        rgba[i * 4 + 0] = alpha[i];
        rgba[i * 4 + 1] = alpha[i];
        rgba[i * 4 + 2] = alpha[i];
        rgba[i * 4 + 3] = alpha[i];
    }
}

/* Same corner order as overlay_blit_quad.  The strip is uploaded top row first,
 * and GL takes the first row as v=0, so the top of the quad is v=0. */
static void capsule_quad(int canvas_w, int canvas_h, int x, int y, int w, int h,
                         float pos[8], float uv[8])
{
    float x0 = 2.0f * (float)x / (float)canvas_w - 1.0f;
    float x1 = 2.0f * (float)(x + w) / (float)canvas_w - 1.0f;
    float y1 = 1.0f - 2.0f * (float)y / (float)canvas_h;
    float y0 = 1.0f - 2.0f * (float)(y + h) / (float)canvas_h;

    pos[0] = x0; pos[1] = y0;   uv[0] = 0.0f; uv[1] = 1.0f;
    pos[2] = x1; pos[3] = y0;   uv[2] = 1.0f; uv[3] = 1.0f;
    pos[4] = x0; pos[5] = y1;   uv[4] = 0.0f; uv[5] = 0.0f;
    pos[6] = x1; pos[7] = y1;   uv[6] = 1.0f; uv[7] = 0.0f;
}

/* draw = 0: we know the layout but cannot draw, Java keeps the text in the VC
 * field.  One open() + one write(), see overlay_ready_write. */
static int capsule_slot_write(const char *path, int slot, int draw)
{
    char buf[16];
    int fd, n, got;

    n = snprintf(buf, sizeof(buf), "%d %d\n", slot, draw);
    if (n < 0 || (size_t)n >= sizeof(buf)) return -1;
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    got = (int)write(fd, buf, (size_t)n);
    if (close(fd) != 0 || got != n) return -1;
    return 0;
}

#endif /* K3346_CAPSULE_TEXT_H */
