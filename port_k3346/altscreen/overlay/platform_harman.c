/*
 * platform_harman — platform.h implementation for MHI2 Harman K3346.
 *
 * This file replaces Luka's platform_qnx.c + common/cluster_surface.c.  It is
 * the ONLY file of his renderer that had to be rewritten: everything else
 * (maneuver.c, render.c, route_path.c, the scene compiler, the lane panel, the
 * wire protocol) is his code unchanged, because platform.h is a clean seam.
 *
 * Two differences from his QNX Screen backend, both forced by the platform:
 *
 *   1. ORDER.  Screen lets him create the window first and he MUST, because
 *      Adreno's libEGL dereferences an existing screen context at
 *      eglGetDisplay.  OpenKODE is the other way round: kdCreateWindow() takes
 *      the EGLDisplay and EGLConfig as arguments, so EGL comes first here.
 *      The one ordering rule we do share is that kdInitializeNV() runs before
 *      any of it (same as renderer/upstream/vc_render.c).
 *
 *   2. CONTEXT ROUTING.  On MHI2Q, Java owns DisplayManager.switchContext.  We
 *      have no Java in this path at all — deployment/mib2high-lvds-control.sh
 *      is the single writer of the cluster context, exactly as it already is
 *      for the map (`dmdt sc 4 77`).  So platform_ensure_focus stays a no-op
 *      and this renderer never shells out to dmdt.  One writer, as Luka's
 *      LANDMINES.md insists.
 *
 * Geometry and the displayable number come from mib2high-overlay.conf, not
 * from constants: Etap 7a measures them in the car and we correct the file on
 * the SD card.  See overlay_cfg.h.
 */

#ifdef PLATFORM_HARMAN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "platform.h"
#include "protocol.h"
#include "kd_surface.h"
#include "overlay_cfg.h"
#include "overlay_ready.h"
#include "overlay_layout.h"
#include "render.h"          /* render_set_visible_area(), Luka's public API */
#include "overlay_blit.h"
#include "overlay_fbo.h"

static EGLDisplay g_dpy     = EGL_NO_DISPLAY;
static EGLSurface g_surface = EGL_NO_SURFACE;
static EGLContext g_ctx     = EGL_NO_CONTEXT;
static EGLConfig  g_econf   = 0;

static kd_surface_t  *g_surf;
static overlay_cfg_t  g_cfg;
static int g_window_expected;
static volatile int g_should_close;

/* 1 once /tmp/vc-overlay-ready has been written for the CURRENT window.  Reset
 * on every recreate so the shell's gate never trusts a marker left over from a
 * window that has since gone away. */
static int g_ready_reported;

/* Failed attempts to write that report.  Bounded, because the write sits in the
 * swap path: if /tmp were read-only, an unbounded retry would mean an fopen and
 * a log line on every frame, 30 times a second. */
static int g_ready_attempts;
#define READY_MAX_ATTEMPTS 10

/* Last VC gauge layout seen in the hook log, and when we last looked. */
static int  g_layout_slot = OVERLAY_LAYOUT_UNKNOWN;
static long g_layout_check_ms;
static overlay_layout_reader_t g_layout_reader;
static int  g_layout_reader_ready;
/* visible_top currently handed to the renderer; -1 = never set. */
static int  g_visible_top_applied = -1;

/* How often the capture ring is looked at.  Was 1000 ms, which alone left
 * the tile up to a second in the old layout's place after VIEW. */
#define LAYOUT_POLL_MS 100

/* VIEW was pressed and the cluster has not reported the new layout yet: the
 * tile stays off the canvas instead of sitting in the old layout's place.
 * The report normally follows the key by ~575 ms; if it never comes (VIEW
 * changed nothing), the tile comes back after this long. */
#define VIEW_HOLD_MAX_MS 1500
static int  g_view_hold;        /* tile kept off the canvas right now */
static long g_view_hold_ms;     /* when the hold started */
static int  g_view_expired;     /* the hold timed out on this press */

/* ---------------------------------------------------------------------------
 * Panel-into-canvas compositing.
 *
 * The car run of 2026-10-27 answered the question Etap 7 could not answer from
 * a desk.  The DisplayManager accepts a window whose displayable number it has
 * no entry for, honours its size exactly — the 210x153 probe measured
 * (0,0)-(209,152) on the screenshot, pixel for pixel — and places it at (0,0).
 * It assigns no position of its own, and there is nothing to ask for one:
 * `dmdt gd`/`gs` return zero bytes on this unit and `dmdt gc` carries no
 * geometry at all.
 *
 * So the window takes the whole cluster canvas and the tile is drawn into the
 * right rectangle of it.  None of Luka's renderer changes for this: it draws
 * into an offscreen FBO, because render_init() adopts whatever framebuffer
 * happens to be bound when it runs (render.c:
 * glGetIntegerv(GL_FRAMEBUFFER_BINDING, &g_default_fbo)).  platform_swap()
 * then blits it onto the otherwise transparent canvas, at the offset for the
 * gauge layout currently on the cluster.
 *
 * THE FBO IS THE RENDER SIZE, NOT THE PANEL SIZE, and that distinction cost
 * the car visit of 2026-10-27.  maneuver_render does not scale: CR_DEFAULT_*
 * is the coordinate system of its scissor, its 2D placement and its lane
 * framing (the full list is in overlay_cfg.h).  Given a 210x153 framebuffer it
 * drew a correct scene in the wrong frame - 34 draw commands, no arrow on the
 * cluster.  So the FBO is render_width x render_height, the renderer frames
 * the maneuver into its CR_POPUP_* crop as designed, and the blit shows that
 * crop at the stock tile's place.  overlay_blit.h does that arithmetic.
 *
 * Blending is OFF for that blit on purpose.  The panel texture already holds
 * premultiplied colour (render.c's glBlendFuncSeparate), so copying it through
 * unchanged is what keeps it premultiplied, and the cluster's own compositor
 * is what blends it over the map.  That it really does blend per pixel was
 * settled on the same run: the probe's alpha staircase came back as exactly
 * 0, 51, 102, 153, 204, 255 — i.e. 255*a, to the integer.
 */
static GLuint g_panel_fbo, g_panel_tex;
static GLuint g_blit_prog;
static GLint  g_blit_attr_pos = -1, g_blit_attr_uv = -1, g_blit_uni_tex = -1;
static GLint  g_max_vattribs;

/* Does the engine have anything to show?  Set from main.c before each swap.
 * Starts at 0, so the first presented canvas is empty rather than a rectangle
 * of whatever an untouched FBO happens to contain. */
static int g_content_present;

