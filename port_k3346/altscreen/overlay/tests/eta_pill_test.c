/*
 * Host test for eta_pill.h - our own arrival-time box on the VC map.
 *
 * The file is the capsule format (its parser has capsule_text_test.c); here it is
 * the pill itself: how wide, where in the strip, which glass, and that no text
 * leaves the buffer.
 *
 * Built with the container's native gcc:  bash overlay/tests/run_host_tests.sh
 * Rasterising needs the MMI fonts, which we do not ship: /work, or SKIP.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>

#pragma GCC diagnostic ignored "-Wunused-function"
#include "../eta_pill.h"

#define FONT_NORMAL "/work/mmx_app/eso/browser/fonts/AudiTypeDisplay-Normal_14.ttf"
#define FONT_BOLD   "/work/mmx_app/eso/browser/fonts/AudiTypeDisplay-Bold_11.ttf"

static int g_pass, g_fail;

static void check(int cond, const char *what)
{
    if (cond) g_pass++;
    else { g_fail++; printf("  FAIL: %s\n", what); }
}

static void check_int(int got, int want, const char *what)
{
    if (got == want) g_pass++;
    else { g_fail++; printf("  FAIL: %s: got %d, want %d\n", what, got, want); }
}

static void check_str(const char *got, const char *want, const char *what)
{
    if (!strcmp(got, want)) g_pass++;
    else { g_fail++; printf("  FAIL: %s: got [%s], want [%s]\n", what, got, want); }
}

static void lines(capsule_lines_t *l, const char *left, const char *right)
{
    memset(l, 0, sizeof(*l));
    snprintf(l->left, sizeof(l->left), "%s", left);
    snprintf(l->right, sizeof(l->right), "%s", right);
}

/* first and last column with a pixel of the text (white and opaque) in columns
 * x0..x1; -1 when there is none */
static void text_span(const unsigned char *rgba, int w, int h, int x0, int x1, int *first, int *last)
{
    int x, y;
    *first = *last = -1;
    for (x = x0; x < x1; x++)
        for (y = 0; y < h; y++) {
            const unsigned char *px = rgba + (y * w + x) * 4;
            if (px[3] == 255 && px[0] == 255 && px[1] == 255 && px[2] == 255) {
                if (*first < 0) *first = x;
                *last = x;
                break;
            }
        }
}

/* first and last column that has any alpha in row y; -1 when the row is empty */
static void span(const unsigned char *rgba, int w, int y, int *first, int *last)
{
    int x;
    *first = *last = -1;
    for (x = 0; x < w; x++)
        if (rgba[(y * w + x) * 4 + 3]) { if (*first < 0) *first = x; *last = x; }
}

static long ink(const unsigned char *rgba, int w, int h)
{
    long total = 0;
    int i;
    for (i = 0; i < w * h; i++) total += rgba[i * 4 + 3];
    return total;
}

