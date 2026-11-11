/* The real command sequence for the Sport skin. */
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

static void skin(const char *text)
{
    put_file(K3346_SKIN_PATH, text);
    eta_mode_poll();
}

static void show_sequence(int wire, const char *layout, int eta)
{
    int b = nsends;

    if (s->last_ui_slot >= 0 && s->last_ui_slot != (wire >= 2)) {
        /* a layout change: the area first */
        step("updateViewArea");
        assert(sends[b].view == wire);
        idle();
        ack(b, 0);
        step("showUI");
        assert(strstr(sends[b + 1].url, layout));
        assert((strstr(sends[b + 1].url, "showETA=yes") != NULL) == eta);
        ack(b + 1, 0);
    } else {
        step("showUI");
        assert(strstr(sends[b].url, layout));
        assert((strstr(sends[b].url, "showETA=yes") != NULL) == eta);
        idle();
        ack(b, 0);
        step("updateViewArea");
        assert(sends[b + 1].view == wire);
        ack(b + 1, 0);
    }
    step("forceKeyFrame");
    ack(b + 2, 0);
}

static void view_only(int wire)
{
    int b = nsends;

    step("updateViewArea");
    assert(sends[b].view == wire);
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

int main(void)
{
    FILE *marker;
    int b, first;

    prepare();
    remove(K3346_CALL_CARD_PATH);
    remove(K3346_LAYOUT_SLOT_PATH);
    remove(K3346_VIEW_SIZE_PATH);
    remove(K3346_CARD_MODE_PATH);
    remove(K3346_NO_RGI_PATH);
    remove(K3346_SKIN_PATH);
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

    /* before the first showUI the skin sends nothing by itself */
    skin("1\n");
    idle();
    skin("0\n");
    idle();

    /* no file, Classic: today's commands */
    remove(K3346_SKIN_PATH);
    eta_mode_poll();
    request_cluster(s);
    show_sequence(3, "topAligned", 0);
    settled();

    /* to Sport and back, live, on big gauges */
    skin("1\n");
    assert(g_sport == 1);
    show_sequence(4, "topAligned", 0);
    assert(s->ui_sport == 1 && s->current_view == 3);
    settled();
    skin("0\n");
    show_sequence(3, "topAligned", 0);
    assert(s->ui_sport == 0);
    settled();

    /* a route change on Sport keeps the area and sends no showUI */
    skin("1\n");
    show_sequence(4, "topAligned", 0);
    s->route_active = 0;
    queue_route_view_for_receiver(s->receiver, 0);
    view_only(4);
    s->route_active = 1;
    queue_route_view_for_receiver(s->receiver, 0);
    view_only(4);

    /* VIEW: small gauges use their own areas on either skin */
    press_view(0);
    show_sequence(1, "leftAligned", 0);
    settled();
    s->route_active = 0;
    queue_route_view_for_receiver(s->receiver, 0);
    view_only(0);
    s->route_active = 1;
    queue_route_view_for_receiver(s->receiver, 0);
    view_only(1);
    /* the skin changes on small gauges: nothing to send until VIEW */
    skin("0\n");
    idle();
    skin("1\n");
    idle();
    press_view(1);
    show_sequence(4, "topAligned", 0);
    settled();

    /* the ETA box waits after a skin change as after any layout change, the full 12 s
     * even when the last VIEW left a shorter wait of the rc22 test */
    s->layout_change_ms = 0;
    s->eta_delay_ms = 3000ULL;
    skin("0\n");
    show_sequence(3, "topAligned", 0);
    settled();
    assert(s->layout_change_ms != 0);
    assert(s->eta_delay_ms == K3346_ETA_AFTER_LAYOUT_MS);
    /* an app with route guidance has no box of the phone to wait for (rc29) */
    put_file(K3346_ETA_MODE_PATH, "1\n");
    eta_mode_poll();
    idle();
    s->layout_change_ms = now_ms() - K3346_ETA_AFTER_LAYOUT_MS - 1;
    eta_mode_poll();
    idle();
    assert(s->pending_view < 0 && s->ui_eta_on_map == 0);
    /* one without it keeps the phone's box, and that one waits */
    s->layout_change_ms = now_ms();
    put_file(K3346_NO_RGI_PATH, "");
    eta_mode_poll();
    idle();
    s->layout_change_ms = now_ms() - 3000 - 1;
    eta_mode_poll();
    idle();
    assert(s->pending_view < 0 && s->ui_eta_on_map == 0);
    s->layout_change_ms = now_ms() - K3346_ETA_AFTER_LAYOUT_MS - 1;
    eta_mode_poll();
    show_sequence(3, "topAligned", 1);
    settled();
    /* a box on screen goes with the showUI of the new area */
    skin("1\n");
    show_sequence(4, "topAligned", 1);
    settled();
    put_file(K3346_ETA_MODE_PATH, "0\n");
    eta_mode_poll();
    show_sequence(4, "topAligned", 0);
    settled();
    remove(K3346_NO_RGI_PATH);
    eta_mode_poll();
    idle();

    /* a half-written file changes nothing */
    skin("x\n");
    idle();
    assert(g_sport == 1);

    /* a change during a sequence waits for its end, then goes out once */
    skin("0\n");
    b = nsends;
    step("showUI");
    first = b;
    skin("1\n");
    idle();
    ack(first, 0);
    step("updateViewArea");
    ack(first + 1, 0);
    step("forceKeyFrame");
    ack(first + 2, 0);
    idle();
    eta_mode_poll();
    show_sequence(4, "topAligned", 0);
    settled();

    /* adjacency names every other one of the 9 areas */
    {
        cf_ref request = make_view_request(3, 1, 0);
        mock_obj *params = (mock_obj *)fake_get(request, obj(3, "params"));
        mock_obj *adjacent = (mock_obj *)fake_get(params, obj(3, "adjacentViewAreas"));
        mock_obj *index = (mock_obj *)fake_get(params, obj(3, "viewAreaIndex"));
        int i;

        assert(atoi(index->value) == 4);
        assert(adjacent->count == 8);
        for (i = 0; i < adjacent->count; i++)
            assert(atoi(adjacent->item[i].val->value) != 4);
    }

    puts("k3346 skin OK");
    return 0;
}
