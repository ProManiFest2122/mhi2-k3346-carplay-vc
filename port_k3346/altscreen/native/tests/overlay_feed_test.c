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

    printf("\noverlay_feed: %d OK, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
