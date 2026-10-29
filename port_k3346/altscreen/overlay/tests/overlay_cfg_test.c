/*
 * Host test for overlay_cfg.h.
 *
 * This file is the one part of Etap 7 that a human edits by hand, on an SD
 * card, in a car park, possibly in Notepad.  So the parser has to survive
 * CRLF, comments, blank lines, keys it has never heard of and a file that is
 * not there at all - and it must never refuse to start over a typo, because a
 * renderer that does not come up is worse than one that comes up in the wrong
 * place and says so in the log.
 *
 * Built with the container's native gcc (overlay_cfg.h is plain C, no QNX):
 *   bash overlay/tests/run_host_tests.sh
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../overlay_cfg.h"

static int g_pass, g_fail;

static void check(int cond, const char *what)
{
    if (cond) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL: %s\n", what);
    }
}

static void check_int(int got, int want, const char *what)
{
    if (got == want) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL: %s (got %d, want %d)\n", what, got, want);
    }
}

static const char *write_tmp(const char *name, const char *body)
{
    static char path[256];
    FILE *f;
    snprintf(path, sizeof(path), "/tmp/%s", name);
    f = fopen(path, "wb");
    if (!f) { perror("fopen"); exit(2); }
    fwrite(body, 1, strlen(body), f);
    fclose(f);
    return path;
}

static void t_defaults(void)
{
    overlay_cfg_t c;
    printf("defaults (no file)\n");
    overlay_cfg_load(&c, "/tmp/definitely-not-here.conf");
    check_int(c.displayable, 98, "displayable");
    /* The renderer's own canvas = Luka's CR_DEFAULT_*.  It is not a window
     * size and it must not follow the measured tile: maneuver_render does not
     * scale (overlay_cfg.h LANDMINE). */
    check_int(c.render_width, 328, "render width (CR_DEFAULT_WIDTH)");
    check_int(c.render_height, 181, "render height (CR_DEFAULT_HEIGHT)");
    /* CR_POPUP_X/Y: the part of that canvas the maneuver is framed into. */
    check_int(c.crop_x, 59, "crop x (CR_POPUP_X)");
    check_int(c.crop_y, 27, "crop y (CR_POPUP_Y)");
    /* 210x153 = KVS_FPK style 2, the size measured in the car; see the conf. */
    check_int(c.width, 210, "width");
    check_int(c.height, 153, "height");
    /* The window is the whole cluster canvas, because the DisplayManager puts
     * a displayable it has no entry for at (0,0) and nowhere else (measured
     * 2026-10-27).  The panel is drawn at an offset inside it. */
    check_int(c.canvas_width, 1440, "canvas width");
    check_int(c.canvas_height, 542, "canvas height");
    /* -1 = do not touch DISPLAY_QNX, which is what the stock recipe does. */
    check_int(c.display, -1, "display left to the default");
    check_int(c.context, 901, "context");
    /* Labels corrected 2026-10-28: (1055,207) is inside the right gauge with
     * big clocks, (1091,110) the top right of the map with small ones. */
    check_int(c.x, 1055, "x (big clocks)");
    check_int(c.y, 207, "y (big clocks)");
    check_int(c.x_alt, 1091, "x_alt (small clocks)");
    check_int(c.y_alt, 110, "y_alt (small clocks)");
    check_int(c.visible_top_big, 0, "whole crop visible with big clocks");
    check_int(c.visible_top_small, 0, "whole crop visible with small clocks");
    check_int(c.alpha, 1, "alpha");
    check_int(c.managed, 1, "managed");
    check_int(c.fps, 30, "fps");
    check(strstr(c.source, "built-in") != NULL, "source says built-in");
}

