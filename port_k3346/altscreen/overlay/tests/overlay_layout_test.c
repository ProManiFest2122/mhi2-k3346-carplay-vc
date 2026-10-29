/*
 * Host test for overlay_layout.h - which VC gauge layout is on screen.
 *
 * Until 2026-10-28 this test exercised a parser for a log line the hook never
 * wrote (V8H_LAYOUT is LOG_INFO, the hook logs WARN and up), and passed every
 * time - while in the car the tile never followed VIEW once.  So this time
 * the fixtures are not made up: they are lines copied byte for byte from the
 * car's own DSITracer capture (collected/run-30, v8l-dsitracer-1798-00.log),
 * and the reader is run against real files that rotate, grow, get truncated
 * and end mid-line, the way the capture ring does.
 *
 * Built with the container's native gcc:  bash overlay/tests/run_host_tests.sh
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <sys/types.h>

#include "../overlay_layout.h"

static int g_pass, g_fail;

static void check_int(int got, int want, const char *what)
{
    if (got == want) g_pass++;
    else { g_fail++; printf("  FAIL: %s (got %d, want %d)\n", what, got, want); }
}

/* Straight from the car, 2026-10-28. */
#define L_NOISE   "46.323 TelnetAppender v1.0 (2 active connections)"
#define L_ZOOM460 "49.739 1#org.dsi.ifc.map.DSIMapViewerZoomEngine#setZoomArea#49656|{org.dsi.ifc.map.Rect:490:49:460:300}"
#define L_ZOOM460_DUP "49.739 3#org.dsi.ifc.map.DSIMapViewerControl#setZoomArea#49655|{org.dsi.ifc.map.Rect:490:49:460:300}"
#define L_BAP_SMALL "52.424 2#org.dsi.ifc.bap.DSIBAPListener#indication#52245|int:50|int:54|int:0|int:0|int:1"
#define L_BAP_SMALL_DUP "52.471 3#org.dsi.ifc.bap.DSIBAPListener#indication#52251|int:50|int:54|int:0|int:0|int:1"
#define L_ZOOM700 "52.446 1#org.dsi.ifc.map.DSIMapViewerZoomEngine#setZoomArea#52249|{org.dsi.ifc.map.Rect:370:49:700:300}"
#define L_BAP_BIG "65.295 2#org.dsi.ifc.bap.DSIBAPListener#indication#65155|int:50|int:54|int:0|int:0|int:0"

/* One VIEW press from the car, 2026-10-28 (run-31): the 12# listener got the
 * key ~190 ms before 1#, and the BAP 54 report came ~575 ms after the key. */
#define K_PRESS_12  "89.326 12#org.dsi.ifc.keypanel.DSIKeyPanelListener#updateKey2#89324|int:4|int:35|int:1|int:0|int:1"
#define K_PRESS_1   "89.515 1#org.dsi.ifc.keypanel.DSIKeyPanelListener#updateKey2#89324|int:4|int:35|int:1|int:0|int:1"
#define K_RELEASE   "89.715 12#org.dsi.ifc.keypanel.DSIKeyPanelListener#updateKey2#89523|int:4|int:35|int:0|int:0|int:1"
#define K_BAP_AFTER "89.901 2#org.dsi.ifc.bap.DSIBAPListener#indication#89771|int:50|int:54|int:0|int:0|int:1"
#define K_OTHER_KEY "76.214 1#org.dsi.ifc.keypanel.DSIKeyPanelListener#updateKey2#76022|int:4|int:36|int:1|int:0|int:1"
#define K_ENCODER   "154.150 12#org.dsi.ifc.keypanel.DSIKeyPanelListener#updateEncoder2#154149|int:1|int:16|int:1|int:0|int:1"

static int parse(const char *line, int *slot)
{
    *slot = -9;
    return overlay_layout_parse_line(line, slot);
}

