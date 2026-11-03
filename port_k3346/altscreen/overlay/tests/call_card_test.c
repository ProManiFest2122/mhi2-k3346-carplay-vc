/*
 * Host test for call_card.h - our call card on the VC map.
 *
 * The file can be caught half written (no rename() on /tmp), and the card draws a
 * name nobody chose for us, so the cases below are mostly about bad input and
 * about staying inside the buffer.
 *
 * Built with the container's native gcc:  bash overlay/tests/run_host_tests.sh
 * Rasterising needs the MMI fonts, which we do not ship: /work, or SKIP.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* The capsule half of capsule_text.h has its own test. */
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../call_card.h"

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

static int parse_str(const char *s, call_card_t *out)
{
    return call_card_parse(s, strlen(s), out);
}

static void t_parse(void)
{
    call_card_t c;
    char big[CALL_CARD_FILE_MAX + 16];
    size_t n;

    printf("file format\n");
    check_int(parse_str("A\nAnna Kowalska\nTrwa rozmowa\nEND\n", &c), 1, "complete file");
    check_int(c.state, 'A', "state");
    check_str(c.name, "Anna Kowalska", "name");
    check_str(c.label, "Trwa rozmowa", "label");
    check_int(parse_str("R\nx\ny\nEND\n", &c), 1, "ringing");
    check_int(c.state, 'R', "state R");
    check_int(parse_str("D\nx\ny\nEND\n", &c), 1, "dialing");
    check_int(c.state, 'D', "state D");
    check_int(parse_str("H\nx\ny\nEND\n", &c), 1, "held");
    check_int(c.state, 'H', "state H");
    check_int(parse_str("\n\n\nEND\n", &c), 1, "no call");
    check_int(c.state, 0, "no card");
    check_str(c.name, "", "no card: no name");

    strcpy(c.name, "keep");
    c.state = 'A';
    check_int(parse_str("", &c), 0, "truncated to nothing");
    check_int(parse_str("A", &c), 0, "cut in the state");
    check_int(parse_str("A\nAnna", &c), 0, "cut in the name");
    check_int(parse_str("A\nAnna\nTrwa", &c), 0, "cut in the label");
    check_int(parse_str("A\nAnna\nTrwa\n", &c), 0, "no marker");
    check_int(parse_str("A\nAnna\nTrwa\nEN", &c), 0, "marker cut");
    check_int(parse_str("A\nAnna\nTrwa\nEND", &c), 0, "marker without newline");
    check_int(parse_str("A\nAnna\nTrwa\nENX\n", &c), 0, "other word for the marker");
    check_int(parse_str("A\nAnna\nTrwa\nEND\nx", &c), 0, "bytes after the marker");
    check_int(parse_str("A\nAnna\nextra\nTrwa\nEND\n", &c), 0, "one line too many");
    check_int(parse_str("X\nAnna\nTrwa\nEND\n", &c), 0, "unknown state");
    check_int(parse_str("AA\nAnna\nTrwa\nEND\n", &c), 0, "state too long");
    check_int(call_card_parse("A\nAn\0na\nT\nEND\n", 14, &c), 0, "NUL inside");
    check_str(c.name, "keep", "refused file leaves the card alone");
    check_int(c.state, 'A', "refused file leaves the state alone");

    n = 0;
    big[n++] = 'A'; big[n++] = '\n';
    memset(big + n, 'x', CALL_CARD_NAME_MAX - 1); n += CALL_CARD_NAME_MAX - 1;
    memcpy(big + n, "\nT\nEND\n", 7); n += 7;
    check_int(call_card_parse(big, n, &c), 1, "longest name fits");
    check_int((int)strlen(c.name), CALL_CARD_NAME_MAX - 1, "longest name kept whole");

    n = 0;
    big[n++] = 'A'; big[n++] = '\n';
    memset(big + n, 'x', CALL_CARD_NAME_MAX); n += CALL_CARD_NAME_MAX;
    memcpy(big + n, "\nT\nEND\n", 7); n += 7;
    check_int(call_card_parse(big, n, &c), 0, "name one byte too long");

    n = 0;
    big[n++] = 'A'; big[n++] = '\n'; big[n++] = 'x'; big[n++] = '\n';
    memset(big + n, 'y', CALL_CARD_LABEL_MAX); n += CALL_CARD_LABEL_MAX;
    memcpy(big + n, "\nEND\n", 5); n += 5;
    check_int(call_card_parse(big, n, &c), 0, "label one byte too long");
}