static GLuint compile_blit_shader(GLenum type, const char *src)
{
    GLuint sh;
    GLint ok = 0;

    sh = glCreateShader(type);
    if (!sh) return 0;
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[256];
        GLsizei n = 0;
        log[0] = '\0';
        glGetShaderInfoLog(sh, (GLsizei)sizeof(log), &n, log);
        log[sizeof(log) - 1] = '\0';
        fprintf(stderr, "platform_harman: blit shader compile failed: %s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static int make_blit_program(void)
{
    static const char *vs_src =
        "attribute vec2 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "varying vec2 v_uv;\n"
        "void main() { v_uv = a_uv; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
    static const char *fs_src =
        "precision mediump float;\n"
        "varying vec2 v_uv;\n"
        "uniform sampler2D u_tex;\n"
        "void main() { gl_FragColor = texture2D(u_tex, v_uv); }\n";
    GLuint vs, fs;
    GLint ok = 0;

    vs = compile_blit_shader(GL_VERTEX_SHADER, vs_src);
    if (!vs) return -1;
    fs = compile_blit_shader(GL_FRAGMENT_SHADER, fs_src);
    if (!fs) { glDeleteShader(vs); return -1; }

    g_blit_prog = glCreateProgram();
    if (!g_blit_prog) { glDeleteShader(vs); glDeleteShader(fs); return -1; }
    glAttachShader(g_blit_prog, vs);
    glAttachShader(g_blit_prog, fs);
    glLinkProgram(g_blit_prog);
    glGetProgramiv(g_blit_prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        fprintf(stderr, "platform_harman: blit program link failed\n");
        glDeleteProgram(g_blit_prog);
        g_blit_prog = 0;
        return -1;
    }

    g_blit_attr_pos = glGetAttribLocation(g_blit_prog, "a_pos");
    g_blit_attr_uv  = glGetAttribLocation(g_blit_prog, "a_uv");
    g_blit_uni_tex  = glGetUniformLocation(g_blit_prog, "u_tex");
    if (g_blit_attr_pos < 0 || g_blit_attr_uv < 0) {
        fprintf(stderr, "platform_harman: blit attributes missing\n");
        return -1;
    }

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &g_max_vattribs);
    if (g_max_vattribs < 1 || g_max_vattribs > 64) g_max_vattribs = 16;
    return 0;
}


/* ---------------------------------------------------------------------------
 * Which texture format this unit keeps alpha in.
 *
 * GLES2 guarantees alpha only for GL_RGBA4 and GL_RGB5_A1; GL_RGBA8 as an FBO
 * attachment is an extension (GL_OES_rgb8_rgba8) and this driver may honour the
 * request and quietly drop the channel.  On 2026-10-28 it did: see the opening
 * note in overlay_fbo.h.  So the format is measured, not assumed, and the
 * measurement asks through RGB because a driver that loses alpha in an FBO may
 * also lie about it in glReadPixels.
 */
typedef struct {
    const char *name;
    GLenum format;   /* GLES2: the internal format is the same enum */
    GLenum type;
} panel_fmt_t;

/* Best first: overlay_fbo_pick() takes the first that passes, not the "best"
 * verdict, because several of these pass and only the order ranks them. */
static const panel_fmt_t k_panel_fmts[] = {
    { "RGBA8",  GL_RGBA, GL_UNSIGNED_BYTE },
    { "RGBA4",  GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4 },
    { "RGB5A1", GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1 }
};
#define PANEL_FMT_COUNT ((int)(sizeof(k_panel_fmts) / sizeof(k_panel_fmts[0])))

static overlay_fbo_verdict_t g_fmt_verdict[PANEL_FMT_COUNT];
static int g_panel_fmt = -1;      /* index into k_panel_fmts, -1 = none */
static int g_panel_alpha_ok;      /* 0 = never put this panel on the cluster */
static int g_no_alpha_warned;     /* say it once, not thirty times a second */
static int g_tile_degenerate;     /* set by the readback probe, see overlay_fbo.h */

/* Solid white, nothing else: the only thing the alpha test draws. */
static GLuint g_white_prog;
static GLint  g_white_attr_pos = -1;

static int make_white_program(void)
{
    static const char *vs_src =
        "attribute vec2 a_pos;\n"
        "void main() { gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
    static const char *fs_src =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0); }\n";
    GLuint vs, fs;
    GLint ok = 0;

    vs = compile_blit_shader(GL_VERTEX_SHADER, vs_src);
    if (!vs) return -1;
    fs = compile_blit_shader(GL_FRAGMENT_SHADER, fs_src);
    if (!fs) { glDeleteShader(vs); return -1; }

    g_white_prog = glCreateProgram();
    if (!g_white_prog) { glDeleteShader(vs); glDeleteShader(fs); return -1; }
    glAttachShader(g_white_prog, vs);
    glAttachShader(g_white_prog, fs);
    glLinkProgram(g_white_prog);
    glGetProgramiv(g_white_prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        fprintf(stderr, "platform_harman: alpha-test program link failed\n");
        glDeleteProgram(g_white_prog);
        g_white_prog = 0;
        return -1;
    }
    g_white_attr_pos = glGetAttribLocation(g_white_prog, "a_pos");
    return g_white_attr_pos < 0 ? -1 : 0;
}

/* Does the framebuffer bound right now really store destination alpha?
 *
 * Clear to (0,0,0,A), draw white with glBlendFunc(GL_DST_ALPHA, GL_ZERO) so
 * that rgb = 1.0 * dst.a, and read the RGB back.  A=0 must come back near 0.
 * If it comes back at 255 the buffer has no alpha channel, because GLES2
 * defines GL_DST_ALPHA as 1 when the destination has none.
 *
 * Fills rgb[2] and raw_a[2] (the alpha channel as glReadPixels reports it, for
 * the log only - the verdict never depends on it). */
static int fbo_stores_alpha(int *rgb, int *raw_a)
{
    static const GLfloat quad[8] = { -1.0f,-1.0f,  1.0f,-1.0f,
                                     -1.0f, 1.0f,  1.0f, 1.0f };
    unsigned char px[4];
    GLint i;
    int k;

    if (!g_white_prog) return 0;

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(g_white_prog);
    for (i = 0; i < g_max_vattribs; i++) glDisableVertexAttribArray((GLuint)i);
    glVertexAttribPointer((GLuint)g_white_attr_pos, 2, GL_FLOAT, GL_FALSE, 0, quad);
    glEnableVertexAttribArray((GLuint)g_white_attr_pos);

    for (k = 0; k < 2; k++) {
        glClearColor(0.0f, 0.0f, 0.0f, k ? 1.0f : 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_ALPHA, GL_ZERO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisable(GL_BLEND);
        px[0] = px[3] = 0;
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        rgb[k]   = px[0];
        raw_a[k] = px[3];
    }
    glDisableVertexAttribArray((GLuint)g_white_attr_pos);

    /* Generous thresholds: 4-bit and 1-bit alpha both land on 0 and 255, and
     * nothing in between is a pass either way. */
    return rgb[0] < 64 && rgb[1] > 192;
}

/* Attach a fresh texture of candidate i and say what it turned out to be. */
static overlay_fbo_verdict_t try_panel_format(int i, GLuint *out_tex)
{
    overlay_fbo_verdict_t verdict;
    int rgb[2], raw_a[2];
    GLint abits = -1;
    GLuint tex = 0;
    GLenum status;

    rgb[0] = rgb[1] = raw_a[0] = raw_a[1] = -1;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)k_panel_fmts[i].format,
                 g_cfg.render_width, g_cfg.render_height, 0,
                 k_panel_fmts[i].format, k_panel_fmts[i].type, NULL);
    /* The blit is 1:1, so filtering has nothing to do; CLAMP_TO_EDGE because
     * GLES2 allows no other wrap mode on a non-power-of-two texture. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, g_panel_fbo);
    /* Colour only.  render.c's last pass clears GL_DEPTH_BUFFER_BIT too, which
     * on a framebuffer without a depth attachment is simply ignored. */
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "platform_harman: fbo alpha test %s: incomplete"
                " status=0x%x\n", k_panel_fmts[i].name, (unsigned)status);
        glDeleteTextures(1, &tex);
        *out_tex = 0;
        return OVERLAY_FBO_INCOMPLETE;
    }

    glGetIntegerv(GL_ALPHA_BITS, &abits);
    glViewport(0, 0, g_cfg.render_width, g_cfg.render_height);
    verdict = fbo_stores_alpha(rgb, raw_a) ? OVERLAY_FBO_ALPHA_OK
                                           : OVERLAY_FBO_ALPHA_LOST;
    fprintf(stderr, "platform_harman: fbo alpha test %s: %s (alpha_bits=%d,"
            " dst.a=0 -> rgb %d, dst.a=1 -> rgb %d, readback a %d/%d)\n",
            k_panel_fmts[i].name, overlay_fbo_verdict_name(verdict), (int)abits,
            rgb[0], rgb[1], raw_a[0], raw_a[1]);
    *out_tex = tex;
    return verdict;
}

