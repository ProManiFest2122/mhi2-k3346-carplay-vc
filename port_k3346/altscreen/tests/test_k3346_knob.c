/* Host simulation compiles the EXACT production source of the display record:
 * knob copy, features=2 and primaryInputDevice go together or not at all. */
#define ENABLE_LOGGING 0
#include "../native/altscreen_airplay.c"
#include <assert.h>

enum { K_DICT = 1, K_ARR, K_STR, K_NUM, K_BOOL };

typedef struct mock_obj {
    int kind, count;
    char value[128];
    struct { char key[48]; struct mock_obj *val; } item[32];
} mock_obj;

static int g_fail_array_copy_at;
static int g_array_copy_count;
static int g_fail_dict_new_at;
static int g_dict_new_count;
static const char *g_fail_string;
static cf_ref g_stock_info;

static mock_obj *obj(int kind, const char *s)
{
    mock_obj *v = calloc(1, sizeof(*v));
    assert(v);
    v->kind = kind;
    if (s) snprintf(v->value, sizeof(v->value), "%s", s);
    return v;
}

static cf_ref fake_str(cf_ref a, const char *s, uint32_t enc)
{
    (void)a; (void)enc;
    if (g_fail_string && !strcmp(s, g_fail_string)) return NULL;
    return obj(K_STR, s);
}
static cf_type_id fake_type(cf_ref x) { return x ? (cf_type_id)((mock_obj *)x)->kind : 0; }
static cf_type_id fake_dict_type(void) { return K_DICT; }
static cf_type_id fake_arr_type(void) { return K_ARR; }
static cf_type_id fake_str_type(void) { return K_STR; }
static cf_type_id fake_num_type(void) { return K_NUM; }
static cf_ref fake_dict_new(cf_ref a, cf_index c, const void *k, const void *v)
{
    (void)a; (void)c; (void)k; (void)v;
    if (g_fail_dict_new_at && ++g_dict_new_count == g_fail_dict_new_at) return NULL;
    return obj(K_DICT, NULL);
}
static cf_ref fake_arr_new(cf_ref a, cf_index c, const void *cb)
{
    (void)a; (void)c; (void)cb;
    return obj(K_ARR, NULL);
}
static void fake_append(cf_ref arr, const void *v)
{
    mock_obj *a = arr;
    assert(a->kind == K_ARR && a->count < 32);
    a->item[a->count++].val = (mock_obj *)v;
}
static cf_index fake_arr_count(cf_ref arr) { return ((mock_obj *)arr)->count; }
static const void *fake_arr_value(cf_ref arr, cf_index i)
{
    assert(i >= 0 && i < ((mock_obj *)arr)->count);
    return ((mock_obj *)arr)->item[i].val;
}
static cf_ref fake_arr_copy(cf_ref a, cf_index c, cf_ref src)
{
    mock_obj *out;
    (void)a; (void)c;
    if (g_fail_array_copy_at && ++g_array_copy_count == g_fail_array_copy_at) return NULL;
    out = obj(K_ARR, NULL);
    memcpy(out->item, ((mock_obj *)src)->item, sizeof(out->item));
    out->count = ((mock_obj *)src)->count;
    return out;
}
static void fake_set(cf_ref d, const void *k, const void *v)
{
    mock_obj *m = d;
    const char *name = ((const mock_obj *)k)->value;
    int i;

    assert(m->kind == K_DICT);
    for (i = 0; i < m->count; i++)
        if (!strcmp(m->item[i].key, name)) break;
    assert(i < 32);
    if (i == m->count) m->count++;
    snprintf(m->item[i].key, sizeof(m->item[i].key), "%s", name);
    m->item[i].val = (mock_obj *)v;
}
static const void *fake_get(cf_ref d, const void *k)
{
    mock_obj *m = d;
    int i;

    for (i = 0; i < m->count; i++)
        if (!strcmp(m->item[i].key, ((const mock_obj *)k)->value)) return m->item[i].val;
    return NULL;
}
static cf_index fake_dict_count(cf_ref d) { return ((mock_obj *)d)->count; }
static void fake_keys_values(cf_ref d, const void **keys, const void **values)
{
    mock_obj *m = d;
    int i;

    for (i = 0; i < m->count; i++) {
        keys[i] = obj(K_STR, m->item[i].key);
        values[i] = m->item[i].val;
    }
}
static os_status fake_set_i64(cf_ref d, const void *k, int64_t v)
{
    mock_obj *n = obj(K_NUM, NULL);

    snprintf(n->value, sizeof(n->value), "%lld", (long long)v);
    fake_set(d, k, n);
    return 0;
}
static cf_ref fake_retain(cf_ref x) { return x; }
static void fake_release(cf_ref x) { (void)x; /* fixed-length unit-test run */ }
static unsigned char fake_equal(cf_ref a, cf_ref b)
{
    return ((mock_obj *)a)->kind == ((mock_obj *)b)->kind &&
           !strcmp(((mock_obj *)a)->value, ((mock_obj *)b)->value);
}
static unsigned char fake_cstring(cf_ref s, char *out, cf_index size, uint32_t enc)
{
    (void)enc;
    snprintf(out, (size_t)size, "%s", ((mock_obj *)s)->value);
    return 1;
}
static unsigned char fake_number_value(cf_ref n, int type, void *out)
{
    (void)type;
    assert(((mock_obj *)n)->kind == K_NUM);
    *(int64_t *)out = atoll(((mock_obj *)n)->value);
    return 1;
}
static cf_ref fake_info_copy(cf_ref s, cf_ref p, uint8_t *mac, os_status *err)
{
    (void)s; (void)p; (void)mac; (void)err;
    return g_stock_info;
}

