/*
 * overlay_feed — see overlay_feed.h.
 *
 * Contains the C port of Luka's RendererMapper.java (BAP semantics -> renderer
 * icon + raw CarPlay geometry).  The first half of the chain, iAP2 type -> BAP,
 * lives in maneuver_map.c and is checked against the Java mapper by
 * tools/run_overlay_map_diff.sh.
 */

#include "overlay_feed.h"
#include "maneuver_map.h"
#include "logging.h"
#include "protocol.h"           /* Luka's wire format, from overlay/upstream */
#include "arrow_progress.h"     /* CR_PROGRESS_* states, same source of truth */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define LOG_MODULE "overlay"

/* Renderer icons (Luka's maneuver.h; duplicated here so the hook does not have
 * to include the renderer's headers wholesale). */
#define ICON_NONE             0
#define ICON_APPROACH         1
#define ICON_TURN             2
#define ICON_UTURN            3
#define ICON_EXIT             5
#define ICON_ROUNDABOUT       6
#define ICON_ARRIVED          7
#define ICON_LANE_CHANGE      8
#define ICON_ROUNDABOUT_EXIT  9

#define FEED_RING        64      /* 48 B each; a lane bundle is at most 10 */
#define RECONNECT_MS    200      /* same cadence Luka's renderer reconnects at */
#define BLINK_HALF_MS   300      /* 600 ms period, as ACTION_BLINK_INTERVAL_MS */

/* Approach thresholds, same numbers BAPBridge uses, so the arrow fills in step
 * with the emphasis the VC already shows (BAPBridge.java:64-66). */
#define CITY_PREPARE_M     1500
#define HIGHWAY_PREPARE_M  3000
#define BLINK_PERCENT        20

#define MAX_JUNCTION_ANGLES 18
#define MAX_WIRE_LANES       8
#define MAX_WIRE_LANE_ANGLES 16

typedef struct {
    int icon;
    int direction;        /* -1 left, 0 centre, +1 right */
    int exit_angle;       /* signed half-degrees */
    int driving_side;     /* 0 RHT, 1 LHT */
    int junction_angles[MAX_JUNCTION_ANGLES];  /* half-degrees */
    int junction_count;
    int snap_to_road;
} overlay_mapping_t;

static struct {
    int                started;
    int                shutdown;
    int                fd;           /* accepted renderer, -1 when none */
    int                listen_fd;    /* our listening socket, -1 until bound */
    pthread_t          thread;
    pthread_mutex_t    lock;
    pthread_cond_t     wake;

    unsigned char      ring[FEED_RING][CR_PKT_SIZE];
    int                head, tail, dropped;

    /* Latest progress, re-sent by the sender thread so the blink has its own
     * 600 ms clock instead of riding on the ~1 Hz RGI updates. */
    int                prog_valid;
    int                prog_level;
    int                prog_mode;
    int                prog_blink;

    /* Last maneuver put on the wire, replayed to a renderer that connects
     * later.  Without it the cluster stays blank until the next RGI update,
     * which on a long straight stretch can be minutes away. */
    unsigned char      last_man[CR_PKT_SIZE];
    int                last_man_valid;
} g_feed = {
    0, 0, -1, -1, 0,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER,
    { { 0 } }, 0, 0, 0,
    0, 0, 0, 0,
    { 0 }, 0
};

/* ------------------------------------------------------------- helpers -- */

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void put_be16(unsigned char *p, int v)
{
    p[0] = (unsigned char)((v >> 8) & 0xFF);
    p[1] = (unsigned char)(v & 0xFF);
}

static void put_be32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)((v >> 24) & 0xFF);
    p[1] = (unsigned char)((v >> 16) & 0xFF);
    p[2] = (unsigned char)((v >> 8) & 0xFF);
    p[3] = (unsigned char)(v & 0xFF);
}

/* Caller holds the lock.  Dropping is correct here: every packet is a full
 * snapshot of its kind, so the next update supersedes anything lost. */
static void ring_push_locked(const unsigned char *pkt)
{
    int next = (g_feed.head + 1) % FEED_RING;
    if (next == g_feed.tail) {
        g_feed.dropped++;
        return;
    }
    memcpy(g_feed.ring[g_feed.head], pkt, CR_PKT_SIZE);
    g_feed.head = next;
}