static int make_panel_target(void)
{
    GLuint fallback_tex = 0;
    GLenum status;
    GLint win_abits = -1;
    int i, chosen;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glGetIntegerv(GL_ALPHA_BITS, &win_abits);
    fprintf(stderr, "platform_harman: window alpha_bits=%d\n", (int)win_abits);

    /* Without the test program nothing can be PROVEN to keep alpha, and an
     * unproven format is exactly how the black slab happened.  So every
     * candidate then reads as "no alpha" (rgb -1 in its log line: not a
     * measurement), the renderer still runs and the probe still reports, but
     * the panel is never put on the cluster. */
    if (make_white_program() != 0) {
        fprintf(stderr, "platform_harman: alpha test unavailable (no shader) -"
                " no format can be proven to keep alpha, the panel will NOT be"
                " composited\n");
    }

    glGenFramebuffers(1, &g_panel_fbo);

    /* The RENDER size, not the panel size.  Luka's renderer does not scale:
     * it measures everything in CR_DEFAULT_WIDTH/HEIGHT and frames the
     * maneuver into the CR_POPUP_* crop, so it gets the canvas it was written
     * for and we show the crop.  See the LANDMINE in overlay_cfg.h. */
    for (i = 0; i < PANEL_FMT_COUNT; i++) {
        GLuint tex = 0;

        g_fmt_verdict[i] = try_panel_format(i, &tex);
        if (g_fmt_verdict[i] == OVERLAY_FBO_ALPHA_OK) {
            g_panel_tex = tex;
            break;
        }
        /* Keep the first merely-complete one: if nothing carries alpha it is
         * still somewhere for the renderer to draw (overlay_fbo.h). */
        if (g_fmt_verdict[i] == OVERLAY_FBO_ALPHA_LOST && !fallback_tex)
            fallback_tex = tex;
        else if (tex)
            glDeleteTextures(1, &tex);
    }

    /* The same function overlay_fbo_test.c checks against all 64 verdict
     * combinations decides whether this panel may ever reach the cluster. */
    g_panel_alpha_ok = overlay_fbo_has_alpha(g_fmt_verdict, PANEL_FMT_COUNT);
    chosen = overlay_fbo_pick(g_fmt_verdict, PANEL_FMT_COUNT);
    if (g_panel_alpha_ok) {
        if (fallback_tex) glDeleteTextures(1, &fallback_tex);
    } else {
        chosen = overlay_fbo_pick_fallback(g_fmt_verdict, PANEL_FMT_COUNT);
        if (chosen < 0 || !fallback_tex) {
            fprintf(stderr, "platform_harman: FAIL no usable FBO format\n");
            return -1;
        }
        g_panel_tex = fallback_tex;
        /* Loud, because this is the difference between "no arrow" and "a black
         * slab over the map".  composite_panel() will refuse to blit. */
        fprintf(stderr, "platform_harman: NO FORMAT KEEPS ALPHA on this unit -"
                " rendering into %s but NOT compositing; the cluster keeps the"
                " stock picture\n", k_panel_fmts[chosen].name);
    }
    g_panel_fmt = chosen;

    /* The loop leaves whichever texture it tested last attached; re-attach the
     * one we actually chose. */
    glBindFramebuffer(GL_FRAMEBUFFER, g_panel_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, g_panel_tex, 0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "platform_harman: panel FBO incomplete status=0x%x\n",
                (unsigned)status);
        return -1;
    }
    if (g_white_prog) { glDeleteProgram(g_white_prog); g_white_prog = 0; }

    fprintf(stderr, "platform_harman: render %dx%d in %s (%s), crop %dx%d"
            " @(%d,%d) into canvas %dx%d\n",
            g_cfg.render_width, g_cfg.render_height, k_panel_fmts[chosen].name,
            g_panel_alpha_ok ? "alpha OK" : "NO ALPHA - not composited",
            g_cfg.width, g_cfg.height, g_cfg.crop_x, g_cfg.crop_y,
            g_cfg.canvas_width, g_cfg.canvas_height);
    return 0;
}

static void composite_panel(void)
{
    int px = g_cfg.x, py = g_cfg.y;
    float pos[8], uv[8];

    /* No surface means a recreate is in flight: drawing now would just pile up
     * GL errors against a context with nothing current. */
    if (!g_panel_tex || !g_blit_prog || g_surface == EGL_NO_SURFACE) return;

    /* Slot 1 (big clocks) is what px/py are seeded with, and an unknown slot
     * leaves them untouched, so the default survives a missing hook log. */
    overlay_layout_origin(g_layout_slot, g_cfg.x, g_cfg.y,
                          g_cfg.x_alt, g_cfg.y_alt, &px, &py);
    /* Both flips, the crop and the 1:1 texel mapping live in one tested place. */
    overlay_blit_quad(&g_cfg, px, py, pos, uv);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_cfg.canvas_width, g_cfg.canvas_height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Nothing to draw: the cleared canvas IS the frame.  The blit copies the
     * panel with blending off, so blitting an idle panel would stamp a
     * rectangle of its contents over the map - which is what the car showed on
     * 2026-10-27, with and without a route.  An empty cluster panel is the
     * stock behaviour anyway: no maneuver, no box. */
    if (!g_content_present) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_panel_fbo);
        return;
    }

    /* Same refusal for a different reason, and the more important one.  The
     * blit copies the panel with blending OFF, so whatever alpha the panel
     * carries becomes the canvas alpha.  If the FBO lost the channel, every
     * pixel reads 255 and the copy is an opaque slab across the tile - which
     * is exactly what the cluster showed on 2026-10-28, in both layouts,
     * regardless of what the renderer had drawn into it.
     *
     * An overlay that cannot blend has to disappear, not cover the map: no
     * arrow is a worse feature, a black slab is a worse car.  The gate in
     * mib2high-lvds-control.sh covers "the renderer never came up"; this
     * covers "it came up and cannot blend". */
    if (!g_panel_alpha_ok) {
        if (!g_no_alpha_warned) {
            g_no_alpha_warned = 1;
            fprintf(stderr, "platform_harman: panel NOT composited (no alpha)"
                    " - the cluster keeps the stock picture\n");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, g_panel_fbo);
        return;
    }

    /* Second net, same principle, independent of the cause.  A tile the
     * readback probe found opaque almost everywhere and lit almost nowhere is
     * not a picture (overlay_tile_is_degenerate) - it is the black slab the car
     * showed four visits running, then because Luka's NPOT render targets were
     * incomplete on this GPU.  Whatever produces it next time, it does not
     * reach the cluster.  The probe runs BEFORE this in platform_swap(), so not
     * even the frame that revealed it gets through. */
    if (g_tile_degenerate) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_panel_fbo);
        return;
    }

    /* VIEW is changing the gauge layout and the cluster has not said where
     * the tile goes next.  An empty canvas for that half second beats the
     * arrow sitting in the old layout's place (2026-10-28 photos). */
    if (g_view_hold) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_panel_fbo);
        return;
    }

    glUseProgram(g_blit_prog);

    /* Luka's frame ends inside vb_flush(), which ENABLES its vertex attribute
     * arrays and never disables them (render.c, the flat-program branch).
     * Those arrays point into his vertex buffer with his stride.  Leaving them
     * on would make this draw fetch from them as well, so every attribute that
     * is not ours goes off first.  Nothing of his breaks: he re-enables what he
     * needs on the next frame, in vb_flush itself. */
    {
        GLint i;
        for (i = 0; i < g_max_vattribs; i++)
            glDisableVertexAttribArray((GLuint)i);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_panel_tex);
    if (g_blit_uni_tex >= 0) glUniform1i(g_blit_uni_tex, 0);
    glVertexAttribPointer((GLuint)g_blit_attr_pos, 2, GL_FLOAT, GL_FALSE, 0, pos);
    glVertexAttribPointer((GLuint)g_blit_attr_uv,  2, GL_FLOAT, GL_FALSE, 0, uv);
    glEnableVertexAttribArray((GLuint)g_blit_attr_pos);
    glEnableVertexAttribArray((GLuint)g_blit_attr_uv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray((GLuint)g_blit_attr_pos);
    glDisableVertexAttribArray((GLuint)g_blit_attr_uv);

    /* Hand the panel FBO back: it is what Luka's renderer believes is the
     * screen.  It rebinds it itself every frame, this is belt and braces. */
    glBindFramebuffer(GL_FRAMEBUFFER, g_panel_fbo);
}

