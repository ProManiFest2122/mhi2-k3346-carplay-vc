/*
 * overlay_cfg — runtime configuration for the K3346 cluster overlay.
 *
 * Geometry and the displayable number are NOT compiled in.  Etap 7a measures
 * them in the car (dmdt gd / dmdt ts 4) and we correct the values by editing a
 * text file on the SD card — no rebuild, no second trip to the toolchain.
 * Both the probe (probe/vc_overlay_probe.c) and the renderer
 * (platform_harman.c) read the same file so a measured value applies to both.
 *
 * Format: one "key value" per line, '#' starts a comment, unknown keys are
 * ignored (so a newer card stays readable by an older binary).
 *
 *   displayable 98      # KD window CAPTION -> DisplayManager displayable id
 *   render_width  328   # the RENDERER's canvas, see LANDMINE below
 *   render_height 181
 *   crop_x 59           # which part of that canvas reaches the cluster
 *   crop_y 27
 *   width  210          # size of that crop = the stock cluster tile
 *   height 153
 *   display 1           # KD display index of the cluster (0 = MMI)
 *   context 901         # dmdt context the shell script declares (informational)
 *   alpha 1             # 1 = per-source alpha, blend over the CarPlay map
 *   managed 1           # 1 = hand the window to the DisplayManager group
 *   swap_interval 2
 *   fps 30
 *   x 1055 / y 207     # tile origin with big clocks (slot 1)
 *   x_alt 1091 / y_alt 110   # ... and with small clocks (slot 0)
 *   visible_top_big 0  # rows of the crop the VC covers with big clocks
 *   visible_top_small 0
 *   capsule_text 0     # 1 = draw arrival/time and distance on the VC bar
 *   capsule_left_x 372 / capsule_right_x 1068 / capsule_y 397 / capsule_h 36
 *   capsule_font_px 24
 *   capsule_font /path/to/font.ttf   # text value, like call_card_font*
 *   call_card 1 / call_card_ringing 1 / call_card_w 320 / call_card_h 78
 *   call_card_x_small 735 / call_card_y_small 300 / call_card_x_big 560 / call_card_y_big 305
 *   call_card_name_px 28 / call_card_status_px 22
 *   call_card_font /path / call_card_font_bold /path
 *
 * THREE RECTANGLES, and mixing two of them up cost a car visit on 2026-10-27:
 *
 *   render_width x render_height   what the renderer draws into.  This is
 *                                  Luka's CR_DEFAULT_WIDTH/HEIGHT and it is
 *                                  his whole coordinate system, not a window
 *                                  size (details in the LANDMINE below).
 *   crop_x,crop_y + width x height the part of it that reaches the cluster.
 *                                  Luka's CR_POPUP_*: the maneuver is framed
 *                                  into exactly this rectangle.
 *   x,y (x_alt,y_alt) + same size  where that crop lands on the 1440x542
 *                                  cluster canvas, per VC gauge layout.
 *
 * LANDMINE (docs/cluster-and-rgi/HUD_RENDERER.md:138): render_width/height is
 * not a knob to "fix" placement with.  maneuver_render does not scale: it
 * centres the maneuver using CR_DEFAULT_HEIGHT and CR_POPUP_Y (render.c:1219),
 * converts every 2D element to NDC through CR_DEFAULT_WIDTH/HEIGHT
 * (render.c:1425), derives its scissor from them (render.c:1407) and solves the
 * lane framing against a hard-coded 328.f/181 aspect (scene/lane_panel.cpp:286).
 * Feed it anything else and the scene renders in the wrong frame — which is
 * precisely what happened when the stock tile measurement (210x153) was written
 * here instead of into width/height.  To move or resize what is SHOWN, use
 * crop_x/crop_y, width/height and x/y.  Those are safe.
 */

