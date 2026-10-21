/*
 * K3346 Etap 5.3 - SONDA okladki CarPlay (tylko odczyt).
 *
 * Pytanie: czy stock demon iAP2 (mm-ipod, /dev/ipod0) oddaje okladke biezacego
 * utworu w sesji CarPlay. iPhone wysyla ja do auta (iap2.log:
 * MediaItemArtworkFileTransferIdentifier), ale Harmanowy dio_manager jej nie
 * pobiera (importuje tylko iap2_connect/...; nie iap2_nowplay_getartwork).
 *
 * API klienta (libiap2client.so.1, eksporty potwierdzone w K3346; sygnatury wg
 * luka-dev docs/reference/MHI2_MU1439_PORT.md):
 *   void *iap2_connect(const char *devpath, int oflag);
 *   int   iap2_nowplay_getartwork(void *h, void *buf, int buflen);
 *         buflen = 0 -> rozmiar; potem drugie wywolanie z buforem
 *   int   iap2_disconnect(void *h);
 * Biblioteka ladowana przez dlopen (build nie wymaga plikow z firmware'u).
 *
 * Uzycie: k3346-artwork-probe <katalog_wyjsciowy>
 * Zapisuje: <katalog>/artwork-probe.txt (przebieg) i artwork.bin (bajty, jesli
 * sa). Niczego w systemie nie zmienia; jedno polaczenie klienta, potem rozlaczenie.
 * Uruchamiany przez command.sh w trybie COLLECT z limitem czasu.
 */
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef void *(*connect_fn)(const char *, int);
typedef int (*getartwork_fn)(void *, void *, int);
typedef int (*disconnect_fn)(void *);

#define MAX_ART (4 * 1024 * 1024)

static FILE *report;

static void say(const char *fmt, const char *a, long n)
{
    if (report == NULL)
        return;
    fprintf(report, fmt, a, n);
    fputc('\n', report);
    fflush(report);
}

int main(int argc, char **argv)
{
    char path[512];
    void *lib, *h;
    connect_fn iap2_connect;
    getartwork_fn iap2_nowplay_getartwork;
    disconnect_fn iap2_disconnect;
    int size, got;
    unsigned char *buf;

    if (argc < 2)
        return 2;
    snprintf(path, sizeof(path), "%s/artwork-probe.txt", argv[1]);
    report = fopen(path, "w");
    if (report == NULL)
        return 3;
    say("K3346_ARTWORK_PROBE v2 %s %ld", "start", 0);

    lib = dlopen("libiap2client.so.1", RTLD_NOW);
    if (lib == NULL)
        lib = dlopen("/mnt/app/armle/usr/lib/libiap2client.so.1", RTLD_NOW);
    if (lib == NULL) {
        say("dlopen FAIL %s %ld", dlerror(), 0);
        return 4;
    }
    iap2_connect = (connect_fn)dlsym(lib, "iap2_connect");
    iap2_nowplay_getartwork = (getartwork_fn)dlsym(lib, "iap2_nowplay_getartwork");
    iap2_disconnect = (disconnect_fn)dlsym(lib, "iap2_disconnect");
    if (!iap2_connect || !iap2_nowplay_getartwork || !iap2_disconnect) {
        say("dlsym FAIL %s %ld", "missing symbol", 0);
        return 5;
    }
    if (access("/dev/ipod0", F_OK) != 0) {
        say("no /dev/ipod0 (iPhone not connected?) %s errno=%ld", "", (long)errno);
        return 6;
    }

    /* v2: iap2_connect przy nieudanym open() nadpisuje errno na EINVAL (22) -
     * najpierw sam open(), zeby poznac prawdziwy blad. */
    {
        int fd;
        errno = 0;
        fd = open("/dev/ipod0", O_RDONLY);
        say("open(O_RDONLY) fd %s %ld", "", (long)fd);
        say("open(O_RDONLY) errno %s %ld", fd < 0 ? strerror(errno) : "", (long)errno);
        if (fd >= 0)
            close(fd);
    }

    /* stock dio_manager: iap2_connect(path, 0) = O_RDONLY (0x15ee30) */
    h = iap2_connect("/dev/ipod0", O_RDONLY);
    if (h == NULL) {
        say("iap2_connect(O_RDONLY) FAIL %s errno=%ld", "", (long)errno);
        h = iap2_connect("/dev/ipod0", O_RDWR);
        if (h == NULL) {
            say("iap2_connect(O_RDWR) FAIL %s errno=%ld", "", (long)errno);
            return 7;
        }
        say("iap2_connect(O_RDWR) OK %s %ld", "", 0);
    } else {
        say("iap2_connect(O_RDONLY) OK %s %ld", "", 0);
    }

    errno = 0;
    size = iap2_nowplay_getartwork(h, NULL, 0);
    say("getartwork(size) rc %s %ld", "", (long)size);
    say("getartwork(size) errno %s %ld", "", (long)errno);
    if (size > 0 && size <= MAX_ART) {
        buf = (unsigned char *)malloc((size_t)size);
        if (buf != NULL) {
            errno = 0;
            got = iap2_nowplay_getartwork(h, buf, size);
            say("getartwork(data) rc %s %ld", "", (long)got);
            say("getartwork(data) errno %s %ld", "", (long)errno);
            if (got > 0) {
                char magic[32];
                FILE *out;
                snprintf(magic, sizeof(magic), "%02x %02x %02x %02x",
                         buf[0], buf[1], buf[2], buf[3]);
                say("magic %s (ffd8ff=JPEG 89504e47=PNG) %ld", magic, 0);
                snprintf(path, sizeof(path), "%s/artwork.bin", argv[1]);
                out = fopen(path, "wb");
                if (out != NULL) {
                    long len = got <= size ? got : size;
                    fwrite(buf, 1, (size_t)len, out);
                    fclose(out);
                    say("saved artwork.bin %s bytes=%ld", "", len);
                }
            }
            free(buf);
        }
    }
    iap2_disconnect(h);
    say("K3346_ARTWORK_PROBE %s %ld", "done", 0);
    fclose(report);
    return 0;
}
