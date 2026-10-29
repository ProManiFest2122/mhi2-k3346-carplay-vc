/*
 * vc_overlay_probe — Etap 7a calibration probe for the K3346 cluster overlay.
 *
 * WHAT THIS ANSWERS
 * The whole Etap 7 plan rests on one unverified assumption: that the MHI2
 * DisplayManager will adopt a window created by a foreign process as a new
 * displayable and composite it over the CarPlay map (displayable 58).  Luka
 * does exactly that on MHI2Q through QNX Screen; K3346 has no libscreen, so we
 * do it through the KD QNX extension instead (see kd_surface.c).  Nothing in
 * the firmware we can read offline settles whether it works.
 *
 * So this binary does the smallest thing that settles it: open the window, draw
 * a pattern that can be MEASURED off a screenshot, and log every step.  It
 * renders no maneuver data and reads no CarPlay state.
 *
 * WHY A MEASURABLE PATTERN AND NOT A SOLID COLOUR
 * We do not know where the VC puts our layer, at what size, or whether it is
 * cropped — the KDK geometry for B9 is documented but never measured
 * (docs/02_edzenis_vs_K3346.md:133).  A solid rectangle would only prove
 * "something appeared".  The grid below lets one `dmdt ts 4` screenshot give
 * the origin, the scale, the orientation and the crop.
 *
 * SAFETY
 * Read-only with respect to the car: no file in the car's filesystem is
 * written except the log on /tmp, no context is switched from here (the shell
 * script owns dmdt, as it does today), and the existing 1 s loop in
 * mib2high-lvds-control.sh re-asserts `sc 4 77` every 3 s, so any experiment
 * self-heals even if this process is killed mid-way.
 *
 * Usage:
 *   vc_overlay_probe [--conf PATH] [--id N] [--w N] [--h N] [--display N]
 *                    [--secs N] [--unmanaged] [--opaque]
 *
 * --unmanaged is a no-op kept for script compatibility: this unit's
 * DisplayManager has no MANAGED_QNX property at all (see kd_surface.h).
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "../overlay_cfg.h"
#include "../kd_surface.h"

#define PROBE_LOG "/tmp/vc-overlay-probe.log"

/* Luka's popup crop on MHI2Q (protocol.h CR_POPUP_*).  Drawn as an inner
 * outline so the screenshot shows at a glance whether the VC on this car crops
 * our source the same way his does. */
#define REF_CROP_X 59
#define REF_CROP_Y 27
#define REF_CROP_W 210
#define REF_CROP_H 153

static volatile int g_quit;
static FILE *g_log;

static void on_sig(int s) { (void)s; g_quit = 1; }

