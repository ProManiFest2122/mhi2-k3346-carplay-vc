/*
 * K3346 Etap 6: ostatnie polaczenia CarPlay na VC - patrz k3346_tel.h.
 *
 * Builder 0x4170 i parser 0x4171 przeniesione z luka-dev
 * hook/telephony/tel_hook.c (GPL-3.0, Copyright (c) 2026 LuKa (@LuKa_dev)):
 * zadanie bajt w bajt jak zweryfikowane w aucie xPaiiN (recents max 25 ->
 * iOS i tak tnie do 12 w CarPlay, favorites max 0 = domyslny limit), parser
 * scala liste wysylana w kilku wiadomosciach (count ustala dlugosc, wpis
 * trafia wg indeksu). Ulubione przychodza, ale sa ignorowane.
 *
 * Wynik: plik tekstowy dla Javy (K3346GatedPhoneService), zapis przez .tmp +
 * rename. Polecenia z Javy: plik "dial <numer>", czytany i kasowany w watku
 * sterownika (k3346_tel_tick) -> 0x415A InitiateCall.
 */
#include "k3346_tel.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "framework/iap2_protocol.h"
#include "diagnostics.h"

#ifndef K3346_TEL_RECENTS_PATH
#define K3346_TEL_RECENTS_PATH "/tmp/k3346-tel-recents.txt"
#endif
#ifndef K3346_TEL_CMD_PATH
#define K3346_TEL_CMD_PATH "/tmp/k3346-tel-cmd"
#endif

enum {
    LINK_HEADER_LEN = 9,
    CONTROL_HEADER_LEN = 6,
    LIST_FIRST_DELAY_MS = 2000,
    LIST_RETRY_MS = 15000,
    LIST_MAX_ATTEMPTS = 3,
    /* test w aucie 2026-10-21: iPhone odpowiedzial "dostepne, count 0" (ostatnie i
     * ulubione) i w sesji bez rozmow nie wyslal nic wiecej -> ponowne prosby,
     * gdy lista pusta (20 s, potem 60 s) i po kazdej zmianie stanu rozmowy (5 s) */
    REREQ_EMPTY_FIRST_MS = 20000,
    REREQ_EMPTY_NEXT_MS = 60000,
    REREQ_AFTER_CALL_MS = 5000,
    REREQ_MAX = 4,
    FILE_MAX = 4096
};

/* ---------------- builders ---------------- */

static size_t put_tlv(uint8_t *out, size_t max, size_t off, uint16_t id, const void *v, size_t n)
{
    size_t w;
    if (off >= max) return 0;
    w = iap2_build_tlv(out + off, max - off, id, (const uint8_t *)v, n);
    return w ? off + w : 0;
}

/* Etap 6c (test 2026-10-21: iPhone zawsze "available 1, count 0", takze z
 * BluetoothTransportComponent): kolejne prosby wysylaja inne warianty, log
 * pokaze, ktory da liste z wpisami.
 *   v0 max 25 / ulubione 0, combine 1  (bajty xPaiiN, jak dotad)
 *   v1 max 20 / 20, combine 1          (dokladnie Luka, tel_build_start_list_updates)
 *   v2 max 0 / 0 (domyslny limit iOS), combine 1
 *   v3 max 12 / 12, combine 0          (bez scalania)
 *   v4 tylko grupy wlasciwosci 1 i 6   (bez max i combine) */
