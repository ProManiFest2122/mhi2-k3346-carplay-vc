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
/* The maneuver card option (overrides K3346EtaRow).  The phone does not give the
 * card back in the same session, whatever is sent (run-59, run-60: the plain URL,
 * VIEW, stopUI), so off goes for every view and on waits for the next session. */
#define K3346_CARD_MODE_PATH "/tmp/k3346-card-mode"
/* Written by BAPBridge's bundle while the app sends no route guidance. */
#define K3346_NO_RGI_PATH "/tmp/k3346-no-rgi"
#define K3346_CARD_FULL 0
#define K3346_CARD_NONE 1
/* run-59: the box asked for 7,7 s after a layout change never came, at 11,8 s it did. */
#define K3346_ETA_AFTER_LAYOUT_MS 12000ULL
#define K3346_ETA_CAPSULE 0
#define K3346_ETA_MAP     1
#define K3346_ETA_BOTH    2
#define K3346_CALL_CARD_PATH "/tmp/vc-call-card"
#define K3346_SLOT_BIG    1
#define K3346_LAYOUT_SLOT_PATH "/tmp/vc-capsule-slot"

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

static int k3346_eta_on_map(int mode)
{
    return mode == K3346_ETA_MAP || mode == K3346_ETA_BOTH;
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

/* Our tile shows the maneuver only when the app sends one, so an app without
 * route guidance keeps the card it draws itself. */
static int k3346_card_effective(int mode, int no_rgi)
{
    return mode == K3346_CARD_NONE && no_rgi ? K3346_CARD_FULL : mode;
}

/* Whether the box may be asked for now.  One already on screen stays. */
static int k3346_eta_held(int on_map, int ui_eta_on_map, uint64_t layout_change_ms, uint64_t now)
{
    if (on_map && !ui_eta_on_map && layout_change_ms != 0 &&
        now < layout_change_ms + K3346_ETA_AFTER_LAYOUT_MS)
        return 0;
    return on_map;
}

static int k3346_eta_map_effective(int mode, int call_visible, int slot)
{
    return k3346_eta_on_map(mode) && !(call_visible && slot == K3346_SLOT_BIG);
}

#endif
