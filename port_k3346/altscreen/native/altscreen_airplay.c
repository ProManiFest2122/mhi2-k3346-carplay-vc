#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "altscreen.h"
#include "gauge_layout_urls.h"
#include "framework/logging.h"

#define ALTINFO_MODULE "AltScreenInfo"
#define ALTSCREEN_ENABLE_MARKER "/mnt/app/carplay_altscreen"
#define ALTSCREEN_URL_CARD "maps:/car/instrumentcluster/instructioncard"
#define ALTSCREEN_URL_BASE "maps:/car/instrumentcluster"
#define ALTSCREEN_PROFILE_PATH "/mnt/app/root/mib2high-lvds-profile.conf"
#define ALTSCREEN_VIEWS_PATH "/mnt/app/root/mib2high-lvds-viewareas.conf"
#define ALTSCREEN_DEFAULT_W 800
#define ALTSCREEN_DEFAULT_H 480
#define ALTSCREEN_DEFAULT_FPS 30
#define ALTSCREEN_VIEW_COUNT_DEFAULT 4
#define ALTSCREEN_VIEW_COUNT_MAX 4
/* Advertise the same pixel canvas as the renderer profile.
 * v8j profile: 1440x542 for both advertised and rendered output.
 * p->w/p->h are loaded from the installed profile. */
#define K_CFSTRING_UTF8 0x08000100U

typedef void *cf_ref;
typedef unsigned long cf_type_id;
typedef long cf_index;
typedef int os_status;

typedef cf_ref (*platform_copy_fn)(cf_ref, uint32_t, cf_ref, cf_ref, os_status *);
typedef cf_ref (*server_info_fn)(cf_ref, cf_ref, uint8_t *, os_status *);
typedef cf_ref (*cf_string_create_fn)(cf_ref, const char *, uint32_t);
typedef cf_type_id (*cf_get_type_fn)(cf_ref);
typedef cf_type_id (*cf_type_getter_fn)(void);
typedef cf_ref (*cf_array_mutable_copy_fn)(cf_ref, cf_index, cf_ref);
typedef cf_ref (*cf_array_create_mutable_fn)(cf_ref, cf_index, const void *);
typedef void (*cf_array_append_fn)(cf_ref, const void *);
typedef cf_index (*cf_array_count_fn)(cf_ref);
typedef const void *(*cf_array_value_fn)(cf_ref, cf_index);
typedef cf_ref (*cf_dict_create_mutable_fn)(cf_ref, cf_index, const void *, const void *);
typedef const void *(*cf_dict_get_value_fn)(cf_ref, const void *);
typedef void (*cf_dict_set_value_fn)(cf_ref, const void *, const void *);
typedef cf_index (*cf_dict_count_fn)(cf_ref);
typedef void (*cf_dict_get_keys_values_fn)(cf_ref, const void **, const void **);
typedef os_status (*cf_dict_set_i64_fn)(cf_ref, const void *, int64_t);
typedef cf_ref (*cf_retain_fn)(cf_ref);
typedef void (*cf_release_fn)(cf_ref);
typedef unsigned char (*cf_equal_fn)(cf_ref, cf_ref);

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
    platform_copy_fn server_copy;
    platform_copy_fn session_copy;
    server_info_fn info_copy;
    cf_string_create_fn string_create;
    cf_get_type_fn get_type;
    cf_type_getter_fn array_type;
    cf_type_getter_fn dict_type;
    cf_array_mutable_copy_fn array_mutable_copy;
    cf_array_create_mutable_fn array_create_mutable;
    cf_array_append_fn array_append;
    cf_array_count_fn array_count;
    cf_array_value_fn array_value;
    cf_dict_create_mutable_fn dict_create_mutable;
    cf_dict_get_value_fn dict_get_value;
    cf_dict_set_value_fn dict_set_value;
    cf_dict_count_fn dict_count;
    cf_dict_get_keys_values_fn dict_get_keys_values;
    cf_dict_set_i64_fn dict_set_i64;
    cf_retain_fn retain;
    cf_release_fn release;
    cf_equal_fn equal;
    const void *dict_key_callbacks;
    const void *dict_value_callbacks;
    const void *array_callbacks;
    const void *boolean_true;
    const void *boolean_false;
    int resolved;
    int ready;
} alt_cf_api;

static alt_cf_api g_cf;
static int g_info_logged;