static void t_parse_real_lines(void)
{
    int s;
    printf("lines from the car's own capture\n");
    check_int(parse(L_BAP_SMALL, &s), OVERLAY_LAYOUT_SRC_BAP54, "BAP 54 = 1 is gauge state");
    check_int(s, 0, "BAP 54 = 1 -> small clocks, slot 0");
    check_int(parse(L_BAP_BIG, &s), OVERLAY_LAYOUT_SRC_BAP54, "BAP 54 = 0 is gauge state");
    check_int(s, 1, "BAP 54 = 0 -> big clocks, slot 1");
    check_int(parse(L_BAP_SMALL_DUP, &s), OVERLAY_LAYOUT_SRC_NONE, "3# duplicate is ignored");
    check_int(parse(L_ZOOM460, &s), OVERLAY_LAYOUT_SRC_ZOOM, "1# zoom area counts");
    check_int(s, 1, "map 460 wide -> big clocks");
    check_int(parse(L_ZOOM700, &s), OVERLAY_LAYOUT_SRC_ZOOM, "1# zoom area counts");
    check_int(s, 0, "map 700 wide -> small clocks");
    check_int(parse(L_ZOOM460_DUP, &s), OVERLAY_LAYOUT_SRC_NONE, "3# DSIMapViewerControl is ignored");
    check_int(parse(L_NOISE, &s), OVERLAY_LAYOUT_SRC_NONE, "noise is not evidence");
}

static void t_parse_edges(void)
{
    int s;
    printf("the same records without the capture's timestamp, and odd ones\n");
    check_int(parse("2#org.dsi.ifc.bap.DSIBAPListener#indication#65155|int:50|int:54|int:0|int:0|int:0", &s),
              OVERLAY_LAYOUT_SRC_BAP54, "live-stream form (no timestamp)");
    check_int(s, 1, "still slot 1");
    check_int(parse("2#org.dsi.ifc.bap.DSIBAPListener#indication#65155|int:50|int:54|int:0|int:0|int:1@", &s),
              OVERLAY_LAYOUT_SRC_BAP54, "'@' record terminator accepted");
    check_int(parse("70.1 2#org.dsi.ifc.bap.DSIBAPListener#indication#70|int:50|int:53|int:0|int:0|int:1", &s),
              OVERLAY_LAYOUT_SRC_NONE, "FctID 53 is not the gauge state");
    check_int(parse("70.1 1#org.dsi.ifc.map.DSIMapViewerZoomEngine#setZoomArea#7|{org.dsi.ifc.map.Rect:0:0:900:300}", &s),
              OVERLAY_LAYOUT_SRC_NONE, "a 900-wide map is no layout we know");
    check_int(parse("52.424x 2#org.dsi.ifc.bap.DSIBAPListener#indication#1|int:50|int:54|int:0|int:0|int:1", &s),
              OVERLAY_LAYOUT_SRC_NONE, "a malformed timestamp is not skipped");
    check_int(parse("", &s), OVERLAY_LAYOUT_SRC_NONE, "empty line");
    check_int(overlay_layout_parse_line(NULL, &s), OVERLAY_LAYOUT_SRC_NONE, "NULL line");
}

static void t_policy(void)
{
    overlay_layout_reader_t r;
    printf("BAP 54 outranks the zoom area, as in the hook\n");
    overlay_layout_reader_init(&r, "/nonexistent/%02d", 4);
    check_int(r.slot, OVERLAY_LAYOUT_UNKNOWN, "starts unknown");
    overlay_layout_apply(&r, OVERLAY_LAYOUT_SRC_ZOOM, 1);
    check_int(r.slot, 1, "zoom area counts before any BAP 54");
    check_int(r.source, OVERLAY_LAYOUT_SRC_ZOOM, "and says so");
    overlay_layout_apply(&r, OVERLAY_LAYOUT_SRC_BAP54, 0);
    check_int(r.slot, 0, "BAP 54 overrides it");
    overlay_layout_apply(&r, OVERLAY_LAYOUT_SRC_ZOOM, 1);
    check_int(r.slot, 0, "zoom area no longer counts after BAP 54");
    overlay_layout_apply(&r, OVERLAY_LAYOUT_SRC_BAP54, 1);
    check_int(r.slot, 1, "the next BAP 54 does");
    overlay_layout_apply(&r, OVERLAY_LAYOUT_SRC_NONE, 0);
    check_int(r.slot, 1, "a line that says nothing changes nothing");
}

