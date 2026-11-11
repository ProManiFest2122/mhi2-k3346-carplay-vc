/* What the bus does with frames queued before Java is there.  The real bus.c: the ring
 * on its own first, then a whole connect over a real socket, as in car run-73: the phone
 * sends a route, the hook connects half a second later. */
#ifndef ENABLE_LOGGING
#define ENABLE_LOGGING 0
#endif
#include "../framework/bus.c"
#include <assert.h>

#define T_STICKY EVT_RGD_UPDATE
#define T_PLAIN  EVT_DEVICE_STATE
#define T_POLICY EVT_COVERART

static void put(uint16_t type, uint8_t flags, const char *text)
{
    assert(bus_send(type, flags, (const uint8_t *)text, (uint32_t)strlen(text)) == HOOK_OK);
}

static void take(uint16_t type, const char *text)
{
    frame_t f;

    assert(q_dequeue_nolock(&f));
    assert(f.type == type);
    assert(f.len == strlen(text));
    assert(!memcmp(f.payload, text, f.len));
    frame_dispose(&f);
}

static void forget(void)
{
    frame_t f;
    int i;

    while (q_dequeue_nolock(&f))
        frame_dispose(&f);
    for (i = 0; i < MAX_TYPES; i++) {
        if (g_types[i].has_last)
            frame_dispose(&g_types[i].last);
        g_types[i].has_last = false;
        g_types[i].sticky = false;
    }
}

static void test_ring(void)
{
    int i;

    /* nothing queued */
    assert(q_drop_covered_nolock() == 0);
    assert(g_ring_count == 0);

    /* the sticky frames go, the others keep their order */
    put(T_STICKY, BUS_FLAG_STICKY, "a1");
    put(T_PLAIN, 0, "b1");
    put(T_STICKY, BUS_FLAG_STICKY, "a2");
    put(T_PLAIN, 0, "b2");
    put(T_STICKY, BUS_FLAG_STICKY, "a3");
    assert(q_drop_covered_nolock() == 3);
    assert(g_ring_count == 2);
    take(T_PLAIN, "b1");
    take(T_PLAIN, "b2");
    assert(g_ring_count == 0);
    /* and the newest of them is what the snapshot sends */
    assert(g_types[T_STICKY].has_last);
    assert(!memcmp(g_types[T_STICKY].last.payload, "a3", 2));
    forget();

    /* sticky by configuration, without the flag */
    bus_set_sticky(T_POLICY, true);
    put(T_POLICY, 0, "c1");
    assert(q_drop_covered_nolock() == 1);
    assert(g_ring_count == 0);
    forget();

    /* a plain frame of a type that also has a sticky one stays */
    put(T_STICKY, BUS_FLAG_STICKY, "a1");
    put(T_STICKY, 0, "a2");
    assert(q_drop_covered_nolock() == 1);
    take(T_STICKY, "a2");
    forget();

    /* nothing cached for the type: nothing replaces the frame */
    put(T_PLAIN, 0, "b1");
    assert(q_drop_covered_nolock() == 0);
    take(T_PLAIN, "b1");
    forget();

    /* sticky by now, but nothing cached yet: the same */
    put(T_POLICY, 0, "c1");
    bus_set_sticky(T_POLICY, true);
    assert(!g_types[T_POLICY].has_last);
    assert(q_drop_covered_nolock() == 0);
    take(T_POLICY, "c1");
    forget();

    /* across the end of the ring */
    for (i = 0; i < SEND_QUEUE_CAPACITY - 2; i++) {
        put(T_PLAIN, 0, "x");
        take(T_PLAIN, "x");
    }
    put(T_PLAIN, 0, "b1");
    put(T_STICKY, BUS_FLAG_STICKY, "a1");
    put(T_PLAIN, 0, "b2");
    put(T_STICKY, BUS_FLAG_STICKY, "a2");
    put(T_PLAIN, 0, "b3");
    assert(q_drop_covered_nolock() == 2);
    assert(g_ring_count == 3);
    /* what comes next goes behind what stayed */
    put(T_PLAIN, 0, "b4");
    take(T_PLAIN, "b1");
    take(T_PLAIN, "b2");
    take(T_PLAIN, "b3");
    take(T_PLAIN, "b4");
    forget();

    /* a full ring */
    for (i = 0; i < SEND_QUEUE_CAPACITY; i++)
        put(T_STICKY, BUS_FLAG_STICKY, "a");
    assert(g_ring_count == SEND_QUEUE_CAPACITY);
    assert(q_drop_covered_nolock() == SEND_QUEUE_CAPACITY);
    assert(g_ring_count == 0);
    put(T_PLAIN, 0, "b1");
    take(T_PLAIN, "b1");
    forget();
}