static void *sym(const char *name)
{
    return dlsym(RTLD_NEXT, name);
}

static const void *local_value_retain(void *allocator, const void *object)
{
    (void)allocator;
    return g_cf.retain != NULL ? g_cf.retain((cf_ref)object) : object;
}

static void local_value_release(void *allocator, const void *object)
{
    (void)allocator;
    if (g_cf.release != NULL && object != NULL)
        g_cf.release((cf_ref)object);
}

static unsigned char local_value_equal(const void *left, const void *right)
{
    if (g_cf.equal == NULL)
        return left == right;
    return g_cf.equal((cf_ref)left, (cf_ref)right);
}

static const cf_dictionary_value_callbacks g_local_value_callbacks = {
    0,
    local_value_retain,
    local_value_release,
    NULL,
    local_value_equal
};

static void resolve_cf(void)
{
    if (g_cf.resolved)
        return;

    g_cf.resolved = 1;
    g_cf.server_copy = (platform_copy_fn)sym("AirPlayReceiverServerPlatformCopyProperty");
    g_cf.session_copy = (platform_copy_fn)sym("AirPlayReceiverSessionPlatformCopyProperty");
    g_cf.info_copy = (server_info_fn)sym("AirPlayCopyServerInfo");
    g_cf.string_create = (cf_string_create_fn)sym("CFStringCreateWithCString");
    g_cf.get_type = (cf_get_type_fn)sym("CFGetTypeID");
    g_cf.array_type = (cf_type_getter_fn)sym("CFArrayGetTypeID");
    g_cf.dict_type = (cf_type_getter_fn)sym("CFDictionaryGetTypeID");
    g_cf.array_mutable_copy = (cf_array_mutable_copy_fn)sym("CFArrayCreateMutableCopy");
    g_cf.array_create_mutable = (cf_array_create_mutable_fn)sym("CFArrayCreateMutable");
    g_cf.array_append = (cf_array_append_fn)sym("CFArrayAppendValue");
    g_cf.array_count = (cf_array_count_fn)sym("CFArrayGetCount");
    g_cf.array_value = (cf_array_value_fn)sym("CFArrayGetValueAtIndex");
    g_cf.dict_create_mutable = (cf_dict_create_mutable_fn)sym("CFDictionaryCreateMutable");
    g_cf.dict_get_value = (cf_dict_get_value_fn)sym("CFDictionaryGetValue");
    g_cf.dict_set_value = (cf_dict_set_value_fn)sym("CFDictionarySetValue");
    g_cf.dict_count = (cf_dict_count_fn)sym("CFDictionaryGetCount");
    g_cf.dict_get_keys_values = (cf_dict_get_keys_values_fn)sym("CFDictionaryGetKeysAndValues");
    g_cf.dict_set_i64 = (cf_dict_set_i64_fn)sym("CFDictionarySetInt64");
    g_cf.retain = (cf_retain_fn)sym("CFRetain");
    g_cf.release = (cf_release_fn)sym("CFRelease");
    g_cf.equal = (cf_equal_fn)sym("CFEqual");

    g_cf.dict_key_callbacks = sym("kCFTypeDictionaryKeyCallBacks");
    if (g_cf.dict_key_callbacks == NULL)
        g_cf.dict_key_callbacks = sym("kCFLDictionaryKeyCallBacksCFLTypes");

    g_cf.dict_value_callbacks = sym("kCFTypeDictionaryValueCallBacks");
    if (g_cf.dict_value_callbacks == NULL)
        g_cf.dict_value_callbacks = sym("kCFLDictionaryValueCallBacksCFLTypes");
    if (g_cf.dict_value_callbacks == NULL)
        g_cf.dict_value_callbacks = &g_local_value_callbacks;

    g_cf.array_callbacks = sym("kCFTypeArrayCallBacks");
    if (g_cf.array_callbacks == NULL)
        g_cf.array_callbacks = sym("kCFLArrayCallBacksCFLTypes");

    {
        const void **p = (const void **)sym("kCFBooleanTrue");
        const void **q = (const void **)sym("kCFBooleanFalse");
        if (p == NULL)
            p = (const void **)sym("kCFLBooleanTrue");
        if (q == NULL)
            q = (const void **)sym("kCFLBooleanFalse");
        g_cf.boolean_true = p != NULL ? *p : NULL;
        g_cf.boolean_false = q != NULL ? *q : NULL;
    }

    g_cf.ready =
        g_cf.string_create != NULL &&
        g_cf.get_type != NULL &&
        g_cf.array_type != NULL &&
        g_cf.dict_type != NULL &&
        g_cf.array_mutable_copy != NULL &&
        g_cf.array_create_mutable != NULL &&
        g_cf.array_append != NULL &&
        g_cf.array_count != NULL &&
        g_cf.array_value != NULL &&
        g_cf.dict_create_mutable != NULL &&
        g_cf.dict_get_value != NULL &&
        g_cf.dict_set_value != NULL &&
        g_cf.dict_count != NULL &&
        g_cf.dict_get_keys_values != NULL &&
        g_cf.dict_set_i64 != NULL &&
        g_cf.retain != NULL &&
        g_cf.release != NULL &&
        g_cf.equal != NULL &&
        g_cf.dict_key_callbacks != NULL &&
        g_cf.dict_value_callbacks != NULL &&
        g_cf.array_callbacks != NULL &&
        g_cf.boolean_true != NULL &&
        g_cf.boolean_false != NULL;

    LOG_INFO(ALTINFO_MODULE,
             "runtime CF ready=%d key_callbacks=%d value_callbacks=%d array_callbacks=%d booleans=%d",
             g_cf.ready,
             g_cf.dict_key_callbacks != NULL,
             g_cf.dict_value_callbacks != NULL,
             g_cf.array_callbacks != NULL,
             g_cf.boolean_true != NULL && g_cf.boolean_false != NULL);
}

