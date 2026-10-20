#define _GNU_SOURCE

#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "altscreen.h"
#include "gauge_bap54.h"
#include "gauge_layout_urls.h"
#include "framework/logging.h"

#define ALTSCREEN_SESSION_MODULE "AltScreenSession"
#define ALTSCREEN_MAX_STATES 4
#define ALTSCREEN_TIMEOUT_SECS 10
#define ALTSCREEN_PRIMARY_SYNC_WAIT_MS 3000
#define ALTSCREEN_PRIMARY_SYNC_POLL_US 10000
#define ALTSCREEN_VIEW_COUNT 4
#define ALTSCREEN_VIEW_REACQUIRE_DELAY_MS 150ULL
#define ALTSCREEN_CONTROL_ACK_TIMEOUT_MS 2500ULL
#define ALTSCREEN_CONTROL_IDLE 0
#define ALTSCREEN_CONTROL_SHOW 1
#define ALTSCREEN_CONTROL_VIEW 2
#define ALTSCREEN_CONTROL_KEYFRAME 3
#define ALTSCREEN_ACTIVE_MARKER "/tmp/mib2high-lvds-active"
#define K_CFSTRING_UTF8 0x08000100U

typedef void *cf_ref;
typedef long cf_index;
typedef unsigned long cf_type_id;
typedef int os_status;

typedef struct {
    int32_t screen;
    int32_t main_audio;
    int32_t phone;
    int32_t speech;
    int32_t speech_detail;
    int32_t turns;
} airplay_mode_state;

typedef os_status (*session_setup_fn)(void *, cf_ref, cf_ref *);
typedef void (*session_teardown_fn)(void *, cf_ref, os_status, unsigned char *);
typedef os_status (*screen_create_fn)(void **);
typedef void (*screen_delete_fn)(void *);
typedef os_status (*screen_setup_fn)(void *, cf_ref, uint32_t);
typedef os_status (*screen_security_fn)(void *, const uint8_t[16], const uint8_t[16]);
typedef os_status (*screen_process_fn)(void *, void *, int);
typedef void (*screen_stop_fn)(void *);
typedef os_status (*screen_send_cmd_fn)(void *, char, const void *, size_t);
typedef os_status (*netsock_create_native_fn)(void **, int);
typedef os_status (*netsock_delete_fn)(void *);
typedef os_status (*screen_process_data_fn)(void *, const uint8_t *, size_t, uint64_t,
                                            cf_ref, void (*)(void *), void *);
typedef os_status (*send_command_fn)(void *, cf_ref, void (*)(os_status, cf_ref, void *), void *);
typedef os_status (*mode_state_parse_fn)(void *, cf_ref, airplay_mode_state *);

typedef cf_ref (*cf_string_create_fn)(cf_ref, const char *, uint32_t);
typedef cf_ref (*cf_dict_mutable_copy_fn)(cf_ref, cf_index, cf_ref);
typedef cf_ref (*cf_array_mutable_copy_fn)(cf_ref, cf_index, cf_ref);
typedef cf_ref (*cf_dict_create_mutable_fn)(cf_ref, cf_index, const void *, const void *);
typedef cf_ref (*cf_array_create_mutable_fn)(cf_ref, cf_index, const void *);
typedef const void *(*cf_dict_get_value_fn)(cf_ref, const void *);
typedef void (*cf_dict_set_value_fn)(cf_ref, const void *, const void *);
typedef int64_t (*cf_dict_get_i64_fn)(cf_ref, const void *, os_status *);
typedef os_status (*cf_dict_set_i64_fn)(cf_ref, const void *, int64_t);
typedef cf_index (*cf_dict_count_fn)(cf_ref);
typedef void (*cf_dict_get_keys_values_fn)(cf_ref, const void **, const void **);
typedef cf_index (*cf_array_count_fn)(cf_ref);
typedef const void *(*cf_array_value_fn)(cf_ref, cf_index);
typedef void (*cf_array_append_fn)(cf_ref, const void *);
typedef unsigned char (*cf_equal_fn)(cf_ref, cf_ref);
typedef cf_type_id (*cf_get_type_fn)(cf_ref);
typedef cf_type_id (*cf_type_getter_fn)(void);
typedef cf_ref (*cf_retain_fn)(cf_ref);
typedef void (*cf_release_fn)(cf_ref);
typedef cf_ref (*cf_number_create_fn)(cf_ref, int, const void *);

typedef const void *(*cf_container_retain_cb)(void *, const void *);
typedef void (*cf_container_release_cb)(void *, const void *);
typedef cf_ref (*cf_container_description_cb)(const void *);
typedef unsigned char (*cf_container_equal_cb)(const void *, const void *);

typedef struct {
    cf_index version;
    cf_container_retain_cb retain;
    cf_container_release_cb release;
    cf_container_description_cb copy_description;
    cf_container_equal_cb equal;
} cf_dictionary_value_callbacks;

typedef struct {
    session_setup_fn session_setup;
    session_teardown_fn session_teardown;
    screen_create_fn screen_create;
    screen_delete_fn screen_delete;
    screen_setup_fn screen_setup;
    screen_security_fn screen_security;
    screen_process_fn screen_process;
    screen_stop_fn screen_stop;
    screen_send_cmd_fn screen_send_cmd;
    netsock_create_native_fn netsock_create_native;
    netsock_delete_fn netsock_delete;
    screen_process_data_fn screen_process_data;
    send_command_fn send_command;

    /* Stock parser used behind our narrow TurnByTurn interposer. */
    mode_state_parse_fn mode_state_parse;

    cf_string_create_fn string_create;
    cf_dict_mutable_copy_fn dict_mutable_copy;
    cf_array_mutable_copy_fn array_mutable_copy;
    cf_dict_create_mutable_fn dict_create_mutable;
    cf_array_create_mutable_fn array_create_mutable;
    cf_dict_get_value_fn dict_get_value;
    cf_dict_set_value_fn dict_set_value;
    cf_dict_get_i64_fn dict_get_i64;
    cf_dict_set_i64_fn dict_set_i64;
    cf_dict_count_fn dict_count;
    cf_dict_get_keys_values_fn dict_get_keys_values;
    cf_array_count_fn array_count;
    cf_array_value_fn array_value;
    cf_array_append_fn array_append;
    cf_equal_fn equal;
    cf_get_type_fn get_type;
    cf_type_getter_fn array_type;
    cf_type_getter_fn dict_type;
    cf_retain_fn retain;
    cf_release_fn release;
    cf_number_create_fn number_create;
    const void *dict_key_callbacks;
    const void *dict_value_callbacks;
    const void *array_callbacks;
    int resolved;
    int ready;
} altscreen_api;

typedef struct {
    void *receiver;
    void *screen;
    uint64_t connection_id;
    int listen_fd;
    int accepted_fd;
    uint16_t data_port;
    pthread_t worker;
    int worker_started;
    int screen_started;
    int show_sent;
    int stop_requested;
    int active;

    /* Phone route ownership is kept privately even when hidden from stock HMI. */
    int route_known;
    int route_active;
    int current_view;
    int pending_view;
    int pending_view_from_gauge; /* 1=gauge update; v8S changes alignment only on slot transition */
    uint64_t view_due_ms;
    int control_inflight; /* Commands currently inside SendCommand, not async callbacks. */
    uint64_t control_generation; /* Never reuse on reconnection. */
    int last_ui_slot; /* -1 until showUI succeeds for this CarPlay session. */
    int control_stage; /* IDLE / SHOW / VIEW / KEYFRAME. */
    int control_target_view;
    int control_target_from_gauge;
    int control_awaiting_ack;
    int control_startup; /* First-frame activation, even before VC active marker. */
    uint64_t control_token;
    uint64_t control_deadline_ms;
} altscreen_state;

static altscreen_api g_api;
static altscreen_state g_states[ALTSCREEN_MAX_STATES];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_view_worker_started; /* 0=no, -1=starting, 1=running */
/* K2161: Joey's event IDs over the stock DSI tracer, no HMI/Java replacement. */
static int g_layout_slot; /* ViewArea slot: 0 small gauges, 1 big gauges. */
static int g_bap54_layout_known; /* Starts UNKNOWN; no inferred layout from factory defaults. */
static int g_pending_zoom; /* positive=in; negative=out; serialized on existing control worker */
static int g_trace_threads_started;
static uint64_t g_control_generation_serial;
static uint64_t g_control_request_serial;

typedef struct {
    void *receiver;
    uint64_t generation;
    uint64_t token;
    int stage;
    int view;
} control_completion_context;


static const void *local_value_retain(void *allocator, const void *object)
{
    (void)allocator;
    return g_api.retain != NULL ? g_api.retain((cf_ref)object) : object;
}

static void local_value_release(void *allocator, const void *object)
{
    (void)allocator;
    if (g_api.release != NULL && object != NULL)
        g_api.release((cf_ref)object);
}

static unsigned char local_value_equal(const void *left, const void *right)
{
    if (g_api.equal == NULL)
        return left == right;
    return g_api.equal((cf_ref)left, (cf_ref)right);
}

static const cf_dictionary_value_callbacks g_local_value_callbacks = {
    0,
    local_value_retain,
    local_value_release,
    NULL,
    local_value_equal
};

static cf_ref dict_mutable_copy_compat(cf_ref allocator, cf_index capacity, cf_ref source)
{
    cf_ref out;
    cf_index count;
    cf_index i;
    const void **keys = NULL;
    const void **values = NULL;

    (void)allocator;
    (void)capacity;

    if (source == NULL || g_api.dict_create_mutable == NULL ||
        g_api.dict_count == NULL || g_api.dict_get_keys_values == NULL ||
        g_api.dict_set_value == NULL)
        return NULL;

    out = g_api.dict_create_mutable(NULL, 0,
                                    g_api.dict_key_callbacks,
                                    g_api.dict_value_callbacks);
    if (out == NULL)
        return NULL;

    count = g_api.dict_count(source);
    if (count <= 0)
        return out;

    keys = (const void **)malloc((size_t)count * sizeof(*keys));
    values = (const void **)malloc((size_t)count * sizeof(*values));
    if (keys == NULL || values == NULL) {
        free(keys);
        free(values);
        g_api.release(out);
        return NULL;
    }

    g_api.dict_get_keys_values(source, keys, values);
    for (i = 0; i < count; ++i)
        g_api.dict_set_value(out, keys[i], values[i]);

    free(keys);
    free(values);
    return out;
}