size_t k3346_tel_build_start_lists_variant(uint8_t *out, size_t max, int variant)
{
    static const uint8_t recent_props[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    static const uint8_t favorite_props[] = { 0, 1, 2, 3, 4, 5 };
    static const uint16_t rmax[K3346_TEL_VARIANTS] = { 25, 20, 0, 12, 0 };
    static const uint16_t fmax[K3346_TEL_VARIANTS] = { 0, 20, 0, 12, 0 };
    uint8_t grp[64];
    uint8_t be[2];
    uint8_t combine;
    size_t g = 0, off, i;
    int with_limits;

    if (variant < 0 || variant >= K3346_TEL_VARIANTS) return 0;
    with_limits = variant != 4;
    combine = variant == 3 ? 0 : 1;
    for (i = 0; i < sizeof(recent_props); i++)
        if (!(g = put_tlv(grp, sizeof(grp), g, recent_props[i], NULL, 0))) return 0;
    if (!(off = put_tlv(out, max, 0, 1, grp, g))) return 0;
    if (with_limits) {
        be[0] = (uint8_t)(rmax[variant] >> 8); be[1] = (uint8_t)rmax[variant];
        if (!(off = put_tlv(out, max, off, 3, be, 2))) return 0;
        if (!(off = put_tlv(out, max, off, 4, &combine, 1))) return 0;
    }
    g = 0;
    for (i = 0; i < sizeof(favorite_props); i++)
        if (!(g = put_tlv(grp, sizeof(grp), g, favorite_props[i], NULL, 0))) return 0;
    if (!(off = put_tlv(out, max, off, 6, grp, g))) return 0;
    if (!with_limits) return off;
    be[0] = (uint8_t)(fmax[variant] >> 8); be[1] = (uint8_t)fmax[variant];
    return put_tlv(out, max, off, 8, be, 2);
}

size_t k3346_tel_build_start_lists(uint8_t *out, size_t max)
{
    return k3346_tel_build_start_lists_variant(out, max, 0);
}

/* InitiateCall: 0 Type (0 = numer), 1 DestinationID (UTF-8 z NUL), 2 Service (1 = telefon) */
size_t k3346_tel_build_initiate_call(uint8_t *out, size_t max, const char *number)
{
    uint8_t type = 0, service = 1;
    size_t off;
    if (number == NULL || number[0] == 0) return 0;
    if (!(off = put_tlv(out, max, 0, 0, &type, 1))) return 0;
    if (!(off = put_tlv(out, max, off, 1, number, strlen(number) + 1))) return 0;
    return put_tlv(out, max, off, 2, &service, 1);
}

/* ---------------- parser ---------------- */

static void copy_str(char *dst, const iap2_tlv_t *t)
{
    size_t n = t->value_len, i;
    if (n >= K3346_TEL_TEXT_MAX) n = K3346_TEL_TEXT_MAX - 1;
    memcpy(dst, t->value, n);
    dst[n] = 0;
    for (i = 0; dst[i]; i++)
        if (dst[i] == '\t' || dst[i] == '\r' || dst[i] == '\n') dst[i] = ' ';
}

static uint64_t be_n(const uint8_t *p, size_t n)
{
    uint64_t v = 0;
    while (n--) v = (v << 8) | *p++;
    return v;
}

static void parse_entry(const uint8_t *p, size_t len, k3346_tel_entry *e)
{
    iap2_tlv_t t;
    size_t off = 0;
    memset(e, 0, sizeof(*e));
    while (off < len && iap2_parse_tlv(p + off, len - off, &t)) {
        switch (t.id) {
            case 0: if (t.value_len == 2) e->index = (uint16_t)be_n(t.value, 2); break;
            case 1: copy_str(e->number, &t); break;
            case 2: copy_str(e->name, &t); break;
            case 6: if (t.value_len == 1) e->type = t.value[0]; break;
            case 7: if (t.value_len == 8) e->timestamp = be_n(t.value, 8); break;
            case 9: if (t.value_len == 1) e->occurrences = t.value[0]; break;
            default: break;
        }
        off += t.len;
    }
}

int k3346_tel_parse_list_update(const uint8_t *payload, size_t len, k3346_tel_list *upd)
{
    iap2_tlv_t t;
    size_t off = 0;
    memset(upd, 0, sizeof(*upd));
    if (payload == NULL) return 0;
    while (off < len && iap2_parse_tlv(payload + off, len - off, &t)) {
        if (t.id == 1) {
            if (upd->n < K3346_TEL_LIST_MAX) parse_entry(t.value, t.value_len, &upd->e[upd->n++]);
        } else if (t.id == 2 && t.value_len == 2) {
            upd->count_set = 1;
            upd->count = (uint16_t)be_n(t.value, 2);
        }
        off += t.len;
    }
    return off == len;
}

void k3346_tel_merge(k3346_tel_list *last, const k3346_tel_list *upd)
{
    size_t i;
    if (upd->count_set) last->n = upd->count < K3346_TEL_LIST_MAX ? upd->count : K3346_TEL_LIST_MAX;
    for (i = 0; i < upd->n; i++) {
        size_t at = upd->e[i].index;
        if (at >= K3346_TEL_LIST_MAX) continue;
        last->e[at] = upd->e[i];
        if (last->n <= at) last->n = at + 1;
    }
}

/* "K3346_TEL_RECENTS v1\nn=N\n" + linie "i\ttyp\tts\tocc\tnumer\tnazwa\n" */
size_t k3346_tel_format(const k3346_tel_list *l, char *out, size_t max)
{
    size_t i, off;
    int w = snprintf(out, max, "K3346_TEL_RECENTS v1\nn=%u\n", (unsigned)l->n);
    if (w < 0 || (size_t)w >= max) return 0;
    off = (size_t)w;
    for (i = 0; i < l->n; i++) {
        const k3346_tel_entry *e = &l->e[i];
        w = snprintf(out + off, max - off, "%u\t%u\t%llu\t%u\t%s\t%s\n", (unsigned)i,
                     (unsigned)e->type, (unsigned long long)e->timestamp,
                     (unsigned)e->occurrences, e->number, e->name);
        if (w < 0 || (size_t)w >= max - off) return 0;
        off += (size_t)w;
    }
    return off;
}

/* "dial <numer>"; numer tylko z 0-9 + * # (max 32) */
int k3346_tel_parse_command(const char *text, char *number, size_t number_max)
{
    size_t n = 0;
    const char *p;
    if (text == NULL || strncmp(text, "dial ", 5) != 0) return 0;
    for (p = text + 5; *p && *p != '\n' && *p != '\r'; p++) {
        if (!((*p >= '0' && *p <= '9') || *p == '+' || *p == '*' || *p == '#')) return 0;
        if (n + 1 >= number_max || n >= 32) return 0;
        number[n++] = *p;
    }
    number[n] = 0;
    return n > 0;
}

/* ---------------- sesja ---------------- */

static pthread_mutex_t tel_lock = PTHREAD_MUTEX_INITIALIZER;
static k3346_tel_list tel_recents;
static int tel_ready;
static uint64_t tel_ready_ms, tel_last_attempt_ms;
static int tel_attempts, tel_lists_ok;
static unsigned int tel_updates, tel_partials;
static uint64_t tel_rereq_ms;   /* 0 = brak zaplanowanej ponownej prosby */
static int tel_rereqs;
static int tel_sends;                   /* wyslane 0x4170 w sesji */
static int tel_last_variant = -1;       /* wariant ostatniej prosby */
static int tel_good_variant = -1;       /* wariant, po ktorym przyszla lista z wpisami */
static char tel_last_text[FILE_MAX];

static int next_variant_locked(void)
{
    if (tel_good_variant >= 0) return tel_good_variant;
    return tel_sends < K3346_TEL_VARIANTS ? tel_sends : K3346_TEL_VARIANTS - 1;
}

int k3346_tel_next_variant(void)
{
    int v;
    pthread_mutex_lock(&tel_lock);
    v = next_variant_locked();
    pthread_mutex_unlock(&tel_lock);
    return v;
}

static void write_recents_locked(void)
{
    char text[FILE_MAX];
    char tmp[256];
    size_t len = k3346_tel_format(&tel_recents, text, sizeof(text));
    int fd;
    if (len == 0 || strcmp(text, tel_last_text) == 0) return;
    snprintf(tmp, sizeof(tmp), "%s.tmp", K3346_TEL_RECENTS_PATH);
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    if (write(fd, text, len) != (ssize_t)len) {
        close(fd);
        unlink(tmp);
        return;
    }
    close(fd);
    if (rename(tmp, K3346_TEL_RECENTS_PATH) == 0) {
        memcpy(tel_last_text, text, len + 1);
        rgi_diag_log("TEL", "recents_written n=%u", (unsigned)tel_recents.n);
        return;
    }
    /* /tmp na MMX = /dev/shmem: rename nie dziala (test 2026-10-21) -> zapis wprost;
     * Java czyta plik co 300 ms i porownuje tresc, niepelny odczyt poprawi nastepny */
    unlink(tmp);
    fd = open(K3346_TEL_RECENTS_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    if (write(fd, text, len) == (ssize_t)len) {
        memcpy(tel_last_text, text, len + 1);
        rgi_diag_log("TEL", "recents_written n=%u mode=direct", (unsigned)tel_recents.n);
    }
    close(fd);
}

void k3346_tel_session_reset(void)
{
    pthread_mutex_lock(&tel_lock);
    tel_ready = 0;
    tel_ready_ms = tel_last_attempt_ms = 0;
    tel_attempts = tel_lists_ok = 0;
    tel_rereq_ms = 0;
    tel_rereqs = 0;
    tel_sends = 0;
    tel_last_variant = tel_good_variant = -1;
    memset(&tel_recents, 0, sizeof(tel_recents));
    write_recents_locked();     /* pusta lista: bez starej z innego telefonu */
    pthread_mutex_unlock(&tel_lock);
}

void k3346_tel_observe(const uint8_t *transport, size_t n, uint64_t now_ms)
{
    const uint8_t *f;
    uint16_t flen, msgid;
    k3346_tel_list upd;

    if (transport == NULL || n < LINK_HEADER_LEN + CONTROL_HEADER_LEN) return;
    f = transport + LINK_HEADER_LEN;
    if (f[0] != 0x40 || f[1] != 0x40) return;
    flen = (uint16_t)((f[2] << 8) | f[3]);
    msgid = (uint16_t)((f[4] << 8) | f[5]);
    if (msgid != K3346_TEL_MSG_IDENT_ACCEPTED && msgid != K3346_TEL_MSG_LIST_UPDATE &&
        (msgid & 0xFF00) != 0x4100)
        return;

    pthread_mutex_lock(&tel_lock);
    if (!tel_ready) {
        /* lacze dziala: identyfikacja przyjeta albo iPhone juz wysyla telefonie */
        tel_ready = 1;
        tel_ready_ms = now_ms;
        rgi_diag_log("TEL", "ready msg=%04x", (unsigned)msgid);
    }
    if (msgid == K3346_TEL_MSG_LIST_UPDATE) {
        if (flen < CONTROL_HEADER_LEN || (size_t)flen > n - LINK_HEADER_LEN) {
            ++tel_partials;
            rgi_diag_log("TEL", "list_update_partial len=%u have=%u partials=%u",
                         (unsigned)flen, (unsigned)(n - LINK_HEADER_LEN), tel_partials);
        } else if (k3346_tel_parse_list_update(f + CONTROL_HEADER_LEN, flen - CONTROL_HEADER_LEN, &upd)) {
            k3346_tel_merge(&tel_recents, &upd);
            ++tel_updates;
            if (!tel_lists_ok) rgi_diag_log("TEL", "first_list_update");
            tel_lists_ok = 1;
            rgi_diag_log("TEL", "list_update #%u entries=%u count=%d n=%u variant=%d", tel_updates,
                         (unsigned)upd.n, upd.count_set ? (int)upd.count : -1, (unsigned)tel_recents.n,
                         tel_last_variant);
            if (tel_recents.n > 0 && tel_good_variant < 0 && tel_last_variant >= 0) {
                tel_good_variant = tel_last_variant;
                rgi_diag_log("TEL", "variant_ok=%d n=%u", tel_good_variant, (unsigned)tel_recents.n);
            }
            write_recents_locked();
            if (tel_recents.n == 0 && tel_rereq_ms == 0 && tel_rereqs < REREQ_MAX) {
                tel_rereq_ms = now_ms + (tel_rereqs == 0 ? REREQ_EMPTY_FIRST_MS : REREQ_EMPTY_NEXT_MS);
                rgi_diag_log("TEL", "lists_empty rerequest_in_ms=%u",
                             (unsigned)(tel_rereq_ms - now_ms));
            }
        } else {
            rgi_diag_log("TEL", "list_update_malformed len=%u", (unsigned)flen);
        }
    } else if (msgid == 0x4155 && tel_lists_ok && tel_rereqs < REREQ_MAX) {
        /* zmiana stanu rozmowy: historia w iPhonie mogla sie zmienic */
        tel_rereq_ms = now_ms + REREQ_AFTER_CALL_MS;
    }
    pthread_mutex_unlock(&tel_lock);
}

/* Java pisze polecenie wprost (bez rename - /tmp to /dev/shmem). Bierzemy je dopiero,
 * gdy konczy sie '\n'; niepelne zostaje do nastepnego ticku (bez polowy numeru). */
static int read_command(char *buf, size_t max)
{
    int fd = open(K3346_TEL_CMD_PATH, O_RDONLY);
    ssize_t r;
    if (fd < 0) return 0;
    r = read(fd, buf, max - 1);
    close(fd);
    if (r <= 0) return 0;
    buf[r] = 0;
    if (buf[r - 1] != '\n' && (size_t)r < max - 1) return 0;
    unlink(K3346_TEL_CMD_PATH);
    return 1;
}

void k3346_tel_tick(void *ctx, uint64_t now_ms, k3346_tel_send_fn send)
{
    uint8_t payload[256];
    char cmd[96], number[40];
    size_t len;
    int do_lists = 0, rc, variant = 0;

    if (ctx == NULL || send == NULL) return;

    pthread_mutex_lock(&tel_lock);
    if (tel_ready && !tel_lists_ok && tel_attempts < LIST_MAX_ATTEMPTS &&
        now_ms >= tel_ready_ms + LIST_FIRST_DELAY_MS &&
        (tel_attempts == 0 || now_ms >= tel_last_attempt_ms + LIST_RETRY_MS)) {
        ++tel_attempts;
        tel_last_attempt_ms = now_ms;
        do_lists = 1;
    } else if (tel_ready && tel_lists_ok && tel_rereq_ms != 0 && now_ms >= tel_rereq_ms) {
        tel_rereq_ms = 0;
        ++tel_rereqs;
        do_lists = 2;
    }
    if (do_lists) {
        /* wariant ustalony przed wysylka: odpowiedz moze przyjsc w trakcie send() */
        variant = next_variant_locked();
        tel_last_variant = variant;
        ++tel_sends;
    }
    pthread_mutex_unlock(&tel_lock);

    if (do_lists) {
        len = k3346_tel_build_start_lists_variant(payload, sizeof(payload), variant);
        rc = len ? send(ctx, K3346_TEL_MSG_START_LISTS, payload, len) : -1;
        if (do_lists == 1)
            rgi_diag_log("TEL", "start_list_updates attempt=%d variant=%d len=%u rc=%d", tel_attempts, variant, (unsigned)len, rc);
        else
            rgi_diag_log("TEL", "start_list_updates rerequest=%d variant=%d len=%u rc=%d", tel_rereqs, variant, (unsigned)len, rc);
    }

    if (read_command(cmd, sizeof(cmd))) {
        int ready;
        pthread_mutex_lock(&tel_lock);
        ready = tel_ready;
        pthread_mutex_unlock(&tel_lock);
        if (!ready) {
            rgi_diag_log("TEL", "command_dropped reason=not_ready");
        } else if (k3346_tel_parse_command(cmd, number, sizeof(number))) {
            len = k3346_tel_build_initiate_call(payload, sizeof(payload), number);
            rc = len ? send(ctx, K3346_TEL_MSG_INITIATE_CALL, payload, len) : -1;
            rgi_diag_log("TEL", "initiate_call digits=%u rc=%d", (unsigned)strlen(number), rc);
        } else {
            rgi_diag_log("TEL", "command_rejected");
        }
    }
}