static void t_files(void)
{
    char path[64], huge[CALL_CARD_FILE_MAX + 8];
    call_card_t c;
    FILE *f;

    printf("files\n");
    snprintf(path, sizeof(path), "/tmp/call_card_test.%d", (int)getpid());
    remove(path);
    c.state = 'A';
    strcpy(c.name, "old");
    check_int(call_card_read_file(path, &c), 1, "missing file is an answer");
    check_int(c.state, 0, "missing file = no card");
    check_str(c.name, "", "missing file = no name");

    f = fopen(path, "w");
    fputs("H\nAnna\nWstrzymana\nEND\n", f);
    fclose(f);
    check_int(call_card_read_file(path, &c), 1, "complete file read");
    check_int(c.state, 'H', "state from the file");
    check_str(c.label, "Wstrzymana", "label from the file");

    f = fopen(path, "w");
    fclose(f);
    check_int(call_card_read_file(path, &c), 0, "file caught truncated");
    check_int(c.state, 'H', "truncated file keeps the card");

    memset(huge, 'x', sizeof(huge));
    f = fopen(path, "w");
    fwrite(huge, 1, sizeof(huge), f);
    fclose(f);
    check_int(call_card_read_file(path, &c), 0, "oversized file refused");
    remove(path);
    check_int(call_card_read_file("/proc/self/mem/nope", &c), 0, "other open error is no answer");
    check_int(c.state, 'H', "open error keeps the card");
}

static void t_time(void)
{
    char t[24];

    printf("clock\n");
    call_card_format_time(0, t, sizeof(t));     check_str(t, "00:00", "zero");
    call_card_format_time(5, t, sizeof(t));     check_str(t, "00:05", "seconds padded");
    call_card_format_time(135, t, sizeof(t));   check_str(t, "02:15", "minutes");
    call_card_format_time(3599, t, sizeof(t));  check_str(t, "59:59", "last under an hour");
    call_card_format_time(3600, t, sizeof(t));  check_str(t, "1:00:00", "one hour");
    call_card_format_time(3735, t, sizeof(t));  check_str(t, "1:02:15", "hour and minutes");
    call_card_format_time(36000, t, sizeof(t)); check_str(t, "10:00:00", "ten hours");
    call_card_format_time(-5, t, sizeof(t));    check_str(t, "00:00", "negative is zero");
    check_int(call_card_timed('A'), 1, "active has a clock");
    check_int(call_card_timed('H'), 1, "held has a clock");
    check_int(call_card_timed('R'), 0, "ringing has no clock");
    check_int(call_card_timed('D'), 0, "dialing has no clock");
    check_int(call_card_timed(0), 0, "no card has no clock");
}

static long ink(const unsigned char *rgba, int w, int x0, int y0, int x1, int y1)
{
    long total = 0;
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            total += rgba[(y * w + x) * 4];
    return total;
}