static void t_plain(void)
{
    overlay_cfg_t c;
    const char *p = write_tmp("ovl_plain.conf",
        "displayable 77\n"
        "width 640\n"
        "height 200\n"
        "display 4\n"
        "context 902\n"
        "alpha 0\n"
        "managed 0\n"
        "swap_interval 1\n"
        "fps 15\n");
    printf("plain file\n");
    check_int(overlay_cfg_load(&c, p), 9, "9 keys applied");
    check_int(c.displayable, 77, "displayable");
    check_int(c.width, 640, "width");
    check_int(c.height, 200, "height");
    check_int(c.display, 4, "display");
    check_int(c.context, 902, "context");
    check_int(c.alpha, 0, "alpha");
    check_int(c.managed, 0, "managed");
    check_int(c.swap_interval, 1, "swap_interval");
    check_int(c.fps, 15, "fps");
    check(strcmp(c.source, p) == 0, "source is the file path");
}

/* The card ships LF, but the user's editor may not. */
static void t_crlf(void)
{
    overlay_cfg_t c;
    const char *p = write_tmp("ovl_crlf.conf",
        "displayable 66\r\n"
        "width 320\r\n"
        "height 160\r\n");
    printf("CRLF from a Windows editor\n");
    overlay_cfg_load(&c, p);
    check_int(c.displayable, 66, "displayable");
    check_int(c.width, 320, "width");
    check_int(c.height, 160, "height");
}

static void t_comments_and_junk(void)
{
    overlay_cfg_t c;
    const char *p = write_tmp("ovl_junk.conf",
        "# leading comment\n"
        "\n"
        "   \n"
        "displayable 61   # inline comment\n"
        "someFutureKey 12345\n"
        "width\n"                       /* key with no value */
        "   height   200   \n"          /* leading/extra blanks */
        "= = =\n"
        "fps 25\n");
    printf("comments, blanks, unknown keys, malformed lines\n");
    overlay_cfg_load(&c, p);
    check_int(c.displayable, 61, "inline comment stripped");
    check_int(c.height, 200, "extra blanks tolerated");
    check_int(c.width, 210, "valueless key left at default");
    check_int(c.fps, 25, "parsing continued past junk");
}

static void t_sanitize(void)
{
    overlay_cfg_t c;
    const char *p = write_tmp("ovl_bad.conf",
        "displayable 999\n"
        "width 0\n"
        "height -4\n"
        "display 77\n"
        "fps 9000\n"
        "swap_interval 99\n"
        "alpha 7\n"
        "managed 3\n");
    int fixed;
    printf("out-of-range values are clamped, not fatal\n");
    overlay_cfg_load(&c, p);
    fixed = overlay_cfg_sanitize(&c);
    check_int(c.displayable, 98, "displayable clamped");
    check_int(c.width, 210, "width clamped");
    check_int(c.height, 153, "height clamped");
    check_int(c.display, -1, "display clamped");
    check_int(c.fps, 30, "fps clamped");
    check_int(c.swap_interval, 2, "swap_interval clamped");
    check_int(c.alpha, 1, "alpha normalised to 0/1");
    check_int(c.managed, 1, "managed normalised to 0/1");
    check_int(fixed, 6, "reported how many were corrected");
}