#ifndef K3346_OVERLAY_CFG_H
#define K3346_OVERLAY_CFG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OVERLAY_CFG_PATH_PRIMARY  "/mnt/app/root/mib2high-overlay.conf"
#define OVERLAY_CFG_PATH_LOCAL    "./mib2high-overlay.conf"
/* Ships with the MMI firmware, so the font is never in our package. */
#define OVERLAY_CFG_CAPSULE_FONT  "/mnt/app/eso/browser/fonts/AudiTypeDisplay-Normal_14.ttf"
#define OVERLAY_CFG_CALL_FONT_BOLD "/mnt/app/eso/browser/fonts/AudiTypeDisplay-Bold_11.ttf"

typedef struct {
    int displayable;
    /* The renderer's own canvas — Luka's CR_DEFAULT_WIDTH/HEIGHT.  See the
     * LANDMINE above: this is a coordinate system, not a window size. */
    int render_width, render_height;
    /* Top-left of the shown crop inside that canvas — Luka's CR_POPUP_X/Y. */
    int crop_x, crop_y;
    /* Size of that crop, which is also the stock maneuver tile on the cluster
     * (measured 210x153 from a dmdt ts 4 screenshot, 2026-10-26) — Luka's
     * CR_POPUP_W/H.  The two agreeing is what makes a 1:1 blit possible. */
    int width, height;
    /* The WINDOW, which is a different thing since 2026-10-27.  Measured in
     * the car that day: the DisplayManager honours the size of a window whose
     * displayable it has no entry for, but always places it at (0,0) — our
     * 210x153 probe landed in the top-left corner, pixel exact.  So the window
     * takes the whole cluster canvas and the crop is drawn at an offset
     * inside it. */
    int canvas_width, canvas_height;
    int display;
    int context;
    int alpha;
    int managed;
    int swap_interval;
    int fps;
    /* Where the stock maneuver tile sits in each VC layout, measured from
     * dmdt ts 4 screenshots on 2026-10-26.  The slot comes from the DSITracer
     * capture ring (overlay_layout.h); until 2026-10-28 it never arrived and
     * the tile stayed in x/y in BOTH layouts.  The labels of the two
     * measurements were swapped that day as well - see overlay_layout.h. */
    int x, y;              /* slot 1 - big clocks: inside the right gauge */
    int x_alt, y_alt;      /* slot 0 - small clocks: top right of the map */
    /* How many rows at the TOP of the crop the VC covers in each layout, if
     * any.  0 = the whole crop is visible (Luka's default framing).  Anything
     * else is handed to render_set_visible_area(), the same call
     * CMD_VISIBLE_AREA makes on MHI2Q, and the renderer re-frames the
     * maneuver below it - which matters because the arm of a turn arrow sits
     * in the top rows of the crop.  A knob for the card, not a measurement. */
    int visible_top_big, visible_top_small;
    /* The two ends of the VC bar with small clocks (capsule_text.h).  The strip
     * runs from left_x to right_x: left text starts at its first column, right
     * text ends at its last.  Estimated from photos, corrected on the card. */
    int capsule_text;
    int capsule_left_x, capsule_right_x, capsule_y, capsule_h, capsule_font_px;
    char capsule_font[160];
    /* The call card on the map (call_card.h): one strip, placed per layout.
     * Estimated from photos, corrected on the card. */
    int call_card, call_card_ringing;
    int call_card_w, call_card_h;
    int call_card_x_small, call_card_y_small, call_card_x_big, call_card_y_big;
    int call_card_name_px, call_card_status_px;
    char call_card_font[160], call_card_font_bold[160];
    char source[160];      /* which file the values came from, for the log */
} overlay_cfg_t;