/* ------------------------------------------------- RendererMapper port -- */

/* Luka's directionAngleHalfDegrees(): BAP direction code -> signed half-degrees. */
static int direction_angle_half_degrees(int direction)
{
    int d = direction & 255;
    if (d == 128) return 360;
    return d < 128 ? -(d * 45 / 16) : ((256 - d) * 45 / 16);
}

static void from_bap(overlay_mapping_t *out, int main_el, int direction, int driving_side)
{
    memset(out, 0, sizeof(*out));
    out->driving_side = (driving_side == DRIVING_SIDE_LEFT) ? 1 : 0;
    out->exit_angle   = direction_angle_half_degrees(direction);

    switch (main_el) {
    case BAP_FOLLOW_STREET:
        out->icon = ICON_APPROACH;
        break;
    case BAP_TURN:
        out->icon = ICON_TURN;
        break;
    case BAP_UTURN:
        out->icon = ICON_UTURN;
        out->direction = (direction == BAP_DIR_LEFT) ? -1 : 1;
        out->exit_angle = out->direction * 360;
        break;
    case BAP_EXIT_LEFT:
    case BAP_EXIT_RIGHT:
        out->icon = ICON_EXIT;
        out->direction = (main_el == BAP_EXIT_LEFT) ? -1 : 1;
        out->exit_angle = 0;
        break;
    case BAP_CHANGE_LANE:
        out->icon = ICON_LANE_CHANGE;
        out->direction = (direction == BAP_DIR_LEFT) ? -1 : 1;
        out->exit_angle = 0;
        break;
    case BAP_ROUNDABOUT_TRS_LEFT:
    case BAP_ROUNDABOUT_TRS_RIGHT:
        out->icon = ICON_ROUNDABOUT;
        out->driving_side = (main_el == BAP_ROUNDABOUT_TRS_LEFT) ? 1 : 0;
        break;
    case BAP_EXIT_ROUNDABOUT_TRS_LEFT:
    case BAP_EXIT_ROUNDABOUT_TRS_RIGHT:
        out->icon = ICON_ROUNDABOUT_EXIT;
        out->driving_side = (main_el == BAP_EXIT_ROUNDABOUT_TRS_LEFT) ? 1 : 0;
        out->direction = out->driving_side ? -1 : 1;
        out->exit_angle = out->direction * 90;   /* fixed diagonal exit, HUD s112/s113 */
        break;
    case BAP_ARRIVED:
        out->icon = ICON_ARRIVED;
        out->direction = (direction == BAP_DIR_LEFT) ? -1
                       : (direction == BAP_DIR_RIGHT) ? 1 : 0;
        out->exit_angle = 0;
        break;
    default:
        out->icon = ICON_NONE;
        out->exit_angle = 0;
        break;
    }
}

/* Luka's rawRoads(): BAP drops the active exit and quantizes side streets; the
 * procedural renderer wants the originals, including that exit so a roundabout
 * can snap to it.  Sentinels (+/-1000) are left out, not clamped. */
static void raw_roads(overlay_mapping_t *out, const int16_t *angles, int count)
{
    int i;
    out->junction_count = 0;
    for (i = 0; i < count && out->junction_count < MAX_JUNCTION_ANGLES; i++) {
        if (angles[i] >= -180 && angles[i] <= 180)
            out->junction_angles[out->junction_count++] = angles[i] * 2;
    }
}

/* Luka's withForwardRoad(): a single-bend TURN standing in for an off-ramp
 * needs the continuing main road spelled out, as the HUD's fixed side street 0. */
static void with_forward_road(overlay_mapping_t *out)
{
    int i;
    for (i = 0; i < out->junction_count; i++)
        if (out->junction_angles[i] == 0) return;
    if (out->junction_count >= MAX_JUNCTION_ANGLES)
        out->junction_count = MAX_JUNCTION_ANGLES - 1;
    out->junction_angles[out->junction_count++] = 0;
}