static void *sym(const char *name)
{
    return dlsym(RTLD_NEXT, name);
}

static void resolve_api(void)
{
    if (g_api.resolved)
        return;

    g_api.resolved = 1;

    g_api.session_setup = (session_setup_fn)sym("AirPlayReceiverSessionSetup");
    g_api.session_teardown = (session_teardown_fn)sym("AirPlayReceiverSessionTearDown");
    g_api.screen_create = (screen_create_fn)sym("AirPlayReceiverSessionScreen_Create");
    g_api.screen_delete = (screen_delete_fn)sym("AirPlayReceiverSessionScreen_Delete");
    g_api.screen_setup = (screen_setup_fn)sym("AirPlayReceiverSessionScreen_Setup");
    g_api.screen_security = (screen_security_fn)sym("AirPlayReceiverSessionScreen_SetSecurityInfo");
    g_api.screen_process = (screen_process_fn)sym("AirPlayReceiverSessionScreen_ProcessFrames");
    g_api.screen_stop = (screen_stop_fn)sym("AirPlayReceiverSessionScreen_StopSession");
    g_api.screen_send_cmd = (screen_send_cmd_fn)sym("AirPlayReceiverSessionScreen_SendCommand");
    g_api.netsock_create_native = (netsock_create_native_fn)sym("NetSocket_CreateWithNative");
    g_api.netsock_delete = (netsock_delete_fn)sym("NetSocket_Delete");
    g_api.screen_process_data = (screen_process_data_fn)sym("ScreenStreamProcessData");
    g_api.send_command = (send_command_fn)sym("AirPlayReceiverSessionSendCommand");

    g_api.mode_state_parse = (mode_state_parse_fn)sym("AirPlayReceiverSessionMakeModeStateFromDictionary");

    g_api.string_create = (cf_string_create_fn)sym("CFStringCreateWithCString");
    g_api.array_mutable_copy = (cf_array_mutable_copy_fn)sym("CFArrayCreateMutableCopy");
    g_api.dict_create_mutable = (cf_dict_create_mutable_fn)sym("CFDictionaryCreateMutable");
    g_api.array_create_mutable = (cf_array_create_mutable_fn)sym("CFArrayCreateMutable");
    g_api.dict_get_value = (cf_dict_get_value_fn)sym("CFDictionaryGetValue");
    g_api.dict_set_value = (cf_dict_set_value_fn)sym("CFDictionarySetValue");
    g_api.dict_get_i64 = (cf_dict_get_i64_fn)sym("CFDictionaryGetInt64");
    g_api.dict_set_i64 = (cf_dict_set_i64_fn)sym("CFDictionarySetInt64");
    g_api.dict_count = (cf_dict_count_fn)sym("CFDictionaryGetCount");
    g_api.dict_get_keys_values = (cf_dict_get_keys_values_fn)sym("CFDictionaryGetKeysAndValues");
    g_api.array_count = (cf_array_count_fn)sym("CFArrayGetCount");
    g_api.array_value = (cf_array_value_fn)sym("CFArrayGetValueAtIndex");
    g_api.array_append = (cf_array_append_fn)sym("CFArrayAppendValue");
    g_api.equal = (cf_equal_fn)sym("CFEqual");
    g_api.get_type = (cf_get_type_fn)sym("CFGetTypeID");
    g_api.array_type = (cf_type_getter_fn)sym("CFArrayGetTypeID");
    g_api.dict_type = (cf_type_getter_fn)sym("CFDictionaryGetTypeID");
    g_api.retain = (cf_retain_fn)sym("CFRetain");
    g_api.release = (cf_release_fn)sym("CFRelease");
    g_api.number_create = (cf_number_create_fn)sym("CFNumberCreate");

    g_api.dict_key_callbacks = sym("kCFTypeDictionaryKeyCallBacks");
    if (g_api.dict_key_callbacks == NULL)
        g_api.dict_key_callbacks = sym("kCFLDictionaryKeyCallBacksCFLTypes");

    g_api.dict_value_callbacks = sym("kCFTypeDictionaryValueCallBacks");
    if (g_api.dict_value_callbacks == NULL)
        g_api.dict_value_callbacks = sym("kCFLDictionaryValueCallBacksCFLTypes");
    if (g_api.dict_value_callbacks == NULL)
        g_api.dict_value_callbacks = &g_local_value_callbacks;

    g_api.array_callbacks = sym("kCFTypeArrayCallBacks");
    if (g_api.array_callbacks == NULL)
        g_api.array_callbacks = sym("kCFLArrayCallBacksCFLTypes");

    g_api.dict_mutable_copy = (cf_dict_mutable_copy_fn)sym("CFDictionaryCreateMutableCopy");
    if (g_api.dict_mutable_copy == NULL)
        g_api.dict_mutable_copy = dict_mutable_copy_compat;

    g_api.ready =
        g_api.session_setup != NULL &&
        g_api.session_teardown != NULL &&
        g_api.screen_create != NULL &&
        g_api.screen_delete != NULL &&
        g_api.screen_setup != NULL &&
        g_api.screen_security != NULL &&
        g_api.screen_process != NULL &&
        g_api.screen_stop != NULL &&
        g_api.netsock_create_native != NULL &&
        g_api.netsock_delete != NULL &&
        g_api.screen_process_data != NULL &&
        g_api.send_command != NULL &&
        g_api.string_create != NULL &&
        g_api.dict_mutable_copy != NULL &&
        g_api.array_mutable_copy != NULL &&
        g_api.dict_create_mutable != NULL &&
        g_api.array_create_mutable != NULL &&
        g_api.dict_get_value != NULL &&
        g_api.dict_set_value != NULL &&
        g_api.dict_get_i64 != NULL &&
        g_api.dict_set_i64 != NULL &&
        g_api.dict_count != NULL &&
        g_api.dict_get_keys_values != NULL &&
        g_api.array_count != NULL &&
        g_api.array_value != NULL &&
        g_api.array_append != NULL &&
        g_api.equal != NULL &&
        g_api.get_type != NULL &&
        g_api.array_type != NULL &&
        g_api.dict_type != NULL &&
        g_api.retain != NULL &&
        g_api.release != NULL &&
        g_api.dict_key_callbacks != NULL &&
        g_api.dict_value_callbacks != NULL &&
        g_api.array_callbacks != NULL;

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "runtime API ready=%d optional_screen_quit=%d dict_copy=%s "
             "mode_parser=%d number_create=%d",
             g_api.ready, g_api.screen_send_cmd != NULL,
             sym("CFDictionaryCreateMutableCopy") != NULL ? "native" : "compat",
             g_api.mode_state_parse != NULL, g_api.number_create != NULL);

    if (!g_api.ready) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "runtime API missing setup=%d teardown=%d create=%d delete=%d screen_setup=%d security=%d process=%d stop=%d netsock_create=%d netsock_delete=%d process_data=%d send_command=%d cf_string=%d dict_copy=%d array_copy=%d dict_create=%d array_create=%d dict_get=%d dict_set=%d dict_get_i64=%d dict_set_i64=%d dict_count=%d dict_keys_values=%d array_count=%d array_value=%d array_append=%d equal=%d get_type=%d array_type=%d dict_type=%d retain=%d release=%d dict_keys=%d dict_values=%d array_callbacks=%d",
                  g_api.session_setup != NULL,
                  g_api.session_teardown != NULL,
                  g_api.screen_create != NULL,
                  g_api.screen_delete != NULL,
                  g_api.screen_setup != NULL,
                  g_api.screen_security != NULL,
                  g_api.screen_process != NULL,
                  g_api.screen_stop != NULL,
                  g_api.netsock_create_native != NULL,
                  g_api.netsock_delete != NULL,
                  g_api.screen_process_data != NULL,
                  g_api.send_command != NULL,
                  g_api.string_create != NULL,
                  g_api.dict_mutable_copy != NULL,
                  g_api.array_mutable_copy != NULL,
                  g_api.dict_create_mutable != NULL,
                  g_api.array_create_mutable != NULL,
                  g_api.dict_get_value != NULL,
                  g_api.dict_set_value != NULL,
                  g_api.dict_get_i64 != NULL,
                  g_api.dict_set_i64 != NULL,
                  g_api.dict_count != NULL,
                  g_api.dict_get_keys_values != NULL,
                  g_api.array_count != NULL,
                  g_api.array_value != NULL,
                  g_api.array_append != NULL,
                  g_api.equal != NULL,
                  g_api.get_type != NULL,
                  g_api.array_type != NULL,
                  g_api.dict_type != NULL,
                  g_api.retain != NULL,
                  g_api.release != NULL,
                  g_api.dict_key_callbacks != NULL,
                  g_api.dict_value_callbacks != NULL,
                  g_api.array_callbacks != NULL);
    }
}

static cf_ref cfs(const char *s)
{
    if (s == NULL || g_api.string_create == NULL)
        return NULL;
    return g_api.string_create(NULL, s, K_CFSTRING_UTF8);
}

static int is_array(cf_ref obj)
{
    return obj != NULL && g_api.get_type(obj) == g_api.array_type();
}

static int is_dict(cf_ref obj)
{
    return obj != NULL && g_api.get_type(obj) == g_api.dict_type();
}

static cf_ref dict_get(cf_ref dict, const char *name)
{
    cf_ref key;
    cf_ref value = NULL;

    if (dict == NULL || name == NULL)
        return NULL;
    key = cfs(name);
    if (key == NULL)
        return NULL;
    value = (cf_ref)g_api.dict_get_value(dict, key);
    g_api.release(key);
    return value;
}

