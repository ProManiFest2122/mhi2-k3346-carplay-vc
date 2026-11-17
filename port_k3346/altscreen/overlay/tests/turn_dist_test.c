/*
 * Host test for turn_dist.h - the distance to the maneuver we print while the VC
 * hides its own.
 *
 * The file is the capsule format (its parser has capsule_text_test.c); here it is
 * the strip itself: that it lies on the tile, the dark band, the dot, the number
 * and its unit centred as one, that nothing is drawn without a number, and that
 * no text leaves the buffer.
 *
 * Built with the container's native gcc:  bash overlay/tests/run_host_tests.sh
 * Rasterising needs the MMI font, which we do not ship: /work, or SKIP.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>

#pragma GCC diagnostic ignored "-Wunused-function"
#include "../turn_dist.h"

#define FONT_NORMAL "/work/mmx_app/eso/browser/fonts/AudiTypeDisplay-Normal_14.ttf"

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

static long ink(const unsigned char *rgba, int w, int h)
{
    long total = 0;
    int i;
    for (i = 0; i < w * h; i++) total += rgba[i * 4 + 3];
    return total;
}

/* what a pixel is, whatever is under it: the band is dark, the text white, the
 * dot cyan */
static int is_white(const unsigned char *px) { return px[0] >= 128; }
static int is_cyan(const unsigned char *px)  { return px[2] >= 128 && px[0] < 100; }

/* the box of every pixel of one kind in columns x0..x1-1; -1 when there is none */
static void box(const unsigned char *rgba, int w, int h, int x0, int x1,
                int (*kind)(const unsigned char *),
                int *left, int *right, int *top, int *bottom)
{
    int x, y;
    *left = *right = *top = *bottom = -1;
    for (y = 0; y < h; y++)
        for (x = x0; x < x1; x++)
            if (kind(rgba + (y * w + x) * 4)) {
                if (*left < 0 || x < *left) *left = x;
                if (x > *right) *right = x;
                if (*top < 0) *top = y;
                *bottom = y;
            }
}

static void t_plain(void)
{
    capsule_lines_t l;
    const char *path = "turn_dist.tmp";
    FILE *f;

    printf("names, the file, when there is nothing to print\n");
    check_str(TURN_DIST_PATH, "/tmp/vc-turn-dist", "the file BAPBridge writes");
    check_int(TURN_DIST_BASE, 6, "the baseline is 6 rows above the bottom (the conf counts on it)");
    check_int(turn_dist_baseline(28), 22, "baseline of the default strip");
    check_int(TURN_DIST_DOT_W, (int)(2 * TURN_DIST_DOT_R), "the dot is as wide as twice its radius");

    lines(&l, "", "");
    check_int(turn_dist_empty(&l), 1, "two empty lines: nothing");
    lines(&l, "", "m");
    check_int(turn_dist_empty(&l), 1, "a unit without a number: nothing");
    lines(&l, "200", "m");
    check_int(turn_dist_empty(&l), 0, "a number and a unit");
    lines(&l, "200", "");
    check_int(turn_dist_empty(&l), 0, "a number alone is printed");

    /* what Java writes, read with the parser the overlay uses */
    f = fopen(path, "w");
    fputs("250\nm\nEND\n", f);
    fclose(f);
    memset(&l, 0, sizeof(l));
    check_int(capsule_read_file(path, &l), 1, "Java's file parses");
    check_str(l.left, "250", "the number");
    check_str(l.right, "m", "the unit");
    f = fopen(path, "w");
    fputs("\n\nEND\n", f);
    fclose(f);
    check_int(capsule_read_file(path, &l), 1, "Java's empty file parses");
    check_int(turn_dist_empty(&l), 1, "and means nothing to print");
    lines(&l, "90", "m");
    f = fopen(path, "w");
    fputs("250\nm\n", f);
    fclose(f);
    check_int(capsule_read_file(path, &l), 0, "a file caught half written is not read");
    check_str(l.left, "90", "and leaves what was there");
    remove(path);
    check_int(capsule_read_file(path, &l), 1, "no file reads as empty");
    check_int(turn_dist_empty(&l), 1, "no file: nothing to print");
}