static int is_ramp(int mt)
{
    return mt == MT_OFF_RAMP || mt == MT_ON_RAMP
        || mt == MT_HIGHWAY_OFF_RAMP_LEFT || mt == MT_HIGHWAY_OFF_RAMP_RIGHT;
}

/* Luka's withCarPlayGeometry(): BAP has a finite icon catalogue, the renderer
 * does not.  Where CarPlay gave us a real angle, use it. */
static void with_carplay_geometry(overlay_mapping_t *out, int mt, int angle,
                                  int angle_present,
                                  const int16_t *roads, int road_count)
{
    int valid = angle_present && angle >= -180 && angle <= 180;

    if (out->icon != ICON_NONE
        && (mt == MT_ENTER_ROUNDABOUT || mt == MT_EXIT_ROUNDABOUT
            || (mt >= MT_ROUNDABOUT_EXIT_1 && mt <= MT_ROUNDABOUT_EXIT_19))) {
        out->icon = ICON_ROUNDABOUT;
        out->direction = 0;
        out->exit_angle = valid ? angle * 2 : 0;
        /* Only a real angle may snap: a fallback or a typed U-turn must not be
         * redirected onto a neighbouring exit. */
        out->snap_to_road = valid;
    } else if (out->icon == ICON_TURN && !is_ramp(mt) && valid) {
        int generic = (mt == MT_START_ROUTE || mt == MT_EXIT_FERRY
                       || mt == MT_CHANGE_HIGHWAY);
        if (generic || (out->exit_angle < 0 && angle < 0)
                    || (out->exit_angle > 0 && angle > 0))
            out->exit_angle = angle * 2;
    }

    if (out->icon == ICON_APPROACH || out->icon == ICON_TURN
        || out->icon == ICON_ROUNDABOUT)
        raw_roads(out, roads, road_count);

    if (out->icon == ICON_TURN
        && (mt == MT_OFF_RAMP || mt == MT_HIGHWAY_OFF_RAMP_LEFT
            || mt == MT_HIGHWAY_OFF_RAMP_RIGHT))
        with_forward_road(out);
}

/* ------------------------------------------------------ packet builders -- */

static int build_maneuver(unsigned char *pkt, const overlay_mapping_t *m,
                          int level, int mode, int blink_state)
{
    int i;

    memset(pkt, 0, CR_PKT_SIZE);
    pkt[0] = CMD_MANEUVER;
    /* BAP_GEOMETRY: our angles are signed half-degrees.  SNAP_TO_ROAD is added
     * only where the mapper decided a real angle justifies snapping. */
    pkt[1] = (unsigned char)(MAN_FLAG_BAP_GEOMETRY | MAN_FLAG_PROGRESS
                             | CR_PROGRESS_FLAG
                             | (m->snap_to_road ? MAN_FLAG_SNAP_TO_ROAD : 0));

    pkt[2 + 0] = (unsigned char)m->icon;
    pkt[2 + 1] = (unsigned char)(signed char)m->direction;
    put_be16(&pkt[2 + 2], m->exit_angle);
    pkt[2 + 4] = (unsigned char)m->driving_side;
    pkt[2 + 5] = (unsigned char)m->junction_count;
    for (i = 0; i < m->junction_count && i < MAX_JUNCTION_ANGLES; i++)
        put_be16(&pkt[2 + 6 + i * 2], m->junction_angles[i]);

    pkt[2 + 42] = (unsigned char)blink_state;   /* 0 off, 1 fill, 2 low, 3 high */
    pkt[2 + 44] = (unsigned char)level;         /* 0..16, 16 = far away */
    pkt[2 + 45] = (unsigned char)mode;          /* 0 off, 1 on */
    return CR_PKT_SIZE;
}

static void build_progress(unsigned char *pkt, int level, int mode, int phase)
{
    memset(pkt, 0, CR_PKT_SIZE);
    pkt[0] = CMD_PROGRESS;
    pkt[2 + 0] = (unsigned char)level;
    pkt[2 + 1] = (unsigned char)mode;
    pkt[2 + 2] = (unsigned char)phase;
}