static void overlay_cfg_defaults(overlay_cfg_t *c)
{
    memset(c, 0, sizeof(*c));
    /* 98: free range 61..99 — above the highest semantic displayable in this
     * firmware (60 DISPLAYABLE_HUD_MAP_VIEW, verified with javap against the
     * car's lsd.jxe) and below the KDK backings 101/102.  Same number Luka
     * uses on MHI2Q, so his protocol constants stay meaningful. */
    c->displayable   = 98;
    /* 328x181 is CR_DEFAULT_WIDTH/HEIGHT from upstream/protocol.h — the canvas
     * every line of the renderer measures in.  It is also the larger of the two
     * KVS_FPK styles this firmware ships
     * (navigation/resources/app/au/eu/pcconfig/komoviewstyle.conf:
     * "2; KVS_FPK;210x153", "3; KVS_FPK;328x181"), which is no coincidence:
     * both clusters draw their maneuver tile from the same catalogue. */
    c->render_width  = 328;
    c->render_height = 181;
    /* CR_POPUP_X/Y: "big-screen popup crop within the 328x180 content area"
     * (protocol.h:114).  The maneuver is centred into this rectangle by
     * render.c:1219, so showing exactly it is showing the finished frame. */
    c->crop_x        = 59;
    c->crop_y        = 27;
    /* CR_POPUP_W/H, and the same 210x153 measured on this car as the stock
     * maneuver tile. */
    c->width         = 210;
    c->height        = 153;
    /* The cluster canvas, same geometry the Etap 3 map renderer already uses
     * (mib2high-lvds-control.sh: "renderer start out=1 layer=58 1440x542@30"),
     * and the size dmdt ts 4 screenshots come back as. */
    c->canvas_width  = 1440;
    c->canvas_height = 542;
    c->display       = -1;     /* -1 = leave DISPLAY alone, like the stock recipe */
    c->context       = 901;
    c->alpha         = 1;
    c->managed       = 1;      /* ignored on this unit, see kd_surface.h */
    c->swap_interval = 2;
    c->fps           = 30;
    c->x             = 1055;   /* big clocks   */
    c->y             = 207;
    c->x_alt         = 1091;   /* small clocks */
    c->y_alt         = 110;
    c->visible_top_big   = 0;
    c->visible_top_small = 0;
    /* Off: under the bar's base the text was too dark to read (run-53). */
    c->capsule_text    = 0;
    c->capsule_left_x  = 372;
    c->capsule_right_x = 1068;
    c->capsule_y       = 397;
    c->capsule_h       = 36;
    c->capsule_font_px = 24;
    snprintf(c->capsule_font, sizeof(c->capsule_font), "%s", OVERLAY_CFG_CAPSULE_FONT);
    c->call_card         = 1;
    c->call_card_ringing = 1;
    c->call_card_w       = 320;
    c->call_card_h       = 78;
    c->call_card_x_small = 735;
    c->call_card_y_small = 300;
    c->call_card_x_big   = 560;
    c->call_card_y_big   = 305;
    c->call_card_name_px = 28;
    c->call_card_status_px = 22;
    snprintf(c->call_card_font, sizeof(c->call_card_font), "%s", OVERLAY_CFG_CAPSULE_FONT);
    snprintf(c->call_card_font_bold, sizeof(c->call_card_font_bold), "%s", OVERLAY_CFG_CALL_FONT_BOLD);
    snprintf(c->source, sizeof(c->source), "(built-in defaults)");
}

