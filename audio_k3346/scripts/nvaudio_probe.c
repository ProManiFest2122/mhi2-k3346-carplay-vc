/* Emulator-only capability probe. Uses the original K3346 client and driver.
 * Recovered ABI: nvaudio_pcm_open(void **, const char *), pcm_info copies
 * six 32-bit fields plus a 30-byte name; input PCM config is rate/bytes/ch.
 * This never writes a dataset, EEPROM or firmware, and submits no PCM. */
typedef unsigned u32;
extern int nvaudio_pcm_open(void **handle, const char *device);
extern int nvaudio_pcm_info(void *handle, void *info);
extern int nvaudio_set_pcm_info(void *handle, const void *config);
extern int nvaudio_pcm_close(void *handle);
extern int snprintf(char *, unsigned, const char *, ...);
extern int write(int, const void *, unsigned);
extern void exit(int);

static void line(const char *device, int status, const u32 *info)
{
    char text[256];
    int n = snprintf(text, sizeof(text),
        "NVAUDIO_PROBE %s status=%d info=%u,%u,%u,%u,%u,%u\n",
        device, status, info[0], info[1], info[2], info[3], info[4], info[5]);
    write(1, text, (unsigned)n);
}

void _start(void)
{
    static const char *names[] = {"MPL1", "MPL2", "MPL3", "MPL4", "MPL5", "MPL6", "ANN1", "TEL"};
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        void *handle = 0;
        u32 info[32] = {0};
        int status = nvaudio_pcm_open(&handle, names[i]);
        if (!status) status = nvaudio_pcm_info(handle, info);
        line(names[i], status, info);
        if (handle) {
            /* Only configure; no queue write and no invented channel mask. */
            const u32 pcm[3] = {48000, 2, 6};
            int set = nvaudio_set_pcm_info(handle, pcm);
            u32 result[32] = {0};
            result[0] = (u32)set;
            line("CONFIG_48K_S16_6CH", set, result);
            nvaudio_pcm_close(handle);
        }
    }
    write(1, "NVAUDIO_PROBE_DONE\n", 19);
    exit(0);
}