static void build_clear(unsigned char *pkt)
{
    memset(pkt, 0, CR_PKT_SIZE);
    pkt[0] = CMD_CLEAR;
}

/* ----------------------------------------------------- sender thread --- */

/*
 * WHO LISTENS, AND WHY IT HAD TO CHANGE.
 *
 * Until the car run of 2026-10-27 this function was feed_connect(): it dialled
 * 127.0.0.1:19800.  So did the renderer — Luka's overlay/upstream/server.c is
 * a TCP *client* ("connects to Java BAPBridge server").  Two clients, no
 * server: nothing ever bound the port, the renderer logged
 * "127.0.0.1:19800 not yet listening, will retry" for the whole session,
 * drew `commands=0`, and run-23/netstat-an.txt has no 19800 entry at all.
 *
 * The fix keeps Luka's topology instead of inventing one: on MHI2Q the
 * PRODUCER listens (Java) and his renderer connects to it.  Doing the same
 * here means his server.c — including its 200 ms reconnect loop and its
 * clear-on-reconnect handling — needs no edits.  mm-ipod restarts on every
 * iPhone connect, so this listener comes and goes; that is exactly the case
 * his reconnect loop was written for.
 *
 * SO_REUSEADDR because mm-ipod can be restarted while the old socket is still
 * in TIME_WAIT, and a bind failure would cost a whole session.
 */
static int feed_listen(void)
{
    struct sockaddr_in sa;
    int fd, one = 1;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(CR_TCP_PORT);
    /* Loopback only.  The car is on a network; this must never be reachable
     * from it. */
    sa.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 1) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Wait up to RECONNECT_MS for the renderer to connect.  Bounded, because this
 * thread also has to keep the blink clock ticking and honour shutdown. */
