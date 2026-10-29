/*
 * overlay_layout — which VC gauge layout is on screen right now.
 *
 * The maneuver tile sits in a different place in each layout the driver picks
 * with VIEW.  Measured from `dmdt ts 4` screenshots on 2026-10-26, and labelled
 * correctly only on 2026-10-28 (see "THE LABELS" below):
 *
 *     slot 1  big clocks     tile at (1055, 207)  - inside the right gauge
 *     slot 0  small clocks   tile at (1091, 110)  - top right of the map
 *
 * WHERE THE LAYOUT COMES FROM.  The Etap 3 hook already knows it - from the
 * DSITracer stream on port 1798 - but it only ever said so through LOG_INFO
 * ("V8H_LAYOUT slot=", native/altscreen_session.c:1391), and its logger drops
 * everything below LOG_LEVEL_WARN (native/framework/logging.h:57).  So the line
 * this header used to read was never in the file.  Every car run up to
 * 2026-10-28 had the tile in ONE place in both layouts; VIEW never moved it.
 *
 * Now it reads the same evidence the hook reads, from the copy of that stream
 * mib2high-lvds-control.sh already keeps for every session: the v8l capture
 * ring /tmp/v8l-dsitracer-1798-00..03.log (diagnostics/v8l_dsitracer_capture.c,
 * 4 files x 512 KB, opened in turn with "w", one line per record, each line
 * prefixed "<seconds>.<millis> ").  Two kinds of record, in the hook's order of
 * authority (altscreen_session.c:1316-1372):
 *
 *   1. BAP service 50, FctID 54, LIVE 2# listener only - the VC's own gauge
 *      state, sent at session start and on every VIEW.  Parsed by the hook's
 *      own native/gauge_bap54.h, included rather than copied, so both readers
 *      can never disagree on what the record means.
 *   2. 1#...DSIMapViewerZoomEngine#setZoomArea#...Rect:x:y:w:h - the stock map
 *      area: 460 wide with big clocks, 700 with small.  Used only until the
 *      first BAP 54 record, exactly as the hook does.
 *
 * Why the file and not port 1798 itself: the VC states the layout once at
 * session start and then only on VIEW.  On 2026-10-28 that record arrived at
 * 52.424 s and the renderer started at 52.464 s - a live client would have
 * missed it and waited for the next VIEW.  The ring keeps about 50 minutes.
 *
 * THE LABELS.  The 2026-10-26 probe told the driver to "switch VIEW to the
 * other layout" without recording which one it started in, and the first
 * screenshot was labelled "big clocks" by assumption.  Geometry says the
 * opposite: with small clocks the small speedometer starts around y=296 in
 * the bottom right, so a tile ending at y=360 would cover it, and on the car
 * photo of 2026-10-28 the tile at (1091,110) sits correctly above it; with big
 * clocks (1091,110) lies in the top half of the right gauge, under the VC's
 * own distance readout, which is where the arrow disappeared.
 */

#ifndef K3346_OVERLAY_LAYOUT_H
#define K3346_OVERLAY_LAYOUT_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../native/gauge_bap54.h"

#define OVERLAY_LAYOUT_RING_FMT    "/tmp/v8l-dsitracer-1798-%02d.log"
#define OVERLAY_LAYOUT_RING_COUNT  4
#define OVERLAY_LAYOUT_CHUNK       8192       /* read granularity */
#define OVERLAY_LAYOUT_POLL_MAX    (256L * 1024L)  /* bytes per poll, bounded */

#define OVERLAY_LAYOUT_UNKNOWN (-1)

/* Where a slot came from - logged, because the two differ in authority. */
#define OVERLAY_LAYOUT_SRC_NONE   0
#define OVERLAY_LAYOUT_SRC_BAP54  1
#define OVERLAY_LAYOUT_SRC_ZOOM   2

static const char *overlay_layout_src_name(int src)
{
    return src == OVERLAY_LAYOUT_SRC_BAP54 ? "bap54" :
           src == OVERLAY_LAYOUT_SRC_ZOOM  ? "zoomarea" : "none";
}

/* The capture prefixes "<digits>.<digits> ".  Returns the record behind it,
 * or the line itself when it came straight from the tracer without one. */
