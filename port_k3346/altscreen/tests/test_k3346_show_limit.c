/* showUI at most every K3346_SHOW_MIN_INTERVAL_MS, and the layout before the VC reports
 * one.  The real limit on the real command sequence.  now_ms() is the real clock, so
 * time passes by moving last_show_ms back. */
#define K3346_REAL_SHOW_LIMIT
#define main control_mock_main
#include "test_v8s_control_mock.c"
#undef main

static altscreen_state *s;

/* the area sent for a logical view: an app other than Apple Maps without the card has
 * its own on the big gauges of Classic (rc30, view 8) */
static int wire_of(int view)
{
    return view == 3 && g_card_mode == K3346_CARD_NONE && !g_nav_apple && !g_sport ? 8 : view;
}

#define URL_BIG "maps:/car/instrumentcluster?maneuverLayout=topAligned"
#define URL_SMALL "maps:/car/instrumentcluster?maneuverLayout=leftAligned"
#define URL_NONE "maps:/car/instrumentcluster/map"

static void put_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");

    assert(f);
    fputs(text, f);
    fclose(f);
}

static void ago(uint64_t ms)
{
    s->last_show_ms = now_ms() - ms;
}

static void show_sequence(int view, const char *url)
{
    int b = nsends;

    if (s->last_ui_slot >= 0 && s->last_ui_slot != view / 2) {
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
    idle();
}

static void view_only(int view)
{
    int b = nsends;

    step("updateViewArea");
    assert(sends[b].view == wire_of(view));
    ack(b, 0);
    step("forceKeyFrame");
    ack(b + 1, 0);
    idle();
}

static void press_view(int slot)
{
    g_layout_slot = slot;
    queue_route_view_for_receiver(s->receiver, 0);
}

static void remove_files(void)
{
    remove(K3346_CALL_CARD_PATH);
    remove(K3346_LAYOUT_SLOT_PATH);
    remove(K3346_VIEW_SIZE_PATH);
    remove(K3346_CARD_MODE_PATH);
    remove(K3346_NO_RGI_PATH);
    remove(K3346_SKIN_PATH);
}

int main(void)
{
    FILE *marker;
    int b;

    assert(K3346_SHOW_MIN_INTERVAL_MS == 500ULL);

    prepare();
    remove_files();
    put_file(K3346_ETA_MODE_PATH, "0\n");
    g_bap54_layout_known = 1;
    g_layout_slot = 1;
    marker = fopen(ALTSCREEN_ACTIVE_MARKER, "w");
    assert(marker);
    fclose(marker);

    pthread_mutex_lock(&g_lock);
    s = alloc_state_locked((void *)0x1001);
    s->screen_started = 1;
    s->route_known = 1;
    s->route_active = 1;
    pthread_mutex_unlock(&g_lock);

    /* the first showUI of a session waits for nothing */
    assert(s->last_show_ms == 0);
    request_cluster(s);
    show_sequence(3, URL_BIG);
    assert(s->last_show_ms != 0);

    /* VIEW right after: held, and the value stays */
    press_view(0);
    idle();
    idle();
    assert(s->pending_view == 1);
    assert(s->last_ui_slot == 1);
    assert(s->control_stage == ALTSCREEN_CONTROL_IDLE);
    assert(s->show_hold_logged);

    /* until the limit and not longer */
    ago(450);
    idle();
    assert(s->pending_view == 1);
    ago(501);
    show_sequence(1, URL_SMALL);
    assert(s->pending_view == -1);
    assert(!s->show_hold_logged);

    /* there and back inside the limit: the phone hears nothing of it */
    press_view(1);
    idle();
    press_view(0);
    view_only(1);
    assert(s->last_ui_slot == 0);

    /* there, back and there again: the last value, once */
    press_view(1);
    idle();
    press_view(0);
    press_view(1);
    idle();
    ago(2001);
    show_sequence(3, URL_BIG);
    idle();

    /* long after the last one: at once */
    ago(60000);
    press_view(0);
    show_sequence(1, URL_SMALL);

    /* a sequence under way finishes, the newer value waits for its turn */
    ago(60000);
    press_view(1);
    b = nsends;
    step("updateViewArea");
    assert(sends[b].view == 3);
    ack(b, 0);
    step("showUI");
    assert(!strcmp(sends[b + 1].url, URL_BIG));
    ack(b + 1, 0);
    press_view(0);
    step("forceKeyFrame");
    ack(b + 2, 0);
    idle();
    assert(s->current_view == 3);
    assert(s->pending_view == 1);
    ago(2001);
    show_sequence(1, URL_SMALL);

    /* the area of a held layout change waits with its showUI, so the two stay together */
    ago(300);
    press_view(1);
    b = nsends;
    idle();
    assert(nsends == b);
    assert(s->pending_view == 3);
    ago(2001);
    show_sequence(3, URL_BIG);
    ago(2001);
    press_view(0);
    show_sequence(1, URL_SMALL);

    /* an app with route guidance: the ETA option sends nothing (rc29) */
    put_file(K3346_ETA_MODE_PATH, "1\n");
    s->layout_change_ms = 0;
    ago(2001);
    b = nsends;
    eta_mode_poll();
    idle();
    assert(nsends == b && s->pending_view < 0);

    /* the phone's box of an app without route guidance, a showUI of its own, goes
     * through the limit as well */
    put_file(K3346_NO_RGI_PATH, "");
    ago(300);
    eta_mode_poll();
    idle();
    assert(s->pending_view == 1 && s->show_hold_logged);
    ago(501);
    show_sequence(1, "maps:/car/instrumentcluster?showETA=yes&maneuverLayout=leftAligned");
    ago(2001);
    press_view(1);
    show_sequence(3, "maps:/car/instrumentcluster?showETA=yes&maneuverLayout=topAligned");
    put_file(K3346_ETA_MODE_PATH, "0\n");
    ago(2001);
    eta_mode_poll();
    show_sequence(3, URL_BIG);
    remove(K3346_NO_RGI_PATH);
    b = nsends;
    eta_mode_poll();
    idle();
    assert(nsends == b);
    ago(2001);
    press_view(0);
    show_sequence(1, URL_SMALL);

    /* the same layout takes no showUI, so a route change does not wait */
    s->route_active = 0;
    queue_route_view_for_receiver(s->receiver, 0);
    view_only(0);
    s->route_active = 1;
    queue_route_view_for_receiver(s->receiver, 0);
    view_only(1);

    /* an option change is a showUI like any other, and it is not forgotten */
    put_file(K3346_CARD_MODE_PATH, "1\n");
    eta_mode_poll();
    idle();
    assert(s->eta_force_show);
    ago(2001);
    show_sequence(1, URL_NONE);
    assert(!s->eta_force_show);
    put_file(K3346_CARD_MODE_PATH, "0\n");
    ago(2001);
    eta_mode_poll();
    show_sequence(1, URL_SMALL);

    /* before the VC reports: the HMI's layout, in time for the first frame on the VC */
    g_bap54_layout_known = 0;
    put_file(K3346_VIEW_SIZE_PATH, "1\n");
    eta_mode_poll();
    assert(g_layout_slot == 1);
    ago(2001);
    show_sequence(3, URL_BIG);

    /* the overlay comes first, its slot has the VC's report in it when there is one */
    put_file(K3346_VIEW_SIZE_PATH, "0\n");
    put_file(K3346_LAYOUT_SLOT_PATH, "1 0\n");
    eta_mode_poll();
    assert(g_layout_slot == 1);
    idle();
    remove(K3346_LAYOUT_SLOT_PATH);
    eta_mode_poll();
    assert(g_layout_slot == 0);
    ago(2001);
    show_sequence(1, URL_SMALL);

    /* half written, or gone: the layout stays */
    put_file(K3346_VIEW_SIZE_PATH, "1");
    eta_mode_poll();
    assert(g_layout_slot == 0);
    idle();
    remove(K3346_VIEW_SIZE_PATH);
    eta_mode_poll();
    assert(g_layout_slot == 0);
    idle();

    /* once the VC has spoken the HMI's file has no say */
    g_bap54_layout_known = 1;
    put_file(K3346_VIEW_SIZE_PATH, "1\n");
    eta_mode_poll();
    assert(g_layout_slot == 0);
    idle();

    remove_files();
    unlink(ALTSCREEN_ACTIVE_MARKER);
    puts("k3346 show limit OK");
    return 0;
}