/* ---------------------------------------------------------------------------
 * What is actually in the tile.
 *
 * Everything else about this overlay can be read from a log.  Its contents
 * could not: the cluster is only reachable through a photograph, the CarPlay
 * map rides a hardware layer that `dmdt ts` never captures, and a dark
 * rectangle on a night photo looks the same whether it is our tile painted
 * opaque or a stock KDK backing (101/102) showing through where we are
 * transparent.  Two car visits were spent on that ambiguity.
 *
 * The 2026-10-28 run answered it - the tile is ours and the FBO has no alpha -
 * but it answered nothing else, because the first version of this probe had
 * three faults, all fixed here:
 *
 *   1. IT MEASURED THE FIRST FRAME WITH CONTENT, which is the one frame
 *      guaranteed to be empty: main.c sets g_fade_alpha = 0 on reveal and
 *      takes eight frames to fade in.  "brightest=(0,0,0)" on that frame means
 *      nothing.  Now the first read waits a second.
 *   2. ONE FAILED READ DISABLED THE WHOLE SESSION.  glGetError() returns the
 *      first error in the queue, not the error of the call just made, so a
 *      stale one left by the renderer looked like our readback failing.  Now
 *      the queue is drained first - and what was in it is logged, because a
 *      renderer raising GL errors on this driver is itself worth knowing.
 *   3. IT ONLY LOOKED AT THE CROP.  With alpha broken, alpha coverage is 100%
 *      everywhere, so "draws outside the crop" and "draws nothing at all"
 *      produced identical logs.  Now the map covers the whole canvas and RGB
 *      coverage is reported next to alpha coverage.
 *
 * glReadPixels stalls the pipeline, so this runs a handful of times per
 * session and never while the tile is blank.
 */
#define PANEL_PROBE_COLS 16
#define PANEL_PROBE_ROWS 8
#define PANEL_PROBE_MAX  8

static unsigned char *g_probe_buf;
static int  g_probe_count;
static long g_probe_next_ms;

static char probe_cell(long on, long total)
{
    if (total <= 0)            return ' ';
    if (on * 100 < total * 5)  return '.';
    if (on * 100 < total * 40) return ':';
    return '#';
}

/* Empty the GL error queue and report what was in it.  Ours or Luka's, an
 * error pending here is information; silently swallowing it was what made the
 * previous probe give up for no reason. */
static void probe_drain_gl_errors(const char *when, int quiet)
{
    GLenum e;
    int n = 0;

    while ((e = glGetError()) != GL_NO_ERROR) {
        if (n < 4 && !quiet)
            fprintf(stderr, "platform_harman: GL error 0x%x pending %s\n",
                    (unsigned)e, when);
        if (++n > 32) break;   /* a stuck queue must not stall the frame */
    }
    if (n > 4 && !quiet)
        fprintf(stderr, "platform_harman: ...and %d more pending %s\n",
                n - 4, when);
}