static int64_t dict_get_i64(cf_ref dict, const char *name, int *ok)
{
    cf_ref key;
    os_status err = -1;
    int64_t value = 0;

    if (ok != NULL)
        *ok = 0;
    key = cfs(name);
    if (key == NULL)
        return 0;
    value = g_api.dict_get_i64(dict, key, &err);
    g_api.release(key);
    if (ok != NULL)
        *ok = (err == 0);
    return value;
}

static int dict_set_obj(cf_ref dict, const char *name, cf_ref value)
{
    cf_ref key;

    if (dict == NULL || value == NULL)
        return 0;
    key = cfs(name);
    if (key == NULL)
        return 0;
    g_api.dict_set_value(dict, key, value);
    g_api.release(key);
    return 1;
}

static int dict_set_i64(cf_ref dict, const char *name, int64_t value)
{
    cf_ref key;
    os_status rc;

    key = cfs(name);
    if (key == NULL)
        return 0;
    rc = g_api.dict_set_i64(dict, key, value);
    g_api.release(key);
    return rc == 0;
}

static int dict_set_string(cf_ref dict, const char *name, const char *value)
{
    cf_ref s;
    int ok;

    s = cfs(value);
    if (s == NULL)
        return 0;
    ok = dict_set_obj(dict, name, s);
    g_api.release(s);
    return ok;
}

static cf_ref new_dict(void)
{
    return g_api.dict_create_mutable(NULL, 0,
                                     g_api.dict_key_callbacks,
                                     g_api.dict_value_callbacks);
}

static cf_ref new_array(void)
{
    return g_api.array_create_mutable(NULL, 0, g_api.array_callbacks);
}

static uint64_t now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
}

static int array_contains_string(cf_ref array, const char *value)
{
    cf_ref needle;
    cf_index i, n;
    int found = 0;

    if (!is_array(array))
        return 0;
    needle = cfs(value);
    if (needle == NULL)
        return 0;
    n = g_api.array_count(array);
    for (i = 0; i < n; ++i) {
        cf_ref item = (cf_ref)g_api.array_value(array, i);
        if (item != NULL && g_api.equal(item, needle)) {
            found = 1;
            break;
        }
    }
    g_api.release(needle);
    return found;
}

static int append_string(cf_ref array, const char *value)
{
    cf_ref s = cfs(value);

    if (s == NULL)
        return 0;
    g_api.array_append(array, s);
    g_api.release(s);
    return 1;
}

static int set_reference_enabled_features(cf_ref response)
{
    cf_ref features = NULL;
    cf_ref alt = NULL;
    cf_ref views = NULL;

    if (!is_dict(response))
        return 0;

    features = new_array();
    alt = cfs("altScreen");
    views = cfs("viewAreas");
    if (features == NULL || alt == NULL || views == NULL)
        goto fail;

    g_api.array_append(features, alt);
    g_api.array_append(features, views);
    if (!dict_set_obj(response, "enabledFeatures", features))
        goto fail;

    g_api.release(views);
    g_api.release(alt);
    g_api.release(features);
    return 1;

fail:
    if (views != NULL) g_api.release(views);
    if (alt != NULL) g_api.release(alt);
    if (features != NULL) g_api.release(features);
    return 0;
}

static cf_ref clone_response_with_reference_features(cf_ref stock_response)
{
    cf_ref response;

    if (!is_dict(stock_response))
        return NULL;
    response = g_api.dict_mutable_copy(NULL, 0, stock_response);
    if (response == NULL)
        return NULL;
    if (!set_reference_enabled_features(response)) {
        g_api.release(response);
        return NULL;
    }
    return response;
}

static void log_setup_request(void *receiver, cf_ref request)
{
    cf_ref streams;
    cf_index i, n;

    if (!is_dict(request)) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_REQUEST receiver=%p level=unknown request_not_dict=1",
                 receiver);
        return;
    }

    streams = dict_get(request, "streams");
    if (!is_array(streams)) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_REQUEST receiver=%p level=session streams_present=0",
                 receiver);
        return;
    }

    n = g_api.array_count(streams);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "SETUP_REQUEST receiver=%p level=stream streams_present=1 count=%ld",
             receiver, (long)n);

    for (i = 0; i < n; ++i) {
        cf_ref desc = (cf_ref)g_api.array_value(streams, i);
        int ok = 0;
        int64_t type = 0;

        if (is_dict(desc))
            type = dict_get_i64(desc, "type", &ok);

        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_STREAM receiver=%p index=%ld type_valid=%d type=%lld",
                 receiver, (long)i, ok, (long long)type);
    }
}

static void classify_teardown_request(cf_ref request,
                                      int *has_alt,
                                      int *has_main,
                                      int *other_streams)
{
    cf_ref streams;
    cf_index i, n;

    if (has_alt != NULL) *has_alt = 0;
    if (has_main != NULL) *has_main = 0;
    if (other_streams != NULL) *other_streams = 0;

    if (!is_dict(request))
        return;

    streams = dict_get(request, "streams");
    if (!is_array(streams))
        return;

    n = g_api.array_count(streams);
    for (i = 0; i < n; ++i) {
        cf_ref desc = (cf_ref)g_api.array_value(streams, i);
        int ok = 0;
        int64_t type = 0;

        if (is_dict(desc))
            type = dict_get_i64(desc, "type", &ok);

        if (ok && type == AIRPLAY_STREAM_TYPE_ALT_SCREEN) {
            if (has_alt != NULL) *has_alt = 1;
        } else {
            if (other_streams != NULL) ++(*other_streams);
            if (ok && type == 110 && has_main != NULL) *has_main = 1;
        }
    }
}

static void log_setup_response_features(void *receiver,
                                        os_status rc,
                                        cf_ref response)
{
    cf_ref features;
    cf_index count;

    if (rc != 0) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_RESPONSE_FEATURES receiver=%p rc=%d skipped=setup_error",
                 receiver, rc);
        return;
    }

    if (!is_dict(response)) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_RESPONSE_FEATURES receiver=%p rc=%d response_dict=0",
                 receiver, rc);
        return;
    }

    features = dict_get(response, "enabledFeatures");
    if (!is_array(features)) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_RESPONSE_FEATURES receiver=%p rc=%d present=0 "
                 "altScreen=0 viewAreas=0",
                 receiver, rc);
        return;
    }

    count = g_api.array_count(features);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "SETUP_RESPONSE_FEATURES receiver=%p rc=%d present=1 count=%ld "
             "altScreen=%d viewAreas=%d",
             receiver, rc, (long)count,
             array_contains_string(features, "altScreen"),
             array_contains_string(features, "viewAreas"));
}

static cf_ref build_stock_only_request(cf_ref request, cf_ref *out_alt_desc)
{
    cf_ref streams;
    cf_ref stock_request = NULL;
    cf_ref stock_streams = NULL;
    cf_index i, n;
    int alt_count = 0;

    if (out_alt_desc != NULL)
        *out_alt_desc = NULL;
    if (!is_dict(request))
        return NULL;

    streams = dict_get(request, "streams");
    if (!is_array(streams))
        return NULL;

    n = g_api.array_count(streams);
    stock_request = g_api.dict_mutable_copy(NULL, 0, request);
    stock_streams = new_array();
    if (stock_request == NULL || stock_streams == NULL)
        goto fail;

    for (i = 0; i < n; ++i) {
        cf_ref desc = (cf_ref)g_api.array_value(streams, i);
        int ok = 0;
        int64_t type;

        if (!is_dict(desc)) {
            g_api.array_append(stock_streams, desc);
            continue;
        }
        type = dict_get_i64(desc, "type", &ok);
        if (ok && type == AIRPLAY_STREAM_TYPE_ALT_SCREEN) {
            ++alt_count;
            if (out_alt_desc != NULL)
                *out_alt_desc = desc;
            continue;
        }
        g_api.array_append(stock_streams, desc);
    }

    if (alt_count == 0) {
        g_api.release(stock_streams);
        g_api.release(stock_request);
        return NULL;
    }
    if (alt_count != 1) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "request split refused alt_count=%d expected=1 primary_untouched=1",
                  alt_count);
        goto fail;
    }

    if (!dict_set_obj(stock_request, "streams", stock_streams))
        goto fail;

    g_api.release(stock_streams);
    return stock_request;

fail:
    if (stock_streams != NULL) g_api.release(stock_streams);
    if (stock_request != NULL) g_api.release(stock_request);
    if (out_alt_desc != NULL) *out_alt_desc = NULL;
    return NULL;
}

static cf_ref merge_altscreen_response(cf_ref stock_response,
                                       cf_ref requested_alt_desc,
                                       uint16_t port)
{
    cf_ref response = NULL;
    cf_ref streams = NULL;
    cf_ref out_streams = NULL;
    cf_ref alt_stream = NULL;

    if (stock_response != NULL && is_dict(stock_response))
        response = g_api.dict_mutable_copy(NULL, 0, stock_response);
    else
        response = new_dict();
    if (response == NULL || !is_dict(requested_alt_desc))
        goto fail;
    if (!set_reference_enabled_features(response))
        goto fail;

    streams = dict_get(response, "streams");
    if (is_array(streams))
        out_streams = g_api.array_mutable_copy(NULL, g_api.array_count(streams) + 1, streams);
    else
        out_streams = new_array();

    /*
     * Preserve the exact peer-requested Stream-111 descriptor (including
     * streamConnectionID and any iOS-version-specific keys), then add only
     * the receiver-owned response fields.  This matches the vehicle-proven
     * Joey/Harman GEN2 handshake instead of synthesizing a minimal response.
     */
    alt_stream = g_api.dict_mutable_copy(NULL, 0, requested_alt_desc);
    if (out_streams == NULL || alt_stream == NULL)
        goto fail;

    if (!dict_set_i64(alt_stream, "dataPort", (int64_t)port) ||
        !dict_set_i64(alt_stream, "streamID", AIRPLAY_STREAM_TYPE_ALT_SCREEN))
        goto fail;

    g_api.array_append(out_streams, alt_stream);
    if (!dict_set_obj(response, "streams", out_streams))
        goto fail;

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "SETUP_RESPONSE_ALT cloned_request=1 dataPort=%u streamID=111 unknown_fields_preserved=1",
             (unsigned int)port);

    g_api.release(alt_stream);
    g_api.release(out_streams);
    return response;

