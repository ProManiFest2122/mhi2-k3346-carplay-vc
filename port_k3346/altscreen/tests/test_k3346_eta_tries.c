/* The real showUI sequence and the ETA box.  Since rc29 the phone is asked for its box
 * (showETA=yes) only by an app without route guidance; for Apple Maps and Google Maps
 * the overlay draws our own (overlay/eta_pill.h), so their URLs never name it and a
 * layout change is one showUI: the two-step of rc24 made the map blink (rc28, run-99). */
#define main control_mock_main
#include "test_v8s_control_mock.c"
#undef main

#define MAP_PLAIN  "maps:/car/instrumentcluster/map"
#define CARD_SMALL "maps:/car/instrumentcluster?maneuverLayout=leftAligned"
#define CARD_BIG   "maps:/car/instrumentcluster?maneuverLayout=topAligned"
#define CARD_SMALL_BOX "maps:/car/instrumentcluster?showETA=yes&maneuverLayout=leftAligned"
#define CARD_BIG_BOX   "maps:/car/instrumentcluster?showETA=yes&maneuverLayout=topAligned"

static altscreen_state *s;

/* the area sent for a logical view: an app other than Apple Maps without the card has
 * its own on the big gauges of Classic (rc30, view 8) */
static int wire_of(int view)
{
    return view == 3 && g_card_mode == K3346_CARD_NONE && !g_nav_apple && !g_sport ? 8 : view;
}

static void put_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");

    assert(f);
    fputs(text, f);
    fclose(f);
}

static void mode(int value)
{
    FILE *f = fopen(K3346_ETA_MODE_PATH, "w");

    assert(f);
    fprintf(f, "%d\n", value);
    fclose(f);
    eta_mode_poll();
}

/* one showUI + updateViewArea + forceKeyFrame, acked in order; the URL exactly */
static void show_sequence(int view, const char *url)
{
    int b = nsends;

    if (s->last_ui_slot >= 0 && s->last_ui_slot != view / 2) {
        /* a layout change: the area first */
        step("updateViewArea");
        assert(sends[b].view == wire_of(view));
        idle();
        ack(b, 0);
        step("showUI");
        assert(!strcmp(sends[b + 1].url, url));
        ack(b + 1, 0);
    } else {
        step("showUI");
        assert(!strcmp(sends[b].url, url));
        idle();
        ack(b, 0);
        step("updateViewArea");
        assert(sends[b + 1].view == wire_of(view));
        ack(b + 1, 0);
    }
    step("forceKeyFrame");
    ack(b + 2, 0);
    assert(nsends == b + 3);
    assert((strstr(url, "showETA=yes") != NULL) == s->ui_eta_on_map);
}

static void quiet(void)
{
    idle();
    assert(s->pending_view < 0);
    assert(s->control_stage == ALTSCREEN_CONTROL_IDLE);
    eta_mode_poll();
    idle();
}

/* nothing at all goes to the phone, now or after the 12 s of run-59 */
static void nothing_sent(void)
{
    int b = nsends;

    eta_mode_poll();
    idle();
    s->layout_change_ms = s->layout_change_ms ? now_ms() - K3346_ETA_AFTER_LAYOUT_MS - 1 : 0;
    eta_mode_poll();
    idle();
    assert(nsends == b);
    assert(s->pending_view < 0);
}

static void press_view(int slot)
{
    g_layout_slot = slot;
    queue_route_view_for_receiver(s->receiver, 0);
}

static void wait_over(void)
{
    s->layout_change_ms = now_ms() - K3346_ETA_AFTER_LAYOUT_MS - 1;
}

