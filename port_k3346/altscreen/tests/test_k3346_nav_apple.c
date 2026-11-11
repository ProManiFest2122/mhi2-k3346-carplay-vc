/* Apple Maps gets its own areas with a route (nav_app.h, views 5 to 7 in
 * mib2high-lvds-viewareas.conf).  The real command sequence.  Since rc29 the phone is
 * not asked for an ETA box for an app with route guidance (overlay/eta_pill.h draws
 * ours), so no URL here names showETA. */
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

static void apple(const char *text)
{
    if (text == NULL)
        remove(K3346_NAV_APPLE_PATH);
    else
        put_file(K3346_NAV_APPLE_PATH, text);
    eta_mode_poll();
}

static void card_none(int on)
{
    if (on)
        put_file(K3346_CARD_MODE_PATH, "1\n");
    else
        remove(K3346_CARD_MODE_PATH);
    eta_mode_poll();
}

/* showUI + updateViewArea + forceKeyFrame; a layout change sends the area first */
static void show_sequence(int slot, int wire, const char *url)
{
    int b = nsends;

    if (s->last_ui_slot >= 0 && s->last_ui_slot != slot) {
        step("updateViewArea");
        assert(sends[b].view == wire);
        idle();
        ack(b, 0);
        step("showUI");
        assert(strstr(sends[b + 1].url, url));
        ack(b + 1, 0);
    } else {
        step("showUI");
        assert(strstr(sends[b].url, url));
        idle();
        ack(b, 0);
        step("updateViewArea");
        assert(sends[b + 1].view == wire);
        ack(b + 1, 0);
    }
    step("forceKeyFrame");
    ack(b + 2, 0);
}

/* the area alone: no showUI, the URL stays */
static void area_only(int wire)
{
    int b = nsends;

    step("updateViewArea");
    assert(sends[b].view == wire);
    ack(b, 0);
    step("forceKeyFrame");
    ack(b + 1, 0);
    idle();
}

#define URL_NO_BOX    "maps:/car/instrumentcluster/map"
#define URL_CARD_SMALL "maps:/car/instrumentcluster?maneuverLayout=leftAligned"
#define URL_CARD_BIG   "maps:/car/instrumentcluster?maneuverLayout=topAligned"

/* a layout change: the area, and after its ack showUI with exactly this URL */
static void change_sequence(int wire, const char *url)
{
    int b = nsends;

    step("updateViewArea");
    assert(sends[b].view == wire);
    idle();
    ack(b, 0);
    step("showUI");
    assert(!strcmp(sends[b + 1].url, url));
    idle();
    ack(b + 1, 0);
    step("forceKeyFrame");
    ack(b + 2, 0);
    idle();
}

/* the same layout: showUI with exactly this URL, then the area */
static void url_sequence(int wire, const char *url)
{
    int b = nsends;

    step("showUI");
    assert(!strcmp(sends[b].url, url));
    idle();
    ack(b, 0);
    step("updateViewArea");
    assert(sends[b + 1].view == wire);
    ack(b + 1, 0);
    step("forceKeyFrame");
    ack(b + 2, 0);
    idle();
}

static void settled(void)
{
    idle();
    assert(s->pending_view < 0);
    assert(s->control_stage == ALTSCREEN_CONTROL_IDLE);
    eta_mode_poll();
    idle();
    assert(s->pending_view < 0);
}

static void press_view(int slot)
{
    g_layout_slot = slot;
    queue_route_view_for_receiver(s->receiver, 0);
}

static void route(int on)
{
    s->route_active = on;
    queue_route_view_for_receiver(s->receiver, 0);
}

static void test_names(void)
{
    const char *path = "/tmp/k3346-nav-apple-test";

    assert(k3346_nav_is_apple("Apple Maps"));
    assert(k3346_nav_is_apple("Maps"));
    assert(k3346_nav_is_apple("Mapy"));
    assert(!k3346_nav_is_apple("Google Maps"));
    assert(!k3346_nav_is_apple("Waze"));
    assert(!k3346_nav_is_apple("Apple Maps "));
    assert(!k3346_nav_is_apple("apple maps"));
    assert(!k3346_nav_is_apple(""));
    assert(!k3346_nav_is_apple(NULL));

    /* reader: no file = not Apple; only "0\n" or "1\n" counts */
    remove(path);
    assert(k3346_nav_apple_read(path, 1) == 0);
    assert(k3346_nav_apple_write(path, 1) == 0);
    assert(k3346_nav_apple_read(path, 0) == 1);
    assert(k3346_nav_apple_write(path, 0) == 0);
    assert(k3346_nav_apple_read(path, 1) == 0);
    put_file(path, "1");
    assert(k3346_nav_apple_read(path, 0) == 0 && k3346_nav_apple_read(path, 1) == 1);
    put_file(path, "");
    assert(k3346_nav_apple_read(path, 1) == 1);
    put_file(path, "2\n");
    assert(k3346_nav_apple_read(path, 0) == 0);
    put_file(path, "1\n\n");
    assert(k3346_nav_apple_read(path, 0) == 0);
    remove(path);
    assert(strcmp(K3346_NAV_APPLE_PATH, "/tmp/k3346-nav-apple") == 0);
}