static void probe_panel(int quiet)
{
    const int rw = g_cfg.render_width,  rh = g_cfg.render_height;
    const int cx = g_cfg.crop_x,        cy = g_cfg.crop_y;
    const int cw = g_cfg.width,         ch = g_cfg.height;
    long a_all = 0, a_crop = 0, rgb_all = 0, rgb_crop = 0;
    int amin = 255, amax = 0, best = -1;
    int best_x = -1, best_y = -1, best_r = 0, best_g = 0, best_b = 0;
    char row[PANEL_PROBE_COLS + 1];
    int x, y, r, c;

    probe_drain_gl_errors("before the readback", quiet);

    glBindFramebuffer(GL_FRAMEBUFFER, g_panel_fbo);
    glReadPixels(0, 0, rw, rh, GL_RGBA, GL_UNSIGNED_BYTE, g_probe_buf);
    if (glGetError() != GL_NO_ERROR) {
        /* Only this attempt. The next tick tries again - a readback that
         * failed once is not a reason to stop looking for a whole drive. */
        if (!quiet)
            fprintf(stderr, "platform_harman: panel probe %d/%d: readback failed,"
                    " will retry\n", g_probe_count, PANEL_PROBE_MAX);
        return;
    }

    for (y = 0; y < rh; y++) {
        /* glReadPixels hands back the bottom row first; crop_y counts from
         * the top, the same flip overlay_blit.h makes for the texture. */
        const unsigned char *line = g_probe_buf + (size_t)y * (size_t)rw * 4;
        int ytop = rh - 1 - y;
        int in_rows = (ytop >= cy && ytop < cy + ch);
        for (x = 0; x < rw; x++) {
            int rr = line[x * 4], gg = line[x * 4 + 1], bb = line[x * 4 + 2];
            int a  = line[x * 4 + 3];
            int lit = (rr | gg | bb) != 0;
            int in_crop = in_rows && x >= cx && x < cx + cw;

            if (a) a_all++;
            if (lit) rgb_all++;
            if (in_crop) {
                if (a) a_crop++;
                if (lit) rgb_crop++;
                if (a < amin) amin = a;
                if (a > amax) amax = a;
            }
            if (rr + gg + bb > best) {
                best = rr + gg + bb;
                best_r = rr; best_g = gg; best_b = bb;
                best_x = x; best_y = ytop;
            }
        }
    }

    {
        long all_px  = (long)rw * rh;
        long crop_px = (long)cw * ch;
        long pa  = all_px  ? a_all    * 1000 / all_px  : 0;
        long pc  = crop_px ? a_crop   * 1000 / crop_px : 0;
        long pra = all_px  ? rgb_all  * 1000 / all_px  : 0;
        long prc = crop_px ? rgb_crop * 1000 / crop_px : 0;
        int in_crop = (best_x >= cx && best_x < cx + cw &&
                       best_y >= cy && best_y < cy + ch);
        int degenerate = overlay_tile_is_degenerate(pc, prc);

        /* Transitions are logged even when quiet: they change what the
         * cluster shows, and that is never something to do silently. */
        if (degenerate != g_tile_degenerate) {
            g_tile_degenerate = degenerate;
            if (degenerate)
                fprintf(stderr, "platform_harman: TILE IS A BLACK SLAB (alpha in"
                        " %ld.%ld%% of the crop, colour in %ld.%ld%%) - not"
                        " compositing it\n", pc / 10, pc % 10, prc / 10, prc % 10);
            else
                fprintf(stderr, "platform_harman: tile has content again (colour"
                        " in %ld.%ld%% of the crop) - compositing\n",
                        prc / 10, prc % 10);
        }
        if (quiet) return;

        fprintf(stderr, "platform_harman: panel probe %d/%d alpha canvas=%ld.%ld%%"
                " crop=%ld.%ld%% | rgb canvas=%ld.%ld%% crop=%ld.%ld%%\n",
                g_probe_count, PANEL_PROBE_MAX, pa / 10, pa % 10, pc / 10, pc % 10,
                pra / 10, pra % 10, prc / 10, prc % 10);
        fprintf(stderr, "platform_harman: panel probe a=[%d..%d] in crop,"
                " brightest=(%d,%d,%d) at canvas(%d,%d) %s crop\n",
                amax ? amin : 0, amax, best_r, best_g, best_b, best_x, best_y,
                in_crop ? "INSIDE" : "outside");
    }

    fprintf(stderr, "platform_harman: panel map of the whole %dx%d canvas,"
            " rgb per cell ('.' under 5%%, ':' under 40%%, '#' above)\n", rw, rh);
    for (r = 0; r < PANEL_PROBE_ROWS; r++) {
        for (c = 0; c < PANEL_PROBE_COLS; c++) {
            int x0 = c * rw / PANEL_PROBE_COLS;
            int x1 = (c + 1) * rw / PANEL_PROBE_COLS;
            int y0 = r * rh / PANEL_PROBE_ROWS;
            int y1 = (r + 1) * rh / PANEL_PROBE_ROWS;
            long on = 0, total = 0;
            for (y = y0; y < y1; y++) {
                const unsigned char *line =
                    g_probe_buf + (size_t)(rh - 1 - y) * (size_t)rw * 4;
                for (x = x0; x < x1; x++) {
                    total++;
                    if ((line[x * 4] | line[x * 4 + 1] | line[x * 4 + 2]) != 0)
                        on++;
                }
            }
            row[c] = probe_cell(on, total);
        }
        row[PANEL_PROBE_COLS] = '\0';
        fprintf(stderr, "platform_harman: panel map |%s|\n", row);
    }
    fprintf(stderr, "platform_harman: panel map: the shown crop covers cells"
            " x %d..%d, y %d..%d\n",
            cx * PANEL_PROBE_COLS / rw, (cx + cw - 1) * PANEL_PROBE_COLS / rw,
            cy * PANEL_PROBE_ROWS / rh, (cy + ch - 1) * PANEL_PROBE_ROWS / rh);
}

/* Decides whether this frame is one of the few that gets read back.
 *
 * Two jobs with different needs.  The LOG wants a settled frame: content
 * appearing means the engine has just revealed from CLEAR with global alpha 0
 * and fades in over eight frames, so the logged reads start a second later.
 * The GUARD (overlay_tile_is_degenerate) wants the very first frame, because a
 * black slab is a black slab from frame one - so that frame is read silently,
 * for the guard alone.
 *
 * While the tile is degenerate the reads continue past the budget, once a
 * minute and silently, so that a tile which recovers is shown again; only the
 * transition is logged. */
static void probe_panel_tick(void)
{
    struct timespec now;
    long now_ms;

    if (!g_content_present || !g_panel_fbo) return;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return;
    now_ms = now.tv_sec * 1000L + now.tv_nsec / 1000000L;

    if (!g_probe_buf) {
        g_probe_buf = (unsigned char *)malloc((size_t)g_cfg.render_width *
                                              (size_t)g_cfg.render_height * 4);
        if (!g_probe_buf) {
            fprintf(stderr, "platform_harman: panel probe: no memory for a"
                    " %dx%d readback - skipping it\n",
                    g_cfg.render_width, g_cfg.render_height);
            g_probe_count = PANEL_PROBE_MAX;
            g_probe_next_ms = -1;
            return;
        }
    }
    if (g_probe_next_ms < 0) return;          /* gave up: no buffer */

    if (g_probe_next_ms == 0) {               /* first content of the session */
        probe_panel(1);
        g_probe_next_ms = now_ms + 1000L;
        return;
    }
    if (now_ms < g_probe_next_ms) return;
    if (g_probe_count >= PANEL_PROBE_MAX && !g_tile_degenerate) return;

    g_probe_count++;
    probe_panel(g_probe_count > PANEL_PROBE_MAX);
    g_probe_next_ms = now_ms + (g_probe_count == 1 ? 2000L :
                                g_probe_count == 2 ? 7000L : 60000L);
}

static void report_ready_once(void)
{
    if (g_ready_reported || g_ready_attempts >= READY_MAX_ATTEMPTS) return;

    if (overlay_ready_write(OVERLAY_READY_PATH, g_cfg.displayable,
                            g_cfg.canvas_width, g_cfg.canvas_height) == 0) {
        g_ready_reported = 1;
        g_ready_attempts = 0;
        fprintf(stderr, "platform_harman: READY %s -> %d %dx%d\n",
                OVERLAY_READY_PATH, g_cfg.displayable,
                g_cfg.canvas_width, g_cfg.canvas_height);
        return;
    }
    g_ready_attempts++;
    fprintf(stderr, "platform_harman: cannot write %s errno=%d attempt %d/%d%s\n",
            OVERLAY_READY_PATH, errno, g_ready_attempts, READY_MAX_ATTEMPTS,
            g_ready_attempts >= READY_MAX_ATTEMPTS
              ? " - giving up; the gate keeps the cluster on the map context"
              : "");
}

/* Present one fully transparent canvas and report readiness from it.
 *
 * This is what the readiness report actually claims — window realized, EGL
 * surface bound, compositor took a buffer — so it is worth proving once at
 * start-up rather than waiting for content.  Waiting for content is precisely
 * what went wrong on 2026-10-27: the renderer only swaps when it has a scene,
 * no maneuver had arrived yet, so the single swap of the whole session was the
 * one that failed to write the report, and nothing ever came back to retry. */