int altscreen_enabled(void)
{
    const char *env = getenv("CARPLAY_ALTSCREEN");

    if (env != NULL && env[0] == '1' && env[1] == '\0')
        return 1;
    return access(ALTSCREEN_ENABLE_MARKER, F_OK) == 0;
}

int altscreen_info_ready(void)
{
    resolve_cf();
    return g_cf.ready;
}

static cf_ref cfs(const char *s)
{
    if (s == NULL || g_cf.string_create == NULL)
        return NULL;
    return g_cf.string_create(NULL, s, K_CFSTRING_UTF8);
}

static int property_is(cf_ref property, const char *name)
{
    cf_ref key;
    int equal = 0;

    if (property == NULL || g_cf.equal == NULL)
        return 0;

    key = cfs(name);
    if (key != NULL) {
        equal = g_cf.equal(property, key) != 0;
        g_cf.release(key);
    }
    return equal;
}

static const char *tracked_property_name(cf_ref property)
{
    if (!g_cf.ready)
        return NULL;
    if (property_is(property, "displays"))
        return "displays";
    if (property_is(property, "altScreenURLs"))
        return "altScreenURLs";
    if (property_is(property, "altScreenSuggestUIURLs"))
        return "altScreenSuggestUIURLs";
    return NULL;
}

static int is_array(cf_ref obj)
{
    return obj != NULL && g_cf.get_type(obj) == g_cf.array_type();
}

static int is_dict(cf_ref obj)
{
    return obj != NULL && g_cf.get_type(obj) == g_cf.dict_type();
}

static cf_ref new_dict(void)
{
    return g_cf.dict_create_mutable(NULL, 0,
                                    g_cf.dict_key_callbacks,
                                    g_cf.dict_value_callbacks);
}

static cf_ref new_array(void)
{
    return g_cf.array_create_mutable(NULL, 0, g_cf.array_callbacks);
}

static cf_ref dict_get_obj(cf_ref dict, const char *name)
{
    cf_ref key;
    cf_ref value = NULL;

    if (!is_dict(dict) || name == NULL || g_cf.dict_get_value == NULL)
        return NULL;

    key = cfs(name);
    if (key == NULL)
        return NULL;

    value = (cf_ref)g_cf.dict_get_value(dict, key);
    g_cf.release(key);
    return value;
}