static void t_plain(void)
{
    capsule_lines_t l;
    char tail[128];
    const char *path = "eta_pill_ok.tmp";

    printf("names, styles, the text\n");
    check_str(ETA_PILL_PATH, "/tmp/vc-eta-pill", "the file BAPBridge writes");
    check_str(ETA_PILL_OK_PATH, "/tmp/vc-eta-pill-ok", "the file BAPBridge looks for");
    check_int(ETA_PILL_ALIGN_LEFT, 0, "left");
    check_int(ETA_PILL_ALIGN_CENTRE, 1, "centre");

    /* the look is the call card option's; with the card off the pill is smoked */
    check_int(eta_pill_style(CALL_CARD_STYLE_SMOKED), CALL_CARD_STYLE_SMOKED, "smoked");
    check_int(eta_pill_style(CALL_CARD_STYLE_LIGHT), CALL_CARD_STYLE_LIGHT, "light");
    check_int(eta_pill_style(CALL_CARD_STYLE_OFF), CALL_CARD_STYLE_SMOKED, "call card off: smoked");
    check_int(eta_pill_style(7), CALL_CARD_STYLE_SMOKED, "anything else: smoked");

    lines(&l, "", "");
    check_int(eta_pill_empty(&l), 1, "two empty lines: no pill");
    lines(&l, "06:43", "");
    check_int(eta_pill_empty(&l), 0, "the arrival alone is a pill");
    eta_pill_tail(&l, tail, sizeof(tail));
    check_str(tail, "", "no rest, no separator");
    lines(&l, "06:43", "15 min \xC2\xB7 12 km");
    eta_pill_tail(&l, tail, sizeof(tail));
    check_str(tail, " \xC2\xB7 15 min \xC2\xB7 12 km", "the rest follows the separator");
    lines(&l, "", "15 min");
    check_int(eta_pill_empty(&l), 0, "the rest alone is a pill");
    eta_pill_tail(&l, tail, sizeof(tail));
    check_str(tail, "15 min", "no arrival, no separator");

    /* where in the strip, and when the call card takes the place */
    check_int(eta_pill_align(1, 0), ETA_PILL_ALIGN_LEFT, "small gauges: by the rev counter");
    check_int(eta_pill_align(1, 1), ETA_PILL_ALIGN_LEFT, "small gauges, Sport: left");
    check_int(eta_pill_align(0, 0), ETA_PILL_ALIGN_CENTRE, "big gauges: between them");
    check_int(eta_pill_align(0, 1), ETA_PILL_ALIGN_LEFT, "big gauges, Sport: left of its rev counter");
    check_int(eta_pill_hidden_by_call(0, 1), 1, "big gauges, a call: the call card instead");
    check_int(eta_pill_hidden_by_call(0, 0), 0, "big gauges, no call: the pill");
    check_int(eta_pill_hidden_by_call(1, 1), 0, "small gauges, a call: side by side");
    check_int(eta_pill_hidden_by_call(1, 0), 0, "small gauges, no call");

    check_int(eta_pill_x0(300, 400, ETA_PILL_ALIGN_LEFT), 0, "left: from the first column");
    check_int(eta_pill_x0(300, 400, ETA_PILL_ALIGN_CENTRE), 50, "centre: the middle of the strip");
    check_int(eta_pill_x0(400, 400, ETA_PILL_ALIGN_CENTRE), 0, "centre, full width");

    /* the word to Java */
    remove(path);
    check_int(eta_pill_ok_write(path, 0), 0, "taking back what is not there is fine");
    check_int(eta_pill_ok_write(path, 1), 0, "written");
    check_int(access(path, F_OK), 0, "the file exists");
    check_int(eta_pill_ok_write(path, 1), 0, "written twice");
    check_int(eta_pill_ok_write(path, 0), 0, "taken back");
    check(access(path, F_OK) != 0, "the file is gone");
    check_int(eta_pill_ok_write("/nonexistent-dir/x", 1), -1, "unwritable reported");
}

