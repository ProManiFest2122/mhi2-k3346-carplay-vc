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
    check_int(c.capsule_text, 0, "capsule text off (too dark, run-53)");
    check_int(c.capsule_left_x, 372, "capsule from x 372");
    check_int(c.capsule_right_x, 1068, "capsule to x 1068");
    check_int(c.capsule_y, 397, "capsule at y 397");
    check_int(c.capsule_h, 36, "capsule 36 high");
    check_int(c.capsule_font_px, 24, "capsule font 24 px");
    check_int(c.call_card, 1, "call card on");
    check_int(c.call_card_ringing, 1, "call card while ringing");
    check_int(c.call_card_w, 320, "call card 320 wide");
    check_int(c.call_card_h, 44, "call card 44 high, the pill of rc28");
    check_int(c.call_card_x_small, 772, "call card x, small clocks");
    check_int(c.call_card_y_small, 334, "call card y, small clocks");
    check_int(c.call_card_x_big, 560, "call card x, big clocks");
    check_int(c.call_card_y_big, 334, "call card y, big clocks");
    check_int(c.call_card_name_px, 24, "name font");
    check_int(c.call_card_status_px, 22, "status font");
    check(!strcmp(c.call_card_font, OVERLAY_CFG_CAPSULE_FONT), "call card font");
    check(!strcmp(c.call_card_font_bold, OVERLAY_CFG_CALL_FONT_BOLD), "call card bold font");
    check(!strcmp(c.capsule_font, OVERLAY_CFG_CAPSULE_FONT), "capsule font from the firmware");
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
    check_int(overlay_cfg_sanitize(&c), 2, "the measured x_alt and the Sport tile no longer fit");
    check_int(c.x_alt, 1070, "x_alt pulled to the last column that fits");
    check_int(c.x, 1055, "x still fits and is left alone");
    check_int(c.x_sport, 1055, "Sport falls back to the classic tile");

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

static void t_capsule(void)
{
    overlay_cfg_t c;
    const char *p;

    printf("capsule strip\n");
    overlay_cfg_defaults(&c);
    check_int(c.capsule_text, 0, "off by default");
    check_int(overlay_cfg_sanitize(&c), 0, "defaults need no clamping");
    check(!strcmp(c.capsule_font, OVERLAY_CFG_CAPSULE_FONT), "default font");

    p = write_tmp("ovl_capsule.conf",
        "capsule_text 1\n"
        "capsule_left_x 400\n"
        "capsule_right_x 1000\n"
        "capsule_y 410\n"
        "capsule_h 30\n"
        "capsule_font_px 22\n"
        "capsule_font /tmp/some.ttf # comment\n");
    check_int(overlay_cfg_load(&c, p), 7, "7 keys applied, the font path among them");
    check_int(overlay_cfg_sanitize(&c), 0, "a good strip needs no clamping");
    check_int(c.capsule_text, 1, "on from the card");
    check_int(c.capsule_left_x, 400, "left");
    check_int(c.capsule_right_x, 1000, "right");
    check_int(c.capsule_y, 410, "y");
    check_int(c.capsule_h, 30, "h");
    check_int(c.capsule_font_px, 22, "px");
    check(!strcmp(c.capsule_font, "/tmp/some.ttf"), "font path without the comment");

    overlay_cfg_defaults(&c);
    c.capsule_left_x = -1;
    check_int(overlay_cfg_sanitize(&c), 1, "left off the canvas clamped");
    check_int(c.capsule_left_x, 372, "left back to the default");
    check_int(c.capsule_right_x, 1068, "right with it");

    overlay_cfg_defaults(&c);
    c.capsule_right_x = 1441;
    check_int(overlay_cfg_sanitize(&c), 1, "right off the canvas clamped");
    check_int(c.capsule_right_x, 1068, "right back to the default");

    overlay_cfg_defaults(&c);
    c.capsule_right_x = c.capsule_left_x + 63;
    check_int(overlay_cfg_sanitize(&c), 1, "too narrow clamped");
    check_int(c.capsule_right_x, 1068, "narrow strip back to the default");

    overlay_cfg_defaults(&c);
    c.capsule_right_x = c.capsule_left_x + 64;
    check_int(overlay_cfg_sanitize(&c), 0, "narrowest strip allowed");

    overlay_cfg_defaults(&c);
    c.capsule_y = 542 - 36 + 1;
    check_int(overlay_cfg_sanitize(&c), 1, "below the canvas clamped");
    check_int(c.capsule_y, 397, "y back to the default");

    overlay_cfg_defaults(&c);
    c.capsule_y = 542 - 36;
    check_int(overlay_cfg_sanitize(&c), 0, "last row allowed");

    overlay_cfg_defaults(&c);
    c.capsule_h = 15;
    check_int(overlay_cfg_sanitize(&c), 1, "too low clamped");
    check_int(c.capsule_h, 36, "h back to the default");

    overlay_cfg_defaults(&c);
    c.capsule_h = 81;
    check_int(overlay_cfg_sanitize(&c), 1, "too high clamped");

    overlay_cfg_defaults(&c);
    c.capsule_font_px = 9;
    check_int(overlay_cfg_sanitize(&c), 1, "font too small clamped");
    check_int(c.capsule_font_px, 24, "px back to the default");

    overlay_cfg_defaults(&c);
    c.capsule_font_px = 61;
    check_int(overlay_cfg_sanitize(&c), 1, "font too big clamped");

    overlay_cfg_defaults(&c);
    c.capsule_text = 5;
    overlay_cfg_sanitize(&c);
    check_int(c.capsule_text, 1, "any non-zero is on");

    /* The emergency small canvas has no room for the strip: no text at all. */
    overlay_cfg_defaults(&c);
    c.canvas_width = 210;
    c.canvas_height = 153;
    c.x = c.y = c.x_alt = c.y_alt = 0;
    c.capsule_text = 1;
    overlay_cfg_sanitize(&c);
    check_int(c.capsule_text, 0, "emergency canvas turns the text off");
}