typedef struct {
    uint16_t type;
    uint8_t flags;
    char text[64];
} seen_t;

/* 0 when nothing came for a second. */
static int recv_frame(int fd, seen_t *out)
{
    uint8_t h[16];
    uint32_t len;
    size_t got = 0;
    ssize_t n;

    while (got < sizeof(h)) {
        n = recv(fd, h + got, sizeof(h) - got, 0);
        if (n <= 0)
            return 0;
        got += (size_t)n;
    }
    assert(((uint32_t)h[0] << 24 | h[1] << 16 | h[2] << 8 | h[3]) == BUS_MAGIC);
    out->type = (uint16_t)(h[8] << 8 | h[9]);
    out->flags = h[10];
    len = (uint32_t)h[12] << 24 | h[13] << 16 | h[14] << 8 | h[15];
    assert(len < sizeof(out->text));
    got = 0;
    while (got < len) {
        n = recv(fd, out->text + got, len - got, 0);
        assert(n > 0);
        got += (size_t)n;
    }
    out->text[len] = 0;
    return 1;
}

static void test_connect(void)
{
    struct sockaddr_in a;
    struct timeval tv = { 1, 0 };
    int one = 1, srv, fd, i;
    char text[32];
    seen_t f;

    assert(bus_init() == HOOK_OK);

    /* Java is not listening yet: what the hook publishes meanwhile */
    put(T_STICKY, BUS_FLAG_STICKY, "disconnect_reason:s:session\n");
    for (i = 1; i <= 30; i++) {
        snprintf(text, sizeof(text), "diag_rx_seq:n:%d\n", i);
        put(T_STICKY, BUS_FLAG_STICKY, text);
    }
    put(T_PLAIN, 0, "plain\n");

    srv = socket(AF_INET, SOCK_STREAM, 0);
    assert(srv >= 0);
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(BUS_TCP_PORT);
    a.sin_addr.s_addr = inet_addr(BUS_TCP_HOST);
    assert(bind(srv, (struct sockaddr *)&a, sizeof(a)) == 0);
    assert(listen(srv, 1) == 0);
    fd = accept(srv, NULL, NULL);
    assert(fd >= 0);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    assert(recv_frame(fd, &f) && f.type == EVT_HELLO);
    assert(recv_frame(fd, &f) && f.type == EVT_SYNC_BEGIN);
    /* the newest state, once */
    assert(recv_frame(fd, &f) && f.type == T_STICKY);
    assert(f.flags & BUS_FLAG_REPLAY);
    assert(!strcmp(f.text, "diag_rx_seq:n:30\n"));
    assert(recv_frame(fd, &f) && f.type == EVT_SYNC_END);
    /* the plain frame was queued, and no going back in time after the snapshot */
    assert(recv_frame(fd, &f) && f.type == T_PLAIN);
    assert(!recv_frame(fd, &f));

    /* live frames still come through */
    put(T_STICKY, BUS_FLAG_STICKY, "diag_rx_seq:n:31\n");
    assert(recv_frame(fd, &f) && f.type == T_STICKY);
    assert(!(f.flags & BUS_FLAG_REPLAY));
    assert(!strcmp(f.text, "diag_rx_seq:n:31\n"));

    close(fd);
    close(srv);
    bus_shutdown();
}

int main(void)
{
    test_ring();
    test_connect();
    puts("bus purge OK");
    return 0;
}