static cf_ref clone_dict_manual(cf_ref source)
{
    cf_ref out = NULL;
    const void **keys = NULL;
    const void **values = NULL;
    cf_index count;
    cf_index i;

    if (!is_dict(source) || g_cf.dict_count == NULL ||
        g_cf.dict_get_keys_values == NULL)
        return NULL;

    out = new_dict();
    if (out == NULL)
        return NULL;

    count = g_cf.dict_count(source);
    if (count <= 0)
        return out;

    keys = (const void **)malloc((size_t)count * sizeof(*keys));
    values = (const void **)malloc((size_t)count * sizeof(*values));
    if (keys == NULL || values == NULL)
        goto fail;

    g_cf.dict_get_keys_values(source, keys, values);
    for (i = 0; i < count; ++i)
        g_cf.dict_set_value(out, keys[i], values[i]);

    free(keys);
    free(values);
    return out;

fail:
    free(keys);
    free(values);
    g_cf.release(out);
    return NULL;
}

static int key_is_name(cf_ref key, const char *name)
{
    cf_ref wanted;
    int same = 0;

    if (key == NULL || name == NULL || g_cf.equal == NULL)
        return 0;
    wanted = cfs(name);
    if (wanted != NULL) {
        same = g_cf.equal(key, wanted) != 0;
        g_cf.release(wanted);
    }
    return same;
}

static int alt_clone_skip_key(cf_ref key)
{
    static const char *const names[] = {
        "primaryInputDevice",
        "edid",
        "platformLayer",
        "avcc",
        "xOffset",
        "yOffset",
        "windowWidth",
        "windowHeight"
    };
    unsigned int i;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (key_is_name(key, names[i]))
            return 1;
    }
    return 0;
}

static cf_ref clone_alt_display_base(cf_ref source)
{
    cf_ref out = NULL;
    const void **keys = NULL;
    const void **values = NULL;
    cf_index count;
    cf_index i;

    if (!is_dict(source) || g_cf.dict_count == NULL ||
        g_cf.dict_get_keys_values == NULL)
        return NULL;

    out = new_dict();
    if (out == NULL)
        return NULL;

    count = g_cf.dict_count(source);
    if (count <= 0)
        return out;

    keys = (const void **)malloc((size_t)count * sizeof(*keys));
    values = (const void **)malloc((size_t)count * sizeof(*values));
    if (keys == NULL || values == NULL)
        goto fail;

    g_cf.dict_get_keys_values(source, keys, values);
    for (i = 0; i < count; ++i) {
        if (!alt_clone_skip_key((cf_ref)keys[i]))
            g_cf.dict_set_value(out, keys[i], values[i]);
    }

    free(keys);
    free(values);
    return out;

fail:
    free(keys);
    free(values);
    g_cf.release(out);
    return NULL;
}

static cf_ref make_enabled_features(void)
{
    cf_ref out = NULL;
    cf_ref item = NULL;

    out = new_array();
    if (out == NULL)
        return NULL;

    item = cfs("altScreen");
    if (item == NULL)
        goto fail;
    g_cf.array_append(out, item);
    g_cf.release(item);
    item = NULL;

    item = cfs("viewAreas");
    if (item == NULL)
        goto fail;
    g_cf.array_append(out, item);
    g_cf.release(item);

    return out;

fail:
    if (item != NULL)
        g_cf.release(item);
    g_cf.release(out);
    return NULL;
}

static int set_i64(cf_ref dict, const char *name, int64_t value)
{
    cf_ref key = cfs(name);
    os_status rc;

    if (key == NULL)
        return 0;
    rc = g_cf.dict_set_i64(dict, key, value);
    g_cf.release(key);
    return rc == 0;
}

static int set_obj(cf_ref dict, const char *name, cf_ref value)
{
    cf_ref key = cfs(name);

    if (key == NULL || value == NULL) {
        if (key != NULL)
            g_cf.release(key);
        return 0;
    }
    g_cf.dict_set_value(dict, key, value);
    g_cf.release(key);
    return 1;
}

static int set_string(cf_ref dict, const char *name, const char *value)
{
    cf_ref str = cfs(value);
    int ok;

    if (str == NULL)
        return 0;
    ok = set_obj(dict, name, str);
    g_cf.release(str);
    return ok;
}

static int set_bool(cf_ref dict, const char *name, int value)
{
    return set_obj(dict, name,
                   (cf_ref)(value ? g_cf.boolean_true : g_cf.boolean_false));
}

typedef struct {
    int x;
    int y;
    int w;
    int h;
} altscreen_rect;

typedef struct {
    int w;
    int h;
    int fps;
    int physical_w;
    int physical_h;
    unsigned int view_count;
    altscreen_rect safe[ALTSCREEN_VIEW_COUNT_MAX];
} altscreen_profile;