static void plog(const char *fmt, ...)
{
    va_list ap;
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    va_start(ap, fmt);
    if (g_log) {
        fprintf(g_log, "%ld.%03ld [probe] ", (long)ts.tv_sec, ts.tv_nsec / 1000000L);
        vfprintf(g_log, fmt, ap);
        fputc('\n', g_log);
        fflush(g_log);
    }
    va_end(ap);
    va_start(ap, fmt);
    fprintf(stderr, "[probe] ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* ---------------------------------------------------------------- GLES2 --
 * One program, position + colour.  Deliberately the simplest thing that can
 * draw: if the probe fails we want the failure to be about the window, not
 * about our shaders.
 */
static const char *VS_SRC =
    "attribute vec2 a_pos;\n"
    "attribute vec4 a_col;\n"
    "varying vec4 v_col;\n"
    "uniform vec2 u_size;\n"
    "void main() {\n"
    "  vec2 ndc = vec2(a_pos.x / u_size.x * 2.0 - 1.0,\n"
    "                  1.0 - a_pos.y / u_size.y * 2.0);\n"
    "  v_col = a_col;\n"
    "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "}\n";

static const char *FS_SRC =
    "precision mediump float;\n"
    "varying vec4 v_col;\n"
    "void main() { gl_FragColor = v_col; }\n";

static GLuint g_prog;
static GLint  g_a_pos, g_a_col, g_u_size;

#define MAX_VERTS 8192
static GLfloat g_vb[MAX_VERTS * 6];   /* x,y,r,g,b,a */
static int     g_vcount;

static GLuint compile(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        GLsizei n = 0;
        glGetShaderInfoLog(s, (GLsizei)sizeof(buf), &n, buf);
        buf[(n > 0 && n < (GLsizei)sizeof(buf)) ? n : 0] = '\0';
        plog("shader compile failed: %s", buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static int gl_setup(void)
{
    GLuint vs = compile(GL_VERTEX_SHADER, VS_SRC);
    GLuint fs = compile(GL_FRAGMENT_SHADER, FS_SRC);
    GLint ok = 0;

    if (!vs || !fs) return -1;
    g_prog = glCreateProgram();
    glAttachShader(g_prog, vs);
    glAttachShader(g_prog, fs);
    glLinkProgram(g_prog);
    glGetProgramiv(g_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        GLsizei n = 0;
        glGetProgramInfoLog(g_prog, (GLsizei)sizeof(buf), &n, buf);
        buf[(n > 0 && n < (GLsizei)sizeof(buf)) ? n : 0] = '\0';
        plog("program link failed: %s", buf);
        return -1;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    g_a_pos  = glGetAttribLocation(g_prog, "a_pos");
    g_a_col  = glGetAttribLocation(g_prog, "a_col");
    g_u_size = glGetUniformLocation(g_prog, "u_size");
    return 0;
}

static void vtx(float x, float y, float r, float g, float b, float a)
{
    GLfloat *p;
    if (g_vcount >= MAX_VERTS) return;
    p = &g_vb[g_vcount * 6];
    p[0] = x; p[1] = y; p[2] = r; p[3] = g; p[4] = b; p[5] = a;
    g_vcount++;
}

static void rect(float x, float y, float w, float h,
                 float r, float g, float b, float a)
{
    vtx(x,     y,     r, g, b, a);
    vtx(x + w, y,     r, g, b, a);
    vtx(x + w, y + h, r, g, b, a);
    vtx(x,     y,     r, g, b, a);
    vtx(x + w, y + h, r, g, b, a);
    vtx(x,     y + h, r, g, b, a);
}

/* Outline drawn as four 1 px bars — GL_LINES width is not dependable across
 * drivers, and on a screenshot a bar is easier to measure than a line. */
static void frame(float x, float y, float w, float h, float t,
                  float r, float g, float b, float a)
{
    rect(x,         y,         w, t,         r, g, b, a);
    rect(x,         y + h - t, w, t,         r, g, b, a);
    rect(x,         y,         t, h,         r, g, b, a);
    rect(x + w - t, y,         t, h,         r, g, b, a);
}

/*
 * The calibration pattern.  Everything here exists to be read off a screenshot:
 *
 *   - translucent wash           the map stays visible underneath, which is
 *                                itself the proof that alpha blending works
 *   - magenta 2 px outer frame   exact bounds of our layer on the VC
 *   - ticks every 20 px,         scale: count ticks to the panel edge and you
 *     long every 100 px          know whether the VC scales our source
 *   - corner patches R/G/B/Y     orientation and which corners survive a crop
 *   - centre crosshair           the anchor point
 *   - cyan inner frame           Luka's 210x153 popup crop, for comparison
 *   - alpha staircase, bottom    six white steps at alpha 0, .2, .4, .6, .8, 1
 *                                over bare framebuffer (no wash behind them)
 *
 * THE STAIRCASE IS THE ALPHA ANSWER, and it is on every single screenshot on
 * purpose, so no run can come back without it.  This DisplayManager composites
 * with a `premultiplied_alpha` material and never enables GL_BLEND itself, so
 * what it expects from us is rgb*a - but the exact formula lives in precompiled
 * shader binaries and could not be read offline.  On the screenshot:
 *
 *   map shows through, fading step by step   per-pixel alpha works, as assumed
 *   all six steps equally solid             no blending: we must paint the map
 *                                           background into our own frame
 *   steps visible but washed out / fringed  straight vs premultiplied mismatch
 */
#define ALPHA_BAND_H 22

static void build_pattern(int w, int h, int phase)
{
    int i;
    float pulse = (phase % 20) < 10 ? 1.0f : 0.45f;
    float band_h = (h > 3 * ALPHA_BAND_H) ? (float)ALPHA_BAND_H : 0.0f;
    float grid_h = (float)h - band_h;

    g_vcount = 0;

    /* The wash deliberately stops short of the staircase: a step must sit on
     * bare framebuffer or its alpha is not the alpha we think we are testing. */
    rect(0, 0, (float)w, grid_h, 0.0f, 0.0f, 0.0f, 0.35f);
    frame(0, 0, (float)w, (float)h, 2.0f, 1.0f, 0.0f, 1.0f, 0.95f);

    for (i = 0; i * 20 <= w; i++) {
        float len = (i % 5 == 0) ? 12.0f : 6.0f;
        rect((float)(i * 20), 2.0f, 1.0f, len, 1.0f, 1.0f, 1.0f, 0.9f);
    }
    for (i = 0; i * 20 <= h; i++) {
        float len = (i % 5 == 0) ? 12.0f : 6.0f;
        rect(2.0f, (float)(i * 20), len, 1.0f, 1.0f, 1.0f, 1.0f, 0.9f);
    }

    rect(0,               0,               16, 16, 1.0f, 0.0f, 0.0f, 0.95f);
    rect((float)(w - 16), 0,               16, 16, 0.0f, 1.0f, 0.0f, 0.95f);
    rect(0,               (float)(h - 16), 16, 16, 0.0f, 0.4f, 1.0f, 0.95f);
    rect((float)(w - 16), (float)(h - 16), 16, 16, 1.0f, 1.0f, 0.0f, 0.95f);

    rect((float)(w / 2) - 20.0f, (float)(h / 2) - 1.0f, 40, 2,
         1.0f, 1.0f, 1.0f, pulse);
    rect((float)(w / 2) - 1.0f, (float)(h / 2) - 20.0f, 2, 40,
         1.0f, 1.0f, 1.0f, pulse);

    if (w >= REF_CROP_X + REF_CROP_W && h >= REF_CROP_Y + REF_CROP_H)
        frame((float)REF_CROP_X, (float)REF_CROP_Y,
              (float)REF_CROP_W, (float)REF_CROP_H, 1.0f,
              0.0f, 0.85f, 0.95f, 0.85f);

    if (band_h > 0.0f) {
        /* Six steps; the first is alpha 0 and draws nothing on purpose - it is
         * the reference for "what the map looks like with us out of the way",
         * right next to the step that is fully opaque. */
        float step_w = (float)w / 6.0f;
        for (i = 1; i < 6; i++)
            rect((float)i * step_w, grid_h, step_w, band_h,
                 1.0f, 1.0f, 1.0f, (float)i * 0.2f);
        /* Hairline so the band's extent is unambiguous even at alpha 0. */
        rect(0.0f, grid_h, (float)w, 1.0f, 1.0f, 0.0f, 1.0f, 0.95f);
    }
}

static void draw(int w, int h, int phase)
{
    build_pattern(w, h, phase);

    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    /* Separate alpha: plain glBlendFunc squares the alpha of what we draw and
     * the overlay comes out muddy over the map.  Same landmine Luka documents
     * for the real renderer (docs/LANDMINES.md:347). */
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(g_prog);
    glUniform2f(g_u_size, (float)w, (float)h);
    glVertexAttribPointer(g_a_pos, 2, GL_FLOAT, GL_FALSE,
                          6 * sizeof(GLfloat), g_vb);
    glVertexAttribPointer(g_a_col, 4, GL_FLOAT, GL_FALSE,
                          6 * sizeof(GLfloat), g_vb + 2);
    glEnableVertexAttribArray(g_a_pos);
    glEnableVertexAttribArray(g_a_col);
    glDrawArrays(GL_TRIANGLES, 0, g_vcount);
}

/* ------------------------------------------------------------------ main -- */

int main(int argc, char **argv)
{
    overlay_cfg_t cfg;
    kd_surface_cfg kcfg;
    kd_surface_t *surf = NULL;
    const char *conf_path = NULL;
    EGLDisplay dpy = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext ctx = EGL_NO_CONTEXT;
    EGLConfig  econf = 0;
    EGLint major, minor, nconf = 0;
    EGLNativeWindowType native;
    int secs = 0, fixed, i, phase = 0;
    time_t t_end = 0;

    EGLint cfg_attrs[] = {
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,   8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

    g_log = fopen(PROBE_LOG, "a");
    setvbuf(stderr, NULL, _IOLBF, 0);
    signal(SIGTERM, on_sig);
    signal(SIGINT,  on_sig);

    /* Parse first so --conf is known before the file is read. */
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--conf") && i + 1 < argc) conf_path = argv[++i];

    overlay_cfg_load(&cfg, conf_path);

    for (i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--id")      && i + 1 < argc) cfg.displayable = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--w")       && i + 1 < argc) cfg.width       = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--h")       && i + 1 < argc) cfg.height      = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--display") && i + 1 < argc) cfg.display     = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--secs")    && i + 1 < argc) secs            = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--unmanaged"))               cfg.managed     = 0; /* ignored, see above */
        else if (!strcmp(argv[i], "--opaque"))                  cfg.alpha       = 0;
        else if (!strcmp(argv[i], "--conf"))                    i++;
    }
    /* For the probe the window IS the thing being measured: whatever --w/--h ask
     * for is also the canvas, and the pattern is drawn at the window origin, so
     * the panel offsets mean nothing here.  Spelling that out keeps the sanitizer
     * from "correcting" values the probe never reads and reporting
     * "(values clamped)" on a run where nothing is wrong - noise in exactly the
     * log we read to decide things. */
    cfg.canvas_width  = cfg.width;
    cfg.canvas_height = cfg.height;
    cfg.x = cfg.y = cfg.x_alt = cfg.y_alt = 0;

    fixed = overlay_cfg_sanitize(&cfg);

    plog("start pid=%d conf=%s%s", (int)getpid(), cfg.source,
         fixed ? " (values clamped)" : "");
    plog("cfg displayable=%d size=%dx%d display=%d alpha=%d managed=%d interval=%d",
         cfg.displayable, cfg.width, cfg.height, cfg.display,
         cfg.alpha, cfg.managed, cfg.swap_interval);

    /* KD first: kdInitializeNV must run before EGL on this stack, the same
     * order vc_render uses and the same order loadandshowimage links for. */
    if (kd_surface_global_init() != 0) {
        plog("FAIL kd init");
        return 2;
    }

    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) { plog("FAIL eglGetDisplay"); return 3; }
    if (!eglInitialize(dpy, &major, &minor)) {
        plog("FAIL eglInitialize err=0x%x", eglGetError());
        return 3;
    }
    plog("EGL %d.%d vendor=%s", major, minor, eglQueryString(dpy, EGL_VENDOR));

    if (!eglChooseConfig(dpy, cfg_attrs, &econf, 1, &nconf) || nconf == 0) {
        plog("FAIL eglChooseConfig err=0x%x", eglGetError());
        return 3;
    }

    memset(&kcfg, 0, sizeof(kcfg));
    kcfg.id            = cfg.displayable;
    kcfg.width         = cfg.width;
    kcfg.height        = cfg.height;
    kcfg.display       = cfg.display;
    kcfg.alpha         = cfg.alpha;
    kcfg.managed       = cfg.managed;
    kcfg.swap_interval = cfg.swap_interval;

    surf = kd_surface_create(&kcfg, dpy, econf);
    if (!surf) { plog("FAIL kd_surface_create — DisplayManager did not give us a window"); return 4; }

    native = kd_surface_native(surf);
    surface = eglCreateWindowSurface(dpy, econf, native, NULL);
    if (surface == EGL_NO_SURFACE) {
        plog("FAIL eglCreateWindowSurface err=0x%x", eglGetError());
        kd_surface_destroy(surf);
        return 5;
    }

    eglBindAPI(EGL_OPENGL_ES_API);
    ctx = eglCreateContext(dpy, econf, EGL_NO_CONTEXT, ctx_attrs);
    if (ctx == EGL_NO_CONTEXT) {
        plog("FAIL eglCreateContext err=0x%x", eglGetError());
        return 5;
    }
    if (!eglMakeCurrent(dpy, surface, surface, ctx)) {
        plog("FAIL eglMakeCurrent err=0x%x", eglGetError());
        return 5;
    }
    eglSwapInterval(dpy, cfg.swap_interval);

    plog("GL vendor=%s renderer=%s version=%s",
         (const char *)glGetString(GL_VENDOR),
         (const char *)glGetString(GL_RENDERER),
         (const char *)glGetString(GL_VERSION));

    if (gl_setup() != 0) { plog("FAIL gl_setup"); return 6; }

    plog("READY — drawing %dx%d calibration pattern", cfg.width, cfg.height);
    if (secs > 0) t_end = time(NULL) + secs;

    while (!g_quit) {
        struct timespec nap;

        draw(cfg.width, cfg.height, phase);
        if (eglSwapBuffers(dpy, surface) != EGL_TRUE) {
            EGLint err = eglGetError();
            plog("eglSwapBuffers failed err=0x%x -> recreate", err);
            eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroySurface(dpy, surface);
            surface = EGL_NO_SURFACE;
            if (kd_surface_recreate(surf) == 0) {
                surface = eglCreateWindowSurface(dpy, econf,
                                                 kd_surface_native(surf), NULL);
                if (surface != EGL_NO_SURFACE)
                    eglMakeCurrent(dpy, surface, surface, ctx);
            }
            if (surface == EGL_NO_SURFACE) {
                plog("recreate failed — giving up");
                break;
            }
        }

        if (++phase % 100 == 0)
            plog("alive frames=%d lost=%d", phase, kd_surface_lost(surf));
        if (t_end && time(NULL) >= t_end) {
            plog("--secs elapsed, exiting");
            break;
        }

        nap.tv_sec = 0;
        nap.tv_nsec = 100L * 1000L * 1000L;   /* 10 Hz is plenty for a still */
        while (nanosleep(&nap, &nap) != 0 && errno == EINTR) {}
    }

    plog("shutdown after %d frames", phase);
    kd_surface_hide(surf);
    if (dpy != EGL_NO_DISPLAY) {
        glFinish();
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface != EGL_NO_SURFACE) eglDestroySurface(dpy, surface);
        if (ctx != EGL_NO_CONTEXT)     eglDestroyContext(dpy, ctx);
        /* No eglTerminate: the EGLDisplay is shared with native cluster
         * components and tearing it down would take their surfaces with it. */
    }
    kd_surface_destroy(surf);
    plog("done");
    if (g_log) fclose(g_log);
    return 0;
}