static const char *overlay_layout_skip_stamp(const char *line)
{
    const char *q = line;

    while (*q >= '0' && *q <= '9') q++;
    if (q != line && *q == '.') {
        q++;
        while (*q >= '0' && *q <= '9') q++;
        if (*q == ' ') return q + 1;
    }
    return line;
}

/* One line of the capture ring (or of the hook's live stream, which has no
 * timestamp) -> layout evidence.  Returns the source and sets *slot, or
 * OVERLAY_LAYOUT_SRC_NONE when the line says nothing about the layout. */
static int overlay_layout_parse_line(const char *line, int *slot)
{
    const char *p, *q;
    unsigned int raw = 0;
    int s, x, y, w, h;

    if (!line || !slot) return OVERLAY_LAYOUT_SRC_NONE;
    p = overlay_layout_skip_stamp(line);

    if (altscreen_bap54_layout_slot(p, &s, &raw)) {
        *slot = s;
        return OVERLAY_LAYOUT_SRC_BAP54;
    }

    /* The hook looks at the zoom area only on 1# records (3# is the
     * DSIMapViewerControl duplicate) and maps width the same way. */
    if (p[0] != '1' || p[1] != '#') return OVERLAY_LAYOUT_SRC_NONE;
    q = strstr(p, "DSIMapViewerZoomEngine#setZoomArea#");
    if (!q) return OVERLAY_LAYOUT_SRC_NONE;
    q = strstr(q, "Rect:");
    if (!q || sscanf(q + 5, "%d:%d:%d:%d", &x, &y, &w, &h) != 4)
        return OVERLAY_LAYOUT_SRC_NONE;
    s = (w <= 560) ? 1 : ((w <= 800) ? 0 : -1);
    if (s < 0) return OVERLAY_LAYOUT_SRC_NONE;
    *slot = s;
    return OVERLAY_LAYOUT_SRC_ZOOM;
}

/* The VIEW button on the steering wheel.  The VC changes layout on the key;
 * its BAP 54 report follows about 575 ms later (2026-10-28: 9 of 10 presses
 * 572-593 ms from the first key record, once 392 ms).  Seeing the key is how
 * the tile can get out of the way BEFORE it would sit in the old layout's
 * place.  Device 4 key 35, confirmed 1:1 against BAP 54 over a whole drive
 * (10 presses, 10 layout changes); keys 36/37 never changed the layout.
 * Any listener counts: 12# delivered the press ~190 ms before 1# did.
 * Press and release both count.  *ts gets the tracer's own timestamp. */
#define OVERLAY_LAYOUT_VIEW_DEVICE 4
#define OVERLAY_LAYOUT_VIEW_KEY    35

static int overlay_layout_parse_view_key(const char *line, unsigned long *ts)
{
    static const char tag[] = "#org.dsi.ifc.keypanel.DSIKeyPanelListener#updateKey2#";
    const char *p, *q;
    unsigned long t = 0;
    int dev = -1, key = -1;

    if (!line) return 0;
    p = overlay_layout_skip_stamp(line);
    q = p;
    while (*q >= '0' && *q <= '9') q++;               /* listener number */
    if (q == p || strncmp(q, tag, sizeof(tag) - 1) != 0) return 0;
    if (sscanf(q + sizeof(tag) - 1, "%lu|int:%d|int:%d|", &t, &dev, &key) != 3)
        return 0;
    if (dev != OVERLAY_LAYOUT_VIEW_DEVICE || key != OVERLAY_LAYOUT_VIEW_KEY) return 0;
    if (ts) *ts = t;
    return 1;
}

/* The tracer's own timestamp of a BAP indication record, 0 if none.  Key and
 * BAP records carry the same clock, which is what lets a late duplicate of a
 * key event be told apart from a new press. */
static unsigned long overlay_layout_bap_ts(const char *line)
{
    static const char tag[] = "DSIBAPListener#indication#";
    const char *p = line ? strstr(line, tag) : NULL;
    unsigned long t = 0;

    if (p) sscanf(p + sizeof(tag) - 1, "%lu", &t);
    return t;
}

/* Follows the capture ring.  All state lives here so the host test can run
 * several readers side by side on files of its own. */