fail:
    if (alt_stream != NULL) g_api.release(alt_stream);
    if (out_streams != NULL) g_api.release(out_streams);
    if (response != NULL) g_api.release(response);
    return NULL;
}

static int open_listener(uint16_t *out_port)
{
    int fd = -1;
    int one = 1;

    if (out_port == NULL)
        return -1;

#ifdef AF_INET6
    {
        int off = 0;
        struct sockaddr_in6 addr6;
        socklen_t len6 = sizeof(addr6);

        fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        if (fd >= 0) {
            (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#ifdef IPV6_V6ONLY
            (void)setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
#endif
            memset(&addr6, 0, sizeof(addr6));
            addr6.sin6_family = AF_INET6;
            addr6.sin6_port = htons(6000);

            if (bind(fd, (struct sockaddr *)&addr6, sizeof(addr6)) == 0 &&
                listen(fd, 1) == 0 &&
                getsockname(fd, (struct sockaddr *)&addr6, &len6) == 0) {
                *out_port = ntohs(addr6.sin6_port);
                if (*out_port != 0) {
                    LOG_INFO(ALTSCREEN_SESSION_MODULE,
                             "listener ready port=%u family=IPv6-dualstack",
                             (unsigned int)*out_port);
                    return fd;
                }
            }
            close(fd);
            fd = -1;
        }
    }
#endif

    {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);

        fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fd < 0)
            return -1;

        (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(6000);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                      "listener bind failed port=6000 errno=%d primary_preserved=1",
                      errno);
            goto fail;
        }
        if (listen(fd, 1) != 0)
            goto fail;
        if (getsockname(fd, (struct sockaddr *)&addr, &len) != 0)
            goto fail;

        *out_port = ntohs(addr.sin_port);
        if (*out_port == 0)
            goto fail;
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "listener ready port=%u family=IPv4-fallback",
                 (unsigned int)*out_port);
        return fd;
    }

fail:
    if (fd >= 0)
        close(fd);
    return -1;
}

static int wake_listener(uint16_t port)
{
    int fd;
    int rc;

    if (port == 0)
        return 0;

#ifdef AF_INET6
    {
        struct sockaddr_in6 addr6;
        fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        if (fd >= 0) {
            memset(&addr6, 0, sizeof(addr6));
            addr6.sin6_family = AF_INET6;
            addr6.sin6_port = htons(port);
            addr6.sin6_addr.s6_addr[15] = 1; /* ::1 */
            rc = connect(fd, (struct sockaddr *)&addr6, sizeof(addr6));
            close(fd);
            if (rc == 0)
                return 1;
        }
    }
#endif

    {
        struct sockaddr_in addr;
        fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fd < 0)
            return 0;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);
        rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        close(fd);
        return rc == 0;
    }
}

static altscreen_state *find_state_receiver_locked(void *receiver)
{
    int i;

    for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
        if (g_states[i].active && g_states[i].receiver == receiver)
            return &g_states[i];
    }
    return NULL;
}

static altscreen_state *find_state_worker_locked(pthread_t thread)
{
    int i;

    for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
        if (g_states[i].active && g_states[i].worker_started &&
            pthread_equal(g_states[i].worker, thread))
            return &g_states[i];
    }
    return NULL;
}

static altscreen_state *alloc_state_locked(void *receiver)
{
    int i;

    if (find_state_receiver_locked(receiver) != NULL)
        return NULL;
    for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
        if (!g_states[i].active) {
            memset(&g_states[i], 0, sizeof(g_states[i]));
            g_states[i].receiver = receiver;
            g_states[i].listen_fd = -1;
            g_states[i].accepted_fd = -1;
            g_states[i].current_view = -1;
            g_states[i].pending_view = -1;
            g_states[i].control_target_view = -1;
            g_states[i].last_ui_slot = -1;
            g_states[i].control_generation = ++g_control_generation_serial;
            g_states[i].active = 1;
            return &g_states[i];
        }
    }
    return NULL;
}

static int private111_active_for_receiver(void *receiver)
{
    altscreen_state *state;
    int active = 0;

    if (access(ALTSCREEN_ACTIVE_MARKER, F_OK) != 0)
        return 0;

    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(receiver);
    if (state != NULL && state->active && state->screen_started &&
        !state->stop_requested)
        active = 1;
    pthread_mutex_unlock(&g_lock);
    return active;
}

static void note_route_state(void *receiver, int route_active, int *changed)
{
    altscreen_state *state;

    if (changed != NULL)
        *changed = 0;
    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(receiver);
    if (state != NULL) {
        if (!state->route_known || state->route_active != route_active) {
            if (changed != NULL)
                *changed = 1;
            state->route_active = route_active;
            state->route_known = 1;
        }
    }
    pthread_mutex_unlock(&g_lock);
}

static int route_view_for_state(const altscreen_state *state)
{
    int route = (state != NULL && state->route_known && state->route_active) ? 1 : 0;
    return g_layout_slot * 2 + route;
}

/* Route transitions must preserve the current gauge layout (0/1 or 2/3).
 * Choose the ViewArea from live route + layout state under the same lock used
 * by the DSITracer layout reader; do not hardcode the small-gauge views. */
static void queue_route_view_for_receiver(void *receiver, uint64_t delay_ms)
{
    altscreen_state *state;

    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(receiver);
    if (state != NULL && state->active && !state->stop_requested) {
        state->pending_view = route_view_for_state(state);
        state->pending_view_from_gauge = 0; /* Preserve route showUI behavior. */
        state->view_due_ms = now_ms() + delay_ms;
    }
    pthread_mutex_unlock(&g_lock);
}

static cf_ref make_event_request(const char *type, const char *url)
{
    cf_ref request = NULL;
    cf_ref params = NULL;

    request = new_dict();
    params = new_dict();
    if (request == NULL || params == NULL)
        goto fail;

    if (!dict_set_string(request, "type", type) ||
        !dict_set_string(params, "uuid", ALTSCREEN_UUID))
        goto fail;
    if (url != NULL && !dict_set_string(params, "url", url))
        goto fail;
    if (!dict_set_obj(request, "params", params))
        goto fail;

    g_api.release(params);
    return request;

fail:
    if (params != NULL) g_api.release(params);
    if (request != NULL) g_api.release(request);
    return NULL;
}

static cf_ref make_view_request(int view)
{
    cf_ref request = NULL;
    cf_ref params = NULL;
    cf_ref adjacent = NULL;
    int i;

    if (view < 0 || view >= ALTSCREEN_VIEW_COUNT)
        return NULL;

    if (g_api.number_create == NULL) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "updateViewArea refused reason=CFNumberCreate_missing adjacency_required=1");
        return NULL;
    }

    request = make_event_request("updateViewArea", NULL);
    if (request == NULL)
        return NULL;
    params = dict_get(request, "params");
    if (!is_dict(params) || !dict_set_i64(params, "viewAreaIndex", view) ||
        !dict_set_i64(params, "animationDurationMillis", 0))
        goto fail;

    /* Joey GEN2 parity: adjacency belongs to the updateViewArea command. */
    adjacent = new_array();
    if (adjacent == NULL)
        goto fail;
    for (i = 0; i < ALTSCREEN_VIEW_COUNT; ++i) {
        int64_t v = i;
        cf_ref number;
        if (i == view)
            continue;
        number = g_api.number_create(NULL, 4, &v);
        if (number == NULL)
            goto fail;
        g_api.array_append(adjacent, number);
        g_api.release(number);
    }
    if (!dict_set_obj(params, "adjacentViewAreas", adjacent))
        goto fail;
    g_api.release(adjacent);
    adjacent = NULL;
    return request;

fail:
    if (adjacent != NULL) g_api.release(adjacent);
    if (request != NULL) g_api.release(request);
    return NULL;
}

static os_status send_zoom_event(void *receiver, int direction)
{
    cf_ref request = NULL, params = NULL;
    os_status rc;
    if (receiver == NULL || g_api.send_command == NULL || (direction != 0 && direction != 1))
        return -1;
    /* Joey's vehicle-tested control command_zoom(dir,1) includes the target
     * cluster display uuid. v8g2 omitted it; iOS accepted transport (rc=0)
     * but did not zoom the cluster. This is the focused v8i correction. */
    request = new_dict();
    params = new_dict();
    if (request == NULL || params == NULL ||
        !dict_set_string(request, "type", "changeMapZoomLevel") ||
        !dict_set_string(params, "uuid", ALTSCREEN_UUID) ||
        !dict_set_i64(params, "zoomDirection", direction) ||
        !dict_set_obj(request, "params", params)) {
        if (params != NULL) g_api.release(params);
        if (request != NULL) g_api.release(request);
        return -1;
    }
    g_api.release(params);
    rc = g_api.send_command(receiver, request, NULL, NULL);
    g_api.release(request);
    LOG_INFO(ALTSCREEN_SESSION_MODULE, "V8I_ZOOM direction=%d uuid=%s rc=%d ack=UNKNOWN", direction, ALTSCREEN_UUID, rc);
    return rc;
}

static os_status send_view_event(void *receiver, int view)
{
    cf_ref request;
    os_status rc;

    if (receiver == NULL || g_api.send_command == NULL)
        return -1;
    request = make_view_request(view);
    if (request == NULL)
        return -1;
    rc = g_api.send_command(receiver, request, NULL, NULL);
    g_api.release(request);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "event type=updateViewArea rc=%d view=%d adjacent=all_other_views",
             rc, view);
    return rc;
}

static os_status send_event(void *receiver, const char *type, const char *url)
{
    cf_ref request;
    os_status rc = -1;

    if (receiver == NULL || g_api.send_command == NULL)
        return -1;
    request = make_event_request(type, url);
    if (request == NULL)
        return -1;

    rc = g_api.send_command(receiver, request, NULL, NULL);
    g_api.release(request);

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "event type=%s rc=%d uuid=%s url=%s",
             type, rc, ALTSCREEN_UUID, url != NULL ? url : "-");
    return rc;
}

