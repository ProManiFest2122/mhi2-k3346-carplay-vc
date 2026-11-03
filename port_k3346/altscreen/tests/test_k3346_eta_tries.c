/* The real showUI sequence for the ETA box: one showUI, on the same layout, not
 * right after a layout change. */
#define main control_mock_main
#include "test_v8s_control_mock.c"
#undef main

static altscreen_state *s;

static void mode(int value)
{
    FILE *f = fopen(K3346_ETA_MODE_PATH, "w");

    assert(f);
    fprintf(f, "%d\n", value);
    fclose(f);
    eta_mode_poll();
}

/* one showUI + updateViewArea + forceKeyFrame, acked in order */
static void show_sequence(int view, const char *layout, int eta)
{
    int b = nsends;

    step("showUI");
    assert(strstr(sends[b].url, layout));
    assert((strstr(sends[b].url, "showETA=yes") != NULL) == eta);
    idle();
    ack(b, 0);
    step("updateViewArea");
    assert(sends[b + 1].view == view);
    ack(b + 1, 0);
    step("forceKeyFrame");
    ack(b + 2, 0);
}

static void quiet(void)
{
    idle();
    assert(s->pending_view < 0);
    assert(s->control_stage == ALTSCREEN_CONTROL_IDLE);
    eta_mode_poll();
    idle();
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
    int b;
    uint64_t first;

    prepare();
    remove(K3346_CALL_CARD_PATH);
    remove(K3346_CARD_MODE_PATH);
    remove(K3346_NO_RGI_PATH);
    remove(K3346_LAYOUT_SLOT_PATH);
    g_bap54_layout_known = 1;
    g_layout_slot = 0;
    marker = fopen(ALTSCREEN_ACTIVE_MARKER, "w");
    assert(marker);
    fclose(marker);
    mode(0);

    pthread_mutex_lock(&g_lock);
    s = alloc_state_locked((void *)0x1001);
    s->screen_started = 1;
    s->route_known = 1;
    s->route_active = 1;
    pthread_mutex_unlock(&g_lock);

    /* the first showUI of a session does not wait */
    mode(1);
    show_sequence(1, "leftAligned", 1);
    quiet();
    assert(s->layout_change_ms == 0);

    /* small gauges: one showUI each way, on the layout */
    mode(0);
    show_sequence(1, "leftAligned", 0);
    quiet();
    mode(1);
    show_sequence(1, "leftAligned", 1);
    quiet();

    /* VIEW with the box on: it stays in the showUI of the new layout */
    press_view(1);
    show_sequence(3, "topAligned", 1);
    quiet();
    assert(s->layout_change_ms != 0);

    /* hiding right after a VIEW does not wait */
    mode(0);
    show_sequence(3, "topAligned", 0);
    quiet();

    /* run-59: the box asked for right after VIEW waits, once, on the new layout */
    press_view(0);
    show_sequence(1, "leftAligned", 0);
    quiet();
    first = s->layout_change_ms;
    mode(1);
    idle();
    eta_mode_poll();
    idle();
    wait_over();
    eta_mode_poll();
    show_sequence(1, "leftAligned", 1);
    quiet();
    assert(s->ui_eta_on_map == 1);
    (void)first;

    /* a route change during the wait keeps the box away and the wait running */
    mode(0);
    show_sequence(1, "leftAligned", 0);
    quiet();
    press_view(1);
    show_sequence(3, "topAligned", 0);
    quiet();
    mode(1);
    s->route_active = 0;
    queue_route_view_for_receiver(s->receiver, 0);
    b = nsends;
    step("updateViewArea");
    assert(sends[b].view == 2);
    ack(b, 0);
    step("forceKeyFrame");
    ack(b + 1, 0);
    quiet();
    s->route_active = 1;
    queue_route_view_for_receiver(s->receiver, 0);
    b = nsends;
    step("updateViewArea");
    ack(b, 0);
    step("forceKeyFrame");
    ack(b + 1, 0);
    quiet();

    /* a second VIEW during the wait: no box in its showUI, and the wait starts over */
    first = s->layout_change_ms;
    s->layout_change_ms = first - 5000;
    press_view(0);
    show_sequence(1, "leftAligned", 0);
    assert(s->layout_change_ms > first - 5000);
    quiet();
    wait_over();
    eta_mode_poll();
    show_sequence(1, "leftAligned", 1);
    quiet();

    /* asked for during another sequence: after it, once */
    mode(0);
    show_sequence(1, "leftAligned", 0);
    quiet();
    wait_over();
    /* a queued route change keeps its time, and carries the value itself */
    queue_route_view_for_receiver(s->receiver, 5000);
    mode(1);
    idle();
    s->view_due_ms = 1;
    show_sequence(1, "leftAligned", 1);
    quiet();
    mode(0);
    show_sequence(1, "leftAligned", 0);
    quiet();
    wait_over();
    press_view(1);
    b = nsends;
    step("showUI");
    wait_over();
    mode(1);
    eta_mode_poll();
    ack(b, 0);
    step("updateViewArea");
    ack(b + 1, 0);
    step("forceKeyFrame");
    ack(b + 2, 0);
    idle();
    eta_mode_poll();
    show_sequence(3, "topAligned", 1);
    quiet();

    puts("k3346 eta tries OK");
    return 0;
}
