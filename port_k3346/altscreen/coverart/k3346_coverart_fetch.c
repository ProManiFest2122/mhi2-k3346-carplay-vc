/*
 * K3346 Etap 5.3 - pobiera okladke biezacego utworu CarPlay z demona iAP2.
 *
 * Ta sama sciezka co sonda diagnostics/k3346_artwork_probe.c (v2, PASS w aucie
 * 2026-10-20: JPEG 600x600 z sesji CarPlay):
 *   iap2_connect("/dev/ipod0", O_RDONLY)   - jak stock dio_manager (0x15ee30)
 *   iap2_nowplay_getartwork(h, NULL, 0)    - rozmiar
 *   iap2_nowplay_getartwork(h, buf, size)  - dane
 * libiap2client.so.1 ladowana przez dlopen (build bez plikow z firmware'u).
 *
 * Uzycie: k3346-coverart-fetch <sciezka_bez_rozszerzenia>
 * Zapisuje <sciezka>.jpg albo <sciezka>.png (wg sygnatury) przez plik .tmp +
 * rename, wiec czytelnik nigdy nie widzi polowy obrazka. Tworzy brakujace
 * katalogi nadrzedne. Wynik na stdout (czyta go Java):
 *   OK <bajty> <plik>
 *   ERR <krok> <errno>
 * Kod wyjscia 0 = OK. Uruchamiany przez HMI (K3346CoverArt) z limitem czasu.
 */
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef void *(*connect_fn)(const char *, int);
typedef int (*getartwork_fn)(void *, void *, int);
typedef int (*disconnect_fn)(void *);

#define MIN_ART 100
#define MAX_ART (1024 * 1024)

static int fail(const char *step, int err)
{
    printf("ERR %s %d\n", step, err);
    return 1;
}

/* mkdir -p dla katalogow nadrzednych sciezki */
static void make_parents(const char *path)
{
    char dir[512];
    size_t i, n = strlen(path);
    if (n >= sizeof(dir))
        return;
    memcpy(dir, path, n + 1);
    for (i = 1; i < n; i++) {
        if (dir[i] == '/') {
            dir[i] = '\0';
            mkdir(dir, 0777);
            dir[i] = '/';
        }
    }
}

static int write_file(const char *tmp, const char *final, const unsigned char *buf, int len)
{
    FILE *out = fopen(tmp, "wb");
    if (out == NULL)
        return -1;
    if (fwrite(buf, 1, (size_t)len, out) != (size_t)len) {
        fclose(out);
        unlink(tmp);
        return -1;
    }
    if (fclose(out) != 0) {
        unlink(tmp);
        return -1;
    }
    chmod(tmp, 0644);
    if (rename(tmp, final) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    char final[512], tmp[520];
    void *lib, *h;
    connect_fn iap2_connect;
    getartwork_fn iap2_nowplay_getartwork;
    disconnect_fn iap2_disconnect;
    unsigned char *buf;
    const char *ext;
    int size, got, rc;

    if (argc < 2 || strlen(argv[1]) > 480)
        return fail("args", EINVAL);

    lib = dlopen("libiap2client.so.1", RTLD_NOW);
    if (lib == NULL)
        lib = dlopen("/mnt/app/armle/usr/lib/libiap2client.so.1", RTLD_NOW);
    if (lib == NULL)
        return fail("dlopen", errno);
    iap2_connect = (connect_fn)dlsym(lib, "iap2_connect");
    iap2_nowplay_getartwork = (getartwork_fn)dlsym(lib, "iap2_nowplay_getartwork");
    iap2_disconnect = (disconnect_fn)dlsym(lib, "iap2_disconnect");
    if (!iap2_connect || !iap2_nowplay_getartwork || !iap2_disconnect)
        return fail("dlsym", ENOENT);

    h = iap2_connect("/dev/ipod0", O_RDONLY);
    if (h == NULL)
        return fail("connect", errno);

    errno = 0;
    size = iap2_nowplay_getartwork(h, NULL, 0);
    if (size < MIN_ART || size > MAX_ART) {
        rc = errno;
        iap2_disconnect(h);
        return fail(size <= 0 ? "noart" : "size", size <= 0 ? rc : EFBIG);
    }
    buf = (unsigned char *)malloc((size_t)size);
    if (buf == NULL) {
        iap2_disconnect(h);
        return fail("malloc", ENOMEM);
    }
    errno = 0;
    got = iap2_nowplay_getartwork(h, buf, size);
    rc = errno;
    iap2_disconnect(h);
    if (got < MIN_ART || got > size) {
        free(buf);
        return fail("data", rc ? rc : EIO);
    }

    if (buf[0] == 0xff && buf[1] == 0xd8 && buf[2] == 0xff)
        ext = "jpg";
    else if (buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G')
        ext = "png";
    else {
        free(buf);
        return fail("format", EILSEQ);
    }

    snprintf(final, sizeof(final), "%s.%s", argv[1], ext);
    snprintf(tmp, sizeof(tmp), "%s.tmp", final);
    make_parents(final);
    if (write_file(tmp, final, buf, got) != 0) {
        rc = errno;
        free(buf);
        return fail("write", rc);
    }
    free(buf);
    printf("OK %d %s\n", got, final);
    return 0;
}