/* First real stream frame starts the UI sequence.  A fresh state/generation
 * forces showUI once, even if the Audi VC active marker is not yet present. */
static void request_cluster(altscreen_state *state)
{
    int view = -1;
    if (state == NULL) return;
    pthread_mutex_lock(&g_lock);
    if (state->active && !state->stop_requested) {
        view = route_view_for_state(state);
        state->pending_view = view;
        state->pending_view_from_gauge = 0;
        state->view_due_ms = now_ms();
        state->control_startup = 1;
    }
    pthread_mutex_unlock(&g_lock);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "V8S_CONTROL_STARTUP receiver=%p view=%d queued=%d",
             state->receiver, view, view >= 0);
}

int altscreen_request_fresh_keyframe(void)
{
    void *receiver = NULL;
    os_status rc;
    int i;

    pthread_mutex_lock(&g_lock);
    for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
        if (g_states[i].active && g_states[i].receiver != NULL &&
            g_states[i].screen_started && !g_states[i].stop_requested) {
            receiver = g_states[i].receiver;
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);

    if (receiver == NULL) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "fresh keyframe request skipped reason=no_active_stream111");
        return 0;
    }

    rc = send_event(receiver, "forceKeyFrame", NULL);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "fresh keyframe request receiver=%p rc=%d source=renderer_reprime",
             receiver, rc);
    return rc == 0;
}

/* Format and values are verbatim from Joey r52/r55/r56. Never intercept stock wheel input. */
/* Passive trace samples; at most one line per class/second so diagnostics
 * cannot saturate the hook's bounded asynchronous logging queue.  These
 * observations NEVER change the sender or AirPlay/BAP command paths. */
static void v8g2_diag_trace_sample(const char *line, int buffered)
{
    static uint64_t last_sample_ms[4];
    int class_id = -1, emit = 0;
    uint64_t now;
    if (line[0] != '1' || line[1] != '#') return;
    if (strstr(line, "DSIKOMOView") != NULL)
        class_id = 0;
    else if (strstr(line, "DSIMapViewerZoomEngine") != NULL)
        class_id = 1;
    else if (strstr(line, "DSIMapViewerControl") != NULL &&
             (strstr(line, "ViewScreenViewPort") != NULL ||
              strstr(line, "ViewPort") != NULL))
        class_id = 3;
    else if (strstr(line, "DSIKeyPanel") != NULL &&
             (strstr(line, "|int:4|int:40|") != NULL ||
              strstr(line, "|int:4|int:35|") != NULL))
        class_id = 2;
    if (class_id < 0) return;
    now = now_ms();
    pthread_mutex_lock(&g_lock);
    if (last_sample_ms[class_id] == 0 || now < last_sample_ms[class_id] ||
        now - last_sample_ms[class_id] >= 1000ULL) {
        last_sample_ms[class_id] = now;
        emit = 1;
    }
    pthread_mutex_unlock(&g_lock);
    if (emit)
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "DIAG_DSI_SAMPLE class=%d source=%s raw=%.360s",
                 class_id, buffered ? "buffered" : "live", line);
}

/* Authoritative K2161 VIEW-result state (not a steering-wheel press counter).
 * The v8M car trace verifies this indication appears on the LIVE port 1798
 * as a 2# subscriber record; 3#/4# are duplicate subscriptions.
 * Preserve the existing layout when the state is not yet known. */
static void v8n_observe_gauge_bap54(const char *line, int buffered)
{
    int slot, previous, first, i, queued = 0;
    unsigned int raw = 0;
    if (buffered || !altscreen_bap54_layout_slot(line, &slot, &raw))
        return; /* Buffered playback must NEVER override current live state. */
    pthread_mutex_lock(&g_lock);
    previous = g_layout_slot;
    first = !g_bap54_layout_known;
    g_bap54_layout_known = 1;
    if (slot != previous) {
        g_layout_slot = slot;
        for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
            altscreen_state *state = &g_states[i];
            if (!state->active || !state->screen_started || state->stop_requested)
                continue;
            state->pending_view = route_view_for_state(state);
            state->pending_view_from_gauge = 1;
            state->view_due_ms = now_ms() + ALTSCREEN_VIEW_REACQUIRE_DELAY_MS;
            ++queued;
        }
    }
    pthread_mutex_unlock(&g_lock);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "V8N_GAUGE_BAP54 value=%u gauge=%s slot=%d previous_slot=%d "
             "first_state=%d queued=%d source=DSIBAP_live_1798",
             raw, slot == 1 ? "BIG" : "SMALL", slot, previous, first, queued);
}

static void v8h_trace_line(const char *line, int buffered)
{
    const char *p;
    int x, y, w, h, slot, step, i;
    /* BAP indications are 2#, not 1# on this car. Must process before
     * the original Joey 1# filter for zoom and viewport telemetry. */
    v8n_observe_gauge_bap54(line, buffered);
    v8g2_diag_trace_sample(line, buffered);
    if (line[0] != '1' || line[1] != '#')
        return; /* Joey 1# and 5# are duplicates; 1# only for original signals */
    /* On K2161, the old A4 setZoomArea signal was absent in v8g2 car
     * traces. Capture alternative exact K2161 viewer-viewport attributes
     * passively; a viewport event is NOT yet a validated gauge-size change. */
    if (strstr(line, "DSIMapViewerControl") != NULL &&
        (strstr(line, "ViewScreenViewPort") != NULL ||
         strstr(line, "ViewPort") != NULL)) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "V8I_GAUGE_CANDIDATE validated=0 source=%s raw=%.320s",
                 buffered ? "buffered" : "live", line);
    }
    p = strstr(line, "DSIMapViewerZoomEngine#setZoomArea#");
    if (p != NULL) {
        p = strstr(p, "Rect:");
        if (p == NULL || sscanf(p + 5, "%d:%d:%d:%d", &x, &y, &w, &h) != 4)
            return;
        /* K2161 Q7 supports the two layouts; Joey's slot 2 (third layout) is untested. */
        slot = (w <= 560) ? 1 : ((w <= 800) ? 0 : -1);
        if (slot < 0)
            return;
        pthread_mutex_lock(&g_lock);
        /* Once FctID 54 is observed, unverified ZoomArea size heuristics
         * must not reverse its confirmed absolute gauge state. */
        if (g_bap54_layout_known) {
            pthread_mutex_unlock(&g_lock);
            return;
        }
        if (slot != g_layout_slot) {
            g_layout_slot = slot;
            for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
                altscreen_state *state = &g_states[i];
                if (!state->active || !state->screen_started || state->stop_requested)
                    continue;
                state->pending_view = route_view_for_state(state);
                state->pending_view_from_gauge = 1;
                state->view_due_ms = now_ms() + ALTSCREEN_VIEW_REACQUIRE_DELAY_MS;
            }
            LOG_INFO(ALTSCREEN_SESSION_MODULE,
                     "V8H_LAYOUT slot=%d Rect=%d:%d:%d:%d view_index=%d source=%s",
                     slot, x,y,w,h, slot*2, buffered ? "buffered" : "live");
        }
        pthread_mutex_unlock(&g_lock);
        return;
    }
    if (buffered) return; /* replay must never generate stale wheel zoom */
    p = strstr(line, "DSIKeyPanelListener#updateEncoder2#");
    if (p == NULL || (p = strstr(p, "|int:4|int:40|int:")) == NULL)
        return;
    step = atoi(p + strlen("|int:4|int:40|int:"));
    if (step == 0) return;
    /* Bound outstanding steps. No zoom command until active stream 111 is confirmed. */
    pthread_mutex_lock(&g_lock);
    if (g_pending_zoom > -20 && g_pending_zoom < 20)
        g_pending_zoom += step > 0 ? 1 : -1;
    pthread_mutex_unlock(&g_lock);
}

static void *v8h_trace_reader(void *arg)
{
    int buffered = (arg != NULL);
    unsigned short port = buffered ? 20991 : 1798;
    char buf[1024], line[512]; size_t ll = 0;
    unsigned int attempts = 0;
    for (;;) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        ssize_t n = 0, j;
        uint64_t lines_seen = 0;
        if (fd < 0) {
            if (++attempts == 1 || attempts % 15 == 0)
                LOG_WARN(ALTSCREEN_SESSION_MODULE,
                         "DIAG_TRACER_SOCKET_FAIL port=%u errno=%d attempts=%u",
                         port, errno, attempts);
            sleep(2); continue;
        }
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            int err = errno;
            if (++attempts == 1 || attempts % 15 == 0)
                LOG_WARN(ALTSCREEN_SESSION_MODULE,
                         "DIAG_TRACER_CONNECT_FAIL port=%u errno=%d attempts=%u",
                         port, err, attempts);
            close(fd); sleep(buffered ? 3 : 2); continue;
        }
        attempts = 0;
        LOG_INFO(ALTSCREEN_SESSION_MODULE, "V8H_TRACER_CONNECTED port=%d", port);
        ll = 0;
        for (;;) {
            fd_set rd;
            struct timeval tv;
            int ready;
            FD_ZERO(&rd);
            FD_SET(fd, &rd);
            tv.tv_sec = 20;
            tv.tv_usec = 0;
            ready = select(fd + 1, &rd, NULL, NULL, &tv);
            if (ready == 0) {
                LOG_INFO(ALTSCREEN_SESSION_MODULE,
                         "DIAG_TRACER_HEARTBEAT port=%u connected=1 received_lines=%llu",
                         port, (unsigned long long)lines_seen);
                continue;
            }
            if (ready < 0) {
                if (errno == EINTR) continue;
                n = -1;
                break;
            }
            n = read(fd, buf, sizeof(buf));
            if (n <= 0) break;
            for (j = 0; j < n; ++j) {
                if (buf[j] == '\n' || buf[j] == '\r') {
                    if (ll != 0) {
                        char *q;
                        line[ll] = 0;
                        ++lines_seen;
                        /* Joey buffered record prefix: <seq>?1#... */
                        q = buffered ? strchr(line, '?') : NULL;
                        v8h_trace_line(q != NULL ? q + 1 : line, buffered);
                        ll = 0;
                    }
                } else if (ll < sizeof(line) - 1) line[ll++] = buf[j];
            }
        }
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "DIAG_TRACER_DISCONNECT port=%u result=%d errno=%d received_lines=%llu reconnecting=1",
                 port, (int)n, n < 0 ? errno : 0,
                 (unsigned long long)lines_seen);
        close(fd); sleep(buffered ? 3 : 2);
    }
    return NULL;
}

