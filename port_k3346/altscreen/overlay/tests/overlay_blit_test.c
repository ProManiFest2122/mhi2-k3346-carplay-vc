/*
 * Host test for overlay_blit.h - where the rendered maneuver tile lands.
 *
 * This is four pieces of arithmetic that all look right and cannot all be
 * checked by eye: the crop offset inside the rendered canvas, the 1:1 scale,
 * and two independent y flips (crop_y and the panel origin count from the top,
 * clip space and texture v count from the bottom).  Get one of them wrong and
 * the arrow still appears - mirrored, or half a tile off, or smeared from a
 * clamped edge texel - and the only place to notice is a car park at night.
 *
 * So the tests do not check the eight numbers one by one.  They check the
 * property those numbers exist for:
 *
 *     source texel (crop_x + i, crop_y + j)
 *         is shown at cluster pixel (dst_x + i, dst_y + j)
 *
 * for every corner and a spread of interior points.  A mirror, an off-by-one
 * or a wrong scale all break it; nothing else has to be asserted about the
 * flips at all.
 *
 * Built with the container's native gcc:  bash overlay/tests/run_host_tests.sh
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../overlay_blit.h"

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

static void check_near(float got, float want, const char *what)
{
    if (fabsf(got - want) < 1e-3f) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL: %s (got %.4f, want %.4f)\n", what, got, want);
    }
}

/* Follow one source texel through the quad the renderer draws, and come back
 * out in cluster pixels.  This is what the GPU does, with the sampling written
 * out: the texel centre picks a point on the quad, the quad is axis aligned so
 * the two interpolation parameters are independent, and clip space converts
 * back to pixels counted from the top of the cluster. */
static void texel_lands_at(const overlay_cfg_t *c, const float pos[8],
                           const float uv[8], int i, int j,
                           float *out_x, float *out_y)
{
    float u = ((float)(c->crop_x + i) + 0.5f) / (float)c->render_width;
    float v = 1.0f - ((float)(c->crop_y + j) + 0.5f) / (float)c->render_height;
    float s = (u - uv[0]) / (uv[6] - uv[0]);
    float t = (v - uv[1]) / (uv[7] - uv[1]);
    float ndc_x = pos[0] + s * (pos[6] - pos[0]);
    float ndc_y = pos[1] + t * (pos[7] - pos[1]);

    *out_x = (ndc_x + 1.0f) * 0.5f * (float)c->canvas_width;
    *out_y = (1.0f - ndc_y) * 0.5f * (float)c->canvas_height;
}

/* The whole point of the file, asserted on one configuration. */
static void expect_one_to_one(const overlay_cfg_t *c, int dst_x, int dst_y,
                              const char *label)
{
    float pos[8], uv[8], gx, gy;
    int k;
    /* corners first, then a diagonal spread that is not a corner */
    const int probe[][2] = {
        {0, 0}, {1, 0}, {0, 1},
        {7, 5}, {31, 19}, {104, 76}, {173, 121}
    };
    char what[160];

    overlay_blit_quad(c, dst_x, dst_y, pos, uv);

    for (k = 0; k < (int)(sizeof(probe) / sizeof(probe[0])); k++) {
        int i = probe[k][0], j = probe[k][1];
        if (i >= c->width || j >= c->height) continue;
        texel_lands_at(c, pos, uv, i, j, &gx, &gy);
        sprintf(what, "%s: texel (%d,%d) lands at column %d", label, i, j, dst_x + i);
        check_near(gx, (float)(dst_x + i) + 0.5f, what);
        sprintf(what, "%s: texel (%d,%d) lands at row %d", label, i, j, dst_y + j);
        check_near(gy, (float)(dst_y + j) + 0.5f, what);
    }

    /* The far corner, written out rather than indexed, because an off-by-one
     * at the end of the tile is the one a spread of samples can miss. */
    texel_lands_at(c, pos, uv, c->width - 1, c->height - 1, &gx, &gy);
    sprintf(what, "%s: last texel lands at the last column", label);
    check_near(gx, (float)(dst_x + c->width - 1) + 0.5f, what);
    sprintf(what, "%s: last texel lands at the last row", label);
    check_near(gy, (float)(dst_y + c->height - 1) + 0.5f, what);
}

static void t_shipped_geometry(void)
{
    overlay_cfg_t c;
    float pos[8], uv[8];

    printf("the shipped geometry, corner by corner\n");
    overlay_cfg_defaults(&c);
    overlay_blit_quad(&c, c.x, c.y, pos, uv);

    /* Destination: 210x153 at (1055,207) - big clocks, inside the right
     * gauge - on a 1440x542 canvas. */
    check_near(pos[0], 2.0f * 1055.0f / 1440.0f - 1.0f, "left edge");
    check_near(pos[6], 2.0f * 1265.0f / 1440.0f - 1.0f, "right edge");
    check_near(pos[7], 1.0f - 2.0f * 207.0f / 542.0f, "top edge");
    check_near(pos[1], 1.0f - 2.0f * 360.0f / 542.0f, "bottom edge");

    /* Source: the CR_POPUP_* crop of a 328x181 canvas. */
    check_near(uv[0], 59.0f / 328.0f, "left of the crop");
    check_near(uv[6], 269.0f / 328.0f, "right of the crop");
    check_near(uv[7], 1.0f - 27.0f / 181.0f, "top of the crop");
    check_near(uv[1], 1.0f - 180.0f / 181.0f, "bottom of the crop");

    /* Corner 0 is bottom-left in BOTH rectangles - that is the pairing the
     * two flips exist to produce. */
    check(pos[1] < pos[7], "corner 0 is below corner 2 on the cluster");
    check(uv[1]  < uv[7],  "corner 0 is below corner 2 in the texture");
}