static int rect_valid(const altscreen_rect *r, int canvas_w, int canvas_h)
{
    if (r == NULL || r->x < 0 || r->y < 0 || r->w <= 0 || r->h <= 0)
        return 0;
    if (r->x + r->w > canvas_w || r->y + r->h > canvas_h)
        return 0;
    return 1;
}

static void profile_defaults(altscreen_profile *p)
{
    unsigned int i;

    memset(p, 0, sizeof(*p));
    p->w = ALTSCREEN_DEFAULT_W;
    p->h = ALTSCREEN_DEFAULT_H;
    p->fps = ALTSCREEN_DEFAULT_FPS;
    p->physical_w = 200;
    p->physical_h = 75; /* Q7 1440x542 advertised canvas, ~200:75 aspect */
    p->view_count = ALTSCREEN_VIEW_COUNT_DEFAULT;
    for (i = 0; i < ALTSCREEN_VIEW_COUNT_MAX; ++i) {
        p->safe[i].x = 0;
        p->safe[i].y = 0;
        p->safe[i].w = p->w;
        p->safe[i].h = p->h;
    }
}

static void load_profile(altscreen_profile *p)
{
    FILE *f;
    int w = 0, h = 0, fps = 0, ignored_features = 0;
    int sx = 0, sy = 0, sw = 0, sh = 0;
    int n;
    char line[192];
    unsigned int max_view = 0;

    profile_defaults(p);

    /*
     * Existing v5 profile format:
     *   width height fps features safeX safeY safeW safeH
     *
     * The renderer controller already consumes width/height/fps from this file.
     * Reusing it here prevents phone-facing geometry from drifting away from
     * the actual vc_render geometry.  Feature/HID policy deliberately remains
     * v5-safe (features=0, no primaryInputDevice) in this merge.
     */
    f = fopen(ALTSCREEN_PROFILE_PATH, "r");
    if (f != NULL) {
        n = fscanf(f, "%d %d %d %d %d %d %d %d",
                   &w, &h, &fps, &ignored_features,
                   &sx, &sy, &sw, &sh);
        fclose(f);

        if (n >= 3 && w >= 320 && w <= 4096 &&
            h >= 200 && h <= 2160 && fps >= 1 && fps <= 60) {
            p->w = w;
            p->h = h;
            p->fps = fps;
            p->safe[0].x = 0;
            p->safe[0].y = 0;
            p->safe[0].w = p->w;
            p->safe[0].h = p->h;
            p->safe[1] = p->safe[0];

            if (n >= 8) {
                altscreen_rect r;
                r.x = sx; r.y = sy; r.w = sw; r.h = sh;
                if (rect_valid(&r, p->w, p->h)) {
                    p->safe[0] = r;
                    p->safe[1] = r;
                }
            }
        }
    }

    /*
     * Optional measured-Q7 view file.  Nothing here contains guessed A4/Q7
     * coordinates.  Until the car supplies measured rectangles, both views
     * intentionally use the profile's safe rectangle.
     *
     * Supported lines:
     *   physical <width_mm> <height_mm>
     *   view <index> <x> <y> <w> <h>
     */
    f = fopen(ALTSCREEN_VIEWS_PATH, "r");
    if (f != NULL) {
        while (fgets(line, sizeof(line), f) != NULL) {
            int a, b, c, d, e;
            if (sscanf(line, "physical %d %d", &a, &b) == 2) {
                if (a >= 0 && a <= 1000 && b >= 0 && b <= 1000) {
                    p->physical_w = a;
                    p->physical_h = b;
                }
                continue;
            }
            if (sscanf(line, "view %d %d %d %d %d", &a, &b, &c, &d, &e) == 5) {
                altscreen_rect r;
                if (a < 0 || a >= ALTSCREEN_VIEW_COUNT_MAX)
                    continue;
                r.x = b; r.y = c; r.w = d; r.h = e;
                if (!rect_valid(&r, p->w, p->h))
                    continue;
                p->safe[a] = r;
                if ((unsigned int)(a + 1) > max_view)
                    max_view = (unsigned int)(a + 1);
            }
        }
        fclose(f);
    }

    /* Keep 4 advertised slots, even if individual overrides are absent.
     * Unspecified slots inherit the safe default rather than generating invalid
     * updateViewArea indexes when the VC dial layout changes. */
    p->view_count = ALTSCREEN_VIEW_COUNT_MAX;

    LOG_INFO(ALTINFO_MODULE,
             "profile geometry=%dx%d@%d physical=%dx%d views=%u "
             "view0=%d,%d,%d,%d view1=%d,%d,%d,%d source=%s",
             p->w, p->h, p->fps, p->physical_w, p->physical_h,
             p->view_count,
             p->safe[0].x, p->safe[0].y, p->safe[0].w, p->safe[0].h,
             p->safe[1].x, p->safe[1].y, p->safe[1].w, p->safe[1].h,
             ALTSCREEN_PROFILE_PATH);
}