/* The shipped file must parse to the values its comments promise. */
static void t_shipped_file(void)
{
    overlay_cfg_t c;
    const char *p = "config/mib2high-overlay.conf";
    if (access(p, R_OK) != 0) {
        printf("shipped config: not reachable from here, skipped\n");
        return;
    }
    printf("shipped config/mib2high-overlay.conf\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 0, "nothing needed clamping");
    check_int(c.displayable, 98, "displayable 98");
    check_int(c.render_width, 328, "render 328 wide (CR_DEFAULT_WIDTH)");
    check_int(c.render_height, 181, "render 181 high (CR_DEFAULT_HEIGHT)");
    check_int(c.crop_x, 59, "crop x 59 (CR_POPUP_X)");
    check_int(c.crop_y, 27, "crop y 27 (CR_POPUP_Y)");
    check_int(c.width, 210, "width 210 (KVS_FPK style 2, measured)");
    check_int(c.height, 153, "height 153");
    check_int(c.canvas_width, 1440, "canvas 1440 wide (whole cluster)");
    check_int(c.canvas_height, 542, "canvas 542 high");
    check_int(c.display, -1, "display -1 (stock recipe)");
    check_int(c.x, 1055, "x 1055 (big clocks)");
    check_int(c.y, 207, "y 207");
    check_int(c.x_alt, 1091, "x_alt 1091 (small clocks)");
    check_int(c.y_alt, 110, "y_alt 110");
    check_int(c.visible_top_big, 0, "visible_top_big 0");
    check_int(c.visible_top_small, 0, "visible_top_small 0");
    check_int(c.alpha, 1, "alpha on");
    check_int(c.managed, 1, "managed present (ignored by the renderer)");
}

/*
 * The caption bound is not cosmetic.  The DisplayManager turns the window
 * caption into the displayable number with atoi() and checks it against
 * nothing: "0" or a non-number would hand us somebody else's displayable 0,
 * and "-1" makes it drop the window without a word.  So a card that says 0,
 * -1 or something above the HMI's MAX_DISPLAYABLES must come back as 98,
 * never as what it said.
 */
static void t_displayable_bounds(void)
{
    static const int bad[]  = { 0, -1, -98, 110, 255, 1000 };
    static const int good[] = { 1, 20, 98, 109 };
    unsigned i;

    printf("displayable bounds (caption must never be 0, negative or > 109)\n");

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        overlay_cfg_t c;
        char body[64], what[96];
        const char *p;
        overlay_cfg_defaults(&c);
        sprintf(body, "displayable %d\n", bad[i]);
        p = write_tmp("ovl_id.conf", body);
        overlay_cfg_load(&c, p);
        overlay_cfg_sanitize(&c);
        sprintf(what, "displayable %d rejected -> 98", bad[i]);
        check_int(c.displayable, 98, what);
    }

    for (i = 0; i < sizeof(good) / sizeof(good[0]); i++) {
        overlay_cfg_t c;
        char body[64], what[96];
        const char *p;
        overlay_cfg_defaults(&c);
        sprintf(body, "displayable %d\n", good[i]);
        p = write_tmp("ovl_id.conf", body);
        overlay_cfg_load(&c, p);
        overlay_cfg_sanitize(&c);
        sprintf(what, "displayable %d kept", good[i]);
        check_int(c.displayable, good[i], what);
    }
}

/* The panel origins are what the next geometry round will need, so a card that
 * carries nonsense must not silently turn them into nonsense. */