/* The strip lies on the tile.  2.0.1-rc4 put it above the tile, where the VC's own
 * header covers it: the car showed nothing (run-11). */
static void t_place(void)
{
    int x = -7, y = -7;

    printf("the place: on the top rows of the tile\n");
    check_int(turn_dist_place(1091, 110, 210, 0, 0, 28, 1440, 542, &x, &y), 1, "small gauges: on the canvas");
    check_int(x, 1091, "small gauges: the left edge of the tile");
    check_int(y, 110, "small gauges: the top row of the tile, not above it");
    check_int(turn_dist_place(1055, 207, 210, 0, 0, 28, 1440, 542, &x, &y), 1, "big gauges: on the canvas");
    check_int(x, 1055, "big gauges: the left edge of the tile");
    check_int(y, 207, "big gauges: the top row of the tile");
    check_int(turn_dist_place(984, 139, 328, 0, 0, 28, 1440, 542, &x, &y), 1, "sport: on the canvas");
    check_int(x, 984, "sport: the left edge of its tile");
    check_int(y, 139, "sport: the top row of its tile");

    check_int(turn_dist_place(1091, 110, 210, -5, 9, 28, 1440, 542, &x, &y), 1, "a correction from the card");
    check_int(x, 1086, "dx moves it left");
    check_int(y, 119, "dy moves it down");

    check_int(turn_dist_place(0, 0, 210, 0, 0, 28, 210, 153, &x, &y), 1, "the emergency canvas is the tile: it fits");
    check_int(turn_dist_place(3, 0, 210, -3, 0, 28, 1440, 542, &x, &y), 1, "at the left edge");
    check_int(turn_dist_place(3, 0, 210, -4, 0, 28, 1440, 542, &x, &y), 0, "left of the canvas: not drawn");
    check_int(turn_dist_place(0, 3, 210, 0, -3, 28, 1440, 542, &x, &y), 1, "at the top edge");
    check_int(turn_dist_place(0, 3, 210, 0, -4, 28, 1440, 542, &x, &y), 0, "above the canvas: not drawn");
    check_int(turn_dist_place(1230, 0, 210, 0, 0, 28, 1440, 542, &x, &y), 1, "at the right edge");
    check_int(turn_dist_place(1230, 0, 210, 1, 0, 28, 1440, 542, &x, &y), 0, "past the right edge: not drawn");
    check_int(turn_dist_place(0, 514, 210, 0, 0, 28, 1440, 542, &x, &y), 1, "at the bottom edge");
    check_int(turn_dist_place(0, 514, 210, 0, 1, 28, 1440, 542, &x, &y), 0, "past the bottom edge: not drawn");
    check_int(turn_dist_place(100, 100, 0, 0, 0, 28, 1440, 542, &x, &y), 0, "a tile without a width: not drawn");

    check_int(turn_dist_start(210, 70), 70, "centred: the same room on both sides");
    check_int(turn_dist_start(210, 71), 69, "an odd width: one column more on the right");
    check_int(turn_dist_start(60, 100), 0, "wider than the strip: from its first column, never before it");
    check_int(turn_dist_text_x(70, 0), 70, "no dot: the number starts where the content does");
    check_int(turn_dist_text_x(70, 1), 70 + TURN_DIST_DOT_W + TURN_DIST_DOT_GAP,
              "a dot: the number starts after it and its gap");
}

