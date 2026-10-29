/*
 * overlay_ready — the renderer's own "the window was adopted" report.
 *
 * WHY THIS EXISTS AT ALL.  mib2high-lvds-control.sh must not switch the cluster
 * to our overlay context before the overlay really has a window, or the cluster
 * would be told to composite a displayable that does not exist.  Etap 7a asked
 * `dmdt gd` for that proof and got zero bytes back — and we now know that was
 * never going to work: the DisplayManager stores the displayable number ON THE
 * WINDOW (from its caption) and does not register it in any list `dmdt gd`
 * prints.  So the only process that can honestly answer "is the window up" is
 * this one.  See docs/19_displayable_na_mhi2.md.
 *
 * The file is written only after a frame has actually been presented, so its
 * presence means "window realized AND eglSwapBuffers returned EGL_TRUE", not
 * merely "the process started".
 *
 * Header-only, like overlay_cfg.h, so the host test links it without a build
 * rule and the renderer and the probe share one implementation.
 */

#ifndef K3346_OVERLAY_READY_H
#define K3346_OVERLAY_READY_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define OVERLAY_READY_PATH "/tmp/vc-overlay-ready"

/* The renderer writes the report and never reads it back; the host test and the
 * probe do.  Marked so the renderer's build stays warning-free without making
 * the parse live in two places. */
#ifdef __GNUC__
#  define OVERLAY_READY_MAYBE_UNUSED __attribute__((unused))
#else
#  define OVERLAY_READY_MAYBE_UNUSED
#endif

/* Write "<id> <width> <height>\n".  Returns 0 on success.
 *
 * WHY NOT write-to-.tmp-then-rename().  That is what this did until the car
 * run of 2026-10-27, and on the car it failed every time:
 *
 *     platform_harman: cannot write /tmp/vc-overlay-ready errno=18
 *
 * errno 18 is EXDEV.  /tmp itself is perfectly writable — every log this
 * project collects lives there — so it was rename() alone that refused: on
 * QNX /tmp is /dev/shmem, and that filesystem does not implement rename().
 * The gate then never saw a report and kept the cluster on the map context,
 * which is why the first full-overlay visit produced no arrow.
 *
 * One open() + one write() of a line this short is a single filesystem
 * operation, so a reader sees either nothing or the whole line; and the reader
 * validates what it gets anyway (overlay_ready_read below, and the same parse
 * in mib2high-lvds-control.sh).  O_TRUNC rather than unlink-first so the file
 * never briefly disappears while the gate is polling for it. */
static int overlay_ready_write(const char *path, int id, int w, int h)
{
    char buf[64];
    int fd, n, written = 0;

    if (!path || !path[0]) return -1;

    n = snprintf(buf, sizeof(buf), "%d %d %d\n", id, w, h);
    if (n < 0 || (size_t)n >= sizeof(buf)) return -1;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;

    while (written < n) {
        int got = (int)write(fd, buf + written, (size_t)(n - written));
        if (got <= 0) { close(fd); return -1; }
        written += got;
    }
    if (close(fd) != 0) return -1;
    return 0;
}

/* Remove the report.  Called when the window goes away (recreate, shutdown) so
 * the shell never keeps trusting a stale marker.  The ".tmp" sweep is kept for
 * the one left behind by the rename() version described above. */
static void overlay_ready_clear(const char *path)
{
    char tmp[256];
    int n;

    if (!path || !path[0]) return;
    remove(path);
    n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n > 0 && (size_t)n < sizeof(tmp)) remove(tmp);
}

/* Read a report back.  Returns 1 and fills the out params when the file holds
 * three sane integers, 0 otherwise.  Used by the host test and available to
 * anything else that wants the same parse as the shell. */
static int overlay_ready_read(const char *path, int *id, int *w, int *h)
    OVERLAY_READY_MAYBE_UNUSED;
static int overlay_ready_read(const char *path, int *id, int *w, int *h)
{
    FILE *f;
    int a = 0, b = 0, c = 0, got;

    if (!path || !path[0]) return 0;
    f = fopen(path, "r");
    if (!f) return 0;
    got = fscanf(f, "%d %d %d", &a, &b, &c);
    fclose(f);
    if (got != 3) return 0;
    if (a < 1 || a > 109) return 0;
    if (b < 16 || c < 16) return 0;
    if (id) *id = a;
    if (w)  *w  = b;
    if (h)  *h  = c;
    return 1;
}

#endif /* K3346_OVERLAY_READY_H */
