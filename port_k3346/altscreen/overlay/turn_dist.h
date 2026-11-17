/*
 * turn_dist — the distance to the maneuver, printed by us when the VC hides its own.
 *
 * The VC prints "● 22 m" itself from BAP FctID 18.  When that message carries the
 * bargraph ("CarPlay HUD: Factory", the last metres before the turn) the VC takes
 * the dot and the number away: with the factory navigation a bar stands in the tile
 * then, and the tile is ours (car, 2.0.1-rc3, run-10).  So Java
 * (BAPBridge.writeTurnDist, K3346TurnDist) hands us the same number for as long as
 * the bargraph is on:
 *
 *   /tmp/vc-turn-dist   Java -> overlay   "<number>\n<unit>\nEND\n"
 *
 * The format and its rule are those of capsule_text.h (no rename() on /tmp, a
 * file without the marker is "not ready yet"); an empty number means nothing to
 * print.
 *
 * WHERE.  Not where the VC's number stood.  The VC frames our tile itself: a dark
 * band above it (the number is printed there) and a dark band below, both opaque,
 * both the VC's - our picture shows in the grey field of the tile only.  2.0.1-rc4
 * printed into the place of the VC's number and the car showed nothing of it
 * (run-11, photographs of 2026-11-16).  So the number goes on the top rows of the
 * tile, in a dark band of our own as wide as the tile: it reads as the VC's header
 * grown one line taller, and stays readable when the head of the arrow reaches
 * under it.  The driver picked this from a mock-up on his photographs.
 *
 * The strip: dot, number and unit are centred as one; the number and the unit
 * share a baseline TURN_DIST_BASE rows above the bottom of the strip.
 *
 * No GL in this file, so overlay/tests/turn_dist_test.c runs all of it on the host.
 */

#ifndef K3346_TURN_DIST_H
#define K3346_TURN_DIST_H

#include "call_card.h"

#define TURN_DIST_PATH "/tmp/vc-turn-dist"

#define TURN_DIST_BASE    6     /* rows from the baseline to the bottom of the strip */
#define TURN_DIST_GAP     5     /* between the number and its unit */
#define TURN_DIST_DOT_R   7.0f  /* the dot as the VC draws it */
#define TURN_DIST_DOT_W   14    /* twice the radius */
#define TURN_DIST_DOT_GAP 8     /* between the dot and the number */
#define TURN_DIST_DOT_UP  10.0f /* the centre of the dot above the baseline */
/* the VC's own header band, off the photographs */
#define TURN_DIST_BG_R 38
#define TURN_DIST_BG_G 38
#define TURN_DIST_BG_B 40

static int turn_dist_empty(const capsule_lines_t *lines)
{
    return !lines->left[0];
}

static int turn_dist_baseline(int h)
{
    return h - TURN_DIST_BASE;
}

/* Everything that is printed, as one: the dot (when drawn), the number, the unit. */
static int turn_dist_width(const capsule_font_t *number, const capsule_font_t *unit,
                           const capsule_lines_t *lines, int dot)
{
    int width;

    if (turn_dist_empty(lines)) return 0;
    width = capsule_text_width(number, lines->left);
    if (dot) width += TURN_DIST_DOT_W + TURN_DIST_DOT_GAP;
    if (lines->right[0]) width += TURN_DIST_GAP + capsule_text_width(unit, lines->right);
    return width;
}

/* The first column of what is printed: centred, and never left of the strip. */
static int turn_dist_start(int w, int content)
{
    int x = (w - content) / 2;
    return x < 0 ? 0 : x;
}

/* The first column of the number. */
static int turn_dist_text_x(int start, int dot)
{
    return start + (dot ? TURN_DIST_DOT_W + TURN_DIST_DOT_GAP : 0);
}

/* The first column of the unit. */
static int turn_dist_unit_x(int text_x, const capsule_font_t *number, const capsule_lines_t *lines)
{
    return text_x + capsule_text_width(number, lines->left) + TURN_DIST_GAP;
}

/* The strip lies on the top rows of the tile: (tile_x, tile_y) is the tile on the
 * canvas, dx/dy the correction from the card.  0 when that leaves the canvas -
 * then nothing is drawn rather than a part of it. */
static int turn_dist_place(int tile_x, int tile_y, int tile_w, int dx, int dy, int h,
                           int canvas_w, int canvas_h, int *x, int *y)
{
    *x = tile_x + dx;
    *y = tile_y + dy;
    return tile_w > 0 && *x >= 0 && *y >= 0
        && *x + tile_w <= canvas_w && *y + h <= canvas_h;
}

/* rgba top row first, premultiplied; scratch is w*h bytes.  No number: the strip
 * is cleared, the band too.  bg is the band in percent, 0 = the text alone.  What
 * does not fit the strip is cut by the walk. */
static void turn_dist_render(const capsule_font_t *number, const capsule_font_t *unit,
                             const capsule_lines_t *lines, int dot, int bg,
                             unsigned char *scratch, unsigned char *rgba, int w, int h)
{
    int baseline = turn_dist_baseline(h);
    int start, text_x, i;

    memset(rgba, 0, (size_t)w * (size_t)h * 4u);
    if (turn_dist_empty(lines)) return;

    if (bg > 0) {
        float a = (bg > 100 ? 100 : bg) / 100.0f;
        for (i = 0; i < w * h; i++)
            call_card_over(rgba + i * 4, TURN_DIST_BG_R, TURN_DIST_BG_G, TURN_DIST_BG_B, a);
    }

    start = turn_dist_start(w, turn_dist_width(number, unit, lines, dot));
    text_x = turn_dist_text_x(start, dot);
    if (dot) {
        float cx = (float)start + TURN_DIST_DOT_R;
        float cy = (float)baseline - TURN_DIST_DOT_UP;
        int x, y;
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                float d = hypotf(x + 0.5f - cx, y + 0.5f - cy) - TURN_DIST_DOT_R;
                float cover = call_card_clamp(0.5f - d);
                /* the cyan of the VC's own dot */
                if (cover > 0.0f) call_card_over(rgba + (y * w + x) * 4, 40, 200, 235, cover);
            }
        }
    }
    call_card_text(number, lines->left, (float)text_x, baseline, 255, 255, 255,
                   scratch, rgba, w, h);
    if (lines->right[0])
        call_card_text(unit, lines->right, (float)turn_dist_unit_x(text_x, number, lines),
                       baseline, 255, 255, 255, scratch, rgba, w, h);
}

#endif /* K3346_TURN_DIST_H */