static int overlay_cfg_apply(overlay_cfg_t *c, const char *key, long v)
{
    if      (!strcmp(key, "displayable"))   c->displayable   = (int)v;
    else if (!strcmp(key, "render_width"))  c->render_width  = (int)v;
    else if (!strcmp(key, "render_height")) c->render_height = (int)v;
    else if (!strcmp(key, "crop_x"))        c->crop_x        = (int)v;
    else if (!strcmp(key, "crop_y"))        c->crop_y        = (int)v;
    else if (!strcmp(key, "width"))         c->width         = (int)v;
    else if (!strcmp(key, "height"))        c->height        = (int)v;
    else if (!strcmp(key, "canvas_width"))  c->canvas_width  = (int)v;
    else if (!strcmp(key, "canvas_height")) c->canvas_height = (int)v;
    else if (!strcmp(key, "display"))       c->display       = (int)v;
    else if (!strcmp(key, "context"))       c->context       = (int)v;
    else if (!strcmp(key, "alpha"))         c->alpha         = (int)v;
    else if (!strcmp(key, "managed"))       c->managed       = (int)v;
    else if (!strcmp(key, "swap_interval")) c->swap_interval = (int)v;
    else if (!strcmp(key, "fps"))           c->fps           = (int)v;
    else if (!strcmp(key, "x"))             c->x             = (int)v;
    else if (!strcmp(key, "y"))             c->y             = (int)v;
    else if (!strcmp(key, "x_alt"))         c->x_alt         = (int)v;
    else if (!strcmp(key, "y_alt"))         c->y_alt         = (int)v;
    else if (!strcmp(key, "visible_top_big"))   c->visible_top_big   = (int)v;
    else if (!strcmp(key, "visible_top_small")) c->visible_top_small = (int)v;
    else if (!strcmp(key, "capsule_text"))      c->capsule_text      = (int)v;
    else if (!strcmp(key, "capsule_left_x"))    c->capsule_left_x    = (int)v;
    else if (!strcmp(key, "capsule_right_x"))   c->capsule_right_x   = (int)v;
    else if (!strcmp(key, "capsule_y"))         c->capsule_y         = (int)v;
    else if (!strcmp(key, "capsule_h"))         c->capsule_h         = (int)v;
    else if (!strcmp(key, "capsule_font_px"))   c->capsule_font_px   = (int)v;
    else if (!strcmp(key, "call_card"))           c->call_card           = (int)v;
    else if (!strcmp(key, "call_card_ringing"))   c->call_card_ringing   = (int)v;
    else if (!strcmp(key, "call_card_w"))         c->call_card_w         = (int)v;
    else if (!strcmp(key, "call_card_h"))         c->call_card_h         = (int)v;
    else if (!strcmp(key, "call_card_x_small"))   c->call_card_x_small   = (int)v;
    else if (!strcmp(key, "call_card_y_small"))   c->call_card_y_small   = (int)v;
    else if (!strcmp(key, "call_card_x_big"))     c->call_card_x_big     = (int)v;
    else if (!strcmp(key, "call_card_y_big"))     c->call_card_y_big     = (int)v;
    else if (!strcmp(key, "call_card_name_px"))   c->call_card_name_px   = (int)v;
    else if (!strcmp(key, "call_card_status_px")) c->call_card_status_px = (int)v;
    else return 0;
    return 1;
}

/* Reads the first readable path.  A missing file is not an error: the built-in
 * defaults are the Etap 7a starting point.  A malformed line is skipped, never
 * fatal — a renderer that refuses to start because of a typo on the card would
 * be worse than one that starts in the wrong place and says so in the log. */
static int overlay_cfg_load(overlay_cfg_t *c, const char *path_override)
{
    const char *paths[3];
    char line[256];
    int i, applied = 0;
    FILE *f = NULL;

    overlay_cfg_defaults(c);

    paths[0] = path_override;
    paths[1] = OVERLAY_CFG_PATH_PRIMARY;
    paths[2] = OVERLAY_CFG_PATH_LOCAL;

    for (i = 0; i < 3; i++) {
        if (!paths[i] || !paths[i][0]) continue;
        f = fopen(paths[i], "r");
        if (f) { snprintf(c->source, sizeof(c->source), "%s", paths[i]); break; }
    }
    if (!f) return 0;

    while (fgets(line, sizeof(line), f)) {
        char key[64], text[sizeof(c->capsule_font)];
        char *p = line, *hash;
        long v;
        int n = 0;

        hash = strchr(p, '#');
        if (hash) *hash = '\0';
        if (sscanf(p, "%63s %159s", key, text) == 2) {
            char *dst = !strcmp(key, "capsule_font") ? c->capsule_font
                      : !strcmp(key, "call_card_font") ? c->call_card_font
                      : !strcmp(key, "call_card_font_bold") ? c->call_card_font_bold : NULL;
            if (dst) {
                snprintf(dst, sizeof(c->capsule_font), "%s", text);
                applied++;
                continue;
            }
        }
        /* "key value" with any run of blanks; tolerate CR from a card edited
         * on Windows (the installer ships LF, a user's editor may not). */
        if (sscanf(p, "%63s %ld%n", key, &v, &n) < 2) continue;
        applied += overlay_cfg_apply(c, key, v);
    }
    fclose(f);
    return applied;
}