static void t_render(void)
{
    enum { W = 320, H = 78, GUARD = 4096 };
    capsule_font_t name, status;
    call_card_t c;
    unsigned char *mem_rgba, *rgba, *mem_scratch, *scratch;
    char shown[CALL_CARD_NAME_MAX];
    int i, bad = 0;

    printf("rasterising\n");
    if (capsule_font_load(&name, FONT_BOLD, 28) != 0 || capsule_font_load(&status, FONT_NORMAL, 22) != 0) {
        printf("  SKIP: no fonts here\n");
        return;
    }
    mem_rgba = (unsigned char *)malloc((size_t)(W * H * 4 + 2 * GUARD));
    mem_scratch = (unsigned char *)malloc((size_t)(W * H + 2 * GUARD));
    memset(mem_rgba, 0xA5, (size_t)(W * H * 4 + 2 * GUARD));
    memset(mem_scratch, 0xA5, (size_t)(W * H + 2 * GUARD));
    rgba = mem_rgba + GUARD;
    scratch = mem_scratch + GUARD;

    memset(&c, 0, sizeof(c));
    strcpy(c.name, "Anna Kowalska");
    strcpy(c.label, "Trwa rozmowa");
    c.state = 'A';
    call_card_render(&name, &status, &c, 135, scratch, rgba, W, H);

    /* glass: the corner is empty, the middle is the blue-black at alpha 0.84 */
    check_int(rgba[3], 0, "corner transparent");
    check_int(rgba[(H / 2 * W + W - 4) * 4 + 3], 214, "edge is the glass, alpha 0.84");
    check_int(rgba[(H / 2 * W + W - 4) * 4 + 0], 17, "glass red premultiplied");
    check_int(rgba[(H / 2 * W + W - 4) * 4 + 1], 25, "glass green premultiplied");
    check_int(rgba[(H / 2 * W + W - 4) * 4 + 2], 35, "glass blue premultiplied");
    /* premultiplied means no channel can exceed alpha */
    for (i = 0; i < W * H; i++) {
        const unsigned char *px = rgba + i * 4;
        if (px[0] > px[3] || px[1] > px[3] || px[2] > px[3]) bad++;
    }
    check_int(bad, 0, "every pixel premultiplied");

    /* the dot takes the state colour: full green at its centre */
    check_int(rgba[(25 * W + 24) * 4 + 1], 214, "active dot is green");
    check_int(rgba[(25 * W + 24) * 4 + 0], 64, "active dot red part");
    check_int(rgba[(25 * W + 24) * 4 + 3], 255, "dot is opaque");

    {
        static unsigned char other[W * H * 4];
        call_card_t d = c;
        int y, x, first = 0, second = 0;
        strcpy(d.name, "Bartosz Nowak");
        call_card_render(&name, &status, &d, 135, scratch, other, W, H);
        for (y = 0; y < H; y++)
            for (x = 0; x < W * 4; x++)
                if (rgba[y * W * 4 + x] != other[y * W * 4 + x]) { if (y < 38) first++; else second++; }
        check(first > 100, "the name is drawn on the first line");
        check_int(second, 0, "the name stays off the second line");
    }
    check(ink(rgba, W, 42, 44, W - 20, 70) > 40000, "status drawn on the second line");

    c.state = 'R';
    call_card_render(&name, &status, &c, 0, scratch, rgba, W, H);
    check_int(rgba[(25 * W + 24) * 4 + 0], 255, "ringing dot is orange");
    check_int(rgba[(25 * W + 24) * 4 + 1], 164, "ringing dot green part");
    c.state = 'D';
    call_card_render(&name, &status, &c, 0, scratch, rgba, W, H);
    check_int(rgba[(25 * W + 24) * 4 + 0], 255, "dialing dot is orange");
    c.state = 'H';
    call_card_render(&name, &status, &c, 0, scratch, rgba, W, H);
    check_int(rgba[(25 * W + 24) * 4 + 0], 150, "held dot is grey");

    /* the clock changes the picture only where a clock belongs */
    {
        static unsigned char a[W * H * 4], b[W * H * 4];
        c.state = 'A';
        call_card_render(&name, &status, &c, 10, scratch, a, W, H);
        call_card_render(&name, &status, &c, 11, scratch, b, W, H);
        check(memcmp(a, b, sizeof(a)) != 0, "active: the clock ticks");
        c.state = 'R';
        call_card_render(&name, &status, &c, 10, scratch, a, W, H);
        call_card_render(&name, &status, &c, 11, scratch, b, W, H);
        check(memcmp(a, b, sizeof(a)) == 0, "ringing: no clock");
    }

    /* no card: nothing at all, not an empty panel */
    memset(&c, 0, sizeof(c));
    call_card_render(&name, &status, &c, 0, scratch, rgba, W, H);
    check(ink(rgba, W, 0, 0, W, H) == 0, "no card clears the strip");

    /* a name nobody chose: long, Polish, stuffed with bad bytes */
    call_card_fit(&name, "Anna", 100, shown, sizeof(shown));
    check_str(shown, "Anna", "short name untouched");
    call_card_fit(&name, "Świętosława Brzęczyszczykiewicz", 200, shown, sizeof(shown));
    check(capsule_text_width(&name, shown) <= 200, "long name fits");
    check(strlen(shown) >= 3 && !strcmp(shown + strlen(shown) - 3, "\xE2\x80\xA6"), "long name ends with the mark");
    check(strncmp(shown, "Świ", 4) == 0, "long name keeps its start");
    call_card_fit(&name, "Ab cd", capsule_text_width(&name, "Ab \xE2\x80\xA6"), shown, sizeof(shown));
    check_str(shown, "Ab\xE2\x80\xA6", "no space before the mark");
    /* cuts between characters: every byte kept is valid UTF-8 */
    {
        const char *p;
        int invalid = 0;
        call_card_fit(&name, "ąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąą", 90, shown, sizeof(shown));
        for (p = shown; *p; ) if (capsule_utf8_next(&p) < 0) invalid++;
        check_int(invalid, 0, "cut on a character boundary");
    }
    call_card_fit(&name, "WWWWWW", 1, shown, sizeof(shown));
    check_str(shown, "\xE2\x80\xA6", "nothing fits: just the mark");

    memset(&c, 0, sizeof(c));
    c.state = 'A';
    memset(c.name, 'W', sizeof(c.name) - 1);
    memset(c.label, 'W', sizeof(c.label) - 1);
    call_card_render(&name, &status, &c, 359999, scratch, rgba, W, H);
    for (i = 0, bad = 0; i < GUARD; i++) {
        if (mem_rgba[i] != 0xA5 || mem_rgba[GUARD + W * H * 4 + i] != 0xA5) bad++;
        if (mem_scratch[i] != 0xA5 || mem_scratch[GUARD + W * H + i] != 0xA5) bad++;
    }
    check_int(bad, 0, "guard bands intact");

    /* a strip smaller than the fonts stays inside too */
    {
        enum { LOW_W = 120, LOW_H = 48, M = 4096 };
        unsigned char *low = (unsigned char *)malloc((size_t)(LOW_W * LOW_H * 4 + 2 * M));
        memset(low, 0xA5, (size_t)(LOW_W * LOW_H * 4 + 2 * M));
        memset(mem_scratch, 0xA5, (size_t)(W * H + 2 * GUARD));
        strcpy(c.name, "gjpW");
        call_card_render(&name, &status, &c, 5, scratch, low + M, LOW_W, LOW_H);
        for (i = 0, bad = 0; i < M; i++)
            if (low[i] != 0xA5 || low[M + LOW_W * LOW_H * 4 + i] != 0xA5) bad++;
        check_int(bad, 0, "small strip: guard bands intact");
        free(low);
    }

    free(mem_rgba);
    free(mem_scratch);
    capsule_font_free(&name);
    capsule_font_free(&status);
}

int main(void)
{
    t_parse();
    t_files();
    t_time();
    t_render();
    printf("WYNIK_CALL_CARD: %d OK, %d FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
