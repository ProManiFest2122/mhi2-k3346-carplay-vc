/*
 * K3346 Etap 7: test of the cluster-overlay wire format, on a PC (gcc, no car).
 *
 * The point of this test is NOT that our builder produces the bytes we expect.
 * It is that LUKA'S OWN DECODER, compiled from his unmodified
 * overlay/upstream/maneuver_command.h, reads those bytes back as the maneuver
 * we meant.  If the two ever drift apart, the renderer would silently draw the
 * wrong turn - which is exactly the failure Etap 4 already cost us once, when
 * the VC drew "straight" for every maneuver.
 *
 * Built and run by overlay/tests/run_host_tests.sh.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "routeguidance/overlay_feed.h"
#include "routeguidance/maneuver_map.h"
#include "protocol.h"
#include "maneuver.h"            /* ICON_* */
#include "maneuver_command.h"    /* cr_decode_maneuver - Luka's decoder */
#include "arrow_progress.h"      /* CR_PROGRESS_* */
#include "lane_guidance.h"       /* cr_lane_receive - Luka's lane decoder */

static int g_pass, g_fail;

#define CHECK(cond, what) do {                                   \
        if (cond) { g_pass++; }                                  \
        else { g_fail++; printf("  FAIL: %s\n", (what)); }       \
    } while (0)

#define CHECK_EQ(got, want, what) do {                                      \
        long _g = (long)(got), _w = (long)(want);                           \
        if (_g == _w) { g_pass++; }                                         \
        else { g_fail++;                                                    \
               printf("  FAIL: %s (got %ld, want %ld)\n", (what), _g, _w); } \
    } while (0)

/* ------------------------------------------------------------ fixtures -- */

static void mk_update(rgd_update_t *u, int dist_m)
{
    memset(u, 0, sizeof(*u));
    if (dist_m >= 0) {
        u->present |= RGD_UPD_DIST_TO_MANEUVER;
        u->dist_to_maneuver = (uint32_t)dist_m;
    }
}

static void mk_maneuver(rgd_maneuver_t *m, int type, int exit_angle,
                        int junction, int side)
{
    memset(m, 0, sizeof(*m));
    m->present = RGD_MAN_TYPE | RGD_MAN_DRIVING_SIDE | RGD_MAN_JUNCTION_TYPE;
    m->maneuver_type = (uint8_t)type;
    m->driving_side  = (uint8_t)side;
    m->junction_type = (uint8_t)junction;
    if (exit_angle != 1000) {
        m->present |= RGD_MAN_EXIT_ANGLE;
        m->exit_angle = (int16_t)exit_angle;
    }
}

static void add_roads(rgd_maneuver_t *m, const int *angles, int n)
{
    int i;
    for (i = 0; i < n && i < MAX_MANEUVER_LIST; i++)
        m->junction_angles[i] = (int16_t)angles[i];
    m->junction_angle_count = (uint16_t)((n < MAX_MANEUVER_LIST) ? n : MAX_MANEUVER_LIST);
}

/* Decode a built packet exactly as the renderer would. */
static int decode(const unsigned char *pkt, maneuver_state_t *st)
{
    cr_cmd_t cmd;
    memcpy(&cmd, pkt, CR_PKT_SIZE);
    if (cmd.cmd != CMD_MANEUVER) return 0;
    cr_decode_maneuver(&cmd, st);
    return 1;
}

/* ---------------------------------------------------------------- tests -- */