static void t_origins(void)
{
    overlay_cfg_t c;
    const char *p = write_tmp("ovl_xy.conf",
        "x 100\n"
        "y 200\n"
        "x_alt 300\n"
        "y_alt 300\n");
    printf("panel origins read from the card\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 0, "in-range origins need no clamping");
    check_int(c.x, 100, "x");
    check_int(c.y, 200, "y");
    check_int(c.x_alt, 300, "x_alt");
    check_int(c.y_alt, 300, "y_alt");

    p = write_tmp("ovl_xy_bad.conf",
        "x -5\n"
        "y 99999\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 2, "two origins corrected");
    check_int(c.x, 1055, "x back to the measured value");
    check_int(c.y, 207, "y back to the measured value");
}

/*
 * The window is the canvas and the panel is drawn inside it, so an origin is
 * only legal while the whole panel still fits.  An origin that would hang the
 * panel off the edge is a typo on the card, not a position: the renderer would
 * otherwise blit a rectangle partly outside the cluster and the arrow would be
 * cut in half with no indication why.
 */
static void t_panel_fits_canvas(void)
{
    overlay_cfg_t c;
    const char *p;

    printf("the panel has to fit inside the canvas\n");

    /* 1440-210 = 1230 across, 542-153 = 389 down. */
    p = write_tmp("ovl_fit_edge.conf", "x 1230\ny 389\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 0, "exactly flush with the edge is legal");
    check_int(c.x, 1230, "x kept at the last legal column");
    check_int(c.y, 389, "y kept at the last legal row");

    p = write_tmp("ovl_fit_over.conf", "x 1231\ny 390\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 2, "one pixel past the edge is corrected");
    check_int(c.x, 1055, "x back to the measured value");
    check_int(c.y, 207, "y back to the measured value");

    /* A canvas smaller than the panel is nonsense; the panel gives way, and
     * the origins then have nowhere to go but 0. */
    p = write_tmp("ovl_fit_tiny.conf",
        "canvas_width 200\n"
        "canvas_height 100\n");
    overlay_cfg_load(&c, p);
    check(overlay_cfg_sanitize(&c) > 0, "a canvas under the panel is corrected");
    check_int(c.width, 200, "panel width cut to the canvas");
    check_int(c.height, 100, "panel height cut to the canvas");
    check_int(c.x, 0, "x has nowhere to go but 0");
    check_int(c.y, 0, "y has nowhere to go but 0");

    /* The documented escape hatch: shrink the canvas to the panel and the
     * window goes back to a corner-placed tile, no rebuild needed. */
    p = write_tmp("ovl_fit_hatch.conf",
        "canvas_width 210\n"
        "canvas_height 153\n");
    overlay_cfg_load(&c, p);
    overlay_cfg_sanitize(&c);
    check_int(c.width, 210, "panel still 210 wide");
    check_int(c.canvas_width, 210, "canvas shrunk to the panel");
    check_int(c.x, 0, "panel origin collapses to the corner");
    check_int(c.y, 0, "panel origin collapses to the corner");
}

/* canvas_width / canvas_height are read from the card like everything else. */
static void t_canvas_from_card(void)
{
    overlay_cfg_t c;
    /* 1400x500 still leaves room for the measured origins (1091+210 <= 1400,
     * 207+153 <= 500), so nothing should be corrected.  Shrink it further and
     * the small-clocks x really would not fit - that is the next case. */
    const char *p = write_tmp("ovl_canvas.conf",
        "canvas_width 1400\n"
        "canvas_height 500\n");
    printf("canvas size read from the card\n");
    check_int(overlay_cfg_load(&c, p), 2, "2 keys applied");
    check_int(c.canvas_width, 1400, "canvas width");
    check_int(c.canvas_height, 500, "canvas height");
    check_int(overlay_cfg_sanitize(&c), 0, "a smaller sane canvas needs no clamping");

    /* 1280 wide leaves 1070 for the panel origin, and the small-clocks 1091
     * does not fit: a narrower canvas drags the tile back inside by itself. */
    p = write_tmp("ovl_canvas_narrow.conf", "canvas_width 1280\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 1, "the measured x_alt no longer fits");
    check_int(c.x_alt, 1070, "x_alt pulled to the last column that fits");
    check_int(c.x, 1055, "x still fits and is left alone");

    p = write_tmp("ovl_canvas_bad.conf",
        "canvas_width 4000\n"
        "canvas_height 7\n");
    overlay_cfg_load(&c, p);
    check(overlay_cfg_sanitize(&c) >= 2, "absurd canvas corrected");
    check_int(c.canvas_width, 1440, "canvas width back to the cluster size");
    check_int(c.canvas_height, 542, "canvas height back to the cluster size");
}

/*
 * The crop is cut out of what the renderer drew, so it cannot hang off that
 * canvas.  A crop that did would sample the clamped edge texel as a smear
 * down the side of the tile - which looks exactly like a rendering bug and is
 * not one, so it is worth refusing on the card instead of debugging in a car.
 */
static void t_crop_fits_render(void)
{
    overlay_cfg_t c;
    const char *p;

    printf("the crop has to fit inside the rendered canvas\n");

    /* 328-210 = 118 across, 181-153 = 28 down. */
    p = write_tmp("ovl_crop_edge.conf", "crop_x 118\ncrop_y 28\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 0, "flush with the edge is legal");
    check_int(c.crop_x, 118, "crop_x kept at the last legal column");
    check_int(c.crop_y, 28, "crop_y kept at the last legal row");

    p = write_tmp("ovl_crop_over.conf", "crop_x 119\ncrop_y 29\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 2, "one pixel past the edge is corrected");
    check_int(c.crop_x, 59, "crop_x back to CR_POPUP_X");
    check_int(c.crop_y, 27, "crop_y back to CR_POPUP_Y");

    p = write_tmp("ovl_crop_neg.conf", "crop_x -1\ncrop_y -1\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 2, "negative crop corrected");
    check_int(c.crop_x, 59, "crop_x back to CR_POPUP_X");
    check_int(c.crop_y, 27, "crop_y back to CR_POPUP_Y");

    /* A crop larger than the canvas: the crop gives way, and then its origin
     * has nowhere to go but 0. */
    p = write_tmp("ovl_crop_big.conf",
        "render_width 100\n"
        "render_height 80\n");
    overlay_cfg_load(&c, p);
    check(overlay_cfg_sanitize(&c) > 0, "a canvas under the crop is corrected");
    check_int(c.width, 100, "crop width cut to the rendered canvas");
    check_int(c.height, 80, "crop height cut to the rendered canvas");
    check_int(c.crop_x, 0, "crop_x has nowhere to go but 0");
    check_int(c.crop_y, 0, "crop_y has nowhere to go but 0");

    p = write_tmp("ovl_render_bad.conf",
        "render_width 0\n"
        "render_height 99999\n");
    overlay_cfg_load(&c, p);
    check(overlay_cfg_sanitize(&c) >= 2, "absurd render size corrected");
    check_int(c.render_width, 328, "render width back to CR_DEFAULT_WIDTH");
    check_int(c.render_height, 181, "render height back to CR_DEFAULT_HEIGHT");
}

/*
 * The card already in the car was written before render_* and crop_* existed.
 * A newer binary has to read it and arrive at the right geometry anyway,
 * because the alternative is a renderer that works only after the user
 * remembers to copy a new conf - and that is exactly the kind of step that
 * gets skipped at night in a car park.
 */
static void t_old_card_still_works(void)
{
    overlay_cfg_t c;
    const char *p = write_tmp("ovl_old_card.conf",
        "displayable 98\n"
        "width  210\n"
        "height 153\n"
        "canvas_width  1440\n"
        "canvas_height 542\n"
        "x      1091\n"
        "y      110\n"
        "x_alt  1055\n"
        "y_alt  207\n");
    printf("a card from before the render/crop split\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 0, "nothing needed clamping");
    check_int(c.render_width, 328, "render size falls back to CR_DEFAULT_WIDTH");
    check_int(c.render_height, 181, "render size falls back to CR_DEFAULT_HEIGHT");
    check_int(c.crop_x, 59, "crop falls back to CR_POPUP_X");
    check_int(c.crop_y, 27, "crop falls back to CR_POPUP_Y");
    check_int(c.width, 210, "the measured tile is still the tile");
    check_int(c.height, 153, "the measured tile is still the tile");
}

/* The visible-area knob: rows of the crop the VC covers at the top.  Read
 * from the card, and never allowed to leave less than 16 rows to frame into. */
static void t_visible_top(void)
{
    overlay_cfg_t c;
    const char *p;

    printf("visible_top: read from the card, never swallowing the tile\n");
    p = write_tmp("ovl_vis.conf", "visible_top_big 35\nvisible_top_small 0\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 0, "sane values need no clamping");
    check_int(c.visible_top_big, 35, "visible_top_big read");
    check_int(c.visible_top_small, 0, "visible_top_small read");

    /* 153 - 16 = 137 is the most that can be covered. */
    p = write_tmp("ovl_vis_edge.conf", "visible_top_big 137\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 0, "16 rows left is still legal");
    check_int(c.visible_top_big, 137, "kept at the limit");

    p = write_tmp("ovl_vis_bad.conf", "visible_top_big 138\nvisible_top_small -4\n");
    overlay_cfg_load(&c, p);
    check_int(overlay_cfg_sanitize(&c), 2, "both corrected");
    check_int(c.visible_top_big, 0, "too much -> back to the whole crop");
    check_int(c.visible_top_small, 0, "negative -> back to the whole crop");
}

int main(void)
{
    t_defaults();
    t_visible_top();
    t_plain();
    t_crlf();
    t_comments_and_junk();
    t_sanitize();
    t_displayable_bounds();
    t_origins();
    t_panel_fits_canvas();
    t_crop_fits_render();
    t_old_card_still_works();
    t_canvas_from_card();
    t_shipped_file();

    printf("\noverlay_cfg: %d OK, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