static void t_feed_partial(void)
{
    overlay_layout_reader_t r;
    const char whole[] = L_BAP_BIG "\n";
    const char both[] = L_BAP_BIG "\n" "65.9 2#org.dsi.ifc.bap.DSIBAPListener#indication#1|int:50|int:54|int:0|int:0|int:1";
    printf("a line the capture is still writing is not read yet\n");
    overlay_layout_reader_init(&r, "/nonexistent/%02d", 4);
    check_int((int)overlay_layout_feed(&r, both, strlen(both)), (int)strlen(whole),
              "consumed up to the last newline only");
    check_int(r.slot, 1, "the whole line counted");
    check_int((int)overlay_layout_feed(&r, L_BAP_SMALL "\r\n", strlen(L_BAP_SMALL "\r\n")),
              (int)strlen(L_BAP_SMALL "\r\n"), "CRLF line consumed");
    check_int(r.slot, 0, "and parsed");
}

/* ---- the ring itself, on real files ---------------------------------- */

static char g_fmt[96];
static char g_path[128];

static const char *ring_path(int i)
{
    snprintf(g_path, sizeof(g_path), g_fmt, i);
    return g_path;
}

static void ring_write(int i, const char *mode, const char *text)
{
    FILE *f = fopen(ring_path(i), mode);
    if (!f) { perror("fopen"); exit(2); }
    fputs(text, f);
    fclose(f);
}

/* mtime has one-second resolution; the tests set it explicitly instead of
 * sleeping, so ordering is decided by the test, not by the clock. */
static void ring_mtime(int i, long t)
{
    struct utimbuf u;
    u.actime = u.modtime = (time_t)t;
    if (utime(ring_path(i), &u) != 0) { perror("utime"); exit(2); }
}

static void ring_clear(void)
{
    int i;
    for (i = 0; i < OVERLAY_LAYOUT_RING_COUNT; i++) unlink(ring_path(i));
}

static void t_ring_session(void)
{
    overlay_layout_reader_t r;
    printf("one capture file, the car's own sequence\n");
    ring_clear();
    ring_write(0, "w", L_NOISE "\n" L_ZOOM460 "\n" L_BAP_SMALL "\n" L_ZOOM700 "\n" L_BAP_BIG "\n");
    overlay_layout_reader_init(&r, g_fmt, OVERLAY_LAYOUT_RING_COUNT);
    check_int(overlay_layout_reader_poll(&r), 1, "last BAP 54 = 0 -> big clocks");
    check_int(r.source, OVERLAY_LAYOUT_SRC_BAP54, "from BAP 54");

    printf("VIEW pressed while the renderer runs\n");
    ring_write(0, "a", "70.000 2#org.dsi.ifc.bap.DSIBAPListener#indication#1|int:50|int:54|int:0|int:0|int:1\n");
    check_int(overlay_layout_reader_poll(&r), 0, "next poll sees small clocks");
    ring_write(0, "a", "71.000 2#org.dsi.ifc.bap.DSIBAPListener#indication#2|int:50|int:54|int:0|int:0|int:0");
    check_int(overlay_layout_reader_poll(&r), 0, "half-written record is not read yet");
    ring_write(0, "a", "\n");
    check_int(overlay_layout_reader_poll(&r), 1, "read once it is complete");
}

static void t_ring_rotation(void)
{
    overlay_layout_reader_t r;
    int i;
    printf("the capture rotates to the next file\n");
    ring_clear();
    ring_write(0, "w", L_BAP_SMALL "\n");
    ring_mtime(0, 1000);
    overlay_layout_reader_init(&r, g_fmt, OVERLAY_LAYOUT_RING_COUNT);
    check_int(overlay_layout_reader_poll(&r), 0, "following 00");

    ring_write(1, "w", L_BAP_BIG "\n");
    ring_mtime(1, 1001);
    check_int(overlay_layout_reader_poll(&r), 1, "moved to the younger 01");
    check_int(r.cur, 1, "and follows it");

    ring_write(2, "w", L_BAP_SMALL "\n");
    ring_mtime(2, 500);
    check_int(overlay_layout_reader_poll(&r), 1, "a stale 02 from an earlier cycle is not live");
    check_int(r.cur, 1, "still on 01");

    for (i = 0; i < 50; i++) ring_write(1, "a", L_NOISE "\n");
    ring_mtime(1, 1001);
    ring_write(2, "w", L_BAP_SMALL "\n");
    ring_mtime(2, 1001);
    check_int(overlay_layout_reader_poll(&r), 0,
              "same-second rotation: the smaller file is the fresh one");
    check_int(r.cur, 2, "now on 02");
}

