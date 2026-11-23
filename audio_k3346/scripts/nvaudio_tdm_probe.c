/* Emulator-only one-hot PCM transport experiment, not a CarPlay hook.
 * All calls resolve the original K3346 libnvaudio and deva-nvaudio.
 * A successful client write alone is not evidence of a physical speaker route. */
#ifndef PROBE_DEVICE
#define PROBE_DEVICE "MPL1"
#endif
typedef unsigned u32;
extern int nvaudio_pcm_open(void **, const char *);
extern int nvaudio_set_pcm_info(void *, const void *);
extern int nvaudio_pcm_write(void *, const void *, u32, const u32 *, u32);
extern int nvaudio_create_metadata(void *, const void *, u32 *);
extern int nvaudio_pcm_close(void *);
extern int nvaudio_control_open(void **);
extern int nvaudio_control_set_route(void *, const void *, u32);
extern int nvaudio_control_close(void *);
extern int snprintf(char *, unsigned, const char *, ...), write(int, const void *, unsigned), usleep(unsigned);
extern void *memset(void *, int, unsigned);
extern void exit(int);
extern int open(const char *, int, ...), close(int);
extern void *mmap64(void *, unsigned, int, int, int, unsigned long long);
static short pcm[512 * 6];
static void message(const char *name, int status, unsigned detail)
{
    char text[192];
    int n = snprintf(text, sizeof(text), "TDM_PROBE %s status=%d detail=%u\n", name, status, detail);
    write(1, text, (unsigned)n);
}

void _start(void)
{
    void *handle = 0, *control = 0;
    int mem = open("/dev/mem", 2);
    void *mapping = mem >= 0 ? mmap64(0, 4096, 0x700, 1, mem, 0x70080000ull) : (void *)-1;
    if (mem >= 0) close(mem);
    if (mapping == (void *)-1) exit(4);
    volatile u32 *marker = (volatile u32 *)((char *)mapping + 0xe20);
#ifdef PROBE_ROUTE
    struct { char input[30]; char output[30]; } initial_route = {"MPL5", "TDM1_OUT_MPL"};
    int route_status = nvaudio_control_open(&control);
    message("INITIAL_CONTROL_OPEN", route_status, 0);
    if (route_status || !control) exit(5);
    route_status = nvaudio_control_set_route(control, &initial_route, 1);
    message("INITIAL_MPL5_ROUTE", route_status, 0);
    if (route_status) exit(6);
#endif
    const u32 config[3] = {48000, 2, 6};
    /* MPL1 is the default TDM1 media route in this prepared emulator. */
    int status = nvaudio_pcm_open(&handle, PROBE_DEVICE);
    message(PROBE_DEVICE "_OPEN", status, 0);
    if (status || !handle) exit(1);
    status = nvaudio_set_pcm_info(handle, config);
    message(PROBE_DEVICE "_CONFIG_6CH", status, 0);
    if (status) exit(2);
    /* Mirror the original nvaudio_play SAD construction. The fourth write
     * argument is eight metadata bytes, not a written-byte counter. */
    /* Describe the configured S16 payload and six-channel FC allocation.
     * The advertised 8-channel/4-byte capacity is not the payload stride. */
    unsigned char sad[20] = {0, 252, 0, 1, 16, 16};
    u32 metadata[2] = {0};
    *(u32 *)(sad + 8) = 48000;
    status = nvaudio_create_metadata(handle, sad, metadata);
    message("METADATA", status, metadata[0]);
    if (status) exit(3);
    for (unsigned channel = 0; channel < 6; channel++) {
        *marker = 16 + channel;
        memset(pcm, 0, sizeof(pcm));
        for (unsigned frame = 0; frame < 512; frame++) pcm[frame * 6 + channel] = (short)((channel + 1) * 1000);
        unsigned successful_calls = 0, errors = 0;
        for (unsigned block = 0; block < 12; block++) {
            status = nvaudio_pcm_write(handle, pcm, sizeof(pcm), metadata, 30);
            successful_calls += status == 0;
            errors += status != 0;
            usleep(12000);
        }
        message(PROBE_DEVICE "_ONEHOT_CHANNEL", (int)errors, channel);
        message(PROBE_DEVICE "_SUCCESSFUL_CALLS", 0, successful_calls);
    }
    nvaudio_pcm_close(handle);
    *marker = 0;
#ifdef PROBE_ROUTE
    nvaudio_control_close(control);
    control = 0;
#endif
    /* Probe whether the original control API can temporarily select MPL5.
     * Recovered route record: 30-byte input name + 30-byte output name. */
    struct { char input[30]; char output[30]; } route = {"MPL5", "TDM1_OUT_MPL"};
    status = nvaudio_control_open(&control);
    message("CONTROL_OPEN", status, 0);
    if (!status && control) {
        status = nvaudio_control_set_route(control, &route, 1);
        message("MPL5_ROUTE_REQUEST", status, 0);
        nvaudio_control_close(control);
    }
    write(1, "TDM_PROBE_DONE\n", 15);
    exit(0);
}