static void t_one_to_one_both_layouts(void)
{
    overlay_cfg_t c;

    printf("every texel lands on its own cluster pixel\n");
    overlay_cfg_defaults(&c);
    expect_one_to_one(&c, c.x, c.y, "big clocks");
    expect_one_to_one(&c, c.x_alt, c.y_alt, "small clocks");
}

/* VIEW moves the gauges and the tile has to go with them.  The origins come
 * from overlay_layout_origin(), which has its own test; what matters here is
 * that the quad really follows the origin it is given and changes nothing
 * else about the source. */
static void t_view_moves_the_tile(void)
{
    overlay_cfg_t c;
    float big_pos[8], big_uv[8], small_pos[8], small_uv[8];

    printf("the tile follows the gauge layout, the crop does not move\n");
    overlay_cfg_defaults(&c);
    overlay_blit_quad(&c, c.x, c.y, big_pos, big_uv);
    overlay_blit_quad(&c, c.x_alt, c.y_alt, small_pos, small_uv);

    check(big_pos[0] != small_pos[0], "a different layout is a different column");
    check(big_pos[1] != small_pos[1], "a different layout is a different row");
    check(memcmp(big_uv, small_uv, sizeof(big_uv)) == 0,
          "the source crop is the same in both layouts");

    /* With small clocks the tile sits higher and further right, at the top
     * right of the map; with big clocks it drops into the right gauge. */
    check(small_pos[0] > big_pos[0], "small clocks: further right");
    check(small_pos[7] > big_pos[7], "small clocks: higher up");
}

/* A card can say anything; the renderer still has to blit inside its own
 * window and sample inside its own texture, or the GPU clamps and the edge
 * smears down the tile. */
static void t_stays_inside(void)
{
    static const char *cards[] = {
        "crop_x 119\ncrop_y 29\n",
        "render_width 100\nrender_height 80\n",
        "canvas_width 200\ncanvas_height 100\n",
        "x 99999\ny -4\n",
        "width 4000\nheight 4000\n"
    };
    unsigned k;

    printf("clamped cards still produce a quad inside the window\n");

    for (k = 0; k < sizeof(cards) / sizeof(cards[0]); k++) {
        overlay_cfg_t c;
        float pos[8], uv[8];
        char path[64], what[128];
        FILE *f;
        int n;

        sprintf(path, "/tmp/ovl_blit_%u.conf", k);
        f = fopen(path, "wb");
        if (!f) { perror("fopen"); exit(2); }
        fwrite(cards[k], 1, strlen(cards[k]), f);
        fclose(f);

        overlay_cfg_load(&c, path);
        overlay_cfg_sanitize(&c);
        overlay_blit_quad(&c, c.x, c.y, pos, uv);

        for (n = 0; n < 8; n++) {
            sprintf(what, "card %u: pos[%d] inside clip space", k, n);
            check(pos[n] >= -1.0f && pos[n] <= 1.0f, what);
            sprintf(what, "card %u: uv[%d] inside the texture", k, n);
            check(uv[n] >= 0.0f && uv[n] <= 1.0f, what);
        }
        sprintf(what, "card %u: still 1:1 after clamping", k);
        expect_one_to_one(&c, c.x, c.y, what);
    }
}

/* The documented escape hatch in the conf: shrink the window to the tile and
 * the whole thing collapses into the top-left corner of the cluster, still
 * pixel for pixel. */
static void t_corner_hatch(void)
{
    overlay_cfg_t c;
    float pos[8], uv[8];

    printf("the corner escape hatch still blits 1:1\n");
    overlay_cfg_defaults(&c);
    c.canvas_width  = 210;
    c.canvas_height = 153;
    overlay_cfg_sanitize(&c);
    overlay_blit_quad(&c, c.x, c.y, pos, uv);

    check_near(pos[0], -1.0f, "left edge at the canvas edge");
    check_near(pos[7],  1.0f, "top edge at the canvas edge");
    check_near(pos[6],  1.0f, "right edge at the canvas edge");
    check_near(pos[1], -1.0f, "bottom edge at the canvas edge");
    expect_one_to_one(&c, c.x, c.y, "corner hatch");
}

int main(void)
{
    t_shipped_geometry();
    t_one_to_one_both_layouts();
    t_view_moves_the_tile();
    t_stays_inside();
    t_corner_hatch();

    printf("\noverlay_blit: %d OK, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