static void present_empty_canvas(void)
{
    if (g_dpy == EGL_NO_DISPLAY || g_surface == EGL_NO_SURFACE) return;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_cfg.canvas_width, g_cfg.canvas_height);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (eglSwapBuffers(g_dpy, g_surface) == EGL_TRUE)
        report_ready_once();
    else
        fprintf(stderr, "platform_harman: first swap failed err=0x%x\n", eglGetError());

    if (g_panel_fbo) glBindFramebuffer(GL_FRAMEBUFFER, g_panel_fbo);
}

static int bind_egl_surface(void)
{
    EGLNativeWindowType native = kd_surface_native(g_surf);

    if (!native) {
        fprintf(stderr, "platform_harman: no native window to bind\n");
        return -1;
    }

    g_surface = eglCreateWindowSurface(g_dpy, g_econf, native, NULL);
    if (g_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "platform_harman: eglCreateWindowSurface FAILED err=0x%x\n",
                eglGetError());
        return -1;
    }
    if (g_ctx != EGL_NO_CONTEXT &&
        !eglMakeCurrent(g_dpy, g_surface, g_surface, g_ctx)) {
        fprintf(stderr, "platform_harman: eglMakeCurrent FAILED err=0x%x\n",
                eglGetError());
        eglDestroySurface(g_dpy, g_surface);
        g_surface = EGL_NO_SURFACE;
        return -1;
    }
    /* Swap interval belongs to the current draw surface, so it has to be set
     * again after every recovery — a recreated surface starts at the driver
     * default. */
    if (!eglSwapInterval(g_dpy, g_cfg.swap_interval))
        fprintf(stderr, "platform_harman: eglSwapInterval(%d) failed err=0x%x\n",
                g_cfg.swap_interval, eglGetError());
    return 0;
}

static void recreate_window(const char *reason)
{
    fprintf(stderr, "platform_harman: recreating window (reason=%s)\n",
            reason ? reason : "?");

    /* The window we reported is gone: withdraw the report before anything else,
     * so the gate cannot switch the cluster onto a displayable in limbo. */
    if (g_ready_reported) {
        overlay_ready_clear(OVERLAY_READY_PATH);
        g_ready_reported = 0;
    }
    /* A new window deserves a fresh set of attempts. */
    g_ready_attempts = 0;

    if (g_dpy != EGL_NO_DISPLAY && g_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(g_dpy, g_surface);
        g_surface = EGL_NO_SURFACE;
    }
    if (!g_surf || kd_surface_recreate(g_surf) != 0) {
        /* Backoff-suppressed or refused: leave the surface down and let the
         * next health tick retry through the "surface missing" path. */
        fprintf(stderr, "platform_harman: recreate suppressed/failed — will retry\n");
        return;
    }
    if (bind_egl_surface() == 0) {
        fprintf(stderr, "platform_harman: window recreated OK\n");
        /* Re-prove the new window and re-report, instead of waiting for the
         * next scene — same reason as at start-up. */
        present_empty_canvas();
    }
}

void platform_check_and_recover_window(void)
{
    if (!g_window_expected || !g_surf) return;

    if (g_surface == EGL_NO_SURFACE) {
        recreate_window("egl surface missing");
        return;
    }
    if (kd_surface_lost(g_surf))
        recreate_window("window lost/disowned");
}

void platform_release_displayable(void)
{
    if (g_ready_reported) {
        overlay_ready_clear(OVERLAY_READY_PATH);
        g_ready_reported = 0;
    }
    if (g_surf) {
        kd_surface_destroy(g_surf);
        g_surf = NULL;
        fprintf(stderr, "platform_harman: managed window destroyed — displayable %d released\n",
                g_cfg.displayable);
    }
}

/* Context routing is the shell script's job (one writer).  Kept so main.c and
 * the host backend link against the same symbol. */
void platform_ensure_focus(void)
{
}

/* The offscreen colour-attachment type the start-up measurement settled on.
 * render.c asks for it so its own buffers match the panel - see platform.h. */
unsigned platform_fbo_type(void)
{
    if (g_panel_fmt < 0 || g_panel_fmt >= PANEL_FMT_COUNT)
        return (unsigned)GL_UNSIGNED_BYTE;
    return (unsigned)k_panel_fmts[g_panel_fmt].type;
}

void platform_set_content_present(int present)
{
    static int last = -1;

    g_content_present = present ? 1 : 0;
    if (g_content_present != last) {
        last = g_content_present;
        fprintf(stderr, "platform_harman: panel %s\n",
                g_content_present ? "drawn" : "blank (nothing to show)");
    }
}

static void signal_handler(int sig)
{
    /* async-signal-safe only */
    static const char msg_segv[] = "platform_harman: caught SIGSEGV\n";
    static const char msg_abrt[] = "platform_harman: caught SIGABRT\n";
    static const char msg_term[] = "platform_harman: caught SIGTERM\n";
    static const char msg_unk[]  = "platform_harman: caught signal\n";

    if (sig == SIGSEGV)      write(STDERR_FILENO, msg_segv, sizeof(msg_segv) - 1);
    else if (sig == SIGABRT) write(STDERR_FILENO, msg_abrt, sizeof(msg_abrt) - 1);
    else if (sig == SIGTERM) write(STDERR_FILENO, msg_term, sizeof(msg_term) - 1);
    else                     write(STDERR_FILENO, msg_unk,  sizeof(msg_unk)  - 1);

    g_should_close = 1;
    if (sig == SIGTERM) return;
    signal(sig, SIG_DFL);
    raise(sig);
}

