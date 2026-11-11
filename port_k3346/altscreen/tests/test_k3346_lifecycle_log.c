/* The lines that have to tell a stuck reconnect from a healthy one (#7, #11).
 * Only WARN and above reach the log in the car, so that is all this looks at. */
#define ENABLE_LOGGING 1
#define altscreen_enabled always_enabled
#define main control_mock_main
#include "test_v8s_control_mock.c"
#undef main
#include <stdarg.h>

int always_enabled(void)
{
    return 1;
}

static char g_seen[8192];
static volatile int g_join_logged;
static int g_stock_teardowns;

void log_write(log_level_t level, const char *module, const char *fmt, ...)
{
    char line[300];
    va_list ap;

    (void)module;
    if (level < LOG_LEVEL_WARN)
        return;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    assert(strlen(g_seen) + strlen(line) + 2 < sizeof(g_seen));
    strcat(g_seen, line);
    strcat(g_seen, "\n");
    if (strstr(line, "K3346_CLEANUP join"))
        g_join_logged = 1;
}

void log_hexdump(log_level_t level, const char *module, const char *prefix,
                 const uint8_t *data, size_t len, size_t max_bytes)
{
    (void)level; (void)module; (void)prefix; (void)data; (void)len; (void)max_bytes;
}

hook_result_t log_dump_file_once(const char *path, const uint8_t *data, size_t len)
{
    (void)path; (void)data; (void)len;
    return (hook_result_t)0;
}

/* Ends only after the join line was written: a line logged after the wait, which
 * says nothing about a wait that never ends, turns this into a timeout. */
static void *worker_until_logged(void *arg)
{
    (void)arg;
    while (!g_join_logged)
        usleep(1000);
    return NULL;
}

static void fake_teardown(void *receiver, cf_ref request, os_status reason, unsigned char *out_done)
{
    (void)receiver; (void)request; (void)reason; (void)out_done;
    ++g_stock_teardowns;
}

static cf_index fake_count(cf_ref arr)
{
    return ((mock_obj *)arr)->count;
}

static const void *fake_at(cf_ref arr, cf_index i)
{
    return ((mock_obj *)arr)->item[i].val;
}

static altscreen_state *new_state(void *receiver)
{
    altscreen_state *state;

    pthread_mutex_lock(&g_lock);
    state = alloc_state_locked(receiver);
    pthread_mutex_unlock(&g_lock);
    assert(state);
    return state;
}

static int g_lock_is_free(void)
{
    if (pthread_mutex_trylock(&g_lock) != 0)
        return 0;
    pthread_mutex_unlock(&g_lock);
    return 1;
}

static cf_ref setup_request(void)
{
    cf_ref request = fake_dict_new(NULL, 0, NULL, NULL);
    cf_ref streams = fake_arr_new(NULL, 0, NULL);

    fake_set(request, obj(3, "streams"), streams);
    return request;
}

int main(void)
{
    altscreen_state *state;
    const char *begin, *join, *done;
    unsigned char out_done = 0;

    prepare();
    g_api.resolved = 1;
    g_api.array_count = fake_count;
    g_api.array_value = fake_at;
    g_api.session_teardown = fake_teardown;

    /* the waits are named before they start, and the end says how long it took */
    state = new_state((void *)0x1001);
    assert(pthread_create(&state->worker, NULL, worker_until_logged, NULL) == 0);
    state->worker_started = 1;
    cleanup_altscreen((void *)0x1001);
    begin = strstr(g_seen, "K3346_CLEANUP begin receiver=0x1001 worker=1");
    join = strstr(g_seen, "K3346_CLEANUP join receiver=0x1001");
    done = strstr(g_seen, "K3346_CLEANUP done receiver=0x1001 ms=");
    assert(begin && join && done && begin < join && join < done);
    assert(find_state_receiver_locked((void *)0x1001) == NULL);

    /* no worker, no wait to name */
    g_seen[0] = 0;
    new_state((void *)0x1002);
    cleanup_altscreen((void *)0x1002);
    assert(strstr(g_seen, "K3346_CLEANUP begin receiver=0x1002 worker=0"));
    assert(!strstr(g_seen, "K3346_CLEANUP join"));
    assert(strstr(g_seen, "K3346_CLEANUP done receiver=0x1002"));

    /* nothing known about this receiver: nothing to report */
    g_seen[0] = 0;
    cleanup_altscreen((void *)0x1003);
    assert(!g_seen[0]);

    /* the new session says whether it found the old one, and does not count itself */
    log_setup_summary((void *)0x2001, setup_request());
    assert(strstr(g_seen, "K3346_SETUP streams=0 map=0 features=0 others=0"));
    new_state((void *)0x2001);
    g_seen[0] = 0;
    log_setup_summary((void *)0x2001, setup_request());
    assert(strstr(g_seen, "others=0"));
    g_seen[0] = 0;
    log_setup_summary((void *)0x2002, setup_request());
    assert(strstr(g_seen, "others=1"));
    assert(g_lock_is_free());

    /* teardown is visible in the log, also the kind that keeps stream 111 */
    g_seen[0] = 0;
    AirPlayReceiverSessionTearDown((void *)0x2001, setup_request(), 7, &out_done);
    assert(strstr(g_seen, "TEARDOWN receiver=0x2001 request=dict reason=7 alt111=0"));
    assert(!strstr(g_seen, "K3346_CLEANUP"));
    assert(g_stock_teardowns == 1);
    g_seen[0] = 0;
    AirPlayReceiverSessionTearDown((void *)0x2001, NULL, 8, &out_done);
    assert(strstr(g_seen, "TEARDOWN receiver=0x2001 request=NULL reason=8"));
    assert(strstr(g_seen, "K3346_CLEANUP done receiver=0x2001"));
    assert(g_stock_teardowns == 2);

    puts("k3346 lifecycle log OK");
    return 0;
}