static void t_left_right(void)
{
    rgd_update_t u; rgd_maneuver_t m;
    unsigned char pkt[K3346_OVERLAY_PKT];
    k3346_overlay_progress_t pr;
    maneuver_state_t st;

    printf("plain left / right turn\n");

    mk_update(&u, 300);
    mk_maneuver(&m, MT_LEFT_TURN, 1000, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    CHECK(k3346_overlay_build_maneuver(&u, &m, pkt, &pr) == 1, "left: packet built");
    CHECK(decode(pkt, &st), "left: decodes as CMD_MANEUVER");
    CHECK_EQ(st.icon, ICON_TURN, "left: icon TURN");
    CHECK(st.exit_angle < 0, "left: angle points left");

    mk_maneuver(&m, MT_RIGHT_TURN, 1000, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    CHECK(k3346_overlay_build_maneuver(&u, &m, pkt, &pr) == 1, "right: packet built");
    CHECK(decode(pkt, &st), "right: decodes");
    CHECK_EQ(st.icon, ICON_TURN, "right: icon TURN");
    CHECK(st.exit_angle > 0, "right: angle points right");
}

/* Half-degrees are the trap here: the wire carries signed half-degrees and the
 * decoder scales by 0.5 when MAN_FLAG_BAP_GEOMETRY is set.  Get the flag or the
 * doubling wrong and every angle is out by a factor of two. */
static void t_half_degrees(void)
{
    rgd_update_t u; rgd_maneuver_t m;
    unsigned char pkt[K3346_OVERLAY_PKT];
    k3346_overlay_progress_t pr;
    maneuver_state_t st;
    cr_cmd_t cmd;

    printf("half-degree geometry round-trips to real degrees\n");

    mk_update(&u, 500);
    /* A 70 degree right turn with a real angle: the mapper keeps the exact
     * angle, doubles it onto the wire, and the decoder halves it back. */
    mk_maneuver(&m, MT_RIGHT_TURN, 70, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    CHECK(k3346_overlay_build_maneuver(&u, &m, pkt, &pr) == 1, "built");

    memcpy(&cmd, pkt, CR_PKT_SIZE);
    CHECK(cmd.flags & MAN_FLAG_BAP_GEOMETRY, "BAP_GEOMETRY flag set");
    CHECK_EQ(CR_MAN_EXIT_ANGLE(cmd.payload), 140, "wire carries 140 half-degrees");

    cr_decode_maneuver(&cmd, &st);
    CHECK(st.exit_angle > 69.4f && st.exit_angle < 70.6f, "decoder gives back ~70 degrees");
}

static void t_roundabout(void)
{
    rgd_update_t u; rgd_maneuver_t m;
    unsigned char pkt[K3346_OVERLAY_PKT];
    k3346_overlay_progress_t pr;
    maneuver_state_t st;
    cr_cmd_t cmd;
    const int roads[] = { -90, 0, 90, 1000 };   /* last one is the "absent" sentinel */

    printf("roundabout exit: icon, snap and raw roads\n");

    mk_update(&u, 200);
    mk_maneuver(&m, MT_ROUNDABOUT_EXIT_1 + 1, 45, JUNCTION_ROUNDABOUT, DRIVING_SIDE_RIGHT);
    add_roads(&m, roads, 4);

    CHECK(k3346_overlay_build_maneuver(&u, &m, pkt, &pr) == 1, "built");
    memcpy(&cmd, pkt, CR_PKT_SIZE);
    CHECK(cmd.flags & MAN_FLAG_SNAP_TO_ROAD, "a real angle may snap to a road");

    cr_decode_maneuver(&cmd, &st);
    CHECK_EQ(st.icon, ICON_ROUNDABOUT, "icon ROUNDABOUT");
    /* Three valid roads; the +/-1000 sentinel must be dropped, not clamped to
     * 180 - clamping it once fabricated a U-turn at an ordinary exit. */
    CHECK_EQ(st.junction_angle_count, 3, "sentinel road dropped");
    CHECK(st.junction_angles[0] < -89.0f && st.junction_angles[0] > -91.0f, "road -90 survives");
    CHECK(st.junction_angles[2] > 89.0f && st.junction_angles[2] < 91.0f, "road +90 survives");
}

static void t_ramp_and_arrival(void)
{
    rgd_update_t u; rgd_maneuver_t m;
    unsigned char pkt[K3346_OVERLAY_PKT];
    k3346_overlay_progress_t pr;
    maneuver_state_t st;

    printf("off-ramp and arrival\n");

    mk_update(&u, 1000);
    mk_maneuver(&m, MT_HIGHWAY_OFF_RAMP_RIGHT, 30, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    CHECK(k3346_overlay_build_maneuver(&u, &m, pkt, &pr) == 1, "ramp: built");
    CHECK(decode(pkt, &st), "ramp: decodes");
    CHECK_EQ(st.icon, ICON_EXIT, "ramp: icon EXIT");
    CHECK_EQ(st.direction, 1, "ramp: peels right");

    mk_maneuver(&m, MT_ARRIVE_DESTINATION_LEFT, 1000, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    CHECK(k3346_overlay_build_maneuver(&u, &m, pkt, &pr) == 1, "arrival: built");
    CHECK(decode(pkt, &st), "arrival: decodes");
    CHECK_EQ(st.icon, ICON_ARRIVED, "arrival: icon ARRIVED");
    CHECK_EQ(st.direction, -1, "arrival: destination on the left");
}

static void t_uturn(void)
{
    rgd_update_t u; rgd_maneuver_t m;
    unsigned char pkt[K3346_OVERLAY_PKT];
    k3346_overlay_progress_t pr;
    maneuver_state_t st;

    printf("u-turn\n");
    mk_update(&u, 150);
    mk_maneuver(&m, MT_U_TURN, 1000, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    CHECK(k3346_overlay_build_maneuver(&u, &m, pkt, &pr) == 1, "built");
    CHECK(decode(pkt, &st), "decodes");
    CHECK_EQ(st.icon, ICON_UTURN, "icon UTURN");
    /* Right-hand traffic with no angle: a U-turn goes left. */
    CHECK_EQ(st.direction, -1, "RHT u-turn goes left");
}

static void t_progress(void)
{
    rgd_update_t u; rgd_maneuver_t m;
    unsigned char pkt[K3346_OVERLAY_PKT];
    k3346_overlay_progress_t pr;

    printf("arrow fill level and blink\n");

    /* City maneuver: the denominator is 1500 m, so 1500 m away is "full bar". */
    mk_maneuver(&m, MT_LEFT_TURN, 1000, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);

    mk_update(&u, 1500);
    k3346_overlay_build_maneuver(&u, &m, pkt, &pr);
    CHECK_EQ(pr.level, 16, "1500 m -> level 16 (far)");
    CHECK_EQ(pr.blink, 0, "1500 m -> no blink");

    mk_update(&u, 0);
    k3346_overlay_build_maneuver(&u, &m, pkt, &pr);
    CHECK_EQ(pr.level, 0, "0 m -> level 0 (at the maneuver)");
    CHECK_EQ(pr.blink, 1, "0 m -> blinking");

    mk_update(&u, 300);         /* exactly 20% of 1500 */
    k3346_overlay_build_maneuver(&u, &m, pkt, &pr);
    CHECK_EQ(pr.blink, 1, "300 m = 20% -> blink starts");

    mk_update(&u, 301);
    k3346_overlay_build_maneuver(&u, &m, pkt, &pr);
    CHECK_EQ(pr.blink, 0, "301 m -> not yet");

    /* No distance at all: fill must be off, not stuck full. */
    mk_update(&u, -1);
    k3346_overlay_build_maneuver(&u, &m, pkt, &pr);
    CHECK_EQ(pr.mode, 0, "no distance -> fill off");

    /* A motorway ramp uses the 3000 m denominator. */
    mk_maneuver(&m, MT_OFF_RAMP, 30, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    mk_update(&u, 1500);
    k3346_overlay_build_maneuver(&u, &m, pkt, &pr);
    CHECK_EQ(pr.level, 8, "ramp at 1500 m -> half a bar");
}

static void t_nothing_to_draw(void)
{
    rgd_update_t u; rgd_maneuver_t m;
    unsigned char pkt[K3346_OVERLAY_PKT];
    k3346_overlay_progress_t pr;

    printf("nothing to draw\n");

    mk_update(&u, 100);
    CHECK_EQ(k3346_overlay_build_maneuver(&u, NULL, pkt, &pr), 0, "NULL maneuver -> no packet");

    memset(&m, 0, sizeof(m));
    CHECK_EQ(k3346_overlay_build_maneuver(&u, &m, pkt, &pr), 0, "empty maneuver -> no packet");

    /* A type the mapper resolves to NO_INFO must not become a stray arrow. */
    mk_maneuver(&m, MT_LEFT_TURN, 1000, 7 /* nonsense junction type */, DRIVING_SIDE_RIGHT);
    CHECK_EQ(k3346_overlay_build_maneuver(&u, &m, pkt, &pr), 0, "unmappable -> no packet");
}

static void t_lanes(void)
{
    rgd_lane_guidance_t lg;
    unsigned char bundle[K3346_OVERLAY_MAX_LANE_PKTS * K3346_OVERLAY_PKT];
    int n, i;

    printf("lane bundle\n");

    memset(&lg, 0, sizeof(lg));
    lg.present = 1;
    lg.lane_guidance_index = 4;
    lg.lane_count = 3;
    for (i = 0; i < 3; i++) {
        lg.lanes[i].position = (uint16_t)i;
        lg.lanes[i].status = (uint8_t)(i == 1 ? 2 : 0);   /* middle lane preferred */
        lg.lanes[i].direction = (int16_t)(i * 45 - 45);
        lg.lanes[i].angles[0] = (int16_t)(i * 45 - 45);
        lg.lanes[i].angle_count = 1;
    }

    n = k3346_overlay_build_lanes(&lg, 0x11223344UL, bundle, K3346_OVERLAY_MAX_LANE_PKTS);
    CHECK_EQ(n, 5, "BEGIN + 3 lanes + COMMIT");
    CHECK_EQ(bundle[0], CMD_LANES_BEGIN, "first packet is BEGIN");
    CHECK_EQ(bundle[(size_t)4 * K3346_OVERLAY_PKT], CMD_LANES_COMMIT, "last packet is COMMIT");
    CHECK_EQ(bundle[2 + 4], 3, "BEGIN carries the lane count");

    /* Token must match across the whole transaction, or the decoder discards it. */
    for (i = 0; i < n; i++) {
        const unsigned char *p = bundle + (size_t)i * K3346_OVERLAY_PKT;
        unsigned long tok = ((unsigned long)p[2] << 24) | ((unsigned long)p[3] << 16)
                          | ((unsigned long)p[4] << 8) | p[5];
        CHECK_EQ(tok, 0x11223344UL, "token matches in every packet");
    }

    /* Lane angles are WHOLE degrees, unlike maneuver geometry. */
    {
        const unsigned char *p = bundle + (size_t)2 * K3346_OVERLAY_PKT;  /* lane 1 */
        int ang = (int)(short)((p[2 + 11] << 8) | p[2 + 12]);
        CHECK_EQ(ang, 0, "lane 1 angle stays 0 degrees, not doubled");
        CHECK_EQ(p[2 + 7], 2, "lane 1 status preferred");
    }

    /* A caller with no room must get nothing rather than a half bundle. */
    CHECK_EQ(k3346_overlay_build_lanes(&lg, 1, bundle, 3), 0, "too small -> no partial bundle");

    lg.lane_count = 0;
    CHECK_EQ(k3346_overlay_build_lanes(&lg, 1, bundle, K3346_OVERLAY_MAX_LANE_PKTS), 0,
             "no lanes -> nothing");

    /* The pair that hides them (2.0.0-rc11, #16), and what Luka's decoder makes
     * of both bundles: that is what the lane row on the VC is drawn from. */
    {
        cr_lane_decoder_t dec;
        cr_lane_guidance_t shown;
        cr_cmd_t cmd;
        int done = 0;

        lg.lane_count = 3;
        n = k3346_overlay_build_lanes(&lg, 7UL, bundle, K3346_OVERLAY_MAX_LANE_PKTS);
        cr_lane_clear(&dec, &shown);
        for (i = 0; i < n; i++) {
            memcpy(&cmd, bundle + (size_t)i * K3346_OVERLAY_PKT, sizeof(cmd));
            done = cr_lane_receive(&dec, &cmd, &shown);
        }
        CHECK(done == 1 && shown.showing == 1 && shown.count == 3 && shown.complete == 1,
              "Luka's decoder: the lane bundle shows three lanes");
        CHECK_EQ(shown.event_index, 4, "...of the event we named");

        memset(bundle, 0xAA, sizeof(bundle));
        n = k3346_overlay_build_lanes_hide(0x01020304UL, bundle, K3346_OVERLAY_MAX_LANE_PKTS);
        CHECK_EQ(n, 2, "hide: BEGIN + COMMIT");
        CHECK_EQ(bundle[0], CMD_LANES_BEGIN, "hide: first packet is BEGIN");
        CHECK_EQ(bundle[1], 0, "hide: no flags");
        CHECK(bundle[2] == 1 && bundle[3] == 2 && bundle[4] == 3 && bundle[5] == 4, "hide: BEGIN carries the token");
        CHECK_EQ(bundle[2 + 4], 0, "hide: no lanes");
        CHECK_EQ(bundle[2 + 5], 0, "hide: not complete");
        CHECK_EQ(bundle[2 + 6], 0, "hide: not showing");
        CHECK(bundle[2 + 8] == 0xFF && bundle[2 + 9] == 0xFF && bundle[2 + 10] == 0xFF && bundle[2 + 11] == 0xFF,
              "hide: event -1, as Luka's HIDDEN");
        CHECK_EQ(bundle[K3346_OVERLAY_PKT], CMD_LANES_COMMIT, "hide: second packet is COMMIT");
        CHECK(memcmp(bundle + K3346_OVERLAY_PKT + 2, bundle + 2, 4) == 0, "hide: COMMIT carries the same token");
        CHECK_EQ(bundle[2 + 7], 0, "hide: the byte between showing and the event is zero");
        for (i = 2 + 12; i < K3346_OVERLAY_PKT; i++)
            if (bundle[i] != 0) break;
        CHECK_EQ(i, K3346_OVERLAY_PKT, "hide: the rest of BEGIN is zero");
        for (i = 6; i < K3346_OVERLAY_PKT; i++)
            if (bundle[K3346_OVERLAY_PKT + i] != 0) break;
        CHECK_EQ(i, K3346_OVERLAY_PKT, "hide: the rest of COMMIT is zero");

        done = 0;
        for (i = 0; i < n; i++) {
            memcpy(&cmd, bundle + (size_t)i * K3346_OVERLAY_PKT, sizeof(cmd));
            done = cr_lane_receive(&dec, &cmd, &shown);
        }
        CHECK(done == 1, "Luka's decoder: the hide pair is a whole update");
        CHECK(shown.showing == 0 && shown.count == 0, "...after which nothing is showing");

        CHECK_EQ(k3346_overlay_build_lanes_hide(1UL, bundle, 1), 0, "hide: no room for both -> nothing");
        CHECK_EQ(k3346_overlay_build_lanes_hide(1UL, NULL, 2), 0, "hide: no buffer -> nothing");
    }
}

/* ------------------------------------------------- topology (sockets) -- */
/*
 * THE TEST THAT WAS MISSING, AND WHAT IT COST.
 *
 * Every test above checks the BYTES and none of them checked who opens the
 * socket.  So nobody noticed that both ends of the link called connect():
 * this feed did, and so does Luka's renderer (overlay/upstream/server.c is a
 * TCP *client*).  Nothing ever bound port 19800.  The car ran a whole session
 * with the window adopted, the context switched and the panel composited - and
 * no arrow, because not one command could arrive.  run-23/netstat-an.txt has
 * no 19800 line at all.
 *
 * So this test connects to the feed AS A CLIENT, which is the renderer's role.
 * Before the fix it could not have passed: there was nothing to connect to.
 */

static int tcp_connect_retry(int port, int tries)
{
    struct sockaddr_in sa;
    int i;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    sa.sin_addr.s_addr = inet_addr("127.0.0.1");

    for (i = 0; i < tries; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == 0) return fd;
            close(fd);
        }
        { struct timespec n; n.tv_sec = 0; n.tv_nsec = 50L * 1000000L;
          while (nanosleep(&n, &n) != 0 && errno == EINTR) {} }
    }
    return -1;
}

/* Read exactly one 48-byte packet, or give up after ~2 s. */
static int read_pkt(int fd, unsigned char *out)
{
    struct timeval tv;
    size_t off = 0;

    tv.tv_sec = 2; tv.tv_usec = 0;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (off < CR_PKT_SIZE) {
        ssize_t n = recv(fd, out + off, CR_PKT_SIZE - off, 0);
        if (n > 0) { off += (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        return 0;
    }
    return 1;
}

static void t_topology(void)
{
    rgd_update_t u;
    rgd_maneuver_t m;
    unsigned char pkt[K3346_OVERLAY_PKT];
    maneuver_state_t st;
    int fd, fd2;

    printf("the feed LISTENS and the renderer connects to it\n");

    k3346_overlay_feed_init();

    fd = tcp_connect_retry(CR_TCP_PORT, 60);   /* up to ~3 s */
    CHECK(fd >= 0, "a client can connect to the feed on 127.0.0.1:19800");
    if (fd < 0) { k3346_overlay_feed_shutdown(); return; }

    /* 1000 m of a 1500 m city approach: far enough not to blink, so the
     * stream is one maneuver packet and nothing else. */
    mk_update(&u, 1000);
    mk_maneuver(&m, MT_RIGHT_TURN, 1000, JUNCTION_SINGLE_INTERSECTION,
                DRIVING_SIDE_RIGHT);
    k3346_overlay_feed_publish(&u, &m, NULL);

    CHECK(read_pkt(fd, pkt), "the published maneuver reaches the client");
    if (decode(pkt, &st)) {
        CHECK_EQ(st.icon, ICON_TURN, "Luka's decoder reads it as a turn");
        CHECK(st.exit_angle > 0, "and as a right turn");
    } else {
        CHECK(0, "Luka's decoder accepts what arrived on the socket");
    }

    /* mm-ipod restarts on every iPhone connect and the renderer reconnects.
     * Without a replay the panel would stay blank until the next RGI update,
     * which on a long straight stretch is minutes away. */
    close(fd);
    fd2 = tcp_connect_retry(CR_TCP_PORT, 60);
    CHECK(fd2 >= 0, "a second client can connect after the first dropped");
    if (fd2 >= 0) {
        CHECK(read_pkt(fd2, pkt), "state is replayed without a new publish");
        CHECK(decode(pkt, &st), "the replay starts with the maneuver");
        close(fd2);
    }

    k3346_overlay_feed_shutdown();

    /* The listener must go with the thread, or the next mm-ipod start cannot
     * bind and the overlay is dead for the whole session. */
    fd = tcp_connect_retry(CR_TCP_PORT, 4);
    CHECK(fd < 0, "shutdown releases the port");
    if (fd >= 0) close(fd);
}

/* ------------------------------------- only what changed (2.0.0-rc6) -- */
/*
 * The phone repeats the route state about once a second.  Until rc6 every
 * repeat went out as a whole CMD_MANEUVER, and Luka's engine answers each of
 * those with a hand-over that empties the fill: in the car the arrow kept
 * refilling and the renderer took 13 % of a core (run-02).  His own sender
 * sends a maneuver when it changed and the fill by itself.
 */

/* 1 = a packet arrived within ms. */
static int read_pkt_ms(int fd, unsigned char *out, int ms)
{
    struct timeval tv;
    size_t off = 0;

    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    while (off < CR_PKT_SIZE) {
        ssize_t n = recv(fd, out + off, CR_PKT_SIZE - off, 0);
        if (n > 0) { off += (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        return 0;
    }
    return 1;
}

static unsigned long lane_token(const unsigned char *p)
{
    return ((unsigned long)p[2] << 24) | ((unsigned long)p[3] << 16)
         | ((unsigned long)p[4] << 8) | p[5];
}

/* Reads one whole lane bundle of three lanes; 0 when anything else arrives. */
static int read_lanes(int fd, unsigned long *token, int *status1)
{
    unsigned char pkt[K3346_OVERLAY_PKT];
    int i;

    if (!read_pkt(fd, pkt) || pkt[0] != CMD_LANES_BEGIN || pkt[2 + 4] != 3) return 0;
    *token = lane_token(pkt);
    for (i = 0; i < 3; i++) {
        if (!read_pkt(fd, pkt) || pkt[0] != CMD_LANES_LANE || lane_token(pkt) != *token) return 0;
        if (i == 1) *status1 = pkt[2 + 7];
    }
    return read_pkt(fd, pkt) && pkt[0] == CMD_LANES_COMMIT && lane_token(pkt) == *token;
}

/* Reads the pair that hides the lanes; 0 when anything else arrives. */
static int read_hide(int fd, unsigned long *token)
{
    unsigned char pkt[K3346_OVERLAY_PKT];

    if (!read_pkt(fd, pkt) || pkt[0] != CMD_LANES_BEGIN) return 0;
    if (pkt[2 + 4] != 0 || pkt[2 + 5] != 0 || pkt[2 + 6] != 0) return 0;
    if (pkt[2 + 8] != 0xFF || pkt[2 + 9] != 0xFF || pkt[2 + 10] != 0xFF || pkt[2 + 11] != 0xFF) return 0;
    *token = lane_token(pkt);
    return read_pkt(fd, pkt) && pkt[0] == CMD_LANES_COMMIT && lane_token(pkt) == *token;
}

static void mk_lanes(rgd_lane_guidance_t *lg)
{
    int i;

    memset(lg, 0, sizeof(*lg));
    lg->present = 1;
    lg->lane_guidance_index = 4;
    lg->lane_count = 3;
    for (i = 0; i < 3; i++) {
        lg->lanes[i].position = (uint16_t)i;
        lg->lanes[i].status = (uint8_t)(i == 1 ? 2 : 0);
        lg->lanes[i].direction = (int16_t)(i * 45 - 45);
        lg->lanes[i].angles[0] = (int16_t)(i * 45 - 45);
        lg->lanes[i].angle_count = 1;
    }
}

static void nap_ms(int ms)
{
    struct timespec n;
    n.tv_sec = ms / 1000; n.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&n, &n) != 0 && errno == EINTR) {}
}

static void t_only_changes(void)
{
    static const int roads[3] = { -90, 0, 90 };
    rgd_update_t u;
    rgd_maneuver_t m;
    rgd_lane_guidance_t lg;
    unsigned char pkt[K3346_OVERLAY_PKT];
    maneuver_state_t st;
    unsigned long tok1 = 0, tok2 = 0, tok3 = 0;
    int fd, fd2, i, s1 = -1;

    printf("only what changed goes to the renderer\n");

    k3346_overlay_feed_init();
    fd = tcp_connect_retry(CR_TCP_PORT, 60);
    CHECK(fd >= 0, "renderer connected");
    if (fd < 0) { k3346_overlay_feed_shutdown(); return; }
    CHECK(!read_pkt_ms(fd, pkt, 300), "a fresh feed replays nothing (state of an earlier run is gone)");

    /* city approach, 1500 m = level 16: 1400 m -> 14, 900 m -> 9 */
    mk_maneuver(&m, MT_RIGHT_TURN, 1000, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    mk_update(&u, 1400);
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt) && decode(pkt, &st) && st.exit_angle > 0, "first update: the whole maneuver");
    CHECK_EQ(pkt[2 + 44], 14, "...with its fill level");
    /* What a renderer that has just connected draws is not known (it outlives
     * an mm-ipod restart), so the first update without lanes hides them. */
    CHECK(read_hide(fd, &tok1) && tok1 != 0, "...and, for a renderer that just connected, one hide of the lanes");
    tok1 = 0;

    k3346_overlay_feed_publish(&u, &m, NULL);
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(!read_pkt_ms(fd, pkt, 300), "the same update again: nothing is sent");

    mk_update(&u, 900);
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt), "nearer: something is sent");
    CHECK_EQ(pkt[0], CMD_PROGRESS, "...the fill by itself, not a new maneuver");
    CHECK_EQ(pkt[2 + 0], 9, "...level 9");
    CHECK_EQ(pkt[2 + 1], 1, "...fill on");
    CHECK_EQ(pkt[2 + 2], CR_PROGRESS_FILL, "...filling, not blinking");

    mk_update(&u, 905);     /* still level 9 */
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(!read_pkt_ms(fd, pkt, 300), "a few metres within the same level: nothing");

    mk_update(&u, -1);      /* the phone stopped sending the distance */
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt) && pkt[0] == CMD_PROGRESS, "distance gone: the fill by itself");
    CHECK_EQ(pkt[2 + 1], 0, "...fill off");
    CHECK_EQ(pkt[2 + 2], CR_PROGRESS_OFF, "...state off");

    /* far away the level is 16 with the fill on, and 16 again with it off */
    mk_update(&u, 2000);
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt) && pkt[0] == CMD_PROGRESS && pkt[2 + 0] == 16 && pkt[2 + 1] == 1,
          "distance back, far away: level 16, fill on");
    mk_update(&u, -1);
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt) && pkt[0] == CMD_PROGRESS && pkt[2 + 0] == 16 && pkt[2 + 1] == 0,
          "same level, fill switched off: sent all the same");

    /* another arrow */
    mk_maneuver(&m, MT_LEFT_TURN, 1000, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    mk_update(&u, 900);
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt) && decode(pkt, &st) && st.exit_angle < 0, "the next maneuver: a whole maneuver again");
    CHECK_EQ(pkt[2 + 44], 9, "...with its own fill level");
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(!read_pkt_ms(fd, pkt, 300), "...and nothing when it is repeated");

    /* the same turn with the side roads the phone sends a moment later */
    add_roads(&m, roads, 3);
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt) && decode(pkt, &st), "same turn, roads added: a whole maneuver (the picture changed)");
    CHECK(st.junction_angle_count >= 1, "...with the side roads");

    /* route end, and the updates with nothing to draw that follow it */
    k3346_overlay_feed_clear("test");
    CHECK(read_pkt(fd, pkt) && pkt[0] == CMD_CLEAR, "clear goes out");
    k3346_overlay_feed_clear("test again");
    k3346_overlay_feed_publish(&u, NULL, NULL);
    CHECK(!read_pkt_ms(fd, pkt, 300), "blank already: a second clear and an empty update send nothing");
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt) && decode(pkt, &st) && st.junction_angle_count >= 1,
          "after a clear the same maneuver is a whole maneuver again");
    k3346_overlay_feed_clear("once more");
    CHECK(read_pkt(fd, pkt) && pkt[0] == CMD_CLEAR, "a clear after a maneuver goes out again");
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt) && pkt[0] == CMD_MANEUVER, "and the maneuver after it");

    /* lanes: once, and again only when they change */
    memset(&lg, 0, sizeof(lg));
    lg.present = 1;
    lg.lane_guidance_index = 4;
    lg.lane_count = 3;
    for (i = 0; i < 3; i++) {
        lg.lanes[i].position = (uint16_t)i;
        lg.lanes[i].status = (uint8_t)(i == 1 ? 2 : 0);
        lg.lanes[i].direction = (int16_t)(i * 45 - 45);
        lg.lanes[i].angles[0] = (int16_t)(i * 45 - 45);
        lg.lanes[i].angle_count = 1;
    }
    k3346_overlay_feed_publish(&u, &m, &lg);
    CHECK(read_lanes(fd, &tok1, &s1) && s1 == 2, "lanes: the bundle, with nothing before it (arrow and fill unchanged)");
    k3346_overlay_feed_publish(&u, &m, &lg);
    CHECK(!read_pkt_ms(fd, pkt, 300), "the same lanes again: nothing");
    lg.lanes[1].status = 1;
    k3346_overlay_feed_publish(&u, &m, &lg);
    CHECK(read_lanes(fd, &tok2, &s1) && s1 == 1, "a lane changed: a new bundle");
    CHECK(tok2 != tok1 && tok1 != 0 && tok2 != 0, "...under a new token");

    /* a new arrow takes its lanes with it */
    mk_maneuver(&m, MT_RIGHT_TURN, 1000, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    k3346_overlay_feed_publish(&u, &m, &lg);
    CHECK(read_pkt(fd, pkt) && decode(pkt, &st) && st.exit_angle > 0, "new maneuver with the same lanes: the maneuver");
    CHECK(read_lanes(fd, &tok3, &s1) && tok3 != tok2, "...and the lanes once more, for the new arrow");

    /* fill moved on while the renderer was away: the replay has the newest */
    mk_update(&u, 600);     /* level 6 */
    k3346_overlay_feed_publish(&u, &m, &lg);
    CHECK(read_pkt(fd, pkt) && pkt[0] == CMD_PROGRESS && pkt[2 + 0] == 6, "level 6 by itself");
    close(fd);
    fd2 = tcp_connect_retry(CR_TCP_PORT, 60);
    CHECK(fd2 >= 0, "the renderer restarts and connects again");
    if (fd2 >= 0) {
        CHECK(read_pkt(fd2, pkt) && decode(pkt, &st) && st.exit_angle > 0, "replay: the maneuver");
        CHECK_EQ(pkt[2 + 44], 6, "...carrying the newest fill, not the one it was first sent with");
        CHECK(read_pkt(fd2, pkt) && pkt[0] == CMD_PROGRESS && pkt[2 + 0] == 6, "replay: the fill");
        nap_ms(200);        /* the sender notes what the replay gave */
        k3346_overlay_feed_publish(&u, &m, &lg);
        CHECK(read_lanes(fd2, &tok1, &s1), "after a reconnect the unchanged update brings the lanes (not replayed) and no maneuver");
        k3346_overlay_feed_publish(&u, &m, &lg);
        CHECK(!read_pkt_ms(fd2, pkt, 300), "...and then nothing again");

        /* the last 20 %: the phone's update says blink, the sender keeps the clock */
        mk_update(&u, 100);     /* level 1, blinking */
        k3346_overlay_feed_publish(&u, &m, &lg);
        CHECK(read_pkt(fd2, pkt) && pkt[0] == CMD_PROGRESS && pkt[2 + 0] == 1, "close to the turn: the fill by itself");
        CHECK_EQ(pkt[2 + 2], CR_PROGRESS_BLINK_HIGH, "...blinking, from the update itself (the sender's ticks follow)");
        CHECK(read_pkt(fd2, pkt) && pkt[0] == CMD_PROGRESS
              && (pkt[2 + 2] == CR_PROGRESS_BLINK_HIGH || pkt[2 + 2] == CR_PROGRESS_BLINK_LOW),
              "...and the sender keeps the blink going between updates");
        close(fd2);
    }

    k3346_overlay_feed_shutdown();
}