static void setup_cf(void)
{
    static int dummy;

    g_cf.resolved = 1;
    g_cf.ready = 1;
    g_cf.info_copy = fake_info_copy;
    g_cf.string_create = fake_str;
    g_cf.get_type = fake_type;
    g_cf.array_type = fake_arr_type;
    g_cf.dict_type = fake_dict_type;
    g_cf.string_type = fake_str_type;
    g_cf.number_type = fake_num_type;
    g_cf.array_mutable_copy = fake_arr_copy;
    g_cf.array_create_mutable = fake_arr_new;
    g_cf.array_append = fake_append;
    g_cf.array_count = fake_arr_count;
    g_cf.array_value = fake_arr_value;
    g_cf.dict_create_mutable = fake_dict_new;
    g_cf.dict_get_value = fake_get;
    g_cf.dict_set_value = fake_set;
    g_cf.dict_count = fake_dict_count;
    g_cf.dict_get_keys_values = fake_keys_values;
    g_cf.dict_set_i64 = fake_set_i64;
    g_cf.retain = fake_retain;
    g_cf.release = fake_release;
    g_cf.equal = fake_equal;
    g_cf.string_get_cstring = fake_cstring;
    g_cf.number_get_value = fake_number_value;
    g_cf.dict_key_callbacks = &dummy;
    g_cf.dict_value_callbacks = &dummy;
    g_cf.array_callbacks = &dummy;
    g_cf.boolean_true = obj(K_BOOL, "1");
    g_cf.boolean_false = obj(K_BOOL, "0");
}

static void put(mock_obj *d, const char *key, mock_obj *v) { fake_set(d, obj(K_STR, key), v); }
static mock_obj *get(mock_obj *d, const char *key) { return (mock_obj *)fake_get(d, obj(K_STR, key)); }
static mock_obj *str(const char *s) { return obj(K_STR, s); }

static mock_obj *device(const char *name, const char *uuid)
{
    mock_obj *d = obj(K_DICT, NULL);

    put(d, "name", str(name));
    put(d, "uuid", str(uuid));
    put(d, "hidDescriptor", str("desc"));
    put(d, "displayUUID", str("MAIN"));
    return d;
}

static mock_obj *main_display(void)
{
    mock_obj *d = obj(K_DICT, NULL);
    mock_obj *n = obj(K_NUM, "2");

    put(d, "uuid", str("MAIN"));
    put(d, "features", n);
    put(d, "primaryInputDevice", str("STOCK-KNOB"));
    return d;
}