static void v8h_start_tracers(void)
{
    pthread_t worker;
    if (g_trace_threads_started) return;
    g_trace_threads_started = 1;
    if (pthread_create(&worker, NULL, v8h_trace_reader, NULL) == 0)
        pthread_detach(worker);
    if (pthread_create(&worker, NULL, v8h_trace_reader, (void *)1) == 0)
        pthread_detach(worker);
}

/* Commands advance ONLY after their actual AirPlay SendCommand completion.
 * rc==0 from SendCommand means queued, not acknowledged.  A late callback is
 * generation- and token-checked and cannot touch a reused receiver/state. */
static void v8s_control_completion(os_status status, cf_ref response, void *opaque)
{
    control_completion_context *ctx = (control_completion_context *)opaque;
    altscreen_state *state;
    int accepted = 0, next_stage = 0, last_slot = -1;
    (void)response;
    if (ctx == NULL) return;
    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(ctx->receiver);
    if (state != NULL && !state->stop_requested &&
        state->control_generation == ctx->generation &&
        state->control_awaiting_ack &&
        state->control_token == ctx->token &&
        state->control_stage == ctx->stage) {
        accepted = 1;
        state->control_awaiting_ack = 0;
        state->control_deadline_ms = 0;
        if (status == 0) {
            if (ctx->stage == ALTSCREEN_CONTROL_SHOW)
                state->last_ui_slot = ctx->view / 2;
            if (ctx->stage == ALTSCREEN_CONTROL_SHOW)
                state->control_stage = ALTSCREEN_CONTROL_VIEW;
            else if (ctx->stage == ALTSCREEN_CONTROL_VIEW)
                state->control_stage = ALTSCREEN_CONTROL_KEYFRAME;
            else {
                state->current_view = ctx->view;
                state->control_stage = ALTSCREEN_CONTROL_IDLE;
                state->control_startup = 0;
            }
        } else {
            /* Fail closed: no dependent ViewArea/IDR on a failed command.
             * The next genuine state change or session start may reattempt. */
            if (ctx->stage == ALTSCREEN_CONTROL_SHOW)
                state->last_ui_slot = -1;
            state->control_stage = ALTSCREEN_CONTROL_IDLE;
            state->control_startup = 0;
        }
        next_stage = state->control_stage;
        last_slot = state->last_ui_slot;
    }
    pthread_mutex_unlock(&g_lock);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "V8S_CONTROL_ACK receiver=%p token=%llu stage=%d view=%d "
             "status=%d valid=%d next=%d last_slot=%d",
             ctx->receiver, (unsigned long long)ctx->token, ctx->stage,
             ctx->view, status, accepted, next_stage, last_slot);
    free(ctx);
}

/* Used after synchronous transport refusal (no completion is expected). */
static void v8s_control_dispatch_failed(void *receiver, uint64_t generation,
                                        uint64_t token, int stage, int view, os_status rc)
{
    altscreen_state *state;
    int accepted = 0;
    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(receiver);
    if (state != NULL && state->control_generation == generation &&
        state->control_awaiting_ack && state->control_token == token &&
        state->control_stage == stage) {
        state->control_awaiting_ack = 0;
        state->control_deadline_ms = 0;
        state->control_stage = ALTSCREEN_CONTROL_IDLE;
        state->control_startup = 0;
        if (stage == ALTSCREEN_CONTROL_SHOW) state->last_ui_slot = -1;
        accepted = 1;
    }
    pthread_mutex_unlock(&g_lock);
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "V8S_CONTROL_DISPATCH_FAIL receiver=%p token=%llu stage=%d "
             "view=%d rc=%d valid=%d", receiver,
             (unsigned long long)token, stage, view, rc, accepted);
}

static void v8s_control_send(void *receiver, uint64_t generation, uint64_t token,
                             int stage, int view)
{
    control_completion_context *ctx;
    cf_ref request = NULL;
    os_status rc = -1;
    const char *url = NULL;
    if (stage == ALTSCREEN_CONTROL_SHOW) {
        url = altscreen_gauge_url_for_view(view);
        request = make_event_request("showUI", url);
    } else if (stage == ALTSCREEN_CONTROL_VIEW) {
        request = make_view_request(view);
    } else if (stage == ALTSCREEN_CONTROL_KEYFRAME) {
        request = make_event_request("forceKeyFrame", NULL);
    }
    ctx = (control_completion_context *)calloc(1, sizeof(*ctx));
    if (request != NULL && ctx != NULL && g_api.send_command != NULL) {
        ctx->receiver = receiver;
        ctx->generation = generation;
        ctx->token = token;
        ctx->stage = stage;
        ctx->view = view;
        rc = g_api.send_command(receiver, request, v8s_control_completion, ctx);
        /* On rc==0, the completion owns ctx, even when called synchronously. */
        if (rc == 0) ctx = NULL;
    }
    if (request != NULL) g_api.release(request);
    if (ctx != NULL) free(ctx);
    pthread_mutex_lock(&g_lock);
    {
        altscreen_state *state = find_state_receiver_locked(receiver);
        if (state != NULL && state->control_generation == generation &&
            state->control_inflight > 0) --state->control_inflight;
    }
    pthread_mutex_unlock(&g_lock);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "V8S_CONTROL_DISPATCH receiver=%p token=%llu stage=%d view=%d "
             "rc=%d url=%s ack=%s", receiver,
             (unsigned long long)token, stage, view, rc,
             url != NULL ? url : "-", rc == 0 ? "PENDING_OR_INLINE" : "NOT_QUEUED");
    if (rc != 0)
        v8s_control_dispatch_failed(receiver, generation, token, stage, view, rc);
}

static void v8s_control_tick(void)
{
        void *receiver = NULL;
        uint64_t generation = 0, token = 0;
        altscreen_state *picked = NULL;
        int view = -1, stage = ALTSCREEN_CONTROL_IDLE;
        int zoom = 0, i;
        uint64_t now = now_ms();
        int active = access(ALTSCREEN_ACTIVE_MARKER, F_OK) == 0;

        pthread_mutex_lock(&g_lock);
        for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
            altscreen_state *state = &g_states[i];
            if (!state->active || !state->screen_started || state->stop_requested ||
                state->receiver == NULL) continue;
            if (state->control_awaiting_ack && now >= state->control_deadline_ms) {
                LOG_WARN(ALTSCREEN_SESSION_MODULE,
                         "V8S_CONTROL_TIMEOUT receiver=%p token=%llu stage=%d "
                         "view=%d next_state_change_required=1",
                         state->receiver, (unsigned long long)state->control_token,
                         state->control_stage, state->control_target_view);
                state->control_awaiting_ack = 0;
                state->control_deadline_ms = 0;
                if (state->control_stage == ALTSCREEN_CONTROL_SHOW)
                    state->last_ui_slot = -1;
                state->control_stage = ALTSCREEN_CONTROL_IDLE;
                state->control_startup = 0;
            }
            if (state->control_awaiting_ack) continue;

            /* One latest-value slot per session: VIEW bursts replace stale
             * pending requests, including ones arriving during an async ack. */
            if (state->pending_view >= 0 && state->view_due_ms <= now) {
                int desired = state->pending_view;
                int old_target = state->control_target_view;
                state->control_target_view = desired;
                state->control_target_from_gauge = state->pending_view_from_gauge;
                state->pending_view = -1;
                state->pending_view_from_gauge = 0;
                state->view_due_ms = 0;
                state->control_stage = state->last_ui_slot != desired / 2
                    ? ALTSCREEN_CONTROL_SHOW : ALTSCREEN_CONTROL_VIEW;
                if (old_target != desired)
                    LOG_INFO(ALTSCREEN_SESSION_MODULE,
                             "V8S_CONTROL_LATEST receiver=%p old=%d latest=%d "
                             "origin=%s next=%d", state->receiver,
                             old_target, desired,
                             state->control_target_from_gauge ? "gauge" : "route",
                             state->control_stage);
            } else if (state->pending_view >= 0) {
                /* Don't dispatch obsolete steps while another VIEW settles. */
                continue;
            }
            if (state->control_stage == ALTSCREEN_CONTROL_IDLE ||
                (!active && !state->control_startup)) continue;

            receiver = state->receiver;
            generation = state->control_generation;
            view = state->control_target_view;
            stage = state->control_stage;
            token = ++g_control_request_serial;
            state->control_token = token;
            state->control_awaiting_ack = 1;
            state->control_deadline_ms = now + ALTSCREEN_CONTROL_ACK_TIMEOUT_MS;
            state->control_inflight++;
            break;
        }
        /* Existing wheel-zoom path, not rewritten. */
        if (g_pending_zoom != 0) {
            for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
                altscreen_state *state = &g_states[i];
                if (state->active && state->screen_started && !state->stop_requested &&
                    state->receiver != NULL) { picked = state; break; }
            }
        }
        if (picked != NULL && active && g_pending_zoom != 0) {
            zoom = g_pending_zoom > 0 ? 1 : -1;
            g_pending_zoom -= zoom;
            picked->control_inflight++;
        } else if (!active) g_pending_zoom = 0;
        pthread_mutex_unlock(&g_lock);

        if (zoom != 0 && picked != NULL) {
            (void)send_zoom_event(picked->receiver, zoom > 0 ? 0 : 1);
            pthread_mutex_lock(&g_lock);
            if (picked->control_inflight > 0) picked->control_inflight--;
            pthread_mutex_unlock(&g_lock);
        }
        if (receiver != NULL && stage != ALTSCREEN_CONTROL_IDLE)
            v8s_control_send(receiver, generation, token, stage, view);
}