static cf_ref make_safe_area(const altscreen_rect *r)
{
    cf_ref safe = new_dict();

    if (safe == NULL)
        return NULL;
    if (!set_i64(safe, "originXPixels", r->x) ||
        !set_i64(safe, "originYPixels", r->y) ||
        !set_i64(safe, "widthPixels", r->w) ||
        !set_i64(safe, "heightPixels", r->h)) {
        g_cf.release(safe);
        return NULL;
    }
    return safe;
}

static cf_ref make_view_areas(const altscreen_profile *p)
{
    cf_ref areas = NULL;
    unsigned int i;

    areas = new_array();
    if (areas == NULL)
        return NULL;

    for (i = 0; i < p->view_count; ++i) {
        cf_ref area = new_dict();
        cf_ref safe = make_safe_area(&p->safe[i]);

        if (area == NULL || safe == NULL) {
            if (safe != NULL) g_cf.release(safe);
            if (area != NULL) g_cf.release(area);
            g_cf.release(areas);
            return NULL;
        }

        if (!set_i64(area, "originXPixels", 0) ||
            !set_i64(area, "originYPixels", 0) ||
            !set_i64(area, "widthPixels", p->w) ||
            !set_i64(area, "heightPixels", p->h) ||
            !set_bool(area, "drawUIOutsideSafeArea", 0) ||
            !set_bool(area, "viewAreaTransitionControl", 0) ||
            !set_obj(area, "safeArea", safe)) {
            g_cf.release(safe);
            g_cf.release(area);
            g_cf.release(areas);
            return NULL;
        }

        g_cf.array_append(areas, area);
        g_cf.release(safe);
        g_cf.release(area);
    }

    return areas;
}

static cf_ref make_cluster_urls(void);

static cf_ref make_alt_display(cf_ref stock_main)
{
    cf_ref alt = NULL;
    cf_ref view_areas = NULL;
    cf_ref suggest_urls = NULL;
    altscreen_profile profile;

    load_profile(&profile);

    alt = clone_alt_display_base(stock_main);
    view_areas = make_view_areas(&profile);
    suggest_urls = make_cluster_urls();
    if (alt == NULL || view_areas == NULL || suggest_urls == NULL)
        goto fail;

    /*
     * Keep v5's conservative feature/HID policy.  The only profile changes in
     * this merged study are geometry + ViewAreas; feature-2/virtual-HID work is
     * intentionally excluded from this recovery-derived baseline.
     *
     * Joey's current GEN2 path puts adjacentViewAreas on updateViewArea
     * commands, not as an empty top-level display field, so the old empty
     * descriptor-level adjacentViewAreas entry is intentionally removed.
     */
    if (!set_string(alt, "uuid", ALTSCREEN_UUID) ||
        !set_i64(alt, "features", 0) ||
        !set_i64(alt, "maxFPS", profile.fps) ||
        !set_i64(alt, "widthPixels", profile.w) ||
        !set_i64(alt, "heightPixels", profile.h) ||
        !set_i64(alt, "widthPhysical", profile.physical_w) ||
        !set_i64(alt, "heightPhysical", profile.physical_h) ||
        !set_i64(alt, "initialViewArea", 0) ||
        !set_obj(alt, "viewAreas", view_areas) ||
        !set_obj(alt, "altScreenSuggestUIURLs", suggest_urls) ||
        !set_i64(alt, "type", AIRPLAY_STREAM_TYPE_ALT_SCREEN) ||
        !set_bool(alt, "showsInstruments", 1) ||
        !set_string(alt, "initialURL", ALTSCREEN_INITIAL_URL))
        goto fail;

    LOG_INFO(ALTINFO_MODULE,
             "display profile uuid=%s rendered=%dx%d@%d advertised=%dx%d physical=%dx%d "
             "views=%u features=0 primaryInputDevice=absent",
             ALTSCREEN_UUID, profile.w, profile.h, profile.fps,
             profile.w, profile.h, profile.physical_w, profile.physical_h, profile.view_count);

    g_cf.release(suggest_urls);
    g_cf.release(view_areas);
    return alt;

fail:
    if (suggest_urls != NULL) g_cf.release(suggest_urls);
    if (view_areas != NULL) g_cf.release(view_areas);
    if (alt != NULL) g_cf.release(alt);
    return NULL;
}