static void t_render(void)
{
    enum { W = 400, H = 44, GUARD = 4096 };
    capsule_font_t bold, normal;
    capsule_lines_t l;
    unsigned char *mem_rgba, *rgba, *mem_scratch, *scratch;
    static unsigned char other[W * H * 4];
    int i, bad = 0, first, last, pw, text_w;

    printf("rasterising\n");
    if (capsule_font_load(&bold, FONT_BOLD, 24) != 0 || capsule_font_load(&normal, FONT_NORMAL, 22) != 0) {
        printf("  SKIP: no fonts here\n");
        return;
    }
    mem_rgba = (unsigned char *)malloc((size_t)(W * H * 4 + 2 * GUARD));
    mem_scratch = (unsigned char *)malloc((size_t)(W * H + 2 * GUARD));
    memset(mem_rgba, 0xA5, (size_t)(W * H * 4 + 2 * GUARD));
    memset(mem_scratch, 0xA5, (size_t)(W * H + 2 * GUARD));
    rgba = mem_rgba + GUARD;
    scratch = mem_scratch + GUARD;

    /* the usual text: the pill is as wide as it, at the left of the strip */
    lines(&l, "06:43", "15 min \xC2\xB7 12 km");
    text_w = capsule_text_width(&bold, "06:43")
           + capsule_text_width(&normal, " \xC2\xB7 15 min \xC2\xB7 12 km");
    pw = eta_pill_width(&bold, &normal, &l, W);
    check_int(pw, text_w + 2 * ETA_PILL_PAD, "the width is the text and the padding");
    check(pw > 200 && pw < W, "a pill, not the whole strip");
    eta_pill_render(&bold, &normal, &l, CALL_CARD_STYLE_SMOKED, ETA_PILL_ALIGN_LEFT, scratch, rgba, W, H);
    span(rgba, W, H / 2, &first, &last);
    check_int(first, 0, "left: starts at the first column");
    check_int(last, pw - 1, "left: ends where the pill does");
    check_int(rgba[3], 0, "the corner is empty");
    check_int(rgba[(H / 2 * W + pw + 2) * 4 + 3], 0, "nothing after the pill");
    /* smoked glass (84,92,106) at 0.62, premultiplied, where there is no text */
    check_int(rgba[(H / 2 * W + 4) * 4 + 3], 158, "smoked alpha");
    check_int(rgba[(H / 2 * W + 4) * 4 + 0], 52, "smoked red");
    check_int(rgba[(H / 2 * W + 4) * 4 + 1], 57, "smoked green");
    check_int(rgba[(H / 2 * W + 4) * 4 + 2], 66, "smoked blue");
    check_int(rgba[(2 * W + 4) * 4 + 3], 0, "round to half the height");
    for (i = 0; i < W * H; i++) {
        const unsigned char *px = rgba + i * 4;
        if (px[0] > px[3] || px[1] > px[3] || px[2] > px[3]) bad++;
    }
    check_int(bad, 0, "every pixel premultiplied");
    /* white text: somewhere inside there are opaque white pixels */
    {
        int white = 0, x, y;
        for (y = 0; y < H; y++)
            for (x = ETA_PILL_PAD; x < pw - ETA_PILL_PAD; x++) {
                const unsigned char *px = rgba + (y * W + x) * 4;
                if (px[3] == 255 && px[0] == 255 && px[1] == 255 && px[2] == 255) white++;
            }
        check(white > 100, "the text is white and opaque");
        /* 20 px of glass before the text and after it */
        check_int(ETA_PILL_PAD, 20, "the padding of the mock-up");
        text_span(rgba, W, H, 0, W, &first, &last);
        check(first >= ETA_PILL_PAD - 1 && first <= ETA_PILL_PAD + 3, "the text starts after the padding");
        check(last <= pw - ETA_PILL_PAD && last >= pw - ETA_PILL_PAD - 6, "the text ends before the padding");
        /* both parts are drawn: there is text right of the arrival time too */
        text_span(rgba, W, H, ETA_PILL_PAD + capsule_text_width(&bold, "06:43") + 12, W, &first, &last);
        check(first > 0, "the time left and the distance are drawn");
        /* nothing but glass in the padding */
        for (y = H / 2 - 2, bad = 0; y <= H / 2 + 2; y++)
            for (x = pw - ETA_PILL_PAD + 2; x < pw - 2; x++)
                if (rgba[(y * W + x) * 4 + 3] != 158) bad++;
        check_int(bad, 0, "the padding after the text is plain glass");
    }

    /* centred: the same pill in the middle of the strip */
    eta_pill_render(&bold, &normal, &l, CALL_CARD_STYLE_SMOKED, ETA_PILL_ALIGN_CENTRE, scratch, other, W, H);
    span(other, W, H / 2, &first, &last);
    check_int(first, (W - pw) / 2, "centre: starts half the rest in");
    check_int(last, (W - pw) / 2 + pw - 1, "centre: as wide");
    check(ink(other, W, H) == ink(rgba, W, H), "centre: the same picture, moved");
    {
        int x, y, x0 = (W - pw) / 2;
        for (y = 0, bad = 0; y < H; y++)
            for (x = 0; x < pw; x++)
                if (memcmp(rgba + (y * W + x) * 4, other + (y * W + x0 + x) * 4, 4)) bad++;
        check_int(bad, 0, "centre: pixel for pixel");
    }

    /* light glass: white at 0.36; the call card off gives the smoked one */
    eta_pill_render(&bold, &normal, &l, CALL_CARD_STYLE_LIGHT, ETA_PILL_ALIGN_LEFT, scratch, other, W, H);
    check_int(other[(H / 2 * W + 4) * 4 + 3], 92, "light alpha");
    check_int(other[(H / 2 * W + 4) * 4 + 0], 92, "light is white");
    eta_pill_render(&bold, &normal, &l, CALL_CARD_STYLE_OFF, ETA_PILL_ALIGN_LEFT, scratch, other, W, H);
    check(memcmp(other, rgba, sizeof(other)) == 0, "call card off: the smoked pill");

    /* the text decides the width */
    {
        capsule_lines_t longer, shorter;
        lines(&longer, "13:05", "1 h 05 \xC2\xB7 1234 km");
        lines(&shorter, "06:43", "");
        check(eta_pill_width(&bold, &normal, &longer, W) > pw, "more text, a wider pill");
        check(eta_pill_width(&bold, &normal, &shorter, W) < pw, "less text, a narrower pill");
        check_int(eta_pill_width(&bold, &normal, &shorter, W),
                  capsule_text_width(&bold, "06:43") + 2 * ETA_PILL_PAD, "the arrival alone");
        eta_pill_render(&bold, &normal, &shorter, CALL_CARD_STYLE_SMOKED, ETA_PILL_ALIGN_LEFT,
                        scratch, other, W, H);
        span(other, W, H / 2, &first, &last);
        check_int(last, capsule_text_width(&bold, "06:43") + 2 * ETA_PILL_PAD - 1, "the short pill ends early");
    }

    /* the arrival is bold: its ink is as wide as the bold face makes it, which is
     * wider than the lighter one */
    {
        capsule_lines_t only;
        int bw = capsule_text_width(&bold, "06:43"), nw = capsule_text_width(&normal, "06:43");
        lines(&only, "06:43", "");
        eta_pill_render(&bold, &normal, &only, CALL_CARD_STYLE_SMOKED, ETA_PILL_ALIGN_LEFT,
                        scratch, other, W, H);
        text_span(other, W, H, 0, W, &first, &last);
        check(bw >= nw + 3, "bold is wider");
        check(last >= ETA_PILL_PAD + bw - 4, "the arrival is drawn in bold");
    }

    /* no text: the strip is cleared */
    lines(&l, "", "");
    check_int(eta_pill_width(&bold, &normal, &l, W), 0, "no text, no width");
    eta_pill_render(&bold, &normal, &l, CALL_CARD_STYLE_LIGHT, ETA_PILL_ALIGN_CENTRE, scratch, rgba, W, H);
    check(ink(rgba, W, H) == 0, "no text clears the strip");

    /* more than the strip holds: the pill is the strip, the rest ends with the mark,
     * and nothing is written past the buffers */
    memset(&l, 0, sizeof(l));
    snprintf(l.left, sizeof(l.left), "12:59 PM");
    memset(l.right, 'W', sizeof(l.right) - 1);
    check_int(eta_pill_width(&bold, &normal, &l, W), W, "never wider than the strip");
    eta_pill_render(&bold, &normal, &l, CALL_CARD_STYLE_SMOKED, ETA_PILL_ALIGN_CENTRE, scratch, rgba, W, H);
    span(rgba, W, H / 2, &first, &last);
    check_int(first, 0, "full strip: from the first column");
    check_int(last, W - 1, "full strip: to the last");
    for (i = H / 2 - 2, bad = 0; i <= H / 2 + 2; i++) {
        int x;
        for (x = W - ETA_PILL_PAD + 4; x < W - 2; x++)
            if (rgba[(i * W + x) * 4 + 3] != 158) bad++;
    }
    check_int(bad, 0, "long text: the end of the pill stays glass");
    memset(l.left, 'W', sizeof(l.left) - 1);
    eta_pill_render(&bold, &normal, &l, CALL_CARD_STYLE_LIGHT, ETA_PILL_ALIGN_LEFT, scratch, rgba, W, H);
    for (i = 0, bad = 0; i < GUARD; i++) {
        if (mem_rgba[i] != 0xA5 || mem_rgba[GUARD + W * H * 4 + i] != 0xA5) bad++;
        if (mem_scratch[i] != 0xA5 || mem_scratch[GUARD + W * H + i] != 0xA5) bad++;
    }
    check_int(bad, 0, "guard bands intact");

    /* a strip smaller than the fonts stays inside too */
    {
        enum { LOW_W = 160, LOW_H = 20, M = 4096 };
        unsigned char *low = (unsigned char *)malloc((size_t)(LOW_W * LOW_H * 4 + 2 * M));
        memset(low, 0xA5, (size_t)(LOW_W * LOW_H * 4 + 2 * M));
        lines(&l, "06:43", "15 min \xC2\xB7 12 km");
        eta_pill_render(&bold, &normal, &l, CALL_CARD_STYLE_SMOKED, ETA_PILL_ALIGN_CENTRE,
                        scratch, low + M, LOW_W, LOW_H);
        for (i = 0, bad = 0; i < M; i++)
            if (low[i] != 0xA5 || low[M + LOW_W * LOW_H * 4 + i] != 0xA5) bad++;
        check_int(bad, 0, "small strip: guard bands intact");
        free(low);
    }

    free(mem_rgba);
    free(mem_scratch);
    capsule_font_free(&bold);
    capsule_font_free(&normal);
}

int main(void)
{
    t_plain();
    t_render();
    printf("WYNIK_ETA_PILL: %d OK, %d FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