static void *view_control_worker(void *arg)
{
    (void)arg;
    for (;;) {
        v8s_control_tick();
        usleep(50000);
    }
    return NULL;
}

static void ensure_view_worker_started(void)
{
    pthread_t worker;
    int do_start = 0;

    pthread_mutex_lock(&g_lock);
    if (g_view_worker_started == 0) {
        g_view_worker_started = -1;
        do_start = 1;
    }
    pthread_mutex_unlock(&g_lock);

    if (!do_start)
        return;

    if (pthread_create(&worker, NULL, view_control_worker, NULL) == 0) {
        pthread_detach(worker);
        v8h_start_tracers();
        pthread_mutex_lock(&g_lock);
        g_view_worker_started = 1;
        pthread_mutex_unlock(&g_lock);
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "VIEW_WORKER started reason=private111_prepared");
    } else {
        pthread_mutex_lock(&g_lock);
        g_view_worker_started = 0;
        pthread_mutex_unlock(&g_lock);
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "VIEW_WORKER start_failed failsoft=1");
    }
}

/*
 * Narrow native compass-suppression seam.  Exact K2161 SessionControl calls
 * AirPlayReceiverSessionMakeModeStateFromDictionary through the PLT.  By
 * interposing the parser rather than patching SessionControl text or the Java
 * boot classpath, we can preserve the phone's real state privately and change
 * only the value returned to stock libairplay.
 *
 * kAirPlayEntity_Controller == 1 (phone owns turn-by-turn)
 * kAirPlayEntity_NotApplicable == 0
 */
os_status AirPlayReceiverSessionMakeModeStateFromDictionary(void *receiver,
                                                              cf_ref params,
                                                              airplay_mode_state *out)
{
    mode_state_parse_fn real;
    os_status rc;
    int original_turns;
    int route;
    int changed = 0;

    resolve_api();
    real = g_api.mode_state_parse;
    if (real == NULL)
        return -1;

    rc = real(receiver, params, out);
    if (rc != 0 || out == NULL || !private111_active_for_receiver(receiver))
        return rc;

    original_turns = out->turns;
    route = (original_turns == 1);
    note_route_state(receiver, route, &changed);

    /* Observe phone route state for our ViewArea logic; leave stock ownership unchanged. */
    /* v8B intentionally does not modify out->turns. */

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "TURN_STATE original=%d stock=%d route=%d changed=%d policy=OBSERVE_ONLY",
             original_turns, out->turns, route, changed);

    /* Never send AirPlay commands recursively from the parser callback. */
    if (changed) {
        int view = -1;
        pthread_mutex_lock(&g_lock);
        {
            altscreen_state *state = find_state_receiver_locked(receiver);
            if (state != NULL) view = route_view_for_state(state);
        }
        pthread_mutex_unlock(&g_lock);
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "DIAG_RGI_DECISION_OBSERVE phone_route=%d private_stream111=1 "
                 "active_marker=%d requested_view=%d java_hud_untouched=1 "
                 "cluster_tile_actual=UNKNOWN",
                 route, access(ALTSCREEN_ACTIVE_MARKER, F_OK) == 0, view);
        queue_route_view_for_receiver(receiver,
                                      ALTSCREEN_VIEW_REACQUIRE_DELAY_MS);
    }
    return rc;
}

static void *altscreen_worker(void *arg)
{
    altscreen_state *state = (altscreen_state *)arg;
    struct sockaddr_storage peer;
    socklen_t peer_len = sizeof(peer);
    int fd;
    int stop_requested;
    void *net = NULL;
    void *delegate_context;
    os_status rc;

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "worker waiting receiver=%p port=%u",
             state->receiver, (unsigned int)state->data_port);

    do {
        fd = accept(state->listen_fd, (struct sockaddr *)&peer, &peer_len);
    } while (fd < 0 && errno == EINTR);

    pthread_mutex_lock(&g_lock);
    if (state->listen_fd >= 0) {
        close(state->listen_fd);
        state->listen_fd = -1;
    }
    state->accepted_fd = fd;
    stop_requested = state->stop_requested;
    pthread_mutex_unlock(&g_lock);

    if (fd < 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "accept failed receiver=%p errno=%d stop=%d",
                  state->receiver, errno, stop_requested);
        return NULL;
    }

    if (stop_requested) {
        close(fd);
        pthread_mutex_lock(&g_lock);
        state->accepted_fd = -1;
        pthread_mutex_unlock(&g_lock);
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "worker wake consumed receiver=%p teardown=1",
                 state->receiver);
        return NULL;
    }

    /*
     * K2161/iOS can SETUP and connect Stream 111 before the stock Stream 110
     * screen has installed its time synchronizer.  Do not fail the private
     * Screen B in that ordering.  The worker is independent from the stock
     * AirPlay setup thread, so it is safe to hold the accepted socket briefly
     * while waiting for the primary screen to publish its synchronizer.
     */
    if (!altscreen_time_sync_ready()) {
        unsigned int waited_ms = 0;
        int stop_wait = 0;

        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "primary sync wait receiver=%p screen=%p timeout_ms=%u",
                 state->receiver, state->screen,
                 (unsigned int)ALTSCREEN_PRIMARY_SYNC_WAIT_MS);

        while (!altscreen_time_sync_ready() &&
               waited_ms < ALTSCREEN_PRIMARY_SYNC_WAIT_MS) {
            pthread_mutex_lock(&g_lock);
            stop_wait = state->stop_requested;
            pthread_mutex_unlock(&g_lock);
            if (stop_wait)
                break;
            usleep(ALTSCREEN_PRIMARY_SYNC_POLL_US);
            waited_ms += ALTSCREEN_PRIMARY_SYNC_POLL_US / 1000U;
        }

        pthread_mutex_lock(&g_lock);
        stop_wait = state->stop_requested;
        pthread_mutex_unlock(&g_lock);

        if (stop_wait || !altscreen_time_sync_ready()) {
            LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                      "primary sync wait failed receiver=%p waited_ms=%u stop=%d",
                      state->receiver, waited_ms, stop_wait);
            close(fd);
            pthread_mutex_lock(&g_lock);
            state->accepted_fd = -1;
            pthread_mutex_unlock(&g_lock);
            return NULL;
        }

        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "primary sync ready receiver=%p waited_ms=%u",
                 state->receiver, waited_ms);
    }

    delegate_context = altscreen_main_delegate_context();
    rc = altscreen_start_session(state->screen, delegate_context);
    if (rc != 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "session start failed receiver=%p rc=%d",
                  state->receiver, rc);
        close(fd);
        pthread_mutex_lock(&g_lock);
        state->accepted_fd = -1;
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }

    pthread_mutex_lock(&g_lock);
    state->screen_started = 1;
    pthread_mutex_unlock(&g_lock);

    rc = g_api.netsock_create_native(&net, fd);
    if (rc != 0 || net == NULL) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "NetSocket create failed receiver=%p rc=%d",
                  state->receiver, rc);
        close(fd);
        pthread_mutex_lock(&g_lock);
        state->accepted_fd = -1;
        pthread_mutex_unlock(&g_lock);
        g_api.screen_stop(state->screen);
        pthread_mutex_lock(&g_lock);
        state->screen_started = 0;
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "processing begin receiver=%p screen=%p delegate=%p",
             state->receiver, state->screen, delegate_context);

    rc = g_api.screen_process(state->screen, net, ALTSCREEN_TIMEOUT_SECS);

    pthread_mutex_lock(&g_lock);
    state->accepted_fd = -1;
    pthread_mutex_unlock(&g_lock);

    (void)g_api.netsock_delete(net);
    g_api.screen_stop(state->screen);

    pthread_mutex_lock(&g_lock);
    state->screen_started = 0;
    pthread_mutex_unlock(&g_lock);

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "processing end receiver=%p rc=%d",
             state->receiver, rc);
    return NULL;
}

static int prepare_altscreen(void *receiver, cf_ref alt_desc, uint16_t *out_port)
{
    altscreen_state *state = NULL;
    void *screen = NULL;
    uint8_t key[16];
    uint8_t iv[16];
    uint64_t connection_id;
    int ok = 0;
    os_status rc;
    int listen_fd = -1;
    uint16_t port = 0;

    connection_id = (uint64_t)dict_get_i64(alt_desc, "streamConnectionID", &ok);
    if (!ok || connection_id == 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "prepare refused reason=missing_connection_id");
        return 0;
    }

    if (!altscreen_aes_ready()) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "prepare refused conn=%llu reason=stock_AES_not_ready primary_preserved=1",
                  (unsigned long long)connection_id);
        return 0;
    }

    pthread_mutex_lock(&g_lock);
    state = alloc_state_locked(receiver);
    pthread_mutex_unlock(&g_lock);
    if (state == NULL) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "prepare refused receiver=%p reason=state_collision_or_full",
                  receiver);
        return 0;
    }

    /* No worker/thread is added during normal boot.  Start only after a real
     * private Stream-111 SETUP allocated state for this receiver. */
    ensure_view_worker_started();

    rc = g_api.screen_create(&screen);
    if (rc != 0 || screen == NULL)
        goto fail;

    rc = g_api.screen_setup(screen, alt_desc, 0);
    if (rc != 0)
        goto fail;

    if (!altscreen_derive_aes(connection_id, key, iv))
        goto fail;
    rc = g_api.screen_security(screen, key, iv);
    memset(key, 0, sizeof(key));
    memset(iv, 0, sizeof(iv));
    if (rc != 0)
        goto fail;

    if (!altscreen_register_session(screen))
        goto fail;

    listen_fd = open_listener(&port);
    if (listen_fd < 0)
        goto fail_registered;

    pthread_mutex_lock(&g_lock);
    state->screen = screen;
    state->connection_id = connection_id;
    state->listen_fd = listen_fd;
    state->data_port = port;
    pthread_mutex_unlock(&g_lock);

    rc = pthread_create(&state->worker, NULL, altscreen_worker, state);
    if (rc != 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "worker create failed receiver=%p rc=%d",
                  receiver, rc);
        goto fail_listener;
    }

    pthread_mutex_lock(&g_lock);
    state->worker_started = 1;
    pthread_mutex_unlock(&g_lock);

    *out_port = port;
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "READY receiver=%p screen=%p conn=%llu dataPort=%u primary_untouched=1",
             receiver, screen, (unsigned long long)connection_id,
             (unsigned int)port);
    return 1;