static cf_ref add_alt_display(cf_ref stock)
{
    cf_ref out = NULL;
    cf_ref alt = NULL;
    cf_index count;
    altscreen_profile profile;

    load_profile(&profile);

    if (!is_array(stock)) {
        LOG_ERROR(ALTINFO_MODULE,
                  "refused property=displays reason=stock_not_array primary_untouched=1");
        return NULL;
    }

    count = g_cf.array_count(stock);
    if (count < 1 || !is_dict((cf_ref)g_cf.array_value(stock, 0))) {
        LOG_ERROR(ALTINFO_MODULE,
                  "refused property=displays reason=no_main_display primary_untouched=1");
        return NULL;
    }

    out = g_cf.array_mutable_copy(NULL, count + 1, stock);
    alt = make_alt_display((cf_ref)g_cf.array_value(stock, 0));
    if (out == NULL || alt == NULL) {
        if (out != NULL) g_cf.release(out);
        if (alt != NULL) g_cf.release(alt);
        return NULL;
    }

    g_cf.array_append(out, alt);
    g_cf.release(alt);

    LOG_INFO(ALTINFO_MODULE,
             "PROPERTY_RETURN property=displays stock_count=%ld returned_count=%ld "
             "alt_uuid=%s type=%d geometry=%dx%d physical=0x0 features=0 "
             "primaryInputDevice=absent cloned_stock=1 suggest_urls=3 "
             "showsInstruments=1 initialURL=%s",
             (long)count, (long)(count + 1), ALTSCREEN_UUID,
             AIRPLAY_STREAM_TYPE_ALT_SCREEN,
             profile.w, profile.h,
             ALTSCREEN_INITIAL_URL);

    if (!g_info_logged) {
        g_info_logged = 1;
        LOG_WARN(ALTINFO_MODULE,
                 "display advertised uuid=%s geometry=profile-driven primary_displays=%ld",
                 ALTSCREEN_UUID, (long)count);
    }
    return out;
}

static cf_ref make_cluster_urls(void)
{
    cf_ref out = new_array();
    cf_ref s = NULL;
    const char *urls[3] = {
        ALTSCREEN_URL_BASE,
        ALTSCREEN_INITIAL_URL,
        ALTSCREEN_URL_CARD
    };
    int i;

    if (out == NULL)
        return NULL;
    for (i = 0; i < 3; ++i) {
        s = cfs(urls[i]);
        if (s == NULL) {
            g_cf.release(out);
            return NULL;
        }
        g_cf.array_append(out, s);
        g_cf.release(s);
    }
    return out;
}

/* v8R: both dynamic alignment URLs must be eligible AirPlay cluster URLs.
 * Keep the existing three suggested URLs unchanged: only allow-list expands. */
static cf_ref make_allowed_cluster_urls(void)
{
    cf_ref out = make_cluster_urls();
    cf_ref s;
    const char *extra[] = { ALTSCREEN_URL_SMALL_GAUGES, ALTSCREEN_URL_BIG_GAUGES };
    unsigned int i;
    if (out == NULL) return NULL;
    for (i = 0; i < 2u; ++i) {
        s = cfs(extra[i]);
        if (s == NULL) { g_cf.release(out); return NULL; }
        g_cf.array_append(out, s);
        g_cf.release(s);
    }
    return out;
}