static mock_obj *stock_info(const char *const *names, int count, int has_list, int with_alt)
{
    mock_obj *info = obj(K_DICT, NULL);
    mock_obj *displays = obj(K_ARR, NULL);
    int i;

    fake_append(displays, main_display());
    if (with_alt) {
        mock_obj *alt = obj(K_DICT, NULL);

        put(alt, "uuid", str(ALTSCREEN_UUID));
        put(alt, "features", obj(K_NUM, "0"));
        fake_append(displays, alt);
    }
    put(info, "displays", displays);
    if (has_list) {
        mock_obj *list = obj(K_ARR, NULL);

        for (i = 0; i < count; i++) fake_append(list, device(names[i], "U"));
        put(info, "hidDevices", list);
    }
    return info;
}

static mock_obj *alt_of(mock_obj *info)
{
    mock_obj *displays = get(info, "displays");
    mock_obj *found = NULL;
    int i, seen = 0;

    for (i = 0; i < displays->count; i++) {
        mock_obj *u = get(displays->item[i].val, "uuid");
        if (u && !strcmp(u->value, ALTSCREEN_UUID)) { found = displays->item[i].val; seen++; }
    }
    assert(seen == 1);
    return found;
}

static void assert_knob_on(mock_obj *out, mock_obj *stock, int stock_count, int pick)
{
    mock_obj *list = get(out, "hidDevices");
    mock_obj *alt = alt_of(out);
    mock_obj *knob;
    int i;

    assert(g_knob_advertised == 1);
    assert(list->count == stock_count + 1);
    knob = list->item[stock_count].val;
    assert(!strcmp(get(knob, "name")->value, ALTSCREEN_KNOB_NAME));
    assert(!strcmp(get(knob, "uuid")->value, ALTSCREEN_KNOB_UUID));
    assert(!strcmp(get(knob, "displayUUID")->value, ALTSCREEN_UUID));
    assert(!strcmp(get(knob, "hidDescriptor")->value, "desc"));
    assert(!strcmp(get(alt, "features")->value, "2"));
    assert(!strcmp(get(alt, "primaryInputDevice")->value, ALTSCREEN_KNOB_UUID));
    /* the stock list, its knob entry and the main display stay untouched */
    assert(get(stock, "hidDevices")->count == stock_count);
    for (i = 0; i < stock_count; i++)
        assert(list->item[i].val == get(stock, "hidDevices")->item[i].val);
    assert(!strcmp(get(list->item[pick].val, "name")->value, "Eso_Knob"));
    assert(!strcmp(get(list->item[pick].val, "uuid")->value, "U"));
    assert(!strcmp(get(list->item[pick].val, "displayUUID")->value, "MAIN"));
    assert(!strcmp(get(get(out, "displays")->item[0].val, "primaryInputDevice")->value, "STOCK-KNOB"));
    assert(get(out, "displays")->count == 2);
}

static void assert_knob_off(mock_obj *out, mock_obj *stock)
{
    mock_obj *alt = alt_of(out);

    assert(g_knob_advertised == 0);
    assert(get(out, "hidDevices") == get(stock, "hidDevices"));
    assert(!strcmp(get(alt, "features")->value, "0"));
    assert(get(alt, "primaryInputDevice") == NULL);
    assert(get(out, "displays")->count == 2);
}

static mock_obj *run_info(mock_obj *stock)
{
    g_stock_info = stock;
    return (mock_obj *)AirPlayCopyServerInfo(NULL, NULL, NULL, NULL);
}