int platform_init(int width, int height)
{
    EGLint major, minor, nconf = 0;
    kd_surface_cfg kcfg;
    int clamped;

    EGLint cfg_attrs[] = {
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,   8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8, EGL_ALPHA_SIZE, 8,
        EGL_NONE                       /* no MSAA — FXAA handles edges */
    };
    EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

    setvbuf(stderr, NULL, _IOLBF, 0);
    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    atexit(platform_release_displayable);

    /* The caller passes the compiled-in CR_DEFAULT_* as a starting point; the
     * card has the last word.  main.c re-reads the size through
     * platform_get_framebuffer_size right after init, so an override here
     * reaches the renderer without touching Luka's code. */
    overlay_cfg_load(&g_cfg, NULL);
    if (width  > 0 && g_cfg.width  <= 0) g_cfg.width  = width;
    if (height > 0 && g_cfg.height <= 0) g_cfg.height = height;
    clamped = overlay_cfg_sanitize(&g_cfg);

    if (!getenv("IPL_CONFIG_DIR"))
        putenv("IPL_CONFIG_DIR=/etc/eso/production");

    fprintf(stderr, "platform_harman: conf=%s%s\n", g_cfg.source,
            clamped ? " (values clamped)" : "");
    fprintf(stderr, "platform_harman: displayable=%d render=%dx%d"
            " crop=%dx%d@(%d,%d) canvas=%dx%d display=%d context=%d\n",
            g_cfg.displayable, g_cfg.render_width, g_cfg.render_height,
            g_cfg.width, g_cfg.height, g_cfg.crop_x, g_cfg.crop_y,
            g_cfg.canvas_width, g_cfg.canvas_height,
            g_cfg.display, g_cfg.context);
    fprintf(stderr, "platform_harman: requested %dx%d by caller\n", width, height);
    fprintf(stderr, "platform_harman: LD_LIBRARY_PATH=%s\n",
            getenv("LD_LIBRARY_PATH") ? getenv("LD_LIBRARY_PATH") : "(unset)");

    if (kd_surface_global_init() != 0) {
        fprintf(stderr, "platform_harman: FAIL kd init\n");
        return -1;
    }

    g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "platform_harman: FAIL eglGetDisplay\n");
        return -1;
    }
    if (!eglInitialize(g_dpy, &major, &minor)) {
        fprintf(stderr, "platform_harman: FAIL eglInitialize err=0x%x\n", eglGetError());
        return -1;
    }
    fprintf(stderr, "platform_harman: EGL %d.%d\n", major, minor);

    if (!eglChooseConfig(g_dpy, cfg_attrs, &g_econf, 1, &nconf) || nconf == 0) {
        fprintf(stderr, "platform_harman: FAIL eglChooseConfig err=0x%x\n", eglGetError());
        return -1;
    }

    eglBindAPI(EGL_OPENGL_ES_API);
    g_ctx = eglCreateContext(g_dpy, g_econf, EGL_NO_CONTEXT, ctx_attrs);
    if (g_ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "platform_harman: FAIL eglCreateContext err=0x%x\n", eglGetError());
        return -1;
    }

    memset(&kcfg, 0, sizeof(kcfg));
    kcfg.id            = g_cfg.displayable;
    /* The WINDOW is the whole cluster canvas; the panel is a rectangle drawn
     * inside it.  See the compositing note at the top of this file. */
    kcfg.width         = g_cfg.canvas_width;
    kcfg.height        = g_cfg.canvas_height;
    kcfg.display       = g_cfg.display;
    kcfg.alpha         = g_cfg.alpha;
    kcfg.managed       = g_cfg.managed;
    kcfg.swap_interval = g_cfg.swap_interval;

    g_surf = kd_surface_create(&kcfg, g_dpy, g_econf);
    if (!g_surf) {
        fprintf(stderr, "platform_harman: FAIL kd_surface_create\n");
        return -1;
    }
    if (bind_egl_surface() != 0) return -1;
    g_window_expected = 1;

    fprintf(stderr, "platform_harman: GL vendor=%s renderer=%s version=%s\n",
            (const char *)glGetString(GL_VENDOR),
            (const char *)glGetString(GL_RENDERER),
            (const char *)glGetString(GL_VERSION));
    /* Without GL_OES_texture_npot, core GLES2 makes an NPOT texture that does
     * not clamp incomplete, and an incomplete texture samples as (0,0,0,1).
     * That was the black slab: Luka's SSAA and FXAA buffers are NPOT and, until
     * K3346 patched render.c, used the default GL_REPEAT.  One line here turns
     * that diagnosis into a fact in the log. */
    {
        const char *ext = (const char *)glGetString(GL_EXTENSIONS);
        fprintf(stderr, "platform_harman: GL_OES_texture_npot: %s\n",
                (ext && strstr(ext, "GL_OES_texture_npot")) ? "yes" : "no");
        fprintf(stderr, "platform_harman: extensions=%s\n", ext ? ext : "(none)");
    }

    if (make_blit_program() != 0 || make_panel_target() != 0) {
        fprintf(stderr, "platform_harman: FAIL panel compositing setup\n");
        return -1;
    }
    /* Leave the panel FBO bound: render_init() runs next and adopts whatever
     * is bound as its "default framebuffer", which is how Luka's renderer ends
     * up drawing the panel offscreen without a single line of his changing. */
    glBindFramebuffer(GL_FRAMEBUFFER, g_panel_fbo);

    present_empty_canvas();

    fprintf(stderr, "platform_harman: OK render %dx%d, crop %dx%d on canvas"
            " %dx%d (swap interval=%d)\n",
            g_cfg.render_width, g_cfg.render_height, g_cfg.width, g_cfg.height,
            g_cfg.canvas_width, g_cfg.canvas_height, g_cfg.swap_interval);
    return 0;
}

/* ---------------------------------------------------------------------------
 * Where GL errors come from, for the first frames of a window.
 *
 * The 2026-10-28 run had GL_INVALID_OPERATION (0x502) pending at the readback,
 * raised somewhere after render_init (the flag atlas load right after it
 * reported gl_err=0x0).  It is not what blackened the tile - sampling an
 * incomplete texture raises no error at all - but "once or every frame" and
 * "the renderer's or ours" are cheap to answer here without touching main.c:
 * errors pending when platform_swap() starts were raised while the renderer
 * drew the frame; errors raised between that and the swap are the probe's or
 * the composite's.
 */
#define GL_ERR_WATCH_FRAMES 150

static unsigned g_watch_frames, g_err_frames_renderer, g_err_frames_ours;
static GLenum   g_first_err_renderer, g_first_err_ours;

/* Bounded: a context that keeps reporting errors must not stall the frame. */
static int drain_gl_errors(GLenum *first)
{
    GLenum e;
    int n = 0;

    *first = GL_NO_ERROR;
    while (n < 16 && (e = glGetError()) != GL_NO_ERROR) {
        if (!n) *first = e;
        n++;
    }
    return n;
}

int platform_swap(void)
{
    static unsigned failure_streak;
    GLenum e;

    if (g_watch_frames < GL_ERR_WATCH_FRAMES && drain_gl_errors(&e)) {
        g_err_frames_renderer++;
        if (g_first_err_renderer == GL_NO_ERROR) {
            g_first_err_renderer = e;
            fprintf(stderr, "platform_harman: first GL error 0x%x raised by the"
                    " renderer, in frame %u\n", (unsigned)e, g_watch_frames + 1);
        }
    }

    /* The renderer has just finished drawing into the panel FBO.  Look at it
     * FIRST, then put it on the canvas: the probe feeds the degenerate-tile
     * guard in composite_panel(), and reading after the blit would let the
     * frame that reveals a black slab reach the cluster before the guard
     * learns of it.  The blit only reads the panel FBO, so the order changes
     * nothing else. */
    probe_panel_tick();
    composite_panel();

    if (g_watch_frames < GL_ERR_WATCH_FRAMES) {
        if (drain_gl_errors(&e)) {
            g_err_frames_ours++;
            if (g_first_err_ours == GL_NO_ERROR) {
                g_first_err_ours = e;
                fprintf(stderr, "platform_harman: first GL error 0x%x raised by"
                        " our probe/composite, in frame %u\n", (unsigned)e,
                        g_watch_frames + 1);
            }
        }
        if (++g_watch_frames == GL_ERR_WATCH_FRAMES)
            fprintf(stderr, "platform_harman: GL errors in the first %u frames:"
                    " renderer %u (first 0x%x), ours %u (first 0x%x)\n",
                    GL_ERR_WATCH_FRAMES, g_err_frames_renderer,
                    (unsigned)g_first_err_renderer, g_err_frames_ours,
                    (unsigned)g_first_err_ours);
    }

    if (eglSwapBuffers(g_dpy, g_surface) == EGL_TRUE) {
        if (failure_streak > 0)
            fprintf(stderr, "platform_harman: eglSwapBuffers recovered after %u failures\n",
                    failure_streak);
        failure_streak = 0;
        /* A frame has been presented, which is the strongest statement we can
         * make: the window was realized AND the compositor took a buffer from
         * it.  Only now may the gate switch the cluster context.  Normally the
         * report is already out from present_empty_canvas() at start-up; this
         * covers the case where that one write failed. */
        report_ready_once();
        return 1;
    }
    /* Swap drives the frame pacing of the whole renderer, so a dead surface
     * would otherwise spin forever. */
    {
        EGLint err = eglGetError();
        failure_streak++;
        if (failure_streak == 1 || (failure_streak % 30u) == 0)
            fprintf(stderr, "platform_harman: eglSwapBuffers failed err=0x%x streak=%u -> recreate\n",
                    err, failure_streak);
    }
    recreate_window("swap-failed");
    return 0;
}