static cf_ref patch_property_result(cf_ref stock, cf_ref property)
{
    cf_ref patched;
    const char *name;

    if (!altscreen_enabled() || !g_cf.ready)
        return stock;

    name = tracked_property_name(property);

    if (name != NULL && strcmp(name, "displays") == 0) {
        patched = add_alt_display(stock);
        if (patched != NULL) {
            if (stock != NULL) g_cf.release(stock);
            return patched;
        }
        return stock;
    }

    if (name != NULL &&
        (strcmp(name, "altScreenURLs") == 0 ||
         strcmp(name, "altScreenSuggestUIURLs") == 0)) {
        int allowed = strcmp(name, "altScreenURLs") == 0;
        patched = allowed ? make_allowed_cluster_urls() : make_cluster_urls();
        if (patched != NULL) {
            if (stock != NULL) g_cf.release(stock);
            LOG_INFO(ALTINFO_MODULE,
                     "PROPERTY_RETURN property=%s count=%d base=%s extra=%s|%s",
                     name, allowed ? 5 : 3, ALTSCREEN_INITIAL_URL,
                     allowed ? ALTSCREEN_URL_SMALL_GAUGES : "-",
                     allowed ? ALTSCREEN_URL_BIG_GAUGES : "-");
            return patched;
        }
    }
    return stock;
}

cf_ref AirPlayReceiverServerPlatformCopyProperty(cf_ref server,
                                                  uint32_t flags,
                                                  cf_ref property,
                                                  cf_ref qualifier,
                                                  os_status *out_err)
{
    cf_ref stock;
    const char *name;

    resolve_cf();
    if (g_cf.server_copy == NULL)
        return NULL;
    name = tracked_property_name(property);
    if (name != NULL)
        LOG_INFO(ALTINFO_MODULE,
                 "PROPERTY_REQUEST source=server property=%s", name);
    stock = g_cf.server_copy(server, flags, property, qualifier, out_err);
    return patch_property_result(stock, property);
}

cf_ref AirPlayReceiverSessionPlatformCopyProperty(cf_ref session,
                                                   uint32_t flags,
                                                   cf_ref property,
                                                   cf_ref qualifier,
                                                   os_status *out_err)
{
    cf_ref stock;
    const char *name;

    resolve_cf();
    if (g_cf.session_copy == NULL)
        return NULL;
    name = tracked_property_name(property);
    if (name != NULL)
        LOG_INFO(ALTINFO_MODULE,
                 "PROPERTY_REQUEST source=session property=%s", name);
    stock = g_cf.session_copy(session, flags, property, qualifier, out_err);
    return patch_property_result(stock, property);
}

cf_ref AirPlayCopyServerInfo(cf_ref session,
                             cf_ref properties,
                             uint8_t *mac,
                             os_status *out_err)
{
    cf_ref stock = NULL;
    cf_ref out = NULL;
    cf_ref enabled = NULL;
    cf_ref displays = NULL;
    cf_ref patched_displays = NULL;
    cf_index display_count = 0;

    resolve_cf();

    if (g_cf.info_copy == NULL) {
        LOG_ERROR(ALTINFO_MODULE,
                  "SERVER_INFO hook_unavailable");
        if (out_err != NULL)
            *out_err = -1;
        return NULL;
    }

    stock = g_cf.info_copy(session, properties, mac, out_err);
    if (stock == NULL)
        return NULL;

    if (!altscreen_enabled() || !g_cf.ready || !is_dict(stock))
        return stock;

    out = clone_dict_manual(stock);
    if (out == NULL) {
        LOG_ERROR(ALTINFO_MODULE,
                  "SERVER_INFO clone_failed stock_preserved=1");
        return stock;
    }

    enabled = make_enabled_features();
    if (enabled == NULL || !set_obj(out, "enabledFeatures", enabled)) {
        if (enabled != NULL)
            g_cf.release(enabled);
        g_cf.release(out);
        LOG_ERROR(ALTINFO_MODULE,
                  "SERVER_INFO enabledFeatures_failed stock_preserved=1");
        return stock;
    }
    g_cf.release(enabled);
    enabled = NULL;

    displays = dict_get_obj(out, "displays");
    if (is_array(displays)) {
        display_count = g_cf.array_count(displays);
        if (display_count == 1) {
            patched_displays = add_alt_display(displays);
            if (patched_displays != NULL) {
                if (set_obj(out, "displays", patched_displays))
                    display_count = g_cf.array_count(patched_displays);
                g_cf.release(patched_displays);
                patched_displays = NULL;
            }
        }
    }

    LOG_WARN(ALTINFO_MODULE,
             "SERVER_INFO enabledFeatures=altScreen|viewAreas displays=%ld type111=%d",
             (long)display_count, display_count >= 2 ? 1 : 0);

    g_cf.release(stock);
    return out;
}