int main(void)
{
    static const char *const first[] = { "Eso_Knob", "Eso_Touch", "Eso_Pad" };
    static const char *const last[] = { "Eso_Touch", "Eso_Pad", "Eso_Knob" };
    static const char *const none[] = { "Eso_Touch", "Eso_Pad" };
    static const char *const twice[] = { "Eso_Knob", "Eso_Knob" };
    static const char *const fail_strings[] = {
        ALTSCREEN_KNOB_NAME, ALTSCREEN_KNOB_UUID, "displayUUID", "hidDevices"
    };
    mock_obj *stock, *out;
    unsigned int i;

    setenv("CARPLAY_ALTSCREEN", "1", 1);
    setup_cf();

    /* knob at any position */
    stock = stock_info(first, 3, 1, 0);
    assert_knob_on(run_info(stock), stock, 3, 0);
    stock = stock_info(last, 3, 1, 0);
    assert_knob_on(run_info(stock), stock, 3, 2);

    /* only the first knob is copied */
    stock = stock_info(twice, 2, 1, 0);
    put(get(stock, "hidDevices")->item[1].val, "hidDescriptor", str("second"));
    assert_knob_on(run_info(stock), stock, 2, 0);

    /* stock already asked through the property hook: record rebuilt, not doubled */
    stock = stock_info(first, 3, 1, 1);
    assert_knob_on(run_info(stock), stock, 3, 0);

    /* no knob, empty list, no list: today's record */
    stock = stock_info(none, 2, 1, 0);
    assert_knob_off(run_info(stock), stock);
    stock = stock_info(NULL, 0, 1, 0);
    assert_knob_off(run_info(stock), stock);
    stock = stock_info(NULL, 0, 0, 0);
    assert_knob_off(run_info(stock), stock);

    /* a knob from the previous session must not survive into a session without one */
    stock = stock_info(first, 3, 1, 0);
    assert_knob_on(run_info(stock), stock, 3, 0);
    stock = stock_info(none, 2, 1, 1);
    assert_knob_off(run_info(stock), stock);

    /* failed copy of the device list: the 1st array copy of an /info */
    g_fail_array_copy_at = 1;
    g_array_copy_count = 0;
    stock = stock_info(first, 3, 1, 0);
    assert_knob_off(run_info(stock), stock);
    g_fail_array_copy_at = 0;

    for (i = 0; i < sizeof(fail_strings) / sizeof(fail_strings[0]); ++i) {
        g_fail_string = fail_strings[i];
        stock = stock_info(first, 3, 1, 0);
        g_stock_info = stock;
        out = (mock_obj *)AirPlayCopyServerInfo(NULL, NULL, NULL, NULL);
        g_fail_string = NULL;
        assert(g_knob_advertised == 0);
        assert(get(out, "hidDevices") == get(stock, "hidDevices"));
        assert(!strcmp(get(alt_of(out), "features")->value, "0"));
        assert(get(alt_of(out), "primaryInputDevice") == NULL);
    }

    /* failed dict for the knob copy: the 2nd dict after the /info clone */
    g_fail_dict_new_at = 2;
    g_dict_new_count = 0;
    stock = stock_info(first, 3, 1, 0);
    assert_knob_off(run_info(stock), stock);
    g_fail_dict_new_at = 0;

    /* property path repeats the same record */
    stock = stock_info(first, 3, 1, 0);
    assert_knob_on(run_info(stock), stock, 3, 0);
    {
        mock_obj *displays = obj(K_ARR, NULL);
        mock_obj *patched;

        fake_append(displays, main_display());
        patched = (mock_obj *)add_alt_display(displays);
        assert(patched && patched->count == 2);
        assert(!strcmp(get(patched->item[1].val, "features")->value, "2"));
        assert(!strcmp(get(patched->item[1].val, "primaryInputDevice")->value, ALTSCREEN_KNOB_UUID));
        assert(!strcmp(get(patched->item[1].val, "maxFPS")->value, "30"));
        assert(get(patched->item[1].val, "viewAreas")->count == 9);
    }

    /* the phone may be told the card-less URL */
    {
        mock_obj *allowed = (mock_obj *)patch_property_result(NULL, str("altScreenURLs"));
        mock_obj *suggested = (mock_obj *)patch_property_result(NULL, str("altScreenSuggestUIURLs"));
        int found = 0, j;

        for (j = 0; j < allowed->count; j++)
            found += !strcmp(allowed->item[j].val->value, ALTSCREEN_URL_BIG_GAUGES_NO_CARD);
        assert(found == 1);
        assert(allowed->count == 6 && suggested->count == 3);
    }

    /* the log survives odd stock data */
    stock = stock_info(none, 2, 1, 0);
    put(get(stock, "hidDevices")->item[0].val, "name", obj(K_NUM, "7"));
    put((mock_obj *)get(stock, "displays")->item[0].val, "primaryInputDevice", obj(K_NUM, "3"));
    put((mock_obj *)get(stock, "displays")->item[0].val, "features", str("2"));
    assert_knob_off(run_info(stock), stock);

    puts("k3346 knob OK");
    return 0;
}
