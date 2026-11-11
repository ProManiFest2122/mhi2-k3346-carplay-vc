/* Where the driver wants the arrival time, chosen in the MMI options.  The HMI
 * row (java overrides, K3346EtaRow) keeps the value in its own storage and
 * mirrors it here, because /tmp is what the HMI and dio_manager share.
 *
 *   0 capsule  time on the VC bar, no ETA box on the map
 *   1 map      ETA box on the map, the bar keeps the street
 *   2 both
 *
 * A missing file means the default.  A file caught half written changes nothing
 * (/tmp on QNX has no rename, see overlay_ready.h).
 *
 * With big clocks the call card takes the place of the ETA box, so while there is
 * a card the map goes without the box and BAPBridge puts the time on the bar.  The
 * choice in the options stays as it is and comes back after the call.  The same
 * table is in K3346NavBarText.timesOnBar. */
#ifndef K3346_ETA_MODE_H
#define K3346_ETA_MODE_H
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#define K3346_ETA_MODE_PATH "/tmp/k3346-eta-mode"
/* The maneuver card option (overrides K3346EtaRow).  Nothing we send brings the card
 * back (run-59, run-60: the plain URL, VIEW, stopUI).  The phone draws it again with
 * the next maneuver (car, rc19) or in the next session, so off goes for every view. */
#define K3346_CARD_MODE_PATH "/tmp/k3346-card-mode"
/* Written by BAPBridge's bundle while the app sends no route guidance. */
#define K3346_NO_RGI_PATH "/tmp/k3346-no-rgi"
/* The VC skin, from the stock LastmodeStorage: 1 = Sport (BAPBridge, RE luka-dev). */
#define K3346_SKIN_PATH "/tmp/vc-skin"
#define K3346_CARD_FULL 0
#define K3346_CARD_NONE 1
/* run-59: the box asked for 7,7 s after a layout change never came, at 11,8 s it did.
 * That was a showUI of its own.  The wait for a new skin, and the fallback. */
#define K3346_ETA_AFTER_LAYOUT_MS 12000ULL
#define K3346_ETA_CAPSULE 0
#define K3346_ETA_MAP     1
#define K3346_ETA_BOTH    2
#define K3346_CALL_CARD_PATH "/tmp/vc-call-card"
#define K3346_SLOT_BIG    1
#define K3346_LAYOUT_SLOT_PATH "/tmp/vc-capsule-slot"
/* The layout the HMI holds, as a slot: BAPBridge.pollViewSize. */
#define K3346_VIEW_SIZE_PATH "/tmp/vc-view-size"
/* VIEW every 2 to 3 s kept the ETA box right, faster could leave it wrong although
 * every showUI was acknowledged in 21 ms at most (car, rc19, run-74).  That was with
 * showUI before updateViewArea.  With the area first (rc21) 2 s was what left the old box
 * on screen after a quick VIEW (films of run-83, run-87), so the gap is shorter now.
 * A press held here also holds its updateViewArea.  The tests of the command sequence
 * compile this out (test_v8s_control_mock.c). */
#ifndef K3346_SHOW_MIN_INTERVAL_MS
#define K3346_SHOW_MIN_INTERVAL_MS 500ULL
#endif

static int k3346_eta_mode_read(const char *path, int previous)
{
    char buf[8];
    size_t got;
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return errno == ENOENT ? K3346_ETA_CAPSULE : previous;
    got = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (got != 2 || buf[1] != '\n' || buf[0] < '0' || buf[0] > '2')
        return previous;
    return buf[0] - '0';
}

/* The MMI option "Call card on VC" (K3346EtaRow): 0 smoked, 1 light, 2 off, in the
 * format of the ETA option, so k3346_eta_mode_read() reads it (no file = 0).  With the
 * card off a call takes nothing from the map: there is no card to make room for. */
#define K3346_CALLCARD_MODE_PATH "/tmp/k3346-callcard-mode"
#define K3346_CALLCARD_OFF 2

static int k3346_call_card_shown(int call_visible, int callcard_mode)
{
    return call_visible && callcard_mode != K3346_CALLCARD_OFF;
}

static int k3346_eta_on_map(int mode)
{
    return mode == K3346_ETA_MAP || mode == K3346_ETA_BOTH;
}

/* 0 = send it now.  No showUI yet in this session: nothing to wait for. */
static int k3346_show_held(uint64_t last_show_ms, uint64_t now)
{
    return last_show_ms != 0 && now - last_show_ms < K3346_SHOW_MIN_INTERVAL_MS;
}

/* The layout slot, or -1 when the file says nothing (missing, or caught half written:
 * /tmp on QNX has no rename). */
static int k3346_view_size_read(const char *path)
{
    char buf[8];
    size_t got;
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return -1;
    got = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (got != 2 || buf[1] != '\n' || (buf[0] != '0' && buf[0] != '1'))
        return -1;
    return buf[0] - '0';
}

/* Before the VC has reported a layout.  The overlay starts seconds later than the HMI
 * knows (run-73: 47.66 s against 56.78 s), and a wrong guess moves the map on the VC. */
static int k3346_layout_early(int overlay_slot, int hmi_slot)
{
    return overlay_slot >= 0 ? overlay_slot : hmi_slot;
}

/* overlay/call_card.h has the format; only the state in the first line counts here. */
static int k3346_call_visible_read(const char *path, int previous)
{
    char buf[256];
    size_t got;
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return errno == ENOENT ? 0 : previous;
    got = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (got < 6 || memcmp(buf + got - 5, "\nEND\n", 5) != 0)
        return previous;
    if (buf[0] == '\n')
        return 0;
    if (buf[1] == '\n' && (buf[0] == 'R' || buf[0] == 'D' || buf[0] == 'A' || buf[0] == 'H'))
        return 1;
    return previous;
}

