/* Emulator-only fixture: redirect the existing mediarouter's MPL1 route to
 * MPL5. No extra controller allocation and no driver patch. This is not a
 * shipping CarPlay policy: it deliberately redirects the simulated media
 * source so that a disconnected-phone MPL5 sink can be tested in isolation. */
typedef unsigned u32;
struct route_record { char input[30], output[30]; };
typedef int (*route_fn)(void *, const struct route_record *, u32);
extern void *dlopen(const char *, int), *dlsym(void *, const char *);
extern int strcmp(const char *, const char *);
extern void *memcpy(void *, const void *, unsigned);
extern int write(int, const void *, unsigned);
static route_fn original;

int nvaudio_control_set_route(void *handle, const struct route_record *routes, u32 count)
{
    struct route_record copy[13];
    if (!original) {
        void *lib = dlopen("libnvaudio.so", 1);
        if (lib) original = (route_fn)dlsym(lib, "nvaudio_control_set_route");
    }
    if (!original) return 1;
    if (!routes || count < 1 || count > 13) return original(handle, routes, count);
    memcpy(copy, routes, count * sizeof(copy[0]));
    for (u32 i = 0; i < count; i++) {
        if (!strcmp(copy[i].input, "MPL1") && !strcmp(copy[i].output, "TDM1_OUT_MPL")) {
            memcpy(copy[i].input, "MPL5", 5);
            write(2, "AUDIO_ROUTE_FIXTURE MPL1->MPL5\n", 30);
        }
    }
    return original(handle, copy, count);
}
