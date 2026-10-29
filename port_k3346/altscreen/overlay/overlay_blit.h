/*
 * overlay_blit — where the rendered maneuver tile goes on the cluster.
 *
 * Two coordinate systems meet here, each with its own idea of which way is up,
 * and getting either flip wrong puts the arrow somewhere plausible but wrong —
 * the kind of mistake that is only visible in a car park.  So the arithmetic
 * lives in one header with no GL in it, and overlay/tests/overlay_blit_test.c
 * checks it on the host.
 *
 * SOURCE.  The renderer draws into render_width x render_height, which is
 * Luka's CR_DEFAULT_WIDTH/HEIGHT and his whole coordinate system (see the
 * LANDMINE in overlay_cfg.h).  The finished maneuver is framed into the crop
 * at crop_x,crop_y sized width x height — his CR_POPUP_* — by render.c:1219.
 * Everything outside that crop is scaffolding: road that runs off the tile,
 * masks, the lane strip's erase band.  Only the crop is shown.
 *
 * DESTINATION.  The window is the whole 1440x542 cluster canvas, because this
 * DisplayManager honours a window's size but always places it at (0,0)
 * (docs/19).  The crop is blitted 1:1 at the origin the stock HMI uses for the
 * current gauge layout, which the caller resolves with overlay_layout_origin().
 *
 * BOTH flips point the same way: crop_y and the panel origin count from the
 * TOP of their rectangle, while clip space and GL texture coordinates count
 * from the bottom.  That is why y0/y1 and v0/v1 are built by subtracting from
 * 1 rather than adding to -1.
 *
 * 1:1, EXACTLY.  Destination pixel i (0..width-1) has its centre at
 * (i+0.5)/width across the quad, which maps to u = (crop_x+i+0.5)/render_width,
 * i.e. the centre of texel crop_x+i.  With GL_NEAREST that is an exact copy,
 * no half-texel drift, no resampling.  The test asserts it for every pixel of
 * the shipped geometry.
 *
 * Call this on a SANITIZED config (overlay_cfg_sanitize): it is the sanitizer
 * that guarantees non-zero sizes and a crop that fits inside the canvas.
 */

#ifndef K3346_OVERLAY_BLIT_H
#define K3346_OVERLAY_BLIT_H

#include "overlay_cfg.h"

/* Triangle-strip corner order, the same one platform_harman.c draws with:
 *     0 = bottom-left   1 = bottom-right   2 = top-left   3 = top-right
 * pos[] is clip space (-1..1) on the cluster canvas, uv[] is 0..1 on the
 * rendered canvas.  dst_x/dst_y are the crop's top-left corner in cluster
 * pixels — overlay_layout_origin() turns a VC gauge layout into them. */
static void overlay_blit_quad(const overlay_cfg_t *c, int dst_x, int dst_y,
                              float pos[8], float uv[8])
{
    float x0, x1, y0, y1, u0, u1, v0, v1;

    x0 = 2.0f * (float)dst_x / (float)c->canvas_width - 1.0f;
    x1 = 2.0f * (float)(dst_x + c->width) / (float)c->canvas_width - 1.0f;
    y1 = 1.0f - 2.0f * (float)dst_y / (float)c->canvas_height;
    y0 = 1.0f - 2.0f * (float)(dst_y + c->height) / (float)c->canvas_height;

    u0 = (float)c->crop_x / (float)c->render_width;
    u1 = (float)(c->crop_x + c->width) / (float)c->render_width;
    v1 = 1.0f - (float)c->crop_y / (float)c->render_height;
    v0 = 1.0f - (float)(c->crop_y + c->height) / (float)c->render_height;

    pos[0] = x0; pos[1] = y0;   uv[0] = u0; uv[1] = v0;
    pos[2] = x1; pos[3] = y0;   uv[2] = u1; uv[3] = v0;
    pos[4] = x0; pos[5] = y1;   uv[4] = u0; uv[5] = v1;
    pos[6] = x1; pos[7] = y1;   uv[6] = u1; uv[7] = v1;
}

#endif /* K3346_OVERLAY_BLIT_H */
