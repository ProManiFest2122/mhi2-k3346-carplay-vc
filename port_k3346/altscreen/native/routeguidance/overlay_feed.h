/*
 * overlay_feed — feeds the cluster overlay renderer (vc_overlay) straight from
 * the RGI hook inside mm-ipod.
 *
 * TOPOLOGY: THE SAME WAY ROUND AS LUKA - THE PRODUCER LISTENS
 * On MHI2Q, Java is the server on :19800 and the renderer connects to it.
 * Here there is no Java in this path at all - deliberately, because the HMI JAR
 * is the one component that can bootloop the car - so THIS HOOK takes the
 * server role Java has there, and Luka's renderer stays the client it already
 * is (overlay/upstream/server.c: "Single-thread TCP CLIENT").  That is what
 * lets his server.c be used unmodified, reconnect loop and all.
 *
 * This comment used to claim the opposite - that the renderer was the server
 * and this hook the client - and the code followed the comment only halfway:
 * BOTH ends called connect() and nobody ever bound the port.  The car run of
 * 2026-10-27 shows the cost exactly: the renderer logged "127.0.0.1:19800 not
 * yet listening, will retry" all session, drew commands=0, no arrow appeared,
 * and netstat had no 19800 entry at all.  mm-ipod restarting on every iPhone
 * connect is not an argument against listening here: that is precisely the
 * case the renderer's 200 ms reconnect loop exists for.
 *
 * The wire format is Luka's, unchanged (overlay/upstream/protocol.h): 48-byte
 * packets, CMD_MANEUVER / CMD_PROGRESS / CMD_CLEAR / CMD_LANES_*.  Keeping his
 * format means his renderer code needs no edits and his protocol checker still
 * means something.
 *
 * THREADING, AND THE SIGPIPE TRAP
 * Nothing here sends from the iAP2 driver thread.  Packets are copied into a
 * bounded ring and a dedicated sender thread owns the socket, exactly as
 * framework/bus.c does for the Java bus - and for the same reason: a socket
 * write to a renderer that has just exited would raise SIGPIPE, and SIGPIPE in
 * mm-ipod kills CarPlay outright.  SIGPIPE is blocked per-thread
 * (pthread_sigmask), never globally, so stock mm-ipod's own disposition is
 * untouched.  MSG_NOSIGNAL is NOT usable: QNX 6.5 io-pkt defines the macro but
 * send() returns ENOSYS, which once silently killed the whole bus.
 */

#ifndef K3346_OVERLAY_FEED_H
#define K3346_OVERLAY_FEED_H

#include "rgd_tlv.h"

/* Starts the sender thread.  Safe to call more than once. */
void k3346_overlay_feed_init(void);

/* Stops the sender thread and closes the link. */
void k3346_overlay_feed_shutdown(void);

/*
 * Called from the RGI publish path once the snapshot for Java has been built,
 * so the overlay and the VC always describe the same maneuver.
 *   upd   - merged route update (never NULL)
 *   man   - the active maneuver, or NULL when there is none
 *   lanes - lane guidance linked to that maneuver, or NULL
 */
void k3346_overlay_feed_publish(const rgd_update_t *upd,
                                const rgd_maneuver_t *man,
                                const rgd_lane_guidance_t *lanes);

/* Route ended / CarPlay gone: blank the overlay but keep the link. */
void k3346_overlay_feed_clear(const char *reason);

/*
 * The wire builders, exposed so native/tests can check the bytes without a
 * socket or a renderer.  publish() uses exactly these, so a passing test is a
 * statement about what actually goes on the wire, not about a parallel copy.
 */
#define K3346_OVERLAY_PKT       48
#define K3346_OVERLAY_MAX_LANE_PKTS 10   /* BEGIN + up to 8 lanes + COMMIT */

/* Arrow fill state that goes with the maneuver; the sender thread keeps it so
 * the blink can run on its own 600 ms clock instead of the ~1 Hz RGI updates. */
typedef struct {
    int level;   /* 0..16; 16 = far away, 0 = at the maneuver */
    int mode;    /* 0 = no usable distance, 1 = fill */
    int blink;   /* 1 once inside the final 20% */
} k3346_overlay_progress_t;

/* Builds the CMD_MANEUVER packet for this maneuver.
 * Returns 1 when a packet was produced, 0 when the maneuver maps to nothing
 * (the caller then sends CMD_CLEAR instead). */
int k3346_overlay_build_maneuver(const rgd_update_t *upd,
                                 const rgd_maneuver_t *man,
                                 unsigned char pkt[K3346_OVERLAY_PKT],
                                 k3346_overlay_progress_t *prog);

/* Builds the BEGIN/LANE.../COMMIT bundle.  Returns the number of packets
 * written to out (0 when there is nothing to show). */
int k3346_overlay_build_lanes(const rgd_lane_guidance_t *lanes,
                              unsigned long token,
                              unsigned char *out, int max_packets);

#endif /* K3346_OVERLAY_FEED_H */
