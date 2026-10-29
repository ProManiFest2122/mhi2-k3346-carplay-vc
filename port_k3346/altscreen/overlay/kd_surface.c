/*
 * kd_surface — see kd_surface.h.
 *
 * QNX 6.5 / GCC 4.9: no __thread, plain structs, declarations at the top of a
 * block — the same house style as native/, so one toolchain builds everything.
 */

#include "kd_surface.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <dlfcn.h>

#include <KD/kd.h>
#include <KD/kd_qnx_window.h>

/* kdInitializeNV is an NVIDIA extension: present in the car's libKD.so but in
 * no SDP header, so it is resolved exactly the way vc_render resolves it
 * (renderer/upstream/vc_render.c:85).  Reusing that lookup means one
 * known-good precedent covers both renderers. */
typedef int (*kd_init_nv_t)(void);
static kd_init_nv_t p_kd_init_nv;
static int g_global_ready;

struct kd_surface {
    kd_surface_cfg      cfg;
    EGLDisplay          dpy;
    EGLConfig           egl_cfg;
    KDWindow           *win;
    EGLNativeWindowType native;
    struct timespec     last_recreate;
    int                 realized;
    /* Sticky: VISIBILITY readability is a property of this DisplayManager, not
     * of one window, so the warning is worth exactly one line per process. */
    int                 vis_read_warned;
};

static void kds_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "kd_surface: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

int kd_surface_global_init(void)
{
    void *kd;
    int rc;

    if (g_global_ready) return 0;

    /* RTLD_GLOBAL: the EGL/GLES2 stack on this unit resolves KD symbols too. */
    kd = dlopen("libKD.so", RTLD_NOW | RTLD_GLOBAL);
    if (!kd) {
        /* Not fatal on its own: we may already be linked against libKD. */
        kds_log("dlopen libKD.so failed: %s", dlerror());
    }
    p_kd_init_nv = (kd_init_nv_t)dlsym(kd ? kd : RTLD_DEFAULT, "kdInitializeNV");
    if (!p_kd_init_nv)
        p_kd_init_nv = (kd_init_nv_t)dlsym(RTLD_DEFAULT, "kdInitializeNV");
    if (!p_kd_init_nv) {
        kds_log("kdInitializeNV not found — cannot open a cluster window");
        return -1;
    }

    rc = p_kd_init_nv();
    kds_log("kdInitializeNV -> %d", rc);
    if (rc != 0) return -1;

    g_global_ready = 1;
    return 0;
}

/*
 * Some QNX KD window properties are booleans in one BSP and ints in another,
 * and this unit's libKD is not the SDP one.  Rather than guess, try boolean
 * then int and report which was accepted — the Etap 7a log then records the
 * answer once and for all.
 */
static int set_flag_prop(KDWindow *w, int pname, int value, const char *label)
{
    KDboolean b = (KDboolean)(value ? 1 : 0);
    KDint32   i = (KDint32)value;

    if (kdSetWindowPropertybv(w, pname, &b) == 0) {
        kds_log("%s=%d via bv", label, value);
        return 0;
    }
    if (kdSetWindowPropertyiv(w, pname, &i) == 0) {
        kds_log("%s=%d via iv", label, value);
        return 0;
    }
    kds_log("%s=%d REFUSED (bv and iv)", label, value);
    return -1;
}

static int kds_open_window(kd_surface_t *s)
{
    kd_surface_cfg *c = &s->cfg;
    char cap[16];
    KDint32 size[2];
    KDint32 disp;

    s->win = kdCreateWindow(s->dpy, s->egl_cfg, NULL);
    if (!s->win) {
        kds_log("kdCreateWindow failed");
        return -1;
    }

    /* CAPTION is the whole trick: this DisplayManager takes the window caption,
     * atoi()s it and stores the result as the window's displayable number.  It
     * checks it against nothing - not the config, not a list of registered ids,
     * not duplicates - so a plain "98" is enough to join the cluster composite.
     *
     * Not ID_STRING, not MANAGED: neither string occurs anywhere in this unit's
     * /eso/bin/apps/displaymanager (583837 B, framework 5.41.10.SR5 MIB2CLU6),
     * which is why Etap 7a never got the window adopted.  The sequence below is
     * the car's own: libdisplayinit.so display_create_window, also used by
     * /eso/bin/apps/loadandshowimage.  See docs/19_displayable_na_mhi2.md.
     *
     * Caption values the DM rejects or mishandles (so overlay_cfg_sanitize must
     * never let them through): "0" or a non-number stores id 0, "-1" is ignored
     * outright, over 64 characters is refused with "CAPTION string seems too
     * long". */
    snprintf(cap, sizeof(cap), "%d", c->id);
    if (kdSetWindowPropertycv(s->win, KD_WINDOWPROPERTY_CAPTION, cap) != 0) {
        kds_log("set CAPTION=\"%s\" FAILED errno=%d", cap, errno);
        goto fail;
    }
    kds_log("CAPTION=\"%s\"", cap);

    /* The stock recipe does not touch DISPLAY_QNX at all, and loadandshowimage
     * lands on the cluster without it, so the default display is already the
     * right one.  Kept as an opt-in knob only: display < 0 (the shipped default)
     * skips it. */
    if (c->display >= 0) {
        disp = (KDint32)c->display;
        if (kdSetWindowPropertyiv(s->win, KD_WINDOWPROPERTY_DISPLAY_QNX, &disp) != 0)
            kds_log("set DISPLAY=%d failed errno=%d (staying on the default display)",
                    c->display, errno);
        else
            kds_log("DISPLAY=%d", c->display);
    } else {
        kds_log("DISPLAY not set (stock recipe, conf display=%d)", c->display);
    }

    size[0] = (KDint32)c->width;
    size[1] = (KDint32)c->height;
    if (kdSetWindowPropertyiv(s->win, KD_WINDOWPROPERTY_SIZE, size) != 0) {
        kds_log("set SIZE=%dx%d FAILED errno=%d", c->width, c->height, errno);
        goto fail;
    }
    kds_log("SIZE=%dx%d", c->width, c->height);

    if (c->alpha)
        (void)set_flag_prop(s->win, KD_WINDOWPROPERTY_SOURCE_ALPHA_QNX, 1, "SOURCE_ALPHA");

    if (c->swap_interval > 0) {
        KDint32 iv = (KDint32)c->swap_interval;
        if (kdSetWindowPropertyiv(s->win, KD_WINDOWPROPERTY_SWAP_INTERVAL_QNX, &iv) != 0)
            kds_log("set SWAP_INTERVAL=%d failed errno=%d (EGL interval still applies)",
                    c->swap_interval, errno);
    }

    /* No MANAGED_QNX here on purpose.  Luka's screen_manage_window has no
     * counterpart on this unit: the DM adopts the window by itself on
     * KD_EVENT_GUI_WINDOWAVAILABLE_NV once the caption is set. */

    (void)set_flag_prop(s->win, KD_WINDOWPROPERTY_VISIBILITY, 1, "VISIBILITY");

    if (kdRealizeWindow(s->win, &s->native) != 0 || !s->native) {
        kds_log("kdRealizeWindow FAILED errno=%d", errno);
        goto fail;
    }
    s->realized = 1;
    kds_log("window caption=\"%s\" %dx%d display=%d alpha=%d realized native=%p",
            cap, c->width, c->height, c->display, c->alpha, (void *)s->native);
    return 0;

fail:
    kdDestroyWindow(s->win);
    s->win = NULL;
    s->native = 0;
    s->realized = 0;
    return -1;
}