static void t_render(void)
{
    enum { W = 210, H = 28, WIDE = 328, GUARD = 4096 };
    capsule_font_t number, unit;
    capsule_lines_t l;
    unsigned char *mem_rgba, *rgba, *mem_scratch, *scratch;
    static unsigned char other[WIDE * H * 4];
    int i, bad = 0, left, right, top, bottom, nw, uw, content, start, text_x, ux;
    int base = turn_dist_baseline(H);

    printf("rasterising\n");
    if (capsule_font_load(&number, FONT_NORMAL, 26) != 0 || capsule_font_load(&unit, FONT_NORMAL, 18) != 0) {
        printf("  SKIP: no fonts here\n");
        return;
    }
    mem_rgba = (unsigned char *)malloc((size_t)(WIDE * H * 4 + 2 * GUARD));
    mem_scratch = (unsigned char *)malloc((size_t)(WIDE * H + 2 * GUARD));
    memset(mem_rgba, 0xA5, (size_t)(WIDE * H * 4 + 2 * GUARD));
    memset(mem_scratch, 0xA5, (size_t)(WIDE * H + 2 * GUARD));
    rgba = mem_rgba + GUARD;
    scratch = mem_scratch + GUARD;

    /* "● 200 m" as shipped: the band, the dot, the number, the unit */
    lines(&l, "200", "m");
    nw = capsule_text_width(&number, "200");
    uw = capsule_text_width(&unit, "m");
    content = turn_dist_width(&number, &unit, &l, 1);
    check_int(content, TURN_DIST_DOT_W + TURN_DIST_DOT_GAP + nw + TURN_DIST_GAP + uw,
              "the width is the dot, the number, the unit and the gaps");
    check_int(turn_dist_width(&number, &unit, &l, 0), nw + TURN_DIST_GAP + uw, "without the dot: the text and its gap");
    check(content < W, "200 m fits the tile");
    start = turn_dist_start(W, content);
    text_x = turn_dist_text_x(start, 1);
    ux = turn_dist_unit_x(text_x, &number, &l);
    check_int(ux, text_x + nw + TURN_DIST_GAP, "the unit follows the number and the gap");
    turn_dist_render(&number, &unit, &l, 1, 85, scratch, rgba, W, H);

    for (i = 0, bad = 0; i < W * H; i++)
        if (rgba[i * 4 + 3] < 217) bad++;
    check_int(bad, 0, "the band covers every pixel of the strip");
    check_int(rgba[0], 32, "the band is the VC's dark grey, premultiplied: red");
    check_int(rgba[1], 32, "green");
    check_int(rgba[2], 34, "blue");
    check_int(rgba[3], 217, "85 percent");
    check(!memcmp(rgba, rgba + ((H - 1) * W + (W - 1)) * 4, 4), "the last pixel is band as the first");

    box(rgba, W, H, 0, W, is_cyan, &left, &right, &top, &bottom);
    check(left >= start - 1 && left <= start + 2, "the dot starts where the content does");
    check(right - left + 1 >= 12 && right - left + 1 <= 15, "the dot is 14 wide");
    check(top > 0 && bottom < base, "the dot is on the line of the digits");
    {
        const unsigned char *px = rgba + ((base - 10) * W + start + 7) * 4;
        check(px[3] == 255 && px[2] > px[0] + 100 && px[1] > px[0] + 100, "the dot is cyan and opaque");
    }
    box(rgba, W, H, 0, ux - 1, is_white, &left, &right, &top, &bottom);
    check(left >= text_x - 1 && left <= text_x + 4, "the number starts after the dot and its gap");
    check(right <= text_x + nw, "the number ends before the gap");
    check(bottom >= base - 2 && bottom <= base, "the digits stand on the baseline");
    check(top >= 1 && top <= 6, "the digits are about 19 rows high, inside the strip");
    {
        int number_top = top, uleft, uright, utop, ubottom;
        box(rgba, W, H, ux - 1, W, is_white, &uleft, &uright, &utop, &ubottom);
        check(uleft >= ux - 1 && uleft <= ux + 4, "the unit starts after the gap");
        check(uright < start + content + 1, "the unit ends where the width says");
        check(ubottom >= base - 2 && ubottom <= base, "the unit shares the baseline");
        check(utop > number_top + 4, "the unit is lower than the digits: a smaller face");
    }
    check(abs(start - (W - (start + content))) <= 1, "centred: the same room left and right");

    /* the band in percent: none, opaque, and past opaque */
    turn_dist_render(&number, &unit, &l, 0, 0, scratch, other, W, H);
    check_int(other[3], 0, "bg 0: no band, the corner is empty");
    for (i = 0, bad = 0; i < W * H; i++) {
        const unsigned char *px = other + i * 4;
        if (px[0] != px[3] || px[1] != px[3] || px[2] != px[3]) bad++;
    }
    check_int(bad, 0, "bg 0, no dot: white, premultiplied, nothing behind it");
    check(ink(other, W, H) > 0, "bg 0: the text is still drawn");
    turn_dist_render(&number, &unit, &l, 0, 100, scratch, other, W, H);
    check(other[0] == 38 && other[1] == 38 && other[2] == 40 && other[3] == 255, "bg 100: the VC's grey, opaque");
    turn_dist_render(&number, &unit, &l, 0, 250, scratch, rgba, W, H);
    check(!memcmp(rgba, other, (size_t)(W * H * 4)), "bg above 100 is 100");

    /* the band does not move the text */
    {
        int al, ar, at, ab, bl, br, bt, bb;
        turn_dist_render(&number, &unit, &l, 1, 0, scratch, rgba, W, H);
        box(rgba, W, H, 0, W, is_white, &al, &ar, &at, &ab);
        turn_dist_render(&number, &unit, &l, 1, 85, scratch, other, W, H);
        box(other, W, H, 0, W, is_white, &bl, &br, &bt, &bb);
        check(al == bl && ar == br && at == bt && ab == bb, "with and without the band: the text in one place");
    }

    /* without the dot the text alone is centred: half the dot's room further left */
    {
        int with_l, without_l;
        turn_dist_render(&number, &unit, &l, 1, 85, scratch, rgba, W, H);
        box(rgba, W, H, 0, W, is_white, &with_l, &right, &top, &bottom);
        turn_dist_render(&number, &unit, &l, 0, 85, scratch, other, W, H);
        box(other, W, H, 0, W, is_white, &without_l, &right, &top, &bottom);
        check(abs((with_l - without_l) - (TURN_DIST_DOT_W + TURN_DIST_DOT_GAP) / 2) <= 1,
              "no dot: the text moves left by half of the dot's room");
        box(other, W, H, 0, W, is_cyan, &left, &right, &top, &bottom);
        check_int(left, -1, "no dot when the conf says so");
    }

    /* a number alone */
    lines(&l, "200", "");
    check_int(turn_dist_width(&number, &unit, &l, 1), TURN_DIST_DOT_W + TURN_DIST_DOT_GAP + nw,
              "no unit: the dot and the number");
    check_int(turn_dist_width(&number, &unit, &l, 0), nw, "no unit, no dot: the number");
    turn_dist_render(&number, &unit, &l, 0, 85, scratch, other, W, H);
    start = turn_dist_start(W, nw);
    box(other, W, H, start + nw + 1, W, is_white, &left, &right, &top, &bottom);
    check_int(left, -1, "no unit, nothing after the number");

    /* more or fewer digits: still centred, on the same line */
    {
        capsule_lines_t a, b;
        int al, ar, at, ab, bl, br, bt, bb, cl, cr, ct, cb;
        lines(&a, "90", "m");
        lines(&b, "600", "m");
        check(turn_dist_width(&number, &unit, &a, 1) < turn_dist_width(&number, &unit, &b, 1), "more digits, more width");
        turn_dist_render(&number, &unit, &a, 1, 85, scratch, rgba, W, H);
        box(rgba, W, H, 0, W, is_white, &al, &ar, &at, &ab);
        box(rgba, W, H, 0, W, is_cyan, &cl, &cr, &ct, &cb);
        check(abs(cl - (W - 1 - ar)) <= 3, "90 m: the dot and the end of the unit are as far from the edges");
        turn_dist_render(&number, &unit, &b, 1, 85, scratch, other, W, H);
        box(other, W, H, 0, W, is_white, &bl, &br, &bt, &bb);
        box(other, W, H, 0, W, is_cyan, &cl, &cr, &ct, &cb);
        check(abs(cl - (W - 1 - br)) <= 3, "600 m: the same");
        check(bl < al && br > ar, "the longer one reaches further both ways");
        check_int(ab, bb, "same baseline");
    }

    /* the Sport tile is wider: the same strip drawn for its width */
    lines(&l, "200", "m");
    content = turn_dist_width(&number, &unit, &l, 1);
    turn_dist_render(&number, &unit, &l, 1, 85, scratch, rgba, WIDE, H);
    for (i = 0, bad = 0; i < WIDE * H; i++)
        if (rgba[i * 4 + 3] < 217) bad++;
    check_int(bad, 0, "wide tile: the band covers all of it");
    box(rgba, WIDE, H, 0, WIDE, is_cyan, &left, &right, &top, &bottom);
    check(abs(left - turn_dist_start(WIDE, content)) <= 2, "wide tile: centred for its own width");
    check(left > turn_dist_start(W, content) + 40, "and not where the narrow tile had it");

    /* no number: the strip is cleared, whatever was in it */
    lines(&l, "", "");
    check_int(turn_dist_width(&number, &unit, &l, 1), 0, "no number, no width");
    memset(rgba, 0x7F, (size_t)(W * H * 4));
    turn_dist_render(&number, &unit, &l, 1, 85, scratch, rgba, W, H);
    check(ink(rgba, W, H) == 0, "no number clears the strip: no band, no dot");
    lines(&l, "", "m");
    turn_dist_render(&number, &unit, &l, 1, 85, scratch, rgba, W, H);
    check(ink(rgba, W, H) == 0, "a unit without a number draws nothing, the band neither");

    /* more than the strip holds: cut, and nothing past the buffers */
    memset(mem_rgba, 0xA5, (size_t)(WIDE * H * 4 + 2 * GUARD));
    memset(mem_scratch, 0xA5, (size_t)(WIDE * H + 2 * GUARD));
    memset(&l, 0, sizeof(l));
    memset(l.left, '8', sizeof(l.left) - 1);
    memset(l.right, 'W', sizeof(l.right) - 1);
    turn_dist_render(&number, &unit, &l, 1, 85, scratch, rgba, W, H);
    box(rgba, W, H, 0, W, is_cyan, &left, &right, &top, &bottom);
    check(left >= 0 && left <= 2, "long text: from the first column of the strip");
    box(rgba, W, H, 0, W, is_white, &left, &right, &top, &bottom);
    check(right >= W - 20, "long text: drawn as far as the strip goes");
    for (i = 0, bad = 0; i < GUARD; i++) {
        if (mem_rgba[i] != 0xA5 || mem_rgba[GUARD + W * H * 4 + i] != 0xA5) bad++;
        if (mem_scratch[i] != 0xA5 || mem_scratch[GUARD + W * H + i] != 0xA5) bad++;
    }
    check_int(bad, 0, "guard bands intact");

    /* the smallest strip the conf allows, on a narrow tile, stays inside too */
    {
        enum { LOW_W = 60, LOW_H = 20, M = 4096 };
        unsigned char *low = (unsigned char *)malloc((size_t)(LOW_W * LOW_H * 4 + 2 * M));
        memset(low, 0xA5, (size_t)(LOW_W * LOW_H * 4 + 2 * M));
        lines(&l, "200", "m");
        turn_dist_render(&number, &unit, &l, 1, 85, scratch, low + M, LOW_W, LOW_H);
        for (i = 0, bad = 0; i < M; i++)
            if (low[i] != 0xA5 || low[M + LOW_W * LOW_H * 4 + i] != 0xA5) bad++;
        check_int(bad, 0, "small strip: guard bands intact");
        free(low);
    }

    free(mem_rgba);
    free(mem_scratch);
    capsule_font_free(&number);
    capsule_font_free(&unit);
}

int main(void)
{
    t_plain();
    t_place();
    t_render();
    printf("WYNIK_TURN_DIST: %d OK, %d FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