/* Sanity clamp.  Returns the number of values that had to be corrected, so the
 * caller can log "card says X, using Y" instead of silently drawing nowhere.
 *
 * The order is deliberate: the renderer's canvas first, then the crop against
 * that canvas, then the crop against the cluster, then the position.  Each
 * later rule can only be checked once the one before it holds. */
static int overlay_cfg_sanitize(overlay_cfg_t *c)
{
    int fixed = 0;
    /* 1..109, and the lower bound is not cosmetic: the DM turns the caption
     * into the displayable number with atoi(), so "0" or a non-number would
     * hand us somebody else's displayable 0, and "-1" would make it drop the
     * window without a word.  109 is the last id below the HMI's own
     * MAX_DISPLAYABLES = 110 (de.audi.tghu.fwhmi.DisplayManager). */
    if (c->displayable < 1 || c->displayable > 109)  { c->displayable = 98; fixed++; }
    if (c->render_width  < 16 || c->render_width  > 1920) { c->render_width  = 328; fixed++; }
    if (c->render_height < 16 || c->render_height > 1080) { c->render_height = 181; fixed++; }
    if (c->width  < 16 || c->width  > 1920)          { c->width  = 210; fixed++; }
    if (c->height < 16 || c->height > 1080)          { c->height = 153; fixed++; }
    /* The crop is cut out of what the renderer drew, so it cannot be larger. */
    if (c->width  > c->render_width)  { c->width  = c->render_width;  fixed++; }
    if (c->height > c->render_height) { c->height = c->render_height; fixed++; }
    if (c->canvas_width  < 64 || c->canvas_width  > 1920) { c->canvas_width  = 1440; fixed++; }
    if (c->canvas_height < 64 || c->canvas_height > 1080) { c->canvas_height = 542;  fixed++; }
    /* ...and it is drawn inside the window, so it has to fit in that too. */
    if (c->width  > c->canvas_width)  { c->width  = c->canvas_width;  fixed++; }
    if (c->height > c->canvas_height) { c->height = c->canvas_height; fixed++; }
    /* A crop hanging off the rendered canvas would sample the clamped edge
     * texel as a smear - that looks like a rendering bug and is not one. */
    {
        int max_cx = c->render_width  - c->width;
        int max_cy = c->render_height - c->height;
        if (c->crop_x < 0 || c->crop_x > max_cx) { c->crop_x = 59 > max_cx ? max_cx : 59; fixed++; }
        if (c->crop_y < 0 || c->crop_y > max_cy) { c->crop_y = 27 > max_cy ? max_cy : 27; fixed++; }
    }
    if (c->display < -1 || c->display > 8)           { c->display = -1;  fixed++; }
    if (c->fps < 1 || c->fps > 60)                   { c->fps = 30;      fixed++; }
    if (c->swap_interval < 0 || c->swap_interval > 4){ c->swap_interval = 2; fixed++; }
    /* x/y are the crop's origin INSIDE the window, so the bound is the canvas
     * minus the crop: a value that would hang it off the edge is a typo on the
     * card, not a position. */
    {
        int max_x = c->canvas_width  - c->width;
        int max_y = c->canvas_height - c->height;
        if (c->x < 0 || c->x > max_x)         { c->x = 1055 > max_x ? max_x : 1055; fixed++; }
        if (c->y < 0 || c->y > max_y)         { c->y = 207  > max_y ? max_y : 207;  fixed++; }
        if (c->x_alt < 0 || c->x_alt > max_x) { c->x_alt = 1091 > max_x ? max_x : 1091; fixed++; }
        if (c->y_alt < 0 || c->y_alt > max_y) { c->y_alt = 110  > max_y ? max_y : 110;  fixed++; }
    }
    /* At least 16 rows must stay visible, or there is nothing to frame into. */
    if (c->visible_top_big < 0 || c->visible_top_big > c->height - 16)
        { c->visible_top_big = 0; fixed++; }
    if (c->visible_top_small < 0 || c->visible_top_small > c->height - 16)
        { c->visible_top_small = 0; fixed++; }
    /* The strip is inside the window or it is not drawn at all: a typo on the
     * card must not put text on top of a gauge. */
    if (c->capsule_h < 16 || c->capsule_h > 80)             { c->capsule_h = 36; fixed++; }
    if (c->capsule_font_px < 10 || c->capsule_font_px > 60) { c->capsule_font_px = 24; fixed++; }
    if (c->capsule_left_x < 0 || c->capsule_right_x > c->canvas_width
        || c->capsule_right_x - c->capsule_left_x < 64) {
        c->capsule_left_x = 372;
        c->capsule_right_x = 1068;
        fixed++;
    }
    if (c->capsule_y < 0 || c->capsule_y + c->capsule_h > c->canvas_height) {
        c->capsule_y = 397;
        fixed++;
    }
    /* Only a small canvas (the emergency canvas_width 210) gets here. */
    if (c->capsule_right_x > c->canvas_width || c->capsule_y + c->capsule_h > c->canvas_height)
        c->capsule_text = 0;
    c->capsule_text = c->capsule_text ? 1 : 0;
    /* Same rule for the call card: inside the window, or not drawn. */
    if (c->call_card_w < 120 || c->call_card_w > 700) { c->call_card_w = 320; fixed++; }
    if (c->call_card_h < 48 || c->call_card_h > 160)  { c->call_card_h = 78;  fixed++; }
    if (c->call_card_name_px < 10 || c->call_card_name_px > 60)     { c->call_card_name_px = 28;   fixed++; }
    if (c->call_card_status_px < 10 || c->call_card_status_px > 60) { c->call_card_status_px = 22; fixed++; }
    if (c->call_card_x_small < 0 || c->call_card_x_small + c->call_card_w > c->canvas_width)
        { c->call_card_x_small = 735; fixed++; }
    if (c->call_card_y_small < 0 || c->call_card_y_small + c->call_card_h > c->canvas_height)
        { c->call_card_y_small = 300; fixed++; }
    if (c->call_card_x_big < 0 || c->call_card_x_big + c->call_card_w > c->canvas_width)
        { c->call_card_x_big = 560; fixed++; }
    if (c->call_card_y_big < 0 || c->call_card_y_big + c->call_card_h > c->canvas_height)
        { c->call_card_y_big = 305; fixed++; }
    /* Only a small canvas (the emergency canvas_width 210) gets here. */
    if (c->call_card_x_small + c->call_card_w > c->canvas_width
        || c->call_card_y_small + c->call_card_h > c->canvas_height
        || c->call_card_x_big + c->call_card_w > c->canvas_width
        || c->call_card_y_big + c->call_card_h > c->canvas_height)
        c->call_card = 0;
    c->call_card = c->call_card ? 1 : 0;
    c->call_card_ringing = c->call_card_ringing ? 1 : 0;
    c->alpha   = c->alpha   ? 1 : 0;
    c->managed = c->managed ? 1 : 0;
    return fixed;
}

#endif /* K3346_OVERLAY_CFG_H */