static void t_call_card(void)
{
    overlay_cfg_t c;
    const char *p;

    printf("call card\n");
    overlay_cfg_defaults(&c);
    check_int(c.call_card, 1, "on by default");
    /* the built-in values, for a card without the conf: the pill of rc28 */
    check_int(c.call_card_w, 320, "default w");
    check_int(c.call_card_h, 44, "default h");
    check_int(c.call_card_x_small, 772, "default x small");
    check_int(c.call_card_y_small, 334, "default y small");
    check_int(c.call_card_x_big, 560, "default x big");
    check_int(c.call_card_y_big, 334, "default y big");
    check_int(c.call_card_x_sport, 110, "default x sport");
    check_int(c.call_card_y_sport, 334, "default y sport");
    check_int(c.call_card_name_px, 24, "default name px");
    check_int(c.call_card_status_px, 22, "default status px");
    /* rc30: right above the VC bar (from about y 384), and on the small gauges at the
     * right end of the map, ending before the maneuver tile (x 1091) */
    check(c.call_card_y_small + c.call_card_h <= 380 && c.call_card_y_small + c.call_card_h >= 374,
          "small: the card ends just above the bar");
    check(c.call_card_y_big + c.call_card_h <= 380, "big: the card ends above the bar");
    /* rc31: the small speedometer's edge in this row is at about x 1106 (photos, +-6):
     * close to it, never under it.  The card stands below the maneuver tile. */
    check(c.call_card_x_small + c.call_card_w <= 1106 - 8, "small: clear of the speedometer");
    check(c.call_card_x_small + c.call_card_w >= 1106 - 20, "small: close to the speedometer");
    check_int(c.call_card_x_big + c.call_card_w / 2, 720, "big: centred between the gauges");
    check_int(overlay_cfg_sanitize(&c), 0, "defaults need no clamping");

    p = write_tmp("ovl_card.conf",
        "call_card 0\n"
        "call_card_ringing 0\n"
        "call_card_w 300\n"
        "call_card_h 90\n"
        "call_card_x_small 700\n"
        "call_card_y_small 280\n"
        "call_card_x_big 570\n"
        "call_card_y_big 290\n"
        "call_card_name_px 26\n"
        "call_card_status_px 20\n"
        "call_card_font /tmp/n.ttf\n"
        "call_card_font_bold /tmp/b.ttf # bold\n");
    check_int(overlay_cfg_load(&c, p), 12, "12 keys applied");
    check_int(overlay_cfg_sanitize(&c), 0, "a good card needs no clamping");
    check_int(c.call_card, 0, "off from the card");
    check_int(c.call_card_ringing, 0, "no ringing from the card");
    check_int(c.call_card_w, 300, "w");
    check_int(c.call_card_h, 90, "h");
    check_int(c.call_card_x_small, 700, "x small");
    check_int(c.call_card_y_small, 280, "y small");
    check_int(c.call_card_x_big, 570, "x big");
    check_int(c.call_card_y_big, 290, "y big");
    check_int(c.call_card_name_px, 26, "name px");
    check_int(c.call_card_status_px, 20, "status px");
    check(!strcmp(c.call_card_font, "/tmp/n.ttf"), "font path");
    check(!strcmp(c.call_card_font_bold, "/tmp/b.ttf"), "bold path without the comment");

#define CARD_BAD(field, value, what, want) do {                          \
        overlay_cfg_defaults(&c);                                        \
        c.field = (value);                                               \
        check_int(overlay_cfg_sanitize(&c), 1, what);                    \
        check_int(c.field, (want), what " back to the default");         \
    } while (0)
