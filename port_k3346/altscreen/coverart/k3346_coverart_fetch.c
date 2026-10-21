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
 * Zapisuje <sciezka>.png: PNG 256x256 RGB (obraz wpisany z zachowaniem
 * proporcji na czarnym tle) - jak luka-dev coverart_hook.c. Serwer obrazkow
 * kombi (MIBRoot [PS]) wysyla plik do VC bez przerabiania, a VC (test w aucie
 * 2026-10-21) z JPEG 600x600 pokazal tylko obrazek zastepczy. Dekoder:
 * stb_image (public domain); kompresja PNG: systemowe libz.so.2 przez dlopen,
 * bez niego bloki "stored" (poprawny PNG, wiekszy plik).
 * Zapis przez .tmp + rename. Tworzy brakujace katalogi nadrzedne.
 * Wynik na stdout (czyta go Java):
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

/* stb_image: tylko JPEG/PNG z pamieci; bez __thread (emutls na QNX 6.5) */
#define STBI_NO_THREAD_LOCALS
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_GIF
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_TGA
#define STBI_NO_BMP
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"

typedef void *(*connect_fn)(const char *, int);
typedef int (*getartwork_fn)(void *, void *, int);
typedef int (*disconnect_fn)(void *);
typedef unsigned long (*compressbound_fn)(unsigned long);
typedef int (*compress2_fn)(unsigned char *, unsigned long *, const unsigned char *, unsigned long, int);

#define MIN_ART 100
#define MAX_ART (1024 * 1024)
#define OUT_W 256
#define OUT_H 256
#define MAX_PIXELS (8 * 1024 * 1024)

/* ---------------- obraz: skalowanie "contain" (luka-dev coverart_hook.c) ---------------- */

static void resize_rgb(const unsigned char *src, int sw, int sh, unsigned char *dst, int dw, int dh)
{
    int x, y;
    for (y = 0; y < dh; y++) {
        const unsigned char *srow = src + (size_t)(y * sh / dh) * sw * 3;
        unsigned char *drow = dst + (size_t)y * dw * 3;
        for (x = 0; x < dw; x++) {
            const unsigned char *p = srow + (size_t)(x * sw / dw) * 3;
            drow[x * 3] = p[0];
            drow[x * 3 + 1] = p[1];
            drow[x * 3 + 2] = p[2];
        }
    }
}

static int resize_contain(const unsigned char *src, int sw, int sh, unsigned char *dst)
{
    int nw, nh, x0, y0, y;
    unsigned char *tmp;
    if (sw * OUT_H >= sh * OUT_W) { nw = OUT_W; nh = (sh * OUT_W + sw / 2) / sw; }
    else { nh = OUT_H; nw = (sw * OUT_H + sh / 2) / sh; }
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;
    tmp = (unsigned char *)malloc((size_t)nw * nh * 3);
    if (tmp == NULL) return -1;
    resize_rgb(src, sw, sh, tmp, nw, nh);
    memset(dst, 0, (size_t)OUT_W * OUT_H * 3);
    x0 = (OUT_W - nw) / 2;
    y0 = (OUT_H - nh) / 2;
    for (y = 0; y < nh; y++)
        memcpy(dst + ((size_t)(y0 + y) * OUT_W + x0) * 3, tmp + (size_t)y * nw * 3, (size_t)nw * 3);
    free(tmp);
    return 0;
}

/* ---------------- PNG ---------------- */

static unsigned long crc_table[256];

static void crc_init(void)
{
    unsigned long c;
    int n, k;
    for (n = 0; n < 256; n++) {
        c = (unsigned long)n;
        for (k = 0; k < 8; k++) c = (c & 1) ? 0xedb88320UL ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}

static unsigned long crc_update(unsigned long crc, const unsigned char *p, size_t n)
{
    while (n--) crc = crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return crc;
}

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v;
}

/* zlib bez kompresji: bloki stored po <= 65535 B */
static size_t zlib_stored(const unsigned char *raw, size_t len, unsigned char *out)
{
    size_t o = 0, off = 0;
    unsigned long a = 1, b = 0;
    size_t i;
    out[o++] = 0x78; out[o++] = 0x01;
    do {
        size_t n = len - off > 65535 ? 65535 : len - off;
        out[o++] = (unsigned char)(off + n == len ? 1 : 0);
        out[o++] = (unsigned char)(n & 0xff); out[o++] = (unsigned char)(n >> 8);
        out[o++] = (unsigned char)(~n & 0xff); out[o++] = (unsigned char)((~n >> 8) & 0xff);
        memcpy(out + o, raw + off, n);
        o += n;
        off += n;
    } while (off < len);
    for (i = 0; i < len; i++) { a = (a + raw[i]) % 65521UL; b = (b + a) % 65521UL; }
    put32(out + o, (b << 16) | a);
    return o + 4;
}