/* Put the last rendered frame on the cluster again, now.  The panel FBO still
 * holds it, so a layout change - or the end of a VIEW hold - does not have to
 * wait for Luka's loop to draw something new.  On a parked car that can take
 * seconds, and while idle his loop sleeps up to 333 ms a turn (main.c:969). */
static void represent_last_frame(void)
{
    static int warned;

    if (g_dpy == EGL_NO_DISPLAY || g_surface == EGL_NO_SURFACE || !g_panel_fbo)
        return;
    composite_panel();
    if (eglSwapBuffers(g_dpy, g_surface) != EGL_TRUE && !warned) {
        warned = 1;
        fprintf(stderr, "platform_harman: re-present swap failed err=0x%x -"
                " the next rendered frame catches up\n", eglGetError());
    }
}

void platform_poll(void)
{
    /* No event loop: the signal flag is the only input.  The one thing worth
     * sampling here is the VC gauge layout, because it decides where the
     * stock maneuver tile sits.  Read-only: the evidence is the DSITracer
     * capture ring the v8s control script already keeps (overlay_layout.h),
     * so nothing in dio_manager has to change. */
    struct timespec now;
    long now_ms;
    int slot, eff, top, pending, repaint = 0;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return;
    now_ms = now.tv_sec * 1000L + now.tv_nsec / 1000000L;
    if (g_layout_check_ms != 0 && (now_ms - g_layout_check_ms) < LAYOUT_POLL_MS) return;
    g_layout_check_ms = now_ms;

    if (!g_layout_reader_ready) {
        overlay_layout_reader_init(&g_layout_reader, NULL, 0);
        g_layout_reader_ready = 1;
    }
    slot = overlay_layout_reader_poll(&g_layout_reader);
    pending = g_layout_reader.view_pending;

    /* VIEW pressed: off the canvas until the cluster says where to go. */
    if (pending && !g_view_hold && !g_view_expired) {
        g_view_hold = 1;
        g_view_hold_ms = now_ms;
        repaint = 1;
        fprintf(stderr, "platform_harman: VIEW pressed - tile hidden until the"
                " cluster reports the new layout\n");
    }

    if (slot != g_layout_slot) {
        g_layout_slot = slot;
        repaint = 1;
        if (slot == OVERLAY_LAYOUT_UNKNOWN) {
            fprintf(stderr, "platform_harman: layout slot unknown (nothing in %s"
                    " yet) - tile at the big-clocks origin\n",
                    OVERLAY_LAYOUT_RING_FMT);
        } else {
            int px = g_cfg.x, py = g_cfg.y;
            overlay_layout_origin(slot, g_cfg.x, g_cfg.y, g_cfg.x_alt,
                                  g_cfg.y_alt, &px, &py);
            /* Applied from here on: composite_panel() reads the same slot
             * when it places the tile on the canvas. */
            fprintf(stderr, "platform_harman: layout slot=%d (%s clocks, %s)"
                    " panel at (%d,%d)\n", slot, slot == 1 ? "big" : "small",
                    overlay_layout_src_name(g_layout_reader.source), px, py);
        }
    }

    /* The report came - or never will.  Either way the tile comes back, and
     * the log says how long it was away, which is the number to watch. */
    if (g_view_hold && (!pending || now_ms - g_view_hold_ms >= VIEW_HOLD_MAX_MS)) {
        int px = g_cfg.x, py = g_cfg.y;
        overlay_layout_origin(g_layout_slot, g_cfg.x, g_cfg.y, g_cfg.x_alt,
                              g_cfg.y_alt, &px, &py);
        fprintf(stderr, "platform_harman: tile shown at (%d,%d) after %ld ms (%s)\n",
                px, py, now_ms - g_view_hold_ms,
                pending ? "timeout - no layout report" : "bap54");
        if (pending) g_view_expired = 1;
        g_view_hold = 0;
        repaint = 1;
    }
    if (!pending) g_view_expired = 0;

    /* The part of the crop the VC leaves visible in this layout.  An unknown
     * slot is treated as big clocks, the same default composite_panel()
     * uses for the position.  Re-sent only when it changes; with the default
     * of 0 it equals Luka's own CR_POPUP_* framing, so nothing moves. */
    eff = (slot == 0) ? 0 : 1;
    top = eff ? g_cfg.visible_top_big : g_cfg.visible_top_small;
    if (top != g_visible_top_applied) {
        g_visible_top_applied = top;
        render_set_visible_area(g_cfg.crop_x, g_cfg.crop_y + top,
                                g_cfg.width, g_cfg.height - top);
        if (top)
            fprintf(stderr, "platform_harman: visible area %dx%d@(%d,%d) -"
                    " top %d rows covered by the VC in this layout\n",
                    g_cfg.width, g_cfg.height - top, g_cfg.crop_x,
                    g_cfg.crop_y + top, top);
    }

    /* Show the change now, not with whatever frame comes next. */
    if (repaint) represent_last_frame();
}

int platform_should_close(void)
{
    return g_should_close;
}

void platform_shutdown(void)
{
    struct timespec release_wait;

    if (g_surf) kd_surface_hide(g_surf);

    if (g_dpy != EGL_NO_DISPLAY) {
        if (g_ctx != EGL_NO_CONTEXT) glFinish();
        release_wait.tv_sec = 0;
        release_wait.tv_nsec = 50L * 1000L * 1000L;
        while (nanosleep(&release_wait, &release_wait) != 0 && errno == EINTR) {}
        eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_surface != EGL_NO_SURFACE) {
            eglDestroySurface(g_dpy, g_surface);
            g_surface = EGL_NO_SURFACE;
        }
        if (g_ctx != EGL_NO_CONTEXT) {
            eglDestroyContext(g_dpy, g_ctx);
            g_ctx = EGL_NO_CONTEXT;
        }
        /* Deliberately no eglTerminate: the EGLDisplay is shared with native
         * cluster components and terminating it takes their surfaces down too.
         * Same reasoning as Luka's platform_qnx.c. */
    }
    platform_release_displayable();
}

/* What the RENDERER draws into — main.c hands this straight to render_init().
 * Not the panel size: see make_panel_target() and overlay_cfg.h. */
void platform_get_framebuffer_size(int *width, int *height)
{
    if (width)  *width  = g_cfg.render_width;
    if (height) *height = g_cfg.render_height;
}

void platform_get_routing_ids(int *display_id, int *context_id, int *displayable_id)
{
    if (display_id)     *display_id     = g_cfg.display;
    if (context_id)     *context_id     = g_cfg.context;
    if (displayable_id) *displayable_id = g_cfg.displayable;
}

int platform_key_tap(int key)
{
    (void)key;
    return 0;
}

#endif /* PLATFORM_HARMAN */