#define CARD_GOOD(field, value, what) do {                               \
        overlay_cfg_defaults(&c);                                        \
        c.field = (value);                                               \
        check_int(overlay_cfg_sanitize(&c), 0, what);                    \
    } while (0)
    CARD_BAD(call_card_w, 119, "too narrow", 320);
    CARD_GOOD(call_card_w, 120, "narrowest allowed");
    CARD_BAD(call_card_w, 701, "too wide", 320);
    CARD_BAD(call_card_h, 31, "too low", 44);
    CARD_GOOD(call_card_h, 32, "lowest allowed");
    CARD_BAD(call_card_h, 161, "too high", 44);
    CARD_BAD(call_card_name_px, 9, "name font small", 24);
    CARD_BAD(call_card_name_px, 61, "name font big", 24);
    CARD_BAD(call_card_status_px, 9, "status font small", 22);
    CARD_BAD(call_card_status_px, 61, "status font big", 22);
    CARD_BAD(call_card_x_small, -1, "x small negative", 772);
    CARD_BAD(call_card_x_small, 1440 - 320 + 1, "x small off the canvas", 772);
    CARD_GOOD(call_card_x_small, 1440 - 320, "x small at the edge");
    CARD_BAD(call_card_y_small, -1, "y small negative", 334);
    CARD_BAD(call_card_y_small, 542 - 44 + 1, "y small off the canvas", 334);
    CARD_GOOD(call_card_y_small, 542 - 44, "y small at the edge");
    CARD_BAD(call_card_x_big, -1, "x big negative", 560);
    CARD_BAD(call_card_x_big, 1440 - 320 + 1, "x big off the canvas", 560);
    CARD_BAD(call_card_y_big, -1, "y big negative", 334);
    CARD_BAD(call_card_y_big, 542 - 44 + 1, "y big off the canvas", 334);

    overlay_cfg_defaults(&c);
    c.call_card = 5;
    c.call_card_ringing = 7;
    overlay_cfg_sanitize(&c);
    check_int(c.call_card, 1, "any non-zero is on");
    check_int(c.call_card_ringing, 1, "any non-zero is ringing on");

    /* The emergency small canvas has no room for the card: none at all. */
    overlay_cfg_defaults(&c);
    c.canvas_width = 210;
    c.canvas_height = 153;
    c.x = c.y = c.x_alt = c.y_alt = 0;
    overlay_cfg_sanitize(&c);
    check_int(c.call_card, 0, "emergency canvas turns the card off");
}