static int feed_accept(int listen_fd)
{
    struct timeval tv;
    fd_set rfds;
    int fd, one = 1;

    FD_ZERO(&rfds);
    FD_SET(listen_fd, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = (long)RECONNECT_MS * 1000L;

    if (select(listen_fd + 1, &rfds, NULL, NULL, &tv) <= 0) return -1;

    fd = accept(listen_fd, NULL, NULL);
    if (fd < 0) return -1;

    /* Bounded sends: a renderer that stops reading must not wedge this
     * thread, because it is the same thread mm-ipod's RGI path signals. */
    tv.tv_sec = 0; tv.tv_usec = 300 * 1000;
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    return fd;
}

static int feed_write_all(int fd, const unsigned char *p, size_t len)
{
    size_t off = 0;
    while (off < len) {
        /* flags MUST be 0: QNX 6.5 io-pkt defines MSG_NOSIGNAL but send()
         * returns ENOSYS for it, which once silently killed the whole Java bus
         * (framework/bus.c:211).  SIGPIPE is blocked for this thread instead. */
        ssize_t n = send(fd, p + off, len - off, 0);
        if (n > 0) { off += (size_t)n; continue; }
        if (n < 0 && (errno == EINTR)) continue;
        return -1;
    }
    return 0;
}

/*
 * Read whatever the renderer sent us and notice when it has gone.
 * Returns 1 when the link is still up, 0 when the peer closed it.
 *
 * TWO REASONS THIS EXISTS, both learned the hard way:
 *
 * 1. The link is bidirectional.  Luka's renderer sends back a frame-ready
 *    packet (upstream/server.c:96, cr_server_mark_frame_ready).  We have no
 *    use for it - on MHI2Q it gates a Java context - but if nobody ever reads
 *    it, that socket's receive buffer fills and eventually the renderer's own
 *    send blocks or fails.  So we drain it and throw it away.
 * 2. Without reading, a renderer that exits is invisible to us until the next
 *    write fails.  With no route there is no write, so a restarted renderer
 *    would sit unaccepted in the backlog.  recv() returning 0 is the only
 *    prompt notice we get.
 */
static int feed_drain_peer(int fd)
{
    for (;;) {
        unsigned char scratch[CR_PKT_SIZE];
        struct timeval tv;
        fd_set rfds;
        ssize_t n;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 0; tv.tv_usec = 0;          /* poll, never block */
        if (select(fd + 1, &rfds, NULL, NULL, &tv) <= 0) return 1;

        n = recv(fd, scratch, sizeof(scratch), 0);
        if (n > 0) continue;                    /* frame-ready chatter */
        if (n == 0) return 0;                   /* peer closed */
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;
        return 0;
    }
}

/* Re-send the current state to a renderer that has just connected.  Returns 0
 * when the link survived.  Called only from the sender thread, so g_feed.fd is
 * stable here; the snapshot is taken under the lock because the RGI thread
 * writes those fields. */
static int feed_replay_state(void)
{
    unsigned char man[CR_PKT_SIZE], prog[CR_PKT_SIZE];
    int have_man, have_prog, level = 0, mode = 0, blink = 0;

    pthread_mutex_lock(&g_feed.lock);
    have_man = g_feed.last_man_valid;
    if (have_man) memcpy(man, g_feed.last_man, CR_PKT_SIZE);
    have_prog = g_feed.prog_valid;
    if (have_prog) {
        level = g_feed.prog_level;
        mode  = g_feed.prog_mode;
        blink = g_feed.prog_blink;
    }
    pthread_mutex_unlock(&g_feed.lock);

    if (!have_man) return 0;     /* no route yet: nothing to replay */

    if (feed_write_all(g_feed.fd, man, CR_PKT_SIZE) != 0) return -1;

    if (have_prog) {
        build_progress(prog, level, mode,
                       mode ? (blink ? CR_PROGRESS_BLINK_HIGH : CR_PROGRESS_FILL)
                            : CR_PROGRESS_OFF);
        if (feed_write_all(g_feed.fd, prog, CR_PKT_SIZE) != 0) return -1;
    }
    LOG_INFO(LOG_MODULE, "state replayed to renderer (level=%d)", level);
    return 0;
}

static void *feed_thread(void *arg)
{
    sigset_t set;
    long last_blink = 0;
    int phase = 0;

    (void)arg;

    /* Block SIGPIPE for THIS thread only.  Changing it process-wide would
     * alter stock mm-ipod behaviour; a SIGPIPE here would kill CarPlay. */
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    (void)pthread_sigmask(SIG_BLOCK, &set, NULL);

    LOG_INFO(LOG_MODULE, "feed thread started, serving 127.0.0.1:%d", CR_TCP_PORT);

    for (;;) {
        unsigned char pkt[CR_PKT_SIZE];
        int have = 0;
        struct timespec deadline;
        int do_blink = 0;
        int level = 0, mode = 0;

        pthread_mutex_lock(&g_feed.lock);
        if (g_feed.shutdown) { pthread_mutex_unlock(&g_feed.lock); break; }

        if (g_feed.tail != g_feed.head) {
            memcpy(pkt, g_feed.ring[g_feed.tail], CR_PKT_SIZE);
            g_feed.tail = (g_feed.tail + 1) % FEED_RING;
            have = 1;
        } else {
            /* Wake at the blink cadence so the arrow can pulse without the
             * ~1 Hz RGI updates having to carry it. */
            clock_gettime(CLOCK_REALTIME, &deadline);
            deadline.tv_nsec += (long)BLINK_HALF_MS * 1000000L;
            if (deadline.tv_nsec >= 1000000000L) {
                deadline.tv_sec += deadline.tv_nsec / 1000000000L;
                deadline.tv_nsec %= 1000000000L;
            }
            (void)pthread_cond_timedwait(&g_feed.wake, &g_feed.lock, &deadline);
            if (g_feed.shutdown) { pthread_mutex_unlock(&g_feed.lock); break; }
        }
        if (g_feed.prog_valid && g_feed.prog_blink) {
            do_blink = 1;
            level = g_feed.prog_level;
            mode  = g_feed.prog_mode;
        }
        pthread_mutex_unlock(&g_feed.lock);

        if (g_feed.listen_fd < 0) {
            g_feed.listen_fd = feed_listen();
            if (g_feed.listen_fd < 0) {
                struct timespec nap;
                LOG_WARN(LOG_MODULE, "cannot listen on 127.0.0.1:%d errno=%d",
                         CR_TCP_PORT, errno);
                nap.tv_sec = 0;
                nap.tv_nsec = (long)RECONNECT_MS * 1000000L;
                while (nanosleep(&nap, &nap) != 0 && errno == EINTR) {}
                continue;
            }
            LOG_INFO(LOG_MODULE, "listening on 127.0.0.1:%d", CR_TCP_PORT);
        }

        /* Notice a renderer that went away while there was nothing to send,
         * so the next one is accepted at once instead of waiting in the
         * backlog for a write to fail. */
        if (g_feed.fd >= 0 && !feed_drain_peer(g_feed.fd)) {
            LOG_INFO(LOG_MODULE, "renderer disconnected");
            close(g_feed.fd);
            g_feed.fd = -1;
        }

        if (g_feed.fd < 0) {
            g_feed.fd = feed_accept(g_feed.listen_fd);
            if (g_feed.fd < 0) continue;   /* select() already waited */
            LOG_INFO(LOG_MODULE, "renderer connected");
            /* Replay the state the renderer missed.  It reconnects after every
             * restart of its own and after every mm-ipod restart, and without
             * this the panel stays blank until the next RGI update. */
            if (feed_replay_state() != 0) {
                close(g_feed.fd);
                g_feed.fd = -1;
                continue;
            }
        }

        if (have && feed_write_all(g_feed.fd, pkt, CR_PKT_SIZE) != 0) {
            LOG_WARN(LOG_MODULE, "send failed errno=%d; dropping link", errno);
            close(g_feed.fd);
            g_feed.fd = -1;
            continue;
        }

        if (do_blink && now_ms() - last_blink >= BLINK_HALF_MS) {
            unsigned char bp[CR_PKT_SIZE];
            last_blink = now_ms();
            phase ^= 1;
            build_progress(bp, level, mode, phase ? CR_PROGRESS_BLINK_HIGH
                                                  : CR_PROGRESS_BLINK_LOW);
            if (feed_write_all(g_feed.fd, bp, CR_PKT_SIZE) != 0) {
                close(g_feed.fd);
                g_feed.fd = -1;
            }
        }
    }

    if (g_feed.fd >= 0) { close(g_feed.fd); g_feed.fd = -1; }
    if (g_feed.listen_fd >= 0) { close(g_feed.listen_fd); g_feed.listen_fd = -1; }
    LOG_INFO(LOG_MODULE, "feed thread stopped (dropped=%d)", g_feed.dropped);
    return NULL;
}

/* ------------------------------------------------------------- public -- */

void k3346_overlay_feed_init(void)
{
    pthread_mutex_lock(&g_feed.lock);
    if (g_feed.started) { pthread_mutex_unlock(&g_feed.lock); return; }
    g_feed.started = 1;
    g_feed.shutdown = 0;
    g_feed.head = g_feed.tail = 0;
    pthread_mutex_unlock(&g_feed.lock);

    if (pthread_create(&g_feed.thread, NULL, feed_thread, NULL) != 0) {
        LOG_WARN(LOG_MODULE, "feed thread create failed; overlay will stay blank");
        g_feed.started = 0;
    }
}

void k3346_overlay_feed_shutdown(void)
{
    pthread_mutex_lock(&g_feed.lock);
    if (!g_feed.started) { pthread_mutex_unlock(&g_feed.lock); return; }
    g_feed.shutdown = 1;
    pthread_cond_broadcast(&g_feed.wake);
    pthread_mutex_unlock(&g_feed.lock);

    (void)pthread_join(g_feed.thread, NULL);
    g_feed.started = 0;
}

void k3346_overlay_feed_clear(const char *reason)
{
    unsigned char pkt[CR_PKT_SIZE];

    (void)reason;   /* only used by the log line; builds with logging off */

    if (!g_feed.started) return;
    build_clear(pkt);

    pthread_mutex_lock(&g_feed.lock);
    g_feed.prog_valid = 0;
    g_feed.prog_blink = 0;
    /* Drop the replay too: a renderer connecting after the route ended must
     * not be handed the arrow the route ended with. */
    g_feed.last_man_valid = 0;
    ring_push_locked(pkt);
    pthread_cond_signal(&g_feed.wake);
    pthread_mutex_unlock(&g_feed.lock);

    LOG_INFO(LOG_MODULE, "clear (%s)", reason ? reason : "?");
}

int k3346_overlay_build_lanes(const rgd_lane_guidance_t *lanes,
                              unsigned long token,
                              unsigned char *out, int max_packets)
{
    int count, i, j, n = 0;
    unsigned char *pkt;

    if (lanes == NULL || out == NULL) return 0;
    count = lanes->lane_count;
    if (count > MAX_WIRE_LANES) count = MAX_WIRE_LANES;
    if (count <= 0) return 0;
    if (max_packets < count + 2) return 0;

    pkt = out + (size_t)n * CR_PKT_SIZE;
    memset(pkt, 0, CR_PKT_SIZE);
    pkt[0] = CMD_LANES_BEGIN;
    put_be32(&pkt[2 + 0], token);
    pkt[2 + 4] = (unsigned char)count;
    pkt[2 + 5] = 1;                                   /* complete */
    pkt[2 + 6] = 1;                                   /* showing */
    put_be32(&pkt[2 + 8], (unsigned long)lanes->lane_guidance_index);
    n++;

    for (i = 0; i < count; i++) {
        const rgd_lane_t *ln = &lanes->lanes[i];
        int na = ln->angle_count;
        if (na > MAX_WIRE_LANE_ANGLES) na = MAX_WIRE_LANE_ANGLES;

        pkt = out + (size_t)n * CR_PKT_SIZE;
        memset(pkt, 0, CR_PKT_SIZE);
        pkt[0] = CMD_LANES_LANE;
        put_be32(&pkt[2 + 0], token);
        pkt[2 + 4] = (unsigned char)i;
        put_be16(&pkt[2 + 5], (int)ln->position);
        pkt[2 + 7] = ln->status;
        pkt[2 + 8] = (unsigned char)na;
        /* Lane angles stay in WHOLE degrees - only maneuver geometry is in
         * half-degrees (Luka's HUD_RENDERER.md:74).  Mixing the two units here
         * would tilt every lane arrow by a factor of two. */
        put_be16(&pkt[2 + 9], (int)ln->direction);
        for (j = 0; j < na; j++)
            put_be16(&pkt[2 + 11 + j * 2], (int)ln->angles[j]);
        n++;
    }

    pkt = out + (size_t)n * CR_PKT_SIZE;
    memset(pkt, 0, CR_PKT_SIZE);
    pkt[0] = CMD_LANES_COMMIT;
    put_be32(&pkt[2 + 0], token);
    n++;

    return n;
}

/* Caller holds the lock.  The whole bundle goes in under one acquisition so
 * BEGIN/LANE/COMMIT can never be split by another publisher. */
static void queue_lanes_locked(const rgd_lane_guidance_t *lanes, unsigned long token)
{
    unsigned char bundle[K3346_OVERLAY_MAX_LANE_PKTS * CR_PKT_SIZE];
    int n = k3346_overlay_build_lanes(lanes, token, bundle,
                                      K3346_OVERLAY_MAX_LANE_PKTS);
    int i;
    for (i = 0; i < n; i++)
        ring_push_locked(bundle + (size_t)i * CR_PKT_SIZE);
}

int k3346_overlay_build_maneuver(const rgd_update_t *upd,
                                 const rgd_maneuver_t *man,
                                 unsigned char pkt[K3346_OVERLAY_PKT],
                                 k3346_overlay_progress_t *prog)
{
    overlay_mapping_t m;
    int main_el = 0, dir = 0;
    int mt, angle, angle_present, junction, side;
    int dist, denom, level, mode, blink, blink_state;

    if (prog) { prog->level = 16; prog->mode = 0; prog->blink = 0; }
    if (upd == NULL || man == NULL || pkt == NULL) return 0;
    if (man->present == 0) return 0;

    mt = (man->present & RGD_MAN_TYPE) ? (int)man->maneuver_type : MT_NOT_SET;
    if (mt == 255) mt = MT_NOT_SET;      /* rgd_tlv's "not set" encoding */

    angle = (man->present & RGD_MAN_EXIT_ANGLE) ? (int)man->exit_angle : 1000;
    angle_present = (angle != 1000 && angle != -1000 && angle != -1);
    junction = (man->present & RGD_MAN_JUNCTION_TYPE) ? (int)man->junction_type
                                                      : JUNCTION_SINGLE_INTERSECTION;
    side = (man->present & RGD_MAN_DRIVING_SIDE) ? (int)man->driving_side
                                                 : DRIVING_SIDE_RIGHT;

    /* Same two-step chain as Luka: iAP2 -> BAP -> renderer icon.  Step one is
     * the mapper the VC already uses, so the arrow cannot contradict the text
     * printed next to it. */
    k3346_maneuver_map(mt, angle, junction, side, &main_el, &dir);
    from_bap(&m, main_el, dir, side);
    with_carplay_geometry(&m, mt, angle, angle_present,
                          man->junction_angles, man->junction_angle_count);

    if (m.icon == ICON_NONE) return 0;

    /* Progress: 16 = far, 0 = at the maneuver. */
    dist = (upd->present & RGD_UPD_DIST_TO_MANEUVER) ? (int)upd->dist_to_maneuver : -1;
    denom = (mt == MT_OFF_RAMP || mt == MT_ON_RAMP
             || mt == MT_HIGHWAY_OFF_RAMP_LEFT || mt == MT_HIGHWAY_OFF_RAMP_RIGHT
             || mt == MT_CHANGE_HIGHWAY || mt == MT_CHANGE_HIGHWAY_LEFT
             || mt == MT_CHANGE_HIGHWAY_RIGHT) ? HIGHWAY_PREPARE_M : CITY_PREPARE_M;

    if (dist >= 0) {
        level = dist * 16 / denom;
        if (level > 16) level = 16;
        if (level < 0)  level = 0;
        mode = 1;
        blink = (dist * 100 <= denom * BLINK_PERCENT);
    } else {
        level = 16;
        mode = 0;
        blink = 0;
    }
    blink_state = mode ? (blink ? CR_PROGRESS_BLINK_HIGH : CR_PROGRESS_FILL)
                       : CR_PROGRESS_OFF;

    build_maneuver(pkt, &m, level, mode, blink_state);
    if (prog) { prog->level = level; prog->mode = mode; prog->blink = blink; }

    LOG_DEBUG(LOG_MODULE,
              "overlay_build icon=%d dir=%d angle=%d side=%d roads=%d level=%d blink=%d",
              m.icon, m.direction, m.exit_angle, m.driving_side,
              m.junction_count, level, blink);
    return 1;
}

void k3346_overlay_feed_publish(const rgd_update_t *upd,
                                const rgd_maneuver_t *man,
                                const rgd_lane_guidance_t *lanes)
{
    unsigned char pkt[CR_PKT_SIZE];
    k3346_overlay_progress_t prog;
    static unsigned long s_token;

    if (!g_feed.started || upd == NULL) return;

    /* No route, no maneuver, or a maneuver that maps to no icon: blank it
     * rather than leave a stale arrow standing. */
    if (!k3346_overlay_build_maneuver(upd, man, pkt, &prog)) {
        k3346_overlay_feed_clear("no drawable maneuver");
        return;
    }

    pthread_mutex_lock(&g_feed.lock);
    g_feed.prog_valid = 1;
    g_feed.prog_level = prog.level;
    g_feed.prog_mode  = prog.mode;
    g_feed.prog_blink = prog.blink;
    /* Keep it for a renderer that connects after this update. */
    memcpy(g_feed.last_man, pkt, CR_PKT_SIZE);
    g_feed.last_man_valid = 1;
    ring_push_locked(pkt);
    if (lanes != NULL && lanes->present != 0 && lanes->lane_count > 0)
        queue_lanes_locked(lanes, ++s_token);
    pthread_cond_signal(&g_feed.wake);
    pthread_mutex_unlock(&g_feed.lock);

    LOG_DEBUG(LOG_MODULE, "overlay_send level=%d blink=%d lanes=%d",
              prog.level, prog.blink,
              (lanes != NULL) ? (int)lanes->lane_count : 0);
}