fail_listener:
    close(listen_fd);
    pthread_mutex_lock(&g_lock);
    state->listen_fd = -1;
    pthread_mutex_unlock(&g_lock);
fail_registered:
    altscreen_unregister_session(screen);
fail:
    if (screen != NULL)
        g_api.screen_delete(screen);
    pthread_mutex_lock(&g_lock);
    memset(state, 0, sizeof(*state));
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static void cleanup_altscreen(void *receiver)
{
    altscreen_state *state;
    pthread_t worker;
    int worker_started;
    int listen_fd;
    int accepted_fd;
    uint16_t data_port;
    void *screen;
    int screen_started;
    int show_sent;
    int waiting_on_accept;
    int wake_ok = 1;

    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(receiver);
    if (state == NULL) {
        pthread_mutex_unlock(&g_lock);
        return;
    }
    worker = state->worker;
    worker_started = state->worker_started;
    listen_fd = state->listen_fd;
    accepted_fd = state->accepted_fd;
    data_port = state->data_port;
    screen = state->screen;
    screen_started = state->screen_started;
    show_sent = state->show_sent;
    state->stop_requested = 1;
    waiting_on_accept = worker_started && listen_fd >= 0 && accepted_fd < 0;
    if (accepted_fd >= 0)
        (void)shutdown(accepted_fd, SHUT_RDWR);
    pthread_mutex_unlock(&g_lock);

    if (show_sent)
        (void)send_event(receiver, "stopUI", NULL);
    if (screen_started && screen != NULL && g_api.screen_send_cmd != NULL)
        (void)g_api.screen_send_cmd(screen, 'q', NULL, 0);

    if (waiting_on_accept) {
        wake_ok = wake_listener(data_port);
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "listener wake receiver=%p port=%u ok=%d",
                 receiver, (unsigned int)data_port, wake_ok);
        if (!wake_ok) {
            (void)shutdown(listen_fd, SHUT_RDWR);
            (void)close(listen_fd);
            pthread_mutex_lock(&g_lock);
            if (state->listen_fd == listen_fd)
                state->listen_fd = -1;
            pthread_mutex_unlock(&g_lock);
            (void)pthread_cancel(worker);
        }
    }

    if (worker_started && !pthread_equal(worker, pthread_self()))
        (void)pthread_join(worker, NULL);

    /* A queued view update may still be finishing on the control worker.
     * Stop scheduling above, then give that bounded operation time to exit
     * before the receiver/session is handed back to stock teardown. */
    {
        int wait_i;
        for (wait_i = 0; wait_i < 50; ++wait_i) {
            int inflight;
            pthread_mutex_lock(&g_lock);
            inflight = state->control_inflight;
            pthread_mutex_unlock(&g_lock);
            if (inflight == 0)
                break;
            usleep(10000);
        }
    }

    pthread_mutex_lock(&g_lock);
    screen_started = state->screen_started;
    pthread_mutex_unlock(&g_lock);

    if (screen != NULL) {
        if (screen_started)
            g_api.screen_stop(screen);
        altscreen_unregister_session(screen);
        g_api.screen_delete(screen);
    }

    pthread_mutex_lock(&g_lock);
    memset(state, 0, sizeof(*state));
    pthread_mutex_unlock(&g_lock);

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "cleanup receiver=%p route_stop=%d wake_ok=%d",
             receiver, show_sent, wake_ok);
}

os_status AirPlayReceiverSessionSetup(void *receiver,
                                      cf_ref request,
                                      cf_ref *out_response)
{
    cf_ref alt_desc = NULL;
    cf_ref stock_request = NULL;
    cf_ref stock_response = NULL;
    cf_ref final_response = NULL;
    uint16_t alt_port = 0;
    os_status rc;

    resolve_api();
    if (g_api.session_setup == NULL)
        return -1;

    if (!altscreen_enabled() || !g_api.ready || out_response == NULL)
        return g_api.session_setup(receiver, request, out_response);

    log_setup_request(receiver, request);

    stock_request = build_stock_only_request(request, &alt_desc);
    if (stock_request == NULL || alt_desc == NULL) {
        rc = g_api.session_setup(receiver, request, &stock_response);
        if (rc == 0 && stock_response != NULL) {
            final_response = clone_response_with_reference_features(stock_response);
            if (final_response != NULL) {
                g_api.release(stock_response);
                stock_response = final_response;
                final_response = NULL;
            } else {
                LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                          "SETUP features patch failed receiver=%p stock_response_preserved=1",
                          receiver);
            }
        }
        *out_response = stock_response;
        log_setup_response_features(receiver, rc, stock_response);
        return rc;
    }

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "requested receiver=%p stock_only_request=%p descriptor=%p primary_protected=1",
             receiver, stock_request, alt_desc);

    rc = g_api.session_setup(receiver, stock_request, &stock_response);
    log_setup_response_features(receiver, rc, stock_response);
    g_api.release(stock_request);
    stock_request = NULL;
    if (rc != 0) {
        if (stock_response != NULL) g_api.release(stock_response);
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "stock setup failed receiver=%p rc=%d",
                  receiver, rc);
        return rc;
    }

    if (!prepare_altscreen(receiver, alt_desc, &alt_port)) {
        *out_response = stock_response;
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "prepare failed receiver=%p stock_response_preserved=1",
                  receiver);
        return 0;
    }

    final_response = merge_altscreen_response(stock_response, alt_desc, alt_port);
    if (final_response == NULL) {
        cleanup_altscreen(receiver);
        *out_response = stock_response;
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "response merge failed receiver=%p stock_response_preserved=1",
                  receiver);
        return 0;
    }

    if (stock_response != NULL)
        g_api.release(stock_response);
    *out_response = final_response;

    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "SETUP_READY receiver=%p dataPort=%u enabledFeatures=altScreen|viewAreas primary_untouched=1",
             receiver, (unsigned int)alt_port);
    return 0;
}

void AirPlayReceiverSessionTearDown(void *receiver,
                                    cf_ref request,
                                    os_status reason,
                                    unsigned char *out_done)
{
    int has_alt = 0;
    int has_main = 0;
    int other_streams = 0;
    int full_session;
    cf_ref stock_request = NULL;
    cf_ref ignored_alt = NULL;

    resolve_api();
    if (g_api.session_teardown == NULL)
        return;

    if (!altscreen_enabled() || !g_api.ready) {
        g_api.session_teardown(receiver, request, reason, out_done);
        return;
    }

    full_session = request == NULL;
    classify_teardown_request(request, &has_alt, &has_main, &other_streams);

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "TEARDOWN receiver=%p request=%s reason=%d alt111=%d main110=%d other=%d private111_decoupled=1",
             receiver, full_session ? "NULL" : "dict", reason,
             has_alt, has_main, other_streams);

    /*
     * Stream 111 is private to this hook.  A teardown for stock 110, alt audio
     * (101), or any other stock stream must never destroy the private screen.
     * The previous build cleaned up 111 for every teardown; the car trace shows
     * exactly such a stock teardown immediately before dio_manager faulted.
     */
    if (!full_session && !has_alt) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "TEARDOWN preserving private111 reason=stock_only_or_other_stream");
        g_api.session_teardown(receiver, request, reason, out_done);
        return;
    }

    if (has_alt) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "TEARDOWN explicit111 cleanup other_streams=%d", other_streams);
        cleanup_altscreen(receiver);

        /* Stock never saw stream 111 during SETUP, so never pass 111 back to
         * stock teardown.  If 111 was the only requested stream, we are done. */
        if (other_streams == 0) {
            if (out_done != NULL)
                *out_done = 0;
            return;
        }

        stock_request = build_stock_only_request(request, &ignored_alt);
        if (stock_request != NULL) {
            g_api.session_teardown(receiver, stock_request, reason, out_done);
            g_api.release(stock_request);
        } else {
            LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                      "TEARDOWN explicit111 split failed; preserving stock request fail-safe");
            g_api.session_teardown(receiver, request, reason, out_done);
        }
        return;
    }

    /* A true whole-session teardown owns private111 cleanup. */
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "TEARDOWN whole-session private111 cleanup");
    cleanup_altscreen(receiver);
    g_api.session_teardown(receiver, request, reason, out_done);
}

os_status ScreenStreamProcessData(void *stream,
                                  const uint8_t *data,
                                  size_t len,
                                  uint64_t display_ticks,
                                  cf_ref options,
                                  void (*completion)(void *),
                                  void *completion_context)
{
    os_status rc;
    altscreen_state *state = NULL;
    pthread_t self;
    int should_show = 0;

    resolve_api();
    if (g_api.screen_process_data == NULL)
        return -1;

    rc = g_api.screen_process_data(stream, data, len, display_ticks,
                                   options, completion, completion_context);
    if (rc != 0 || !altscreen_enabled())
        return rc;

    self = pthread_self();
    pthread_mutex_lock(&g_lock);
    state = find_state_worker_locked(self);
    if (state != NULL && state->screen_started && !state->show_sent &&
        !state->stop_requested) {
        state->show_sent = 1;
        should_show = 1;
    }
    pthread_mutex_unlock(&g_lock);

    if (should_show) {
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "FIRST_REAL_FRAME receiver=%p stream=%p len=%u route=cluster",
                 state->receiver, stream, (unsigned int)len);
        request_cluster(state);
    }
    return rc;
}