typedef struct {
    char fmt[128];      /* ring path pattern, one %02d for the index */
    int  count;         /* files in the ring */
    int  slot;          /* current slot, or OVERLAY_LAYOUT_UNKNOWN */
    int  source;        /* OVERLAY_LAYOUT_SRC_* that set it */
    int  bap54_known;   /* once set, zoom area no longer counts (hook policy) */
    int  started;       /* the one-off scan of the whole ring has run */
    int  cur;           /* ring index being followed, -1 = none yet */
    long cur_off;       /* bytes of it consumed - whole lines only */
    /* VIEW pressed and the VC has not reported the new layout yet.  Set by a
     * VIEW key record, cleared by the next BAP 54 record. */
    int  view_pending;
    unsigned long last_bap_ts;   /* tracer time of the last BAP 54 record */
} overlay_layout_reader_t;

static void overlay_layout_reader_init(overlay_layout_reader_t *r,
                                       const char *fmt, int count)
{
    memset(r, 0, sizeof(*r));
    snprintf(r->fmt, sizeof(r->fmt), "%s", fmt ? fmt : OVERLAY_LAYOUT_RING_FMT);
    r->count  = (count > 0 && count <= 16) ? count : OVERLAY_LAYOUT_RING_COUNT;
    r->slot   = OVERLAY_LAYOUT_UNKNOWN;
    r->source = OVERLAY_LAYOUT_SRC_NONE;
    r->cur    = -1;
}

/* The hook's order of authority: BAP 54 always wins; the zoom area counts
 * only while no BAP 54 record has been seen. */
static void overlay_layout_apply(overlay_layout_reader_t *r, int src, int slot)
{
    if (src == OVERLAY_LAYOUT_SRC_BAP54) {
        r->bap54_known = 1;
        r->slot = slot;
        r->source = src;
    } else if (src == OVERLAY_LAYOUT_SRC_ZOOM && !r->bap54_known) {
        r->slot = slot;
        r->source = src;
    }
}

/* Feeds every complete line of buf[0..len) to the reader.  Returns the bytes
 * consumed, which ends right after the last newline: a line the capture is
 * still writing is left for the next read, never parsed half-written. */
static size_t overlay_layout_feed(overlay_layout_reader_t *r,
                                  const char *buf, size_t len)
{
    char line[512];
    size_t i, start = 0, consumed = 0;

    for (i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            size_t n = i - start;
            int s = -1, src;
            if (n > 0 && buf[start + n - 1] == '\r') n--;
            /* Long records are cut: the ones we read are short, and their
             * head is all the parser looks at. */
            if (n >= sizeof(line)) n = sizeof(line) - 1;
            memcpy(line, buf + start, n);
            line[n] = '\0';
            src = overlay_layout_parse_line(line, &s);
            overlay_layout_apply(r, src, s);
            if (src == OVERLAY_LAYOUT_SRC_BAP54) {
                r->view_pending = 0;
                r->last_bap_ts = overlay_layout_bap_ts(line);
            } else {
                unsigned long kts = 0;
                /* A key record older than the last report is a late copy of
                 * a press already answered - it must not hide the tile again. */
                if (overlay_layout_parse_view_key(line, &kts) && kts > r->last_bap_ts)
                    r->view_pending = 1;
            }
            start = i + 1;
            consumed = start;
        }
    }
    return consumed;
}

static void overlay_layout_path(const overlay_layout_reader_t *r, int i,
                                char *out, size_t n)
{
    snprintf(out, n, r->fmt, i);
}

static int overlay_layout_stat(const overlay_layout_reader_t *r, int i,
                               long *size, long *mtime)
{
    char path[160];
    struct stat st;

    overlay_layout_path(r, i, path, sizeof(path));
    if (stat(path, &st) != 0) return 0;
    *size  = (long)st.st_size;
    *mtime = (long)st.st_mtime;
    return 1;
}

/* Is ring file b younger than ring file a?  mtime has one-second resolution,
 * so a tie is decided by size: the capture truncates the file it rotates to,
 * which makes the fresh one the smaller of the two. */
static int overlay_layout_younger(long mt_b, long sz_b, long mt_a, long sz_a)
{
    return mt_b > mt_a || (mt_b == mt_a && sz_b < sz_a);
}

/* Reads whole lines of path from *off, at most budget bytes, and moves *off
 * past the last of them.  Returns 0 when the file cannot be opened. */
