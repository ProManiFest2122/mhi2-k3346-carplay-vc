/*
 * Host test for capsule_text.h - the text at the two ends of the VC bar.
 *
 * The file format matters most: /tmp on QNX has no rename(), so the overlay can
 * read the file half written, and that must keep the previous text on screen.
 *
 * Built with the container's native gcc:  bash overlay/tests/run_host_tests.sh
 * Rasterising needs the MMI font, which we do not ship: CAPSULE_TEST_FONT, by
 * default the firmware copy under /work.  Without it those cases print SKIP.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../capsule_text.h"

#define DEFAULT_FONT "/work/mmx_app/eso/browser/fonts/AudiTypeDisplay-Normal_14.ttf"

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

static void check_float(float got, float want, const char *what)
{
    float d = got - want;
    if (d < 0.0f) d = -d;
    if (d < 0.0001f) g_pass++;
    else { g_fail++; printf("  FAIL: %s: got %f, want %f\n", what, got, want); }
}

static int parse_str(const char *s, capsule_lines_t *out)
{
    return capsule_parse(s, strlen(s), out);
}

static void t_parse(void)
{
    capsule_lines_t l;
    char big[CAPSULE_LINE_MAX + 16];

    printf("file format\n");
    memset(&l, 0, sizeof(l));
    check_int(parse_str("14:25 \xC2\xB7 3 min\n1,2 km\nEND\n", &l), 1, "complete file");
    check_str(l.left, "14:25 \xC2\xB7 3 min", "left line");
    check_str(l.right, "1,2 km", "right line");

    check_int(parse_str("\n\nEND\n", &l), 1, "explicit clear");
    check_str(l.left, "", "clear empties left");
    check_str(l.right, "", "clear empties right");

    /* Every way a reader can catch the file before the single write finished. */
    strcpy(l.left, "keep");
    strcpy(l.right, "keep");
    check_int(parse_str("", &l), 0, "truncated to nothing");
    check_int(parse_str("14:25", &l), 0, "cut in line 1");
    check_int(parse_str("14:25\n", &l), 0, "cut after line 1");
    check_int(parse_str("14:25\n1 km", &l), 0, "cut in line 2");
    check_int(parse_str("14:25\n1 km\n", &l), 0, "no marker");
    check_int(parse_str("14:25\n1 km\nEN", &l), 0, "marker cut");
    check_int(parse_str("14:25\n1 km\nEND", &l), 0, "marker without newline");
    check_int(parse_str("14:25\n1 km\nENX\n", &l), 0, "other word in the marker's place");
    check_int(parse_str("14:25\n1 km\nEND\nx", &l), 0, "bytes after the marker");
    check_int(parse_str("a\nb\nc\nEND\n", &l), 0, "three lines");
    check_int(capsule_parse("a\0b\nc\nEND\n", 10, &l), 0, "NUL inside");
    check_str(l.left, "keep", "refused file leaves left alone");
    check_str(l.right, "keep", "refused file leaves right alone");

    memset(big, 'x', sizeof(big));
    big[CAPSULE_LINE_MAX - 1] = '\n';
    memcpy(big + CAPSULE_LINE_MAX, "\nEND\n", 5);
    check_int(capsule_parse(big, CAPSULE_LINE_MAX + 5, &l), 1, "longest line fits");
    check_int((int)strlen(l.left), CAPSULE_LINE_MAX - 1, "longest line kept whole");
    strcpy(l.left, "keep");
    memset(big, 'x', sizeof(big));
    big[CAPSULE_LINE_MAX] = '\n';
    memcpy(big + CAPSULE_LINE_MAX + 1, "\nEND\n", 5);
    check_int(capsule_parse(big, CAPSULE_LINE_MAX + 6, &l), 0, "line one byte too long");
    check_str(l.left, "keep", "too long leaves the text alone");
}