static void t_eta_pill(void)
{
    overlay_cfg_t c;
    const char *p;

    printf("eta pill\n");
    overlay_cfg_defaults(&c);
    /* the built-in values, for a card without the conf */
    check_int(c.eta_pill, 1, "on by default");
    check_int(c.eta_pill_w, 400, "default w");
    check_int(c.eta_pill_h, 44, "default h");
    check_int(c.eta_pill_x_small, 348, "default x small");
    check_int(c.eta_pill_y_small, 334, "default y small");
    check_int(c.eta_pill_x_big, 520, "default x big");
    check_int(c.eta_pill_y_big, 334, "default y big");
    check_int(c.eta_pill_x_sport, 110, "default x sport");
    check_int(c.eta_pill_y_sport, 334, "default y sport");
    check_int(overlay_cfg_sanitize(&c), 0, "defaults need no clamping");
    /* by the rev counter and clear of the call card on the small gauges; the same
     * line and height as the call card, so the two read as a pair */
    check(c.eta_pill_x_small + c.eta_pill_w <= c.call_card_x_small, "small: ends before the call card");
    /* rc31: the rev counter's edge in this row is at about x 334 (photos, +-6): the
     * pill starts clear of it, and as far from it as the call card ends before the
     * speedometer.  The two gauges mirror each other about the middle of the canvas. */
    check(c.eta_pill_x_small >= 334 + 8, "small: clear of the rev counter");
    check(c.eta_pill_x_small <= 334 + 20, "small: close to the rev counter");
    check_int(c.eta_pill_x_small + c.call_card_x_small + c.call_card_w, 1440,
              "small: the same gap at both gauges");
    check_int(c.eta_pill_y_small, c.call_card_y_small, "small: the line of the call card");
    check_int(c.eta_pill_h, c.call_card_h, "as high as the call card");
    check_int(c.eta_pill_y_big, c.call_card_y_big, "big: the line of the call card");
    check_int(c.eta_pill_y_sport, c.call_card_y_sport, "sport: the line of the call card");
    /* rc30: the view areas with a route end at y 334 (mib2high-lvds-viewareas.conf), so
     * the pill starts where the phone's own things stop */
    check_int(c.eta_pill_y_small, 334, "small: starts where the view areas end");
    check_int(c.eta_pill_y_big, 334, "big: starts where the view areas end");
    check(c.eta_pill_y_small + c.eta_pill_h <= 380, "small: ends above the bar");
    /* the middle of the strip on the big gauges is the middle between them (x 720) */
    check_int(c.eta_pill_x_big + c.eta_pill_w / 2, 720, "big: centred between the gauges");

    p = write_tmp("ovl_pill.conf",
        "eta_pill 0\n"
        "eta_pill_w 360\n"
        "eta_pill_h 40\n"
        "eta_pill_x_small 300\n"
        "eta_pill_y_small 330\n"
        "eta_pill_x_big 540\n"
        "eta_pill_y_big 300\n"
        "eta_pill_x_sport 90\n"
        "eta_pill_y_sport 310\n");
    check_int(overlay_cfg_load(&c, p), 9, "9 keys applied");
    check_int(overlay_cfg_sanitize(&c), 0, "a good pill needs no clamping");
    check_int(c.eta_pill, 0, "off from the card");
    check_int(c.eta_pill_w, 360, "w");
    check_int(c.eta_pill_h, 40, "h");
    check_int(c.eta_pill_x_small, 300, "x small");
    check_int(c.eta_pill_y_small, 330, "y small");
    check_int(c.eta_pill_x_big, 540, "x big");
    check_int(c.eta_pill_y_big, 300, "y big");
    check_int(c.eta_pill_x_sport, 90, "x sport");
    check_int(c.eta_pill_y_sport, 310, "y sport");

#define PILL_BAD(field, value, what, want) do {                          \
        overlay_cfg_defaults(&c);                                        \
        c.field = (value);                                               \
        check_int(overlay_cfg_sanitize(&c), 1, what);                    \
        check_int(c.field, (want), what " back to the default");         \
    } while (0)