/* ------------------------------- lanes go away again (2.0.0-rc11, #16) -- */
/*
 * The renderer keeps its lanes until it is told to hide them or to clear the
 * panel.  Until rc11 an update without lanes sent nothing about them, so the
 * lanes of the turn just taken stayed under the next arrow (car, rc10, run-08).
 */
static void t_lanes_hide(void)
{
    rgd_update_t u;
    rgd_maneuver_t m;
    rgd_lane_guidance_t lg, none;
    unsigned char pkt[K3346_OVERLAY_PKT];
    maneuver_state_t st;
    unsigned long tok = 0, hide1 = 0, hide2 = 0;
    int fd, s1 = -1;

    printf("lanes go away when the update has none\n");

    k3346_overlay_feed_init();
    fd = tcp_connect_retry(CR_TCP_PORT, 60);
    CHECK(fd >= 0, "renderer connected");
    if (fd < 0) { k3346_overlay_feed_shutdown(); return; }
    CHECK(!read_pkt_ms(fd, pkt, 300), "nothing before the first update");

    /* 900 m of 1500 m: level 9, far from the blink, so the sender adds nothing */
    mk_maneuver(&m, MT_RIGHT_TURN, 1000, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    mk_update(&u, 900);
    mk_lanes(&lg);
    k3346_overlay_feed_publish(&u, &m, &lg);
    CHECK(read_pkt(fd, pkt) && decode(pkt, &st) && st.exit_angle > 0, "the maneuver");
    CHECK(read_lanes(fd, &tok, &s1), "...with its lanes, and no hide before them");

    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_hide(fd, &hide1), "the same arrow, lanes gone: the hide pair");
    CHECK(hide1 != tok && hide1 != 0, "...under a token of its own");
    k3346_overlay_feed_publish(&u, &m, NULL);
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(!read_pkt_ms(fd, pkt, 300), "still no lanes: the hide is not repeated");

    k3346_overlay_feed_publish(&u, &m, &lg);
    CHECK(read_lanes(fd, &tok, &s1) && tok != hide1, "the same lanes come back: the bundle goes out again");

    /* the car's case: the turn is taken, the next arrow has no lanes */
    mk_maneuver(&m, MT_LEFT_TURN, 1000, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt) && decode(pkt, &st) && st.exit_angle < 0, "next maneuver without lanes: the maneuver");
    CHECK(read_hide(fd, &hide2) && hide2 != hide1, "...and the hide pair after it");

    mk_maneuver(&m, MT_RIGHT_TURN, 1000, JUNCTION_SINGLE_INTERSECTION, DRIVING_SIDE_RIGHT);
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt) && pkt[0] == CMD_MANEUVER, "another maneuver, no lanes on screen: the maneuver");
    CHECK(!read_pkt_ms(fd, pkt, 300), "...and no hide");

    /* an empty list and a list marked absent are no lanes either */
    k3346_overlay_feed_publish(&u, &m, &lg);
    CHECK(read_lanes(fd, &tok, &s1), "lanes again");
    none = lg;
    none.lane_count = 0;
    k3346_overlay_feed_publish(&u, &m, &none);
    CHECK(read_hide(fd, &hide1), "a list with no lanes in it: hide");
    k3346_overlay_feed_publish(&u, &m, &lg);
    CHECK(read_lanes(fd, &tok, &s1), "lanes again");
    none = lg;
    none.present = 0;
    k3346_overlay_feed_publish(&u, &m, &none);
    CHECK(read_hide(fd, &hide1), "a list marked absent: hide");

    /* a clear takes the lanes with the panel, so nothing is left to hide */
    k3346_overlay_feed_publish(&u, &m, &lg);
    CHECK(read_lanes(fd, &tok, &s1), "lanes again");
    k3346_overlay_feed_clear("test");
    CHECK(read_pkt(fd, pkt) && pkt[0] == CMD_CLEAR, "clear goes out");
    k3346_overlay_feed_publish(&u, &m, NULL);
    CHECK(read_pkt(fd, pkt) && pkt[0] == CMD_MANEUVER, "after the clear: the maneuver");
    CHECK(!read_pkt_ms(fd, pkt, 300), "...and no hide, the clear did that");

    close(fd);
    k3346_overlay_feed_shutdown();
}

int main(void)
{
    t_left_right();
    t_half_degrees();
    t_roundabout();
    t_ramp_and_arrival();
    t_uturn();
    t_progress();
    t_nothing_to_draw();
    t_lanes();
    t_topology();
    t_only_changes();
    t_lanes_hide();

    printf("\noverlay_feed: %d OK, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