static void t_files(void)
{
    char path[64], huge[CAPSULE_FILE_MAX + 8];
    capsule_lines_t l;
    FILE *f;
    int slot = -1, draw = -1;

    printf("files\n");
    snprintf(path, sizeof(path), "/tmp/capsule_test.%d", (int)getpid());

    remove(path);
    strcpy(l.left, "old");
    strcpy(l.right, "old");
    check_int(capsule_read_file(path, &l), 1, "missing file is an answer");
    check_str(l.left, "", "missing file = nothing on the left");
    check_str(l.right, "", "missing file = nothing on the right");

    f = fopen(path, "w");
    fputs("L\nR\nEND\n", f);
    fclose(f);
    check_int(capsule_read_file(path, &l), 1, "complete file read");
    check_str(l.right, "R", "text from the file");

    f = fopen(path, "w");
    fclose(f);
    check_int(capsule_read_file(path, &l), 0, "file caught truncated");
    check_str(l.right, "R", "truncated file keeps the text");

    memset(huge, 'x', sizeof(huge));
    f = fopen(path, "w");
    fwrite(huge, 1, sizeof(huge), f);
    fclose(f);
    check_int(capsule_read_file(path, &l), 0, "oversized file refused");
    remove(path);

    check_int(capsule_read_file("/proc/self/mem/nope", &l), 0, "other open error is no answer");

    check_int(capsule_slot_write(path, 0, 1), 0, "slot written");
    f = fopen(path, "r");
    check_int(f && fscanf(f, "%d %d", &slot, &draw) == 2, 1, "slot readable");
    if (f) fclose(f);
    check_int(slot, 0, "slot value");
    check_int(draw, 1, "draw value");
    check_int(capsule_slot_write(path, 1, 0), 0, "slot rewritten");
    f = fopen(path, "r");
    check_int(f && fscanf(f, "%d %d", &slot, &draw) == 2, 1, "rewritten slot readable");
    if (f) fclose(f);
    check_int(slot, 1, "second slot value");
    check_int(draw, 0, "second draw value");
    remove(path);
    check_int(capsule_slot_write("/nonexistent-dir/x", 0, 1), -1, "unwritable path reported");
}

static long utf8_one(const char *s, int *used)
{
    const char *p = s;
    long cp = capsule_utf8_next(&p);
    *used = (int)(p - s);
    return cp;
}

static void t_utf8(void)
{
    int used;

    printf("utf-8\n");
    check_int((int)utf8_one("A", &used), 'A', "ascii");
    check_int(used, 1, "ascii length");
    check_int((int)utf8_one("\xC5\x82", &used), 0x142, "l with stroke");
    check_int(used, 2, "two bytes");
    check_int((int)utf8_one("\xC2\xB7", &used), 0xB7, "middle dot");
    check_int((int)utf8_one("\xE2\x80\xA6", &used), 0x2026, "ellipsis");
    check_int(used, 3, "three bytes");
    check_int((int)utf8_one("\xF0\x9F\x9A\x97", &used), 0x1F697, "four bytes");
    check_int(used, 4, "four bytes length");

    check_int((int)utf8_one("\x80x", &used), -1, "lone continuation");
    check_int(used, 1, "moves one byte");
    check_int((int)utf8_one("\xC0\x80", &used), -1, "overlong two bytes");
    check_int((int)utf8_one("\xC1\xBF", &used), -1, "overlong two bytes, top");
    check_int((int)utf8_one("\xE0\x80\x80", &used), -1, "overlong three bytes");
    check_int((int)utf8_one("\xF0\x80\x80\x80", &used), -1, "overlong four bytes");
    check_int((int)utf8_one("\xED\xA0\x80", &used), -1, "surrogate");
    check_int((int)utf8_one("\xF4\x90\x80\x80", &used), -1, "above U+10FFFF");
    check_int((int)utf8_one("\xF5\x80\x80\x80", &used), -1, "invalid lead");
    /* The terminator is not a continuation byte: nothing reads past it. */
    check_int((int)utf8_one("\xC5", &used), -1, "cut at the end, two bytes");
    check_int(used, 1, "cut sequence moves one byte");
    check_int((int)utf8_one("\xE2\x80", &used), -1, "cut at the end, three bytes");
    check_int((int)utf8_one("\xF0\x9F\x9A", &used), -1, "cut at the end, four bytes");
}

static void t_quad(void)
{
    float pos[8], uv[8];

    printf("quad\n");
    capsule_quad(1440, 542, 372, 397, 696, 36, pos, uv);
    check_float(pos[0], 2.0f * 372.0f / 1440.0f - 1.0f, "left edge");
    check_float(pos[2], 2.0f * 1068.0f / 1440.0f - 1.0f, "right edge");
    check_float(pos[5], 1.0f - 2.0f * 397.0f / 542.0f, "top edge counted from the top");
    check_float(pos[1], 1.0f - 2.0f * 433.0f / 542.0f, "bottom edge");
    /* Upload is top row first and GL takes the first row as v=0. */
    check_float(uv[5], 0.0f, "top of the quad is the first row");
    check_float(uv[1], 1.0f, "bottom of the quad is the last row");
    check_float(uv[0], 0.0f, "left of the quad is the first column");
    check_float(uv[2], 1.0f, "right of the quad is the last column");
}

