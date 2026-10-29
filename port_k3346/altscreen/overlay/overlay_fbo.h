/*
 * overlay_fbo — which texture format this unit will actually keep alpha in.
 *
 * WHY THIS EXISTS.  On 2026-10-28 the panel read back as alpha 255 in every
 * pixel of 328x181, uniformly, on the very frame render_end_frame() had just
 * cleared it to (0,0,0,0).  A buffer that was cleared to zero alpha and reads
 * back as opaque is a buffer with no alpha channel.  Our blit copies it onto
 * the cluster canvas with blending off, so it stamped alpha 255 across the
 * whole 210x153 tile: an opaque black slab over the CarPlay map, in both gauge
 * layouts, no matter what the renderer had drawn.
 *
 * The window surface is fine — its EGL config asks for EGL_ALPHA_SIZE 8 and the
 * Etap 7a alpha staircase came back as exactly 255*a.  It is the TEXTURE
 * ATTACHED TO AN FBO whose effective format the driver picks for itself, and
 * GLES2 lets it: only GL_RGBA4 and GL_RGB5_A1 are guaranteed to carry alpha,
 * GL_RGBA8 is an extension (GL_OES_rgb8_rgba8).
 *
 * HOW WE ASK.  Not by reading the alpha channel.  A driver that drops alpha in
 * an FBO may equally well return 255 for alpha in glReadPixels, and then the
 * two failures are indistinguishable.  So the test asks through RGB, which we
 * do trust:
 *
 *     clear the FBO to (0, 0, 0, A)
 *     draw a white full-screen quad with glBlendFunc(GL_DST_ALPHA, GL_ZERO)
 *         -> rgb = 1.0 * dst.a
 *     read RGB back
 *
 * A=0 must come back near 0 and A=1 near 255.  If A=0 also comes back at 255,
 * the buffer has no alpha: GLES2 defines GL_DST_ALPHA as 1 when the destination
 * has no alpha channel, which is precisely the case we are hunting.
 *
 * This header holds only the DECISION — which verdict wins, in what order, and
 * what to do when none of them does.  The GL that produces the verdicts lives
 * in platform_harman.c, because it needs a context; the decision lives here
 * because it needs a test (overlay/tests/overlay_fbo_test.c).
 */

#ifndef K3346_OVERLAY_FBO_H
#define K3346_OVERLAY_FBO_H

/* What one candidate format turned out to be, on this unit, today. */
typedef enum {
    OVERLAY_FBO_UNTESTED = 0,
    OVERLAY_FBO_ALPHA_OK,      /* destination alpha round-tripped: usable */
    OVERLAY_FBO_ALPHA_LOST,    /* FBO is complete, but alpha reads as 1 */
    OVERLAY_FBO_INCOMPLETE     /* the driver refused the attachment outright */
} overlay_fbo_verdict_t;

static const char *overlay_fbo_verdict_name(overlay_fbo_verdict_t v)
{
    switch (v) {
    case OVERLAY_FBO_ALPHA_OK:   return "alpha OK";
    case OVERLAY_FBO_ALPHA_LOST: return "no alpha";
    case OVERLAY_FBO_INCOMPLETE: return "incomplete";
    default:                     return "untested";
    }
}

/* The format to render into: the FIRST candidate that kept alpha.
 * Order is preference, so the candidate list must be written best-first and
 * the loop must not look for "the best verdict" — RGBA8 and RGBA4 both pass
 * the test, and RGBA8 is the one worth having. -1 when none passed. */
static int overlay_fbo_pick(const overlay_fbo_verdict_t *v, int count)
{
    int i;

    if (!v || count <= 0) return -1;
    for (i = 0; i < count; i++)
        if (v[i] == OVERLAY_FBO_ALPHA_OK) return i;
    return -1;
}

/* When nothing keeps alpha we still need somewhere for the renderer to draw:
 * it adopts whatever framebuffer is bound at render_init() and would otherwise
 * have none.  The first merely COMPLETE candidate does for that — the frames
 * are never shown (composite_panel refuses to blit without alpha), but the
 * renderer runs, the readback probe still reports, and the cluster keeps the
 * stock picture instead of a black slab. -1 when even that is unavailable. */
static int overlay_fbo_pick_fallback(const overlay_fbo_verdict_t *v, int count)
{
    int i;

    if (!v || count <= 0) return -1;
    for (i = 0; i < count; i++)
        if (v[i] == OVERLAY_FBO_ALPHA_LOST) return i;
    return -1;
}

/* Did the chosen format keep alpha?  The single question composite_panel()
 * asks before it is allowed to put anything on the cluster. */
static int overlay_fbo_has_alpha(const overlay_fbo_verdict_t *v, int count)
{
    return overlay_fbo_pick(v, count) >= 0;
}

/* Is a measured tile a picture, or the signature of a failure?
 *
 * Luka's renderer clears to (0,0,0,0) every frame, so a real maneuver frame is
 * mostly TRANSPARENT: the arrow, its shadow and the lane strip cover part of
 * the tile, the rest lets the map through.  A tile that is opaque almost
 * everywhere while almost nothing in it is lit is not a design, it is what a
 * sampler returns from an incomplete texture - (0,0,0,1) - and that is exactly
 * what the car showed for four visits in a row (2026-10-28 readback: alpha in
 * 100% of the crop, rgb in 0%).
 *
 * Both arguments are per mille of the crop, as the readback probe counts them.
 * Thresholds are deliberately far from anything a real frame produces, so
 * this guard can only ever hide a broken tile, never a working one. */
#define OVERLAY_TILE_OPAQUE_MIN_PERMILLE 990   /* >= 99.0 % of pixels carry alpha */
#define OVERLAY_TILE_LIT_MAX_PERMILLE     10   /* <   1.0 % of pixels carry colour */

static int overlay_tile_is_degenerate(long alpha_permille, long rgb_permille)
{
    return alpha_permille >= OVERLAY_TILE_OPAQUE_MIN_PERMILLE &&
           rgb_permille   <  OVERLAY_TILE_LIT_MAX_PERMILLE;
}

#endif /* K3346_OVERLAY_FBO_H */
