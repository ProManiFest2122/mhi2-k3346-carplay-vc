/* The real command sequence for the maneuver card option. */
#define main control_mock_main
#include "test_v8s_control_mock.c"
#undef main

static altscreen_state *s;

static void put_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");

    assert(f);
    fputs(text, f);
    fclose(f);
}

static void card(const char *text)
{
    put_file(K3346_CARD_MODE_PATH, text);
    eta_mode_poll();
}

static void show_sequence(int view, const char *url)
{
    int b = nsends;

    step("showUI");
    assert(!strcmp(sends[b].url, url));
    idle();
    ack(b, 0);
    step("updateViewArea");
    assert(sends[b + 1].view == view);
    ack(b + 1, 0);
    step("forceKeyFrame");
    ack(b + 2, 0);
}

static void view_only(int view)
{
    int b = nsends;

    step("updateViewArea");
    assert(sends[b].view == view);
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

static void settled(void)
{
    eta_mode_poll();
    idle();
}

#define URL_BIG "maps:/car/instrumentcluster?maneuverLayout=topAligned"
#define URL_SMALL "maps:/car/instrumentcluster?maneuverLayout=leftAligned"
#define URL_NONE_BIG "maps:/car/instrumentcluster/map"
#define URL_NONE_SMALL "maps:/car/instrumentcluster/map?maneuverLayout=leftAligned"
#define URL_NONE_BIG_ETA "maps:/car/instrumentcluster/map?showETA=yes"

int main(void)
{
    FILE *marker;
    int b;

    prepare();
    remove(K3346_CALL_CARD_PATH);
    remove(K3346_LAYOUT_SLOT_PATH);
    remove(K3346_CARD_MODE_PATH);
    remove(K3346_NO_RGI_PATH);
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

    /* before the first showUI of a session the option sends nothing by itself */
    card("1\n");
    idle();
    card("0\n");
    idle();

    /* full: exactly today's commands */
    request_cluster(s);
    show_sequence(3, URL_BIG);
    assert(s->last_ui_slot == 1 && s->current_view == 3);
    settled();

    /* off and on, live, one showUI each */
    card("1\n");
    show_sequence(3, URL_NONE_BIG);
    assert(s->ui_card == K3346_CARD_NONE);
    settled();
    card("0\n");
    show_sequence(3, URL_BIG);
    assert(s->ui_card == K3346_CARD_FULL);
    settled();

    /* a route change keeps it and sends no showUI */
    card("1\n");
    show_sequence(3, URL_NONE_BIG);
    s->route_active = 0;
    queue_route_view_for_receiver(s->receiver, 0);
    view_only(2);
    s->route_active = 1;
    queue_route_view_for_receiver(s->receiver, 0);
    view_only(3);

    /* VIEW: off holds on small gauges too */
    press_view(0);
    show_sequence(1, URL_NONE_SMALL);
    settled();
    /* the option works on small gauges as well */
    card("0\n");
    show_sequence(1, URL_SMALL);
    settled();
    card("1\n");
    show_sequence(1, URL_NONE_SMALL);
    settled();
    press_view(1);
    show_sequence(3, URL_NONE_BIG);
    settled();

    /* the ETA value travels with the URL */
    s->layout_change_ms = 0;
    put_file(K3346_ETA_MODE_PATH, "1\n");
    eta_mode_poll();
    show_sequence(3, URL_NONE_BIG_ETA);
    settled();
    card("0\n");
    show_sequence(3, "maps:/car/instrumentcluster?showETA=yes&maneuverLayout=topAligned");
    settled();
    card("1\n");
    show_sequence(3, URL_NONE_BIG_ETA);
    settled();
    put_file(K3346_ETA_MODE_PATH, "0\n");
    eta_mode_poll();
    show_sequence(3, URL_NONE_BIG);
    settled();

    /* an app without route guidance keeps its card; the choice comes back with one */
    put_file(K3346_NO_RGI_PATH, "");
    eta_mode_poll();
    show_sequence(3, URL_BIG);
    assert(s->ui_card == K3346_CARD_FULL);
    settled();
    remove(K3346_NO_RGI_PATH);
    eta_mode_poll();
    show_sequence(3, URL_NONE_BIG);
    settled();

    /* a half-written file changes nothing */
    card("9\n");
    idle();
    assert(g_card_mode == K3346_CARD_NONE);

    /* a change during a sequence waits for its end, then goes out once */
    card("0\n");
    b = nsends;
    step("showUI");
    card("1\n");
    idle();
    ack(b, 0);
    step("updateViewArea");
    ack(b + 1, 0);
    step("forceKeyFrame");
    ack(b + 2, 0);
    idle();
    eta_mode_poll();
    show_sequence(3, URL_NONE_BIG);
    settled();

    /* no stopUI anywhere in this run */
    for (b = 0; b < nsends; b++)
        assert(strcmp(sends[b].name, "stopUI") != 0);

    /* the phone is told 4 areas */
    {
        cf_ref request = make_view_request(3);
        mock_obj *params = (mock_obj *)fake_get(request, obj(3, "params"));
        mock_obj *adjacent = (mock_obj *)fake_get(params, obj(3, "adjacentViewAreas"));

        assert(adjacent->count == 3);
    }

    puts("k3346 card OK");
    return 0;
}