static void t_rgba(void)
{
    const unsigned char a[2] = { 0, 200 };
    unsigned char rgba[8];

    printf("premultiplied rgba\n");
    memset(rgba, 7, sizeof(rgba));
    capsule_alpha_to_rgba(a, rgba, 2, 1);
    check_int(rgba[0] | rgba[1] | rgba[2] | rgba[3], 0, "empty stays fully transparent");
    check_int(rgba[4], 200, "red = alpha");
    check_int(rgba[5], 200, "green = alpha");
    check_int(rgba[6], 200, "blue = alpha");
    check_int(rgba[7], 200, "alpha");
}

/* First and last column with any ink, -1 if the strip is empty. */
static void ink_columns(const unsigned char *a, int w, int h, int *first, int *last)
{
    int x, y;
    *first = -1;
    *last = -1;
    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            if (a[y * w + x] > 40) {
                if (*first < 0) *first = x;
                *last = x;
                break;
            }
        }
    }
}

static void t_render(void)
{
    enum { W = 696, H = 36, GUARD = 64 };
    const char *path = getenv("CAPSULE_TEST_FONT");
    capsule_font_t f;
    capsule_lines_t l;
    unsigned char *mem, *a;
    int first, last, i, w_plain, w_dropped, top = -1, bottom = -1, x, y;

    if (!path) path = DEFAULT_FONT;
    printf("rasterising with %s\n", path);
    if (capsule_font_load(&f, path, 24) != 0) {
        printf("  SKIP: no font here\n");
        return;
    }
    check_int(capsule_font_load(&(capsule_font_t){0}, "/nonexistent.ttf", 24), -1,
              "missing font reported");

    /* Guard bands around the strip catch a write outside it even without ASan. */
    mem = (unsigned char *)malloc((size_t)(W * H + 2 * GUARD));
    memset(mem, 0xA5, (size_t)(W * H + 2 * GUARD));
    a = mem + GUARD;

    strcpy(l.left, "14:25 \xC2\xB7 3 min");
    strcpy(l.right, "1,2 km");
    capsule_render(&f, &l, a, W, H);
    ink_columns(a, W, H, &first, &last);
    check(first >= 0 && first < 8, "left text starts at the left end");
    check(last >= W - 8, "right text ends at the right end");

    /* A gap in the middle: that is the street's room in the VC field. */
    {
        int middle_ink = 0;
        for (x = 200; x < W - 120; x++)
            for (y = 0; y < H; y++)
                if (a[y * W + x]) middle_ink++;
        check_int(middle_ink, 0, "nothing drawn in the middle");
    }

    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            if (a[y * W + x]) {
                if (top < 0) top = y;
                bottom = y;
                break;
            }
        }
    }
    check(top >= 2 && bottom <= H - 3, "text vertically inside the strip");
    /* Mostly about the shift below: centred, give or take a descender. */
    check(top + bottom >= H - 8 && top + bottom <= H + 8, "text about centred");

    /* Polish letters are in the firmware font; a missing one would be dropped
     * silently and this is the only place that can notice. */
    {
        static const struct { const char *s; long cp; } pl[] = {
            { "\xC4\x85", 0x105 }, { "\xC4\x87", 0x107 }, { "\xC4\x99", 0x119 },
            { "\xC5\x82", 0x142 }, { "\xC5\x84", 0x144 }, { "\xC3\xB3", 0xF3 },
            { "\xC5\x9B", 0x15B }, { "\xC5\xBA", 0x17A }, { "\xC5\xBC", 0x17C },
            { "\xC2\xB7", 0xB7 },
        };
        for (i = 0; i < (int)(sizeof(pl) / sizeof(pl[0])); i++) {
            check(stbtt_FindGlyphIndex(&f.info, (int)pl[i].cp) != 0, pl[i].s);
            check(capsule_text_width(&f, pl[i].s) > 0, "glyph takes room");
        }
    }

    /* Width and drawing walk the same code. */
    strcpy(l.left, "");
    strcpy(l.right, "12 km");
    capsule_render(&f, &l, a, W, H);
    ink_columns(a, W, H, &first, &last);
    check(first >= W - capsule_text_width(&f, "12 km") - 1, "right text starts where measured");

    /* Invalid bytes and unknown code points take no room and draw nothing. */
    w_plain = capsule_text_width(&f, "ab");
    w_dropped = capsule_text_width(&f, "a\xFF\xC0\x80" "b");
    check_int(w_dropped, w_plain, "invalid bytes take no room");
    w_dropped = capsule_text_width(&f, "a\xF4\x8F\xBF\xBD" "b");
    check_int(w_dropped, w_plain, "unknown code point takes no room");
    check_int(capsule_text_width(&f, ""), 0, "empty text has no width");

    /* Text wider than the strip is clipped, inside the buffer. */
    memset(l.left, 'W', sizeof(l.left) - 1);
    l.left[sizeof(l.left) - 1] = '\0';
    memset(l.right, 'W', sizeof(l.right) - 1);
    l.right[sizeof(l.right) - 1] = '\0';
    capsule_render(&f, &l, a, W, H);
    for (i = 0; i < GUARD; i++) {
        if (mem[i] != 0xA5 || mem[GUARD + W * H + i] != 0xA5) break;
    }
    check_int(i, GUARD, "guard bands intact after clipping");

    /* Clipping cuts the line: ink past the end must not wrap into the next row.
     * The same line in a strip wide enough for all of it is the reference. */
    {
        enum { WIDE = 3000, NARROW = 100 };
        unsigned char *wide = (unsigned char *)malloc((size_t)(WIDE * H));
        unsigned char *narrow = (unsigned char *)malloc((size_t)(NARROW * H));
        int diff_left = 0, diff_right = 0;

        memset(l.left, 'W', sizeof(l.left) - 1);
        l.left[sizeof(l.left) - 1] = '\0';
        l.right[0] = '\0';
        capsule_render(&f, &l, wide, WIDE, H);
        capsule_render(&f, &l, narrow, NARROW, H);
        for (y = 0; y < H; y++)
            for (x = 0; x < NARROW; x++)
                if (narrow[y * NARROW + x] != wide[y * WIDE + x]) diff_left++;
        check_int(diff_left, 0, "long left text cut at the end of the strip");

        l.left[0] = '\0';
        memset(l.right, 'W', sizeof(l.right) - 1);
        l.right[sizeof(l.right) - 1] = '\0';
        capsule_render(&f, &l, wide, WIDE, H);
        capsule_render(&f, &l, narrow, NARROW, H);
        for (y = 0; y < H; y++)
            for (x = 0; x < NARROW; x++)
                if (narrow[y * NARROW + x] != wide[y * WIDE + (WIDE - NARROW) + x]) diff_right++;
        check_int(diff_right, 0, "long right text cut at the start of the strip");

        free(wide);
        free(narrow);
    }

    /* A strip lower than the font: glyphs are cut, nothing lands around it. */
    {
        enum { LOW_W = 200, LOW_H = 8, MARGIN = LOW_W * 64 };
        unsigned char *low = (unsigned char *)malloc((size_t)(LOW_W * LOW_H + 2 * MARGIN));

        memset(low, 0xA5, (size_t)(LOW_W * LOW_H + 2 * MARGIN));
        strcpy(l.left, "gjpW");
        strcpy(l.right, "gjpW");
        capsule_render(&f, &l, low + MARGIN, LOW_W, LOW_H);
        for (i = 0; i < MARGIN; i++) {
            if (low[i] != 0xA5 || low[MARGIN + LOW_W * LOW_H + i] != 0xA5) break;
        }
        check_int(i, MARGIN, "low strip: nothing written above or below");
        free(low);
    }

    /* An empty text is an empty strip, with no ink left from the frame before. */
    l.left[0] = '\0';
    l.right[0] = '\0';
    capsule_render(&f, &l, a, W, H);
    ink_columns(a, W, H, &first, &last);
    check_int(first, -1, "empty text clears the strip");

    free(mem);
    capsule_font_free(&f);
    check(f.data == NULL, "font freed");
}

int main(void)
{
    t_parse();
    t_files();
    t_utf8();
    t_quad();
    t_rgba();
    t_render();
    printf("WYNIK_CAPSULE: %d OK, %d FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