kd_surface_t *kd_surface_create(const kd_surface_cfg *cfg,
                                EGLDisplay dpy, EGLConfig cfg_egl)
{
    kd_surface_t *s;

    if (!cfg || dpy == EGL_NO_DISPLAY) return NULL;
    if (kd_surface_global_init() != 0) return NULL;

    s = (kd_surface_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->cfg     = *cfg;
    s->dpy     = dpy;
    s->egl_cfg = cfg_egl;

    if (kds_open_window(s) != 0) {
        free(s);
        return NULL;
    }
    return s;
}

EGLNativeWindowType kd_surface_native(kd_surface_t *s)
{
    return s ? s->native : (EGLNativeWindowType)0;
}

/*
 * Health probe.  On Screen, Luka reads back MANAGER_STRING to tell whether the
 * DisplayManager still owns the window.  KD has no such stamp, so the idea was
 * to use what KD does expose: a window we no longer own stops answering
 * property reads, and a window the DM hid on a context switch reports
 * VISIBILITY 0.
 *
 * THE FIRST HALF OF THAT IS WRONG ON THIS UNIT, and the car run of 2026-10-27
 * proved it.  kdGetWindowPropertyiv(VISIBILITY) never succeeds here — the
 * property is write-only in this KD implementation.  The giveaway is the errno
 * we used to print: 18, 89, 236, 236, 236 ... — not one errno this call set,
 * but whatever the previous failed call happened to leave behind.  Meanwhile
 * the window was demonstrably fine: the probe logs the identical failure, then
 * draws 282 frames that show up on the `dmdt ts` screenshot.
 *
 * Treating that as "lost" made the renderer tear the window down and rebuild
 * it every 5 s, and each rebuild withdrew the readiness report, so the shell
 * gate never switched the cluster.  So: a failed read tells us nothing and is
 * treated as healthy.  Only a read that SUCCEEDS and says 0 means the DM
 * disowned us.  Real loss still reaches us through eglSwapBuffers failing,
 * which platform_swap() turns into a recreate.
 */
int kd_surface_lost(kd_surface_t *s)
{
    KDint32 vis = -1;

    if (!s || !s->win || !s->realized) return 1;

    if (kdGetWindowPropertyiv(s->win, KD_WINDOWPROPERTY_VISIBILITY, &vis) != 0) {
        /* Once per surface: this is the normal case here, not an incident. */
        if (!s->vis_read_warned) {
            s->vis_read_warned = 1;
            kds_log("health: VISIBILITY not readable on this DM"
                    " - health check falls back to swap failures");
        }
        return 0;
    }
    if (vis == 0) {
        kds_log("health: VISIBILITY=0 -> disowned");
        return 1;
    }
    return 0;
}

static long ms_since(const struct timespec *then)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - then->tv_sec) * 1000L
         + (now.tv_nsec - then->tv_nsec) / 1000000L;
}

int kd_surface_recreate(kd_surface_t *s)
{
    if (!s) return -1;

    /* Backoff so a flapping DisplayManager cannot become a spin loop. */
    if (s->last_recreate.tv_sec != 0 && ms_since(&s->last_recreate) < 100) {
        kds_log("recreate suppressed (backoff)");
        return -1;
    }
    clock_gettime(CLOCK_MONOTONIC, &s->last_recreate);

    if (s->win) {
        kdDestroyWindow(s->win);
        s->win = NULL;
        s->native = 0;
        s->realized = 0;
    }
    return kds_open_window(s);
}

void kd_surface_hide(kd_surface_t *s)
{
    if (s && s->win)
        (void)set_flag_prop(s->win, KD_WINDOWPROPERTY_VISIBILITY, 0, "VISIBILITY");
}

void kd_surface_destroy(kd_surface_t *s)
{
    if (!s) return;
    if (s->win) {
        kdDestroyWindow(s->win);
        kds_log("window destroyed — displayable %d released", s->cfg.id);
    }
    free(s);
}