/* The overlay's layout report, "<slot> <draw>\n": it knows the layout from its saved
 * copy after a replug, when the VC says nothing until VIEW (run-55).  -1 = no answer. */
static int k3346_layout_slot_read(const char *path)
{
    char buf[8];
    size_t got;
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return -1;
    got = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (got != 4 || buf[1] != ' ' || buf[3] != '\n' || (buf[0] != '0' && buf[0] != '1'))
        return -1;
    return buf[0] - '0';
}

static int k3346_card_mode_read(const char *path, int previous)
{
    char buf[8];
    size_t got;
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return errno == ENOENT ? K3346_CARD_FULL : previous;
    got = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (got != 2 || buf[1] != '\n' || (buf[0] != '0' && buf[0] != '1'))
        return previous;
    return buf[0] - '0';
}

/* No file means Classic, what the car does without any of this. */
static int k3346_skin_read(const char *path, int previous)
{
    char buf[8];
    size_t got;
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return errno == ENOENT ? 0 : previous;
    got = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (got != 2 || buf[1] != '\n' || (buf[0] != '0' && buf[0] != '1'))
        return previous;
    return buf[0] - '0';
}

/* The phone is told 9 areas; everything here still counts views 0..3, where
 * view / 2 is the layout.  Sport has one more area for the big-gauges slot: its
 * rev counter sits where Classic has the map.  Apple Maps has two more for the small
 * gauges with a route (nav_app.h): without the card it puts its ETA box in the middle
 * of the area, so that area is moved left; with the card it puts the box under the
 * card, and at 480 px only the arrival time fits (rc23, photo 2), so that area is as
 * wide as the other.  And one for the big gauges with a route, without the card: Apple
 * picks the shape of the box from the width of the area, and at 460 px that is the
 * arrival time alone (rc24, run-95, IMG_0775), so this one is as wide as on the small
 * gauges, with the same middle as view 3.  At 672 px the box was the full one on both
 * layouts, after every VIEW (rc25, run-97, IMG_0776).
 * Since rc29 the phone draws no ETA box for these apps (k3346_eta_map_effective); the
 * areas stay, so the car marker and the card keep their places.
 * Since rc30 the areas with a route end where our own boxes begin (y 334), so the phone
 * keeps its car marker and the street label above them (car, rc29, IMG_0783..0788:
 * Google Maps draws both low, right under our box).  Only the big gauges WITH the card
 * keep the full height: the card on top needs it (mib2high-lvds-viewareas.conf).  So
 * an app other than Apple Maps without the card has its own area there (view 8). */
#define K3346_VIEW_APPLE_FREE 5
#define K3346_VIEW_APPLE_CARD 6
#define K3346_VIEW_APPLE_BIG  7
#define K3346_VIEW_OTHER_BIG  8
#define K3346_APPLE_NONE 0
#define K3346_APPLE_FREE 1
#define K3346_APPLE_CARD 2
#define K3346_OTHER_FREE 3

static int k3346_wire_view(int view, int sport, int apple_area)
{
    if (view == 1 && apple_area == K3346_APPLE_FREE)
        return K3346_VIEW_APPLE_FREE;
    if (view == 1 && apple_area == K3346_APPLE_CARD)
        return K3346_VIEW_APPLE_CARD;
    if (sport && (view == 2 || view == 3))
        return 4;
    if (view == 3 && apple_area == K3346_APPLE_FREE)
        return K3346_VIEW_APPLE_BIG;
    if (view == 3 && apple_area == K3346_OTHER_FREE)
        return K3346_VIEW_OTHER_BIG;
    return view;
}

/* Which areas the app and the card option take.  Other apps differ only on the big
 * gauges, by the card (view 8 or 3); on the small ones they have view 1 either way. */
static int k3346_apple_area(int apple, int card)
{
    if (!apple)
        return card == K3346_CARD_NONE ? K3346_OTHER_FREE : K3346_APPLE_NONE;
    return card == K3346_CARD_NONE ? K3346_APPLE_FREE : K3346_APPLE_CARD;
}

/* Our tile shows the maneuver only when the app sends one, so an app without
 * route guidance keeps the card it draws itself. */
static int k3346_card_effective(int mode, int no_rgi)
{
    return mode == K3346_CARD_NONE && no_rgi ? K3346_CARD_FULL : mode;
}

/* Whether the box may be asked for now.  One already on screen stays. */
static int k3346_eta_held(int on_map, int ui_eta_on_map, uint64_t layout_change_ms,
                          uint64_t delay_ms, uint64_t now)
{
    if (on_map && !ui_eta_on_map && layout_change_ms != 0 &&
        now < layout_change_ms + delay_ms)
        return 0;
    return on_map;
}

/* Is the PHONE asked for its ETA box (showETA=yes)?  Since rc29 only for an app without
 * route guidance.  The phone lays that box out as it likes: under the maneuver card it
 * was cut to the arrival time (Apple) or dropped (Google, big gauges), Google on the
 * small gauges left the distance out, and asking again in a second showUI made the map
 * blink (car, rc28, run-99 and its films).  So for an app that sends route guidance the
 * overlay draws our own box from that data (overlay/eta_pill.h, BAPBridge.publishBar),
 * and the URL never names showETA.  An app without it gives us nothing to draw, so it
 * keeps the phone's box; with big clocks the call card takes its place. */
static int k3346_eta_map_effective(int mode, int call_visible, int slot, int no_rgi)
{
    return no_rgi && k3346_eta_on_map(mode) && !(call_visible && slot == K3346_SLOT_BIG);
}

#endif