static void t_ring_start_order(void)
{
    overlay_layout_reader_t r;
    printf("at start the newest file wins, whatever its index\n");
    ring_clear();
    ring_write(2, "w", L_BAP_SMALL "\n"); ring_mtime(2, 100);
    ring_write(0, "w", L_BAP_SMALL "\n"); ring_mtime(0, 200);
    ring_write(1, "w", L_BAP_BIG "\n");   ring_mtime(1, 300);
    overlay_layout_reader_init(&r, g_fmt, OVERLAY_LAYOUT_RING_COUNT);
    check_int(overlay_layout_reader_poll(&r), 1, "01 is newest -> big clocks");
    check_int(r.cur, 1, "and is the one followed");

    ring_mtime(2, 400);
    overlay_layout_reader_init(&r, g_fmt, OVERLAY_LAYOUT_RING_COUNT);
    check_int(overlay_layout_reader_poll(&r), 0, "02 made newest -> small clocks");
}

static void t_ring_missing_and_truncated(void)
{
    overlay_layout_reader_t r;
    printf("no ring yet, a ring that appears, a file reused by a new session\n");
    ring_clear();
    overlay_layout_reader_init(&r, g_fmt, OVERLAY_LAYOUT_RING_COUNT);
    check_int(overlay_layout_reader_poll(&r), OVERLAY_LAYOUT_UNKNOWN, "nothing to read -> unknown");
    ring_write(0, "w", L_NOISE "\n" L_NOISE "\n" L_NOISE "\n" L_BAP_BIG "\n");
    check_int(overlay_layout_reader_poll(&r), 1, "picked up once it exists");
    ring_write(0, "w", L_BAP_SMALL "\n");
    check_int(overlay_layout_reader_poll(&r), 0, "shorter rewrite read from the top");

    printf("a session that so far only has the zoom area\n");
    ring_clear();
    ring_write(0, "w", L_NOISE "\n" L_ZOOM700 "\n");
    overlay_layout_reader_init(&r, g_fmt, OVERLAY_LAYOUT_RING_COUNT);
    check_int(overlay_layout_reader_poll(&r), 0, "zoom 700 -> small clocks");
    check_int(r.source, OVERLAY_LAYOUT_SRC_ZOOM, "and says it came from the zoom area");
    check_int(strcmp(overlay_layout_src_name(r.source), "zoomarea") == 0, 1, "named for the log");
}

static void t_origin(void)
{
    int x = -1, y = -1;
    printf("slot -> stock tile origin, as in the conf\n");
    check_int(overlay_layout_origin(1, 1055, 207, 1091, 110, &x, &y), 1, "slot 1 known");
    check_int(x, 1055, "big clocks x"); check_int(y, 207, "big clocks y");
    check_int(overlay_layout_origin(0, 1055, 207, 1091, 110, &x, &y), 1, "slot 0 known");
    check_int(x, 1091, "small clocks x"); check_int(y, 110, "small clocks y");
    x = y = -7;
    check_int(overlay_layout_origin(OVERLAY_LAYOUT_UNKNOWN, 1055, 207, 1091, 110, &x, &y), 0,
              "unknown slot");
    check_int(x, -7, "leaves the caller's default alone");
}