static int overlay_layout_read_file(overlay_layout_reader_t *r, const char *path,
                                    long *off, long budget)
{
    char buf[OVERLAY_LAYOUT_CHUNK];
    long done = 0;
    FILE *f = fopen(path, "rb");

    if (!f) return 0;
    if (fseek(f, *off, SEEK_SET) != 0) { fclose(f); return 1; }
    while (done < budget) {
        size_t got = fread(buf, 1, sizeof(buf), f), used;
        if (got == 0) break;
        used = overlay_layout_feed(r, buf, got);
        if (used == 0) {
            if (got < sizeof(buf)) break;  /* half a line at the end: wait */
            used = got;                    /* a line longer than a chunk: skip */
        }
        *off += (long)used;
        done += (long)used;
        if (used < got && fseek(f, *off, SEEK_SET) != 0) break;
    }
    fclose(f);
    return 1;
}

/* The one-off look at the whole ring, oldest file first, so the newest
 * evidence wins.  At most 4 x 512 KB, once per renderer start.  Files from an
 * earlier session are older and only matter when this one has no record yet:
 * then they give the layout the VC was last in, which is the best guess. */
static void overlay_layout_start(overlay_layout_reader_t *r)
{
    long sz[16], mt[16];
    int order[16], n = 0, i, j;

    for (i = 0; i < r->count; i++)
        if (overlay_layout_stat(r, i, &sz[i], &mt[i])) order[n++] = i;

    for (i = 1; i < n; i++) {                 /* insertion sort, oldest first */
        int k = order[i];
        for (j = i - 1; j >= 0 &&
             overlay_layout_younger(mt[order[j]], sz[order[j]], mt[k], sz[k]); j--)
            order[j + 1] = order[j];
        order[j + 1] = k;
    }
    for (i = 0; i < n; i++) {
        char path[160];
        long off = 0;
        overlay_layout_path(r, order[i], path, sizeof(path));
        overlay_layout_read_file(r, path, &off, 4L * 512L * 1024L);
        if (i == n - 1) { r->cur = order[i]; r->cur_off = off; }
    }
    r->started = 1;
}

/* Called about once a second.  Returns the current slot, or
 * OVERLAY_LAYOUT_UNKNOWN while nothing in the ring has said. */
static int overlay_layout_reader_poll(overlay_layout_reader_t *r)
{
    char path[160];
    long sz_c, mt_c, sz_n, mt_n;
    int next;

    if (!r->started || r->cur < 0) {
        overlay_layout_start(r);
        return r->slot;
    }
    if (!overlay_layout_stat(r, r->cur, &sz_c, &mt_c)) {
        r->started = 0;                       /* ring gone: look again later */
        return r->slot;
    }
    if (sz_c < r->cur_off) r->cur_off = 0;    /* reused by a new cycle */
    /* Polled ten times a second: when nothing was appended, a stat is all
     * this costs - the file is not even opened. */
    if (sz_c > r->cur_off) {
        overlay_layout_path(r, r->cur, path, sizeof(path));
        overlay_layout_read_file(r, path, &r->cur_off, OVERLAY_LAYOUT_POLL_MAX);
    }

    /* The capture moves to the next index in turn; it is live once it is
     * younger than the one we follow. */
    next = (r->cur + 1) % r->count;
    if (next != r->cur && overlay_layout_stat(r, next, &sz_n, &mt_n) &&
        overlay_layout_younger(mt_n, sz_n, mt_c, sz_c)) {
        long off = 0;
        overlay_layout_path(r, next, path, sizeof(path));
        overlay_layout_read_file(r, path, &off, OVERLAY_LAYOUT_POLL_MAX);
        r->cur = next;
        r->cur_off = off;
    }
    return r->slot;
}

/* The tile origin the stock HMI uses for a given slot, per the conf file.
 * Returns 1 when the slot is known, 0 when it is not (out params untouched). */
static int overlay_layout_origin(int slot, int x, int y, int x_alt, int y_alt,
                                 int *out_x, int *out_y)
{
    if (slot == 1)      { if (out_x) *out_x = x;     if (out_y) *out_y = y;     return 1; }
    else if (slot == 0) { if (out_x) *out_x = x_alt; if (out_y) *out_y = y_alt; return 1; }
    return 0;
}

#endif /* K3346_OVERLAY_LAYOUT_H */