#define PILL_GOOD(field, value, what) do {                               \
        overlay_cfg_defaults(&c);                                        \
        c.field = (value);                                               \
        check_int(overlay_cfg_sanitize(&c), 0, what);                    \
    } while (0)
    PILL_BAD(eta_pill_w, 159, "too narrow", 400);
    PILL_GOOD(eta_pill_w, 160, "narrowest allowed");
    PILL_BAD(eta_pill_w, 701, "too wide", 400);
    PILL_BAD(eta_pill_h, 31, "too low", 44);
    PILL_GOOD(eta_pill_h, 32, "lowest allowed");
    PILL_BAD(eta_pill_h, 161, "too high", 44);
    PILL_BAD(eta_pill_x_small, -1, "x small negative", 348);
    PILL_BAD(eta_pill_x_small, 1440 - 400 + 1, "x small off the canvas", 348);
    PILL_GOOD(eta_pill_x_small, 1440 - 400, "x small at the edge");
    PILL_BAD(eta_pill_y_small, -1, "y small negative", 334);
    PILL_BAD(eta_pill_y_small, 542 - 44 + 1, "y small off the canvas", 334);
    PILL_GOOD(eta_pill_y_small, 542 - 44, "y small at the edge");
    PILL_BAD(eta_pill_x_big, -1, "x big negative", 520);
    PILL_BAD(eta_pill_x_big, 1440 - 400 + 1, "x big off the canvas", 520);
    PILL_BAD(eta_pill_y_big, -1, "y big negative", 334);
    PILL_BAD(eta_pill_y_big, 542 - 44 + 1, "y big off the canvas", 334);
    PILL_BAD(eta_pill_x_sport, -1, "x sport negative", 110);
    PILL_BAD(eta_pill_x_sport, 1440 - 400 + 1, "x sport off the canvas", 110);
    PILL_BAD(eta_pill_y_sport, -1, "y sport negative", 334);
    PILL_BAD(eta_pill_y_sport, 542 - 44 + 1, "y sport off the canvas", 334);

    overlay_cfg_defaults(&c);
    c.eta_pill = 5;
    overlay_cfg_sanitize(&c);
    check_int(c.eta_pill, 1, "any non-zero is on");

    /* The emergency small canvas has no room for the pill: none at all. */
    overlay_cfg_defaults(&c);
    c.canvas_width = 210;
    c.canvas_height = 153;
    c.x = c.y = c.x_alt = c.y_alt = 0;
    overlay_cfg_sanitize(&c);
    check_int(c.eta_pill, 0, "emergency canvas turns the pill off");
}

static void test_sport(void)
{
    overlay_cfg_t c;

    overlay_cfg_defaults(&c);
    check_int(overlay_cfg_sanitize(&c), 0, "sport defaults are valid");
    check_int(c.x_sport, 984, "sport x");
    check_int(c.y_sport, 139, "sport y");
    check_int(c.width_sport, 328, "sport width");
    check_int(c.height_sport, 180, "sport height");
    check_int(c.crop_x_sport, 0, "sport crop x");
    check_int(c.crop_y_sport, 0, "sport crop y");
    check_int(c.call_card_x_sport, 110, "sport card x");
    check_int(c.call_card_y_sport, 334, "sport card y");

#define SPORT_BAD(field, val, what) do { \
        overlay_cfg_defaults(&c); c.field = (val); overlay_cfg_sanitize(&c); \
        check_int(c.x_sport, c.x, what ": whole rectangle back to classic"); \
        check_int(c.width_sport, c.width, what ": width too"); \
        check_int(c.crop_x_sport, c.crop_x, what ": crop too"); \
    } while (0)
    SPORT_BAD(width_sport, 15, "too narrow");
    SPORT_BAD(height_sport, 15, "too low");
    SPORT_BAD(crop_x_sport, -1, "crop x negative");
    SPORT_BAD(crop_y_sport, -1, "crop y negative");
    SPORT_BAD(width_sport, 329, "wider than the render");
    SPORT_BAD(height_sport, 182, "taller than the render");
    SPORT_BAD(x_sport, -1, "x negative");
    SPORT_BAD(y_sport, -1, "y negative");
    SPORT_BAD(x_sport, 1440 - 328 + 1, "off the canvas right");
    SPORT_BAD(y_sport, 542 - 180 + 1, "off the canvas bottom");

    overlay_cfg_defaults(&c);
    c.height_sport = 181;
    check_int(overlay_cfg_sanitize(&c), 0, "the whole render fits");
    overlay_cfg_defaults(&c);
    c.x_sport = 1440 - 328;
    check_int(overlay_cfg_sanitize(&c), 0, "at the canvas edge");
    check_int(c.x_sport, 1440 - 328, "at the canvas edge stays");

    overlay_cfg_defaults(&c);
    c.call_card_x_sport = -1;
    overlay_cfg_sanitize(&c);
    check_int(c.call_card_x_sport, 110, "sport card x negative");
    overlay_cfg_defaults(&c);
    c.call_card_x_sport = 1440 - 320 + 1;
    overlay_cfg_sanitize(&c);
    check_int(c.call_card_x_sport, 110, "sport card x off the canvas");
    overlay_cfg_defaults(&c);
    c.call_card_y_sport = 542 - 44 + 1;
    overlay_cfg_sanitize(&c);
    check_int(c.call_card_y_sport, 334, "sport card y off the canvas");
}

int main(void)
{
    test_sport();
    t_call_card();
    t_eta_pill();
    t_capsule();
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