static size_t add_chunk(unsigned char *out, const char *type, const unsigned char *data, size_t len)
{
    unsigned long crc;
    put32(out, (unsigned long)len);
    memcpy(out + 4, type, 4);
    if (len) memcpy(out + 8, data, len);
    crc = crc_update(0xffffffffUL, out + 4, len + 4) ^ 0xffffffffUL;
    put32(out + 8 + len, crc);
    return len + 12;
}

/* RGB OUT_WxOUT_H -> PNG w nowym buforze */
static unsigned char *encode_png(const unsigned char *rgb, size_t *out_len, int *compressed)
{
    size_t raw_len = (size_t)OUT_H * (OUT_W * 3 + 1), zmax, zlen = 0, o = 0;
    unsigned char *raw, *z, *png, ihdr[13];
    void *lz;
    int y;

    raw = (unsigned char *)malloc(raw_len);
    if (raw == NULL) return NULL;
    for (y = 0; y < OUT_H; y++) {
        raw[(size_t)y * (OUT_W * 3 + 1)] = 0;   /* filtr None */
        memcpy(raw + (size_t)y * (OUT_W * 3 + 1) + 1, rgb + (size_t)y * OUT_W * 3, (size_t)OUT_W * 3);
    }
    zmax = raw_len + raw_len / 65535 * 5 + 64;
    z = (unsigned char *)malloc(zmax);
    if (z == NULL) { free(raw); return NULL; }
    *compressed = 0;
    lz = dlopen("libz.so.2", RTLD_NOW);
    if (lz == NULL) lz = dlopen("/mnt/app/armle/usr/lib/libz.so.2", RTLD_NOW);
    if (lz != NULL) {
        compress2_fn c2 = (compress2_fn)dlsym(lz, "compress2");
        unsigned long zl = (unsigned long)zmax;
        if (c2 != NULL && c2(z, &zl, raw, (unsigned long)raw_len, 9) == 0) {
            zlen = zl;
            *compressed = 1;
        }
    }
    if (!*compressed) zlen = zlib_stored(raw, raw_len, z);
    free(raw);

    png = (unsigned char *)malloc(8 + 25 + zlen + 12 + 12);
    if (png == NULL) { free(z); return NULL; }
    memcpy(png, "\x89PNG\r\n\x1a\n", 8);
    o = 8;
    put32(ihdr, OUT_W); put32(ihdr + 4, OUT_H);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;   /* 8 bit RGB */
    o += add_chunk(png + o, "IHDR", ihdr, 13);
    o += add_chunk(png + o, "IDAT", z, zlen);
    o += add_chunk(png + o, "IEND", NULL, 0);
    free(z);
    *out_len = o;
    return png;
}

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

    if (!(buf[0] == 0xff && buf[1] == 0xd8 && buf[2] == 0xff) &&
        !(buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G')) {
        free(buf);
        return fail("format", EILSEQ);
    }

    {
        int w = 0, h = 0, ch = 0, compressed = 0;
        unsigned char *rgb, *out, *png;
        size_t png_len = 0;
        if (!stbi_info_from_memory(buf, got, &w, &h, &ch) || w <= 0 || h <= 0 ||
            (long)w * (long)h > MAX_PIXELS) {
            free(buf);
            return fail("dimensions", EFBIG);
        }
        rgb = stbi_load_from_memory(buf, got, &w, &h, &ch, 3);
        free(buf);
        if (rgb == NULL) return fail("decode", EILSEQ);
        out = (unsigned char *)malloc((size_t)OUT_W * OUT_H * 3);
        if (out == NULL || resize_contain(rgb, w, h, out) != 0) {
            stbi_image_free(rgb);
            free(out);
            return fail("resize", ENOMEM);
        }
        stbi_image_free(rgb);
        crc_init();
        png = encode_png(out, &png_len, &compressed);
        free(out);
        if (png == NULL) return fail("png", ENOMEM);

        snprintf(final, sizeof(final), "%s.png", argv[1]);
        snprintf(tmp, sizeof(tmp), "%s.tmp", final);
        make_parents(final);
        if (write_file(tmp, final, png, (int)png_len) != 0) {
            rc = errno;
            free(png);
            return fail("write", rc);
        }
        free(png);
        printf("OK %u %s src=%dx%d zlib=%d\n", (unsigned)png_len, final, w, h, compressed);
    }
    return 0;
}