int main(void)
{
    FILE *marker;
    int b, i;

    test_names();

    prepare();
    remove(K3346_CALL_CARD_PATH);
    remove(K3346_LAYOUT_SLOT_PATH);
    remove(K3346_VIEW_SIZE_PATH);
    remove(K3346_NO_RGI_PATH);
    remove(K3346_SKIN_PATH);
    put_file(K3346_ETA_MODE_PATH, "0\n");
    put_file(K3346_CARD_MODE_PATH, "1\n");
    g_bap54_layout_known = 1;
    g_layout_slot = 0;
    marker = fopen(ALTSCREEN_ACTIVE_MARKER, "w");
    assert(marker);
    fclose(marker);
    eta_mode_poll();

    pthread_mutex_lock(&g_lock);
    s = alloc_state_locked((void *)0x1001);
    s->screen_started = 1;
    s->route_known = 1;
    s->route_active = 1;
    pthread_mutex_unlock(&g_lock);

    /* before the first showUI the name sends nothing by itself */
    apple("1\n");
    idle();
    apple("0\n");
    idle();

    /* Brak, small gauges, Google: the area of rc22 */
    request_cluster(s);
    show_sequence(0, 1, "instrumentcluster/map");
    settled();

    /* Apple: its own area, without a showUI */
    b = nsends;
    apple("1\n");
    area_only(5);
    assert(s->ui_apple == 1);
    settled();
    /* the same name again: nothing */
    apple("1\n");
    idle();
    assert(nsends == b + 2);

    /* a half-written file changes nothing */
    apple("x\n");
    idle();
    assert(nsends == b + 2);

    /* back to Google: view 1, with the card or without */
    apple("0\n");
    area_only(1);
    assert(s->ui_apple == K3346_OTHER_FREE);
    settled();
    apple("1\n");
    area_only(5);
    settled();

    /* Apple with the card (Pełna): the box sits under the card, an area as wide */
    card_none(0);
    show_sequence(0, 6, "instrumentcluster?maneuverLayout=leftAligned");
    assert(s->ui_apple == K3346_APPLE_CARD);
    settled();
    /* Google with the card: the area of rc22 */
    apple("0\n");
    area_only(1);
    settled();
    apple("1\n");
    area_only(6);
    settled();
    card_none(1);
    show_sequence(0, 5, "instrumentcluster/map");
    assert(s->ui_apple == K3346_APPLE_FREE);
    settled();

    /* big gauges: Apple without the card has its own area there too (view 7), and a
     * new app moves the area alone. */
    press_view(1);
    show_sequence(1, 7, "instrumentcluster/map");
    settled();
    b = nsends;
    apple("0\n");
    area_only(8);
    assert(s->ui_apple == K3346_OTHER_FREE);
    settled();
    apple("1\n");
    area_only(7);
    assert(s->ui_apple == K3346_APPLE_FREE && nsends == b + 4);
    settled();
    press_view(0);
    show_sequence(0, 5, "instrumentcluster/map");
    settled();

    /* no route: no box, the area of view 0; the route back: Apple's */
    route(0);
    area_only(0);
    settled();
    apple("0\n");
    idle();
    apple("1\n");
    idle();
    route(1);
    area_only(5);
    settled();

    /* Sport skin on the small gauges: Apple's area as well */
    put_file(K3346_SKIN_PATH, "1\n");
    eta_mode_poll();
    idle();
    remove(K3346_SKIN_PATH);
    eta_mode_poll();
    idle();

    /* the file gone with the session: rc22's area until mm-ipod writes it again */
    apple(NULL);
    area_only(1);
    apple("1\n");
    area_only(5);
    settled();

    /* a change during a sequence waits for its end */
    apple("0\n");
    b = nsends;
    step("updateViewArea");
    apple("1\n");
    idle();
    ack(b, 0);
    step("forceKeyFrame");
    ack(b + 1, 0);
    idle();
    eta_mode_poll();
    area_only(5);
    settled();

    /* rc29: the ETA option on.  The phone is not asked for a box, whatever the app,
     * the layout or the card: nothing is sent for the option, and the URLs and the
     * areas stay what they are without it. */
    s->layout_change_ms = 0;
    put_file(K3346_ETA_MODE_PATH, "1\n");
    b = nsends;
    eta_mode_poll();
    idle();
    assert(nsends == b && s->ui_eta_on_map == 0);

    /* Apple, Brak: areas 7 and 5, the plain URL, one showUI per VIEW */
    for (i = 0; i < 3; ++i) {
        press_view(1);
        change_sequence(7, URL_NO_BOX);
        assert(s->last_ui_slot == 1 && s->ui_eta_on_map == 0 && s->ui_apple == K3346_APPLE_FREE);
        settled();
        press_view(0);
        change_sequence(5, URL_NO_BOX);
        assert(s->last_ui_slot == 0 && s->current_view == 1);
        settled();
    }

    /* Apple with the card: areas 3 and 6, the card URL of the layout, nothing after it */
    card_none(0);
    url_sequence(6, URL_CARD_SMALL);
    assert(s->ui_apple == K3346_APPLE_CARD);
    settled();
    for (i = 0; i < 2; ++i) {
        press_view(1);
        change_sequence(3, URL_CARD_BIG);
        settled();
        b = nsends;
        s->layout_change_ms = now_ms() - K3346_ETA_AFTER_LAYOUT_MS - 1;
        eta_mode_poll();
        idle();
        assert(nsends == b);
        press_view(0);
        change_sequence(6, URL_CARD_SMALL);
        settled();
    }

    /* Google Maps with the card: the areas of rc22, the same URLs */
    apple("0\n");
    area_only(1);
    assert(s->ui_apple == K3346_APPLE_NONE);
    settled();
    press_view(1);
    change_sequence(3, URL_CARD_BIG);
    settled();
    /* with the card the big gauges have one area for every app */
    b = nsends;
    apple("1\n");
    idle();
    apple("0\n");
    idle();
    assert(nsends == b);
    press_view(0);
    change_sequence(1, URL_CARD_SMALL);
    settled();

    /* Google Maps, Brak: view 1 again (the wide area of rc27 is gone), the plain URL;
     * a new app moves the area alone on both layouts */
    card_none(1);
    url_sequence(1, URL_NO_BOX);
    assert(s->ui_apple == K3346_OTHER_FREE);
    settled();
    b = nsends;
    apple("1\n");
    area_only(5);
    settled();
    apple("0\n");
    area_only(1);
    assert(nsends == b + 4);
    settled();
    /* rc30: on the big gauges Google Maps without the card has view 8, which ends
     * above our boxes; with the card view 3, the full height the card needs */
    press_view(1);
    change_sequence(8, URL_NO_BOX);
    settled();
    b = nsends;
    apple("1\n");
    area_only(7);
    settled();
    apple("0\n");
    area_only(8);
    assert(nsends == b + 4);
    settled();
    card_none(0);
    url_sequence(3, URL_CARD_BIG);
    assert(s->ui_apple == K3346_APPLE_NONE);
    settled();
    card_none(1);
    url_sequence(8, URL_NO_BOX);
    assert(s->ui_apple == K3346_OTHER_FREE);
    settled();
    press_view(0);
    change_sequence(1, URL_NO_BOX);
    settled();
    press_view(1);
    change_sequence(8, URL_NO_BOX);
    settled();
    /* a route change on the big gauges: view 2 without a route, view 8 with one */
    route(0);
    area_only(2);
    settled();
    route(1);
    area_only(8);
    settled();
    /* the Sport skin has one area on the big gauges for every app and card */
    put_file(K3346_SKIN_PATH, "1\n");
    eta_mode_poll();
    url_sequence(4, URL_NO_BOX);
    settled();
    remove(K3346_SKIN_PATH);
    eta_mode_poll();
    url_sequence(8, URL_NO_BOX);
    settled();
    press_view(0);
    change_sequence(1, URL_NO_BOX);
    settled();

    /* Sport has its own area on the big gauges: Apple gets that one there */
    apple("1\n");
    area_only(5);
    settled();
    put_file(K3346_SKIN_PATH, "1\n");
    eta_mode_poll();
    idle();
    press_view(1);
    change_sequence(4, URL_NO_BOX);
    settled();
    press_view(0);
    change_sequence(5, URL_NO_BOX);
    settled();
    remove(K3346_SKIN_PATH);
    eta_mode_poll();
    idle();

    /* no showETA went out in this whole run */
    for (i = 0; i < nsends; ++i)
        assert(!strstr(sends[i].url, "showETA"));

    remove(K3346_CARD_MODE_PATH);
    remove(K3346_NAV_APPLE_PATH);
    unlink(ALTSCREEN_ACTIVE_MARKER);
    puts("k3346 nav apple OK");
    return 0;
}