static void t_view_key(void)
{
    unsigned long ts = 0;
    printf("the VIEW key, from the car's own trace\n");
    check_int(overlay_layout_parse_view_key(K_PRESS_12, &ts), 1, "press via 12#");
    check_int((int)ts, 89324, "tracer timestamp of the press");
    check_int(overlay_layout_parse_view_key(K_PRESS_1, &ts), 1, "same press via 1#");
    check_int(overlay_layout_parse_view_key(K_RELEASE, &ts), 1, "release counts too");
    check_int((int)ts, 89523, "tracer timestamp of the release");
    check_int(overlay_layout_parse_view_key(K_OTHER_KEY, &ts), 0, "key 36 is not VIEW");
    check_int(overlay_layout_parse_view_key(K_ENCODER, &ts), 0, "the roller is not VIEW");
    check_int(overlay_layout_parse_view_key(K_BAP_AFTER, &ts), 0, "a BAP record is not a key");
    check_int(overlay_layout_parse_view_key(
        "1.0 1#org.dsi.ifc.keypanel.DSIKeyPanelListener#updateKey2#5|int:1|int:35|int:1|int:0|int:1", &ts),
        0, "key 35 on another device is not VIEW");
    check_int(overlay_layout_parse_view_key(NULL, &ts), 0, "NULL line");
    check_int((int)overlay_layout_bap_ts(K_BAP_AFTER), 89771, "BAP record's tracer timestamp");
    check_int((int)overlay_layout_bap_ts(K_PRESS_12), 0, "a key record has no BAP timestamp");
}

static void feed_line(overlay_layout_reader_t *r, const char *line)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "%s\n", line);
    overlay_layout_feed(r, buf, strlen(buf));
}

static void t_view_pending(void)
{
    overlay_layout_reader_t r;
    printf("VIEW holds the tile until the cluster reports the layout\n");
    overlay_layout_reader_init(&r, "/nonexistent/%02d", 4);
    feed_line(&r, L_BAP_BIG);
    check_int(r.view_pending, 0, "nothing pending at the start");
    feed_line(&r, K_PRESS_12);
    check_int(r.view_pending, 1, "press -> pending");
    feed_line(&r, K_PRESS_1);
    feed_line(&r, K_RELEASE);
    check_int(r.view_pending, 1, "duplicates and release keep it pending");
    check_int(r.slot, 1, "the layout is still the old one");
    feed_line(&r, K_BAP_AFTER);
    check_int(r.view_pending, 0, "the report ends it");
    check_int(r.slot, 0, "and carries the new layout");
    feed_line(&r, K_RELEASE);
    check_int(r.view_pending, 0, "a late copy of the answered press is ignored");
    feed_line(&r, "93.215 1#org.dsi.ifc.keypanel.DSIKeyPanelListener#updateKey2#93100|int:4|int:35|int:1|int:0|int:1");
    check_int(r.view_pending, 1, "the next real press holds again");
    feed_line(&r, K_OTHER_KEY);
    check_int(r.view_pending, 1, "another key changes nothing");
}

static void t_ring_view(void)
{
    overlay_layout_reader_t r;
    printf("the same press arriving through the ring file\n");
    ring_clear();
    ring_write(0, "w", L_BAP_BIG "\n");
    overlay_layout_reader_init(&r, g_fmt, OVERLAY_LAYOUT_RING_COUNT);
    check_int(overlay_layout_reader_poll(&r), 1, "big clocks to begin with");
    check_int(r.view_pending, 0, "nothing pending");
    ring_write(0, "a", K_PRESS_12 "\n");
    overlay_layout_reader_poll(&r);
    check_int(r.view_pending, 1, "key seen on the next poll");
    check_int(overlay_layout_reader_poll(&r), 1, "a poll with nothing new changes nothing");
    check_int(r.view_pending, 1, "and keeps it pending");
    ring_write(0, "a", K_RELEASE "\n" K_BAP_AFTER "\n");
    check_int(overlay_layout_reader_poll(&r), 0, "report -> small clocks");
    check_int(r.view_pending, 0, "hold over");
}

int main(void)
{
    snprintf(g_fmt, sizeof(g_fmt), "/tmp/ovl_ring_%ld_%%02d.log", (long)getpid());

    t_parse_real_lines();
    t_parse_edges();
    t_policy();
    t_feed_partial();
    t_ring_session();
    t_ring_rotation();
    t_ring_start_order();
    t_ring_missing_and_truncated();
    t_origin();
    t_view_key();
    t_view_pending();
    t_ring_view();
    ring_clear();

    printf("\noverlay_layout: %d OK, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
