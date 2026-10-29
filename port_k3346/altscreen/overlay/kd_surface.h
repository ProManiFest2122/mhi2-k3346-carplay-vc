/*
 * kd_surface — managed OpenKODE window for the K3346 (MHI2 Harman) cluster.
 *
 * This is the K3346 counterpart of Luka's common/cluster_surface.c.  He opens a
 * QNX Screen window and hands it to the DisplayManager with
 * screen_manage_window() + SCREEN_PROPERTY_ID_STRING = "98".  K3346 has no
 * libscreen at all (QNX 6.5 SP1 + NVIDIA Tegra winsys); the same handshake
 * lives in the KD QNX extension instead:
 *
 *   Luka (QNX Screen)                     K3346 (OpenKODE)
 *   screen_create_context                 kdInitializeNV
 *   screen_create_window                  kdCreateWindow(EGLDisplay, EGLConfig, ...)
 *   SCREEN_PROPERTY_ID_STRING "98"        KD_WINDOWPROPERTY_CAPTION       (69)
 *   screen_manage_window(group)           (nothing - the DM adopts by itself)
 *   SCREEN_PROPERTY_SIZE                  KD_WINDOWPROPERTY_SIZE          (66)
 *   TRANSPARENCY_SOURCE_OVER              KD_WINDOWPROPERTY_SOURCE_ALPHA_QNX (1011)
 *   screen_create_window_buffers          kdRealizeWindow -> EGLNativeWindowType
 *
 * CAPTION, not ID_STRING.  Etap 7a used KD_WINDOWPROPERTY_ID_STRING_QNX (1002)
 * and KD_WINDOWPROPERTY_MANAGED_QNX (1013) and the window was never adopted:
 * neither string occurs anywhere in this unit's /eso/bin/apps/displaymanager.
 * The caption carries the displayable number as a decimal string; the DM
 * atoi()s it and checks it against nothing.  Full evidence, from our own
 * binary and from an independent IDA pass over the same 583837 B build, is in
 * docs/19_displayable_na_mhi2.md.
 *
 * Worked examples of this exact sequence ship in the car's own firmware:
 * /eso/bin/apps/loadandshowimage and eso/lib/libdisplayinit.so (both
 * kdCreateWindow -> CAPTION -> SIZE -> kdRealizeWindow -> EGL -> GLES2).
 *
 * Video streams are a DIFFERENT channel: NvSS declares its number through
 * KD_STREAMPROPERTY_DISPLAYABLE_ID_NV, which is why vc_render --layer 58 has
 * worked since Etap 3.  Two sources, two mechanisms; do not mix them up again.
 *
 * ORDER MATTERS, and the opposite way round to Luka: KD takes the EGLDisplay
 * and EGLConfig as arguments, so EGL must be up FIRST and the window second.
 * On his Adreno the window had to exist before eglGetDisplay; here that would
 * not even compile.
 *
 * What is still unknown until this runs in the car is the GEOMETRY: whether the
 * DM gives an id it has no position entry for a position at all, or parks it at
 * (0,0).  Every step logs its result so one COLLECT answers that.
 */

#ifndef K3346_KD_SURFACE_H
#define K3346_KD_SURFACE_H

#include <EGL/egl.h>

typedef struct kd_surface kd_surface_t;

typedef struct {
    int id;             /* CAPTION value = DisplayManager displayable number.
                         * Must be > 0: "0" and non-numbers make the DM store
                         * id 0, "-1" makes it ignore the window entirely. */
    int width, height;
    int display;        /* KD display index; < 0 = do not set it (stock recipe) */
    int alpha;          /* 1 = per-source alpha so we blend over the map */
    int managed;        /* IGNORED on this unit - no MANAGED_QNX in its DM.
                         * Kept so an older conf file still parses cleanly. */
    int swap_interval;
} kd_surface_cfg;

/* Resolve libKD + the NV extension and call kdInitializeNV once.
 * 0 = ok.  Must run before any other call here and before EGL. */
int  kd_surface_global_init(void);

/* Create + configure + realize a window.  NULL on failure. */
kd_surface_t *kd_surface_create(const kd_surface_cfg *cfg,
                                EGLDisplay dpy, EGLConfig cfg_egl);

/* Native handle for eglCreateWindowSurface. */
EGLNativeWindowType kd_surface_native(kd_surface_t *s);

/* Cheap health probe: 1 = window lost or no longer ours -> caller tears down
 * its EGL surface, calls kd_surface_recreate(), rebinds.
 * Deliberately conservative: on this DM the VISIBILITY property cannot be read
 * back at all, and reporting "lost" on a failed read cost us a whole car visit
 * (see kd_surface.c).  Only a successful read of 0 counts as disowned. */
int  kd_surface_lost(kd_surface_t *s);

/* Destroy and open a fresh window with the same cfg.  100 ms backoff.  0 = ok. */
int  kd_surface_recreate(kd_surface_t *s);

/* Hide the window so the DisplayManager slot vacates promptly. */
void kd_surface_hide(kd_surface_t *s);

/* Destroy the window and free the handle. */
void kd_surface_destroy(kd_surface_t *s);

#endif /* K3346_KD_SURFACE_H */