int main(void)
{
    FILE *marker;
    int b, i;

    prepare();
    remove(K3346_CALL_CARD_PATH);
    remove(K3346_CARD_MODE_PATH);
    remove(K3346_NO_RGI_PATH);
    remove(K3346_SKIN_PATH);
    remove(K3346_LAYOUT_SLOT_PATH);
    remove(K3346_VIEW_SIZE_PATH);
    remove(K3346_NAV_APPLE_PATH);
    g_bap54_layout_known = 1;
    g_layout_slot = 0;
    marker = fopen(ALTSCREEN_ACTIVE_MARKER, "w");
    assert(marker);
    fclose(marker);
    mode(1);

    pthread_mutex_lock(&g_lock);
    s = alloc_state_locked((void *)0x1001);
    s->screen_started = 1;
    s->route_known = 1;
    s->route_active = 1;
    pthread_mutex_unlock(&g_lock);

    /* --- an app with route guidance: the overlay draws the box, the phone none --- */

    /* the option "Map" from the start, the card on: the first showUI has no showETA */
    request_cluster(s);
    show_sequence(1, CARD_SMALL);
    quiet();
    assert(s->ui_eta_on_map == 0 && s->ui_card == K3346_CARD_FULL);

    /* the ETA option is the overlay's business now: no showUI for any of its values */
    b = nsends;
    mode(0);
    idle();
    mode(2);
    idle();
    mode(1);
    idle();
    assert(nsends == b);
    nothing_sent();

    /* VIEW with the card: the area, ONE showUI, the key frame.  Nothing follows it. */
    for (i = 0; i < 3; i++) {
        press_view(1);
        show_sequence(3, CARD_BIG);
        assert(s->ui_card == K3346_CARD_FULL && s->layout_change_ms != 0);
        assert(s->eta_delay_ms == K3346_ETA_AFTER_LAYOUT_MS);
        quiet();
        nothing_sent();
        press_view(0);
        show_sequence(1, CARD_SMALL);
        quiet();
        nothing_sent();
    }

    /* Brak: the same plain URL on both layouts, one showUI per VIEW */
    put_file(K3346_CARD_MODE_PATH, "1\n");
    eta_mode_poll();
    show_sequence(1, MAP_PLAIN);
    assert(s->ui_card == K3346_CARD_NONE);
    quiet();
    for (i = 0; i < 2; i++) {
        press_view(1);
        show_sequence(3, MAP_PLAIN);
        quiet();
        nothing_sent();
        press_view(0);
        show_sequence(1, MAP_PLAIN);
        quiet();
        nothing_sent();
    }

    /* a route change: the area alone */
    s->route_active = 0;
    queue_route_view_for_receiver(s->receiver, 0);
    b = nsends;
    step("updateViewArea");
    assert(sends[b].view == 0);
    ack(b, 0);
    step("forceKeyFrame");
    ack(b + 1, 0);
    quiet();
    s->route_active = 1;
    queue_route_view_for_receiver(s->receiver, 0);
    b = nsends;
    step("updateViewArea");
    assert(sends[b].view == 1);
    ack(b, 0);
    step("forceKeyFrame");
    ack(b + 1, 0);
    quiet();

    /* a call, on either layout: there is no box of the phone to take away */
    press_view(1);
    show_sequence(3, MAP_PLAIN);
    quiet();
    put_file(K3346_CALL_CARD_PATH, "A\nname\nlabel\nEND\n");
    nothing_sent();
    put_file(K3346_CALLCARD_MODE_PATH, "2\n");
    nothing_sent();
    remove(K3346_CALLCARD_MODE_PATH);
    remove(K3346_CALL_CARD_PATH);
    nothing_sent();

    /* Pełna again */
    remove(K3346_CARD_MODE_PATH);
    eta_mode_poll();
    show_sequence(3, CARD_BIG);
    quiet();

    /* --- an app without route guidance: nothing to draw from, the phone's box --- */

    /* run-59: a box asked for in a showUI of its own right after VIEW waits 12 s */
    put_file(K3346_NO_RGI_PATH, "1\n");
    s->layout_change_ms = now_ms() - K3346_ETA_AFTER_LAYOUT_MS + 1000;
    b = nsends;
    eta_mode_poll();
    idle();
    assert(nsends == b && s->pending_view < 0);
    wait_over();
    eta_mode_poll();
    show_sequence(3, CARD_BIG_BOX);
    quiet();

    /* VIEW: the card and the box in the one showUI of the layout change, no second one */
    press_view(0);
    show_sequence(1, CARD_SMALL_BOX);
    quiet();
    nothing_sent();
    press_view(1);
    show_sequence(3, CARD_BIG_BOX);
    quiet();
    nothing_sent();

    /* the option: one showUI each way, on the layout, the card stays */
    mode(0);
    show_sequence(3, CARD_BIG);
    quiet();
    wait_over();
    mode(1);
    show_sequence(3, CARD_BIG_BOX);
    quiet();
    mode(2);
    idle();
    quiet();

    /* Brak chosen: this app keeps the card it draws itself, so nothing changes */
    put_file(K3346_CARD_MODE_PATH, "1\n");
    nothing_sent();
    assert(s->ui_card == K3346_CARD_FULL);
    remove(K3346_CARD_MODE_PATH);
    nothing_sent();
    mode(1);
    idle();

    /* run-81: a call.  The big gauges go without the box, the small ones keep it */
    put_file(K3346_CALL_CARD_PATH, "A\nname\nlabel\nEND\n");
    eta_mode_poll();
    show_sequence(3, CARD_BIG);
    quiet();
    press_view(0);
    show_sequence(1, CARD_SMALL_BOX);
    quiet();
    press_view(1);
    show_sequence(3, CARD_BIG);
    quiet();
    nothing_sent();

    /* rc28: the call card switched off in the MMI menu.  A call takes nothing from the
     * map then; switched on (smoked or light) during the call, it does again. */
    wait_over();
    put_file(K3346_CALLCARD_MODE_PATH, "2\n");
    eta_mode_poll();
    show_sequence(3, CARD_BIG_BOX);
    quiet();
    put_file(K3346_CALLCARD_MODE_PATH, "0\n");
    eta_mode_poll();
    show_sequence(3, CARD_BIG);
    quiet();
    wait_over();
    put_file(K3346_CALLCARD_MODE_PATH, "2\n");
    eta_mode_poll();
    show_sequence(3, CARD_BIG_BOX);
    quiet();
    put_file(K3346_CALLCARD_MODE_PATH, "1\n");
    eta_mode_poll();
    show_sequence(3, CARD_BIG);
    quiet();
    /* a half-written option keeps what was there: still light, still no box */
    put_file(K3346_CALLCARD_MODE_PATH, "2");
    nothing_sent();
    /* the option file gone: the default, smoked, so the card is on */
    remove(K3346_CALLCARD_MODE_PATH);
    nothing_sent();
    wait_over();
    remove(K3346_CALL_CARD_PATH);
    eta_mode_poll();
    show_sequence(3, CARD_BIG_BOX);
    quiet();

    /* the app sends route guidance again: the phone's box goes, ours takes over */
    remove(K3346_NO_RGI_PATH);
    eta_mode_poll();
    show_sequence(3, CARD_BIG);
    quiet();
    nothing_sent();
    assert(s->ui_eta_on_map == 0);

    assert(strcmp(K3346_CALLCARD_MODE_PATH, "/tmp/k3346-callcard-mode") == 0);
    assert(k3346_call_card_shown(1, 0) && k3346_call_card_shown(1, 1));
    assert(!k3346_call_card_shown(1, 2) && !k3346_call_card_shown(0, 0));

    remove(K3346_CARD_MODE_PATH);
    remove(K3346_NO_RGI_PATH);
    remove(K3346_CALLCARD_MODE_PATH);
    unlink(ALTSCREEN_ACTIVE_MARKER);
    puts("k3346 eta tries OK");
    return 0;
}
