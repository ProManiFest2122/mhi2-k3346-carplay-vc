/* K3346 Etap 6: test modulu telefonii na PC (gcc, bez sterownika).
 * Ramki 0x4171 i bajty 0x4170: fixtures z luka-dev tests/tel_hook_test.c
 * (GPL-3.0; numery i nazwy zastapione placeholderami tej samej dlugosci). */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "telephony/k3346_tel.h"

void rgi_diag_log(const char *layer, const char *fmt, ...) { (void)layer; (void)fmt; }

/* xPaiiN send_start_list_updates_probe (recents max 25, favorites max 0) - zweryfikowane w aucie */
static const uint8_t verified_start[] =
    "\x00\x2c\x00\x01"
    "\x00\x04\x00\x00\x00\x04\x00\x01\x00\x04\x00\x02\x00\x04\x00\x03\x00\x04\x00\x04"
    "\x00\x04\x00\x05\x00\x04\x00\x06\x00\x04\x00\x07\x00\x04\x00\x08\x00\x04\x00\x09"
    "\x00\x06\x00\x03\x00\x19"
    "\x00\x05\x00\x04\x01"
    "\x00\x1c\x00\x06"
    "\x00\x04\x00\x00\x00\x04\x00\x01\x00\x04\x00\x02\x00\x04\x00\x03\x00\x04\x00\x04\x00\x04\x00\x05"
    "\x00\x06\x00\x08\x00\x00";

static const uint8_t cap_avail[] =
    "\x40\x40\x00\x11\x41\x71\x00\x05\x00\x00\x01\x00\x06\x00\x02\x00"
    "\x03";
static const uint8_t cap_entries[] =
    "\x40\x40\x01\x4b\x41\x71\x00\x7b\x00\x01\x00\x06\x00\x00\x00\x00"
    "\x00\x13\x00\x01\x2b\x38\x36\x31\x33\x38\x30\x30\x30\x30\x30\x30"
    "\x30\x31\x00\x00\x0f\x00\x02\x41\x6c\x69\x63\x65\x20\x54\x65\x73"
    "\x74\x00\x00\x0b\x00\x03\xe6\x89\x8b\xe6\x9c\xba\x00\x00\x29\x00"
    "\x04\x37\x44\x31\x39\x31\x37\x30\x41\x2d\x39\x35\x39\x43\x2d\x34"
    "\x32\x30\x34\x2d\x41\x43\x32\x41\x2d\x36\x42\x45\x33\x38\x36\x34"
    "\x44\x43\x43\x42\x43\x00\x00\x05\x00\x05\x03\x00\x05\x00\x06\x02"
    "\x00\x0c\x00\x07\x00\x00\x00\x00\x69\xec\x67\x54\x00\x05\x00\x09"
    "\x02\x00\x65\x00\x01\x00\x06\x00\x00\x00\x01\x00\x13\x00\x01\x2b"
    "\x38\x36\x31\x33\x38\x30\x30\x30\x30\x30\x30\x30\x32\x00\x00\x16"
    "\x00\x02\x2b\x38\x36\x20\x31\x33\x38\x20\x30\x30\x30\x30\x20\x30"
    "\x30\x30\x32\x00\x00\x12\x00\x03\xe6\xb5\x99\xe6\xb1\x9f\x20\xe6"
    "\x9d\xad\xe5\xb7\x9e\x00\x00\x05\x00\x04\x00\x00\x05\x00\x05\x03"
    "\x00\x05\x00\x06\x02\x00\x0c\x00\x07\x00\x00\x00\x00\x69\x45\x7b"
    "\x8f\x00\x05\x00\x09\x02\x00\x65\x00\x01\x00\x06\x00\x00\x00\x02"
    "\x00\x13\x00\x01\x2b\x38\x36\x31\x33\x38\x30\x30\x30\x30\x30\x30"
    "\x30\x32\x00\x00\x16\x00\x02\x2b\x38\x36\x20\x31\x33\x38\x20\x30"
    "\x30\x30\x30\x20\x30\x30\x30\x32\x00\x00\x12\x00\x03\xe6\xb5\x99"
    "\xe6\xb1\x9f\x20\xe6\x9d\xad\xe5\xb7\x9e\x00\x00\x05\x00\x04\x00"
    "\x00\x05\x00\x05\x00\x00\x05\x00\x06\x02\x00\x0c\x00\x07\x00\x00"
    "\x00\x00\x69\x45\x75\x38\x00\x05\x00\x09\x07";
static const uint8_t cap_fav[] =
    "\x40\x40\x00\x11\x41\x71\x00\x05\x00\x05\x01\x00\x06\x00\x07\x00"
    "\x00";

static int fails;
#define CHECK(c, name) do { if (c) printf("  OK   %s\n", name); else { printf("  FAIL %s\n", name); fails++; } } while (0)

/* pakiet lacza: 9 B naglowka + ramka kontrolna */
static size_t link_packet(uint8_t *out, const uint8_t *frame, size_t n)
{
    memset(out, 0xAA, 9);
    out[0] = 0xff; out[1] = 0x5a;
    memcpy(out + 9, frame, n);
    return n + 9;
}

static uint16_t sent_msg[16];
static uint8_t sent_payload[16][256];
static size_t sent_len[16];
static int sent_count;
static int fake_send(void *ctx, uint16_t msgid, const uint8_t *p, size_t n)
{
    (void)ctx;
    if (sent_count < 16) {
        sent_msg[sent_count] = msgid;
        sent_len[sent_count] = n < 256 ? n : 256;
        memcpy(sent_payload[sent_count], p, sent_len[sent_count]);
    }
    sent_count++;
    return 0;
}

static char *read_file(const char *path)
{
    static char buf[8192];
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return NULL;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    return buf;
}

int main(void)
{
    uint8_t buf[512], pkt[600];
    size_t n;
    k3346_tel_list upd, last;
    char text[4096], num[40];
    int ctx = 1;
    char *f;
    unsigned int i, seed = 12345;

    n = k3346_tel_build_start_lists(buf, sizeof(buf));
    CHECK(n == sizeof(verified_start) - 1 && memcmp(buf, verified_start, n) == 0,
          "0x4170 bajt w bajt jak zweryfikowane w aucie zadanie xPaiiN");
    CHECK(k3346_tel_build_start_lists(buf, 60) == 0, "0x4170: za maly bufor -> 0");

    /* Etap 6c: warianty 0x4170 */
    {
        uint8_t v[512], want[128];
        size_t vn;
        memcpy(want, verified_start, sizeof(verified_start) - 1);
        vn = k3346_tel_build_start_lists_variant(v, sizeof(v), 0);
        CHECK(vn == sizeof(verified_start) - 1 && memcmp(v, verified_start, vn) == 0, "wariant 0 = xPaiiN");
        /* Luka tel_build_start_list_updates(…, 20, 20): max 0x0014 w obu miejscach */
        want[49] = 0x14; want[sizeof(verified_start) - 2] = 0x14;
        vn = k3346_tel_build_start_lists_variant(v, sizeof(v), 1);
        CHECK(vn == sizeof(verified_start) - 1 && memcmp(v, want, vn) == 0, "wariant 1 = Luka 20/20");
        want[49] = 0x00; want[sizeof(verified_start) - 2] = 0x00;
        vn = k3346_tel_build_start_lists_variant(v, sizeof(v), 2);
        CHECK(vn == sizeof(verified_start) - 1 && memcmp(v, want, vn) == 0, "wariant 2 = max 0/0");
        want[49] = 0x0c; want[54] = 0x00; want[sizeof(verified_start) - 2] = 0x0c;
        vn = k3346_tel_build_start_lists_variant(v, sizeof(v), 3);
        CHECK(vn == sizeof(verified_start) - 1 && memcmp(v, want, vn) == 0, "wariant 3 = max 12/12, combine 0");
        vn = k3346_tel_build_start_lists_variant(v, sizeof(v), 4);
        CHECK(vn == 44 + 28 && memcmp(v, verified_start, 44) == 0 && memcmp(v + 44, verified_start + 55, 28) == 0,
              "wariant 4 = tylko grupy 1 i 6");
        CHECK(k3346_tel_build_start_lists_variant(v, sizeof(v), 5) == 0 &&
              k3346_tel_build_start_lists_variant(v, sizeof(v), -1) == 0, "wariant spoza zakresu -> 0");
    }

    n = k3346_tel_build_initiate_call(buf, sizeof(buf), "+00123");
    CHECK(n == 5 + 4 + 7 + 5 && memcmp(buf, "\x00\x05\x00\x00\x00", 5) == 0 &&
          memcmp(buf + 5, "\x00\x0b\x00\x01+00123\x00", 11) == 0 &&
          memcmp(buf + 16, "\x00\x05\x00\x02\x01", 5) == 0,
          "0x415A: Type 0, DestinationID z NUL, Service 1");
    CHECK(k3346_tel_build_initiate_call(buf, sizeof(buf), "") == 0, "0x415A: pusty numer -> 0");

    memset(&last, 0, sizeof(last));
    CHECK(k3346_tel_parse_list_update(cap_avail + 6, sizeof(cap_avail) - 1 - 6, &upd) && upd.count_set && upd.count == 3,
          "ramka 1: dostepnosc + count 3");
    k3346_tel_merge(&last, &upd);
    CHECK(k3346_tel_parse_list_update(cap_entries + 6, sizeof(cap_entries) - 1 - 6, &upd) && upd.n == 3,
          "ramka 2: 3 wpisy");
    k3346_tel_merge(&last, &upd);
    CHECK(last.n == 3 && strcmp(last.e[0].number, "+8613800000001") == 0 && strcmp(last.e[0].name, "Alice Test") == 0 &&
          last.e[0].type == 2 && last.e[0].occurrences == 2 && last.e[0].timestamp == 0x69ec6754ULL,
          "wpis 0: numer, nazwa, typ 2 (wybrane), wystapienia 2, czas");
    CHECK(strcmp(last.e[1].name, "+86 138 0000 0002") == 0 && last.e[2].occurrences == 7, "wpisy 1-2: nazwa = sformatowany numer, occ 7");
    CHECK(k3346_tel_parse_list_update(cap_fav + 6, sizeof(cap_fav) - 1 - 6, &upd) && upd.n == 0 && !upd.count_set,
          "ramka 3: ulubione ignorowane (nie ruszaja ostatnich)");
    k3346_tel_merge(&last, &upd);
    CHECK(last.n == 3, "po ulubionych lista ostatnich bez zmian");

    n = k3346_tel_format(&last, text, sizeof(text));
    {
        static const char want[] = "K3346_TEL_RECENTS v1\nn=3\n0\t2\t1777100628\t2\t+8613800000001\tAlice Test\n";
        CHECK(n > 0 && strncmp(text, want, sizeof(want) - 1) == 0, "format pliku dla Javy");
    }

    CHECK(k3346_tel_parse_command("dial +48123*#\n", num, sizeof(num)) && strcmp(num, "+48123*#") == 0, "polecenie dial");
    CHECK(!k3346_tel_parse_command("dial 12;rm", num, sizeof(num)) && !k3346_tel_parse_command("call 1", num, sizeof(num)) &&
          !k3346_tel_parse_command("dial ", num, sizeof(num)), "polecenie: odrzuca smieci");

    /* sesja: gotowosc po 0x1D02, 0x4170 po 2 s, ponowienia, koniec po pierwszym 0x4171 */
    unlink("/tmp/k3346-tel-test-recents.txt");
    k3346_tel_session_reset();
    f = read_file("/tmp/k3346-tel-test-recents.txt");
    CHECK(f && strstr(f, "n=0\n"), "reset sesji -> pusta lista w pliku");
    k3346_tel_tick(&ctx, 1000, fake_send);
    CHECK(sent_count == 0, "bez polaczenia nic nie wysyla");
    n = link_packet(pkt, (const uint8_t *)"\x40\x40\x00\x06\x1d\x02", 6);
    k3346_tel_observe(pkt, n, 10000);
    k3346_tel_tick(&ctx, 11000, fake_send);
    CHECK(sent_count == 0, "przed 2 s od identyfikacji nic");
    k3346_tel_tick(&ctx, 12100, fake_send);
    CHECK(sent_count == 1 && sent_msg[0] == 0x4170 && sent_len[0] == sizeof(verified_start) - 1, "0x4170 po 2 s");
    k3346_tel_tick(&ctx, 13000, fake_send);
    CHECK(sent_count == 1, "bez ponowienia przed 15 s");
    k3346_tel_tick(&ctx, 27200, fake_send);
    CHECK(sent_count == 2 && sent_msg[1] == 0x4170, "ponowienie po 15 s bez odpowiedzi");
    n = link_packet(pkt, cap_avail, sizeof(cap_avail) - 1);
    k3346_tel_observe(pkt, n, 28000);
    n = link_packet(pkt, cap_entries, sizeof(cap_entries) - 1);
    k3346_tel_observe(pkt, n, 28100);
    k3346_tel_tick(&ctx, 60000, fake_send);
    CHECK(sent_count == 2, "po pierwszym 0x4171 koniec ponowien");
    f = read_file("/tmp/k3346-tel-test-recents.txt");
    CHECK(f && strstr(f, "n=3\n") && strstr(f, "Alice Test"), "plik listy zapisany po 0x4171");
    n = link_packet(pkt, cap_entries, 100);   /* ucieta ramka (len 0x14b, jest 91) */
    k3346_tel_observe(pkt, n, 28200);
    f = read_file("/tmp/k3346-tel-test-recents.txt");
    CHECK(f && strstr(f, "n=3\n"), "ucieta ramka ignorowana");

    /* pusta lista (jak w aucie 2026-10-21) -> ponowne prosby 20 s / 60 s, max 4 */
    {
        static const uint8_t empty_recents[] = "\x40\x40\x00\x11\x41\x71\x00\x05\x00\x00\x01\x00\x06\x00\x02\x00\x00";
        int before;
        k3346_tel_session_reset();
        n = link_packet(pkt, (const uint8_t *)"\x40\x40\x00\x06\x1d\x02", 6);
        k3346_tel_observe(pkt, n, 100000);
        k3346_tel_tick(&ctx, 102100, fake_send);           /* 0x4170 */
        before = sent_count;
        n = link_packet(pkt, empty_recents, sizeof(empty_recents) - 1);
        k3346_tel_observe(pkt, n, 102200);
        k3346_tel_tick(&ctx, 110000, fake_send);
        CHECK(sent_count == before, "pusta lista: brak natychmiastowej ponownej prosby");
        k3346_tel_tick(&ctx, 122300, fake_send);
        CHECK(sent_count == before + 1 && sent_msg[(before) % 16] == 0x4170, "pusta lista: ponowna prosba po 20 s");
        k3346_tel_observe(pkt, n, 122400);                  /* znow pusta */
        k3346_tel_tick(&ctx, 150000, fake_send);
        CHECK(sent_count == before + 1, "druga pusta: czeka 60 s");
        k3346_tel_tick(&ctx, 182500, fake_send);
        CHECK(sent_count == before + 2, "druga pusta: ponowna prosba po 60 s");
        n = link_packet(pkt, (const uint8_t *)"\x40\x40\x00\x0a\x41\x55\x00\x04\x00\x00", 10);
        k3346_tel_observe(pkt, n, 190000);                  /* zmiana stanu rozmowy */
        k3346_tel_tick(&ctx, 194000, fake_send);
        CHECK(sent_count == before + 2, "po zmianie rozmowy czeka 5 s");
        k3346_tel_tick(&ctx, 195100, fake_send);
        CHECK(sent_count == before + 3, "po zmianie rozmowy ponowna prosba");
        k3346_tel_observe(pkt, n, 196000);
        k3346_tel_tick(&ctx, 201200, fake_send);
        CHECK(sent_count == before + 4, "czwarta ponowna prosba");
        k3346_tel_observe(pkt, n, 202000);
        k3346_tel_tick(&ctx, 400000, fake_send);
        CHECK(sent_count == before + 4, "limit 4 ponownych prosb na sesje");
        {
            uint8_t v[512];
            int k, okv = 1;
            for (k = 0; k < 5; k++) {
                size_t vn = k3346_tel_build_start_lists_variant(v, sizeof(v), k);
                int idx = before - 1 + k;
                if (idx >= 16 || sent_len[idx] != vn || memcmp(sent_payload[idx], v, vn) != 0) okv = 0;
            }
            CHECK(okv, "pusta lista: kolejne prosby = warianty 0,1,2,3,4");
        }
    }

    /* lista z wpisami po wariancie 1 -> kolejne prosby (np. po rozmowie) tylko wariantem 1 */
    {
        static const uint8_t empty_recents[] = "\x40\x40\x00\x11\x41\x71\x00\x05\x00\x00\x01\x00\x06\x00\x02\x00\x00";
        uint8_t v[512];
        size_t vn;
        int before;
        sent_count = 0;
        k3346_tel_session_reset();
        CHECK(k3346_tel_next_variant() == 0, "nowa sesja: zaczyna od wariantu 0");
        n = link_packet(pkt, (const uint8_t *)"\x40\x40\x00\x06\x1d\x02", 6);
        k3346_tel_observe(pkt, n, 600000);
        k3346_tel_tick(&ctx, 602100, fake_send);                       /* v0 */
        n = link_packet(pkt, empty_recents, sizeof(empty_recents) - 1);
        k3346_tel_observe(pkt, n, 602200);
        k3346_tel_tick(&ctx, 622300, fake_send);                       /* v1 */
        n = link_packet(pkt, cap_avail, sizeof(cap_avail) - 1);
        k3346_tel_observe(pkt, n, 622400);
        n = link_packet(pkt, cap_entries, sizeof(cap_entries) - 1);
        k3346_tel_observe(pkt, n, 622500);
        CHECK(sent_count == 2 && k3346_tel_next_variant() == 1, "wpisy po wariancie 1 -> wariant 1 zapamietany");
        before = sent_count;
        n = link_packet(pkt, (const uint8_t *)"\x40\x40\x00\x0a\x41\x55\x00\x04\x00\x00", 10);
        k3346_tel_observe(pkt, n, 630000);
        k3346_tel_tick(&ctx, 635100, fake_send);
        vn = k3346_tel_build_start_lists_variant(v, sizeof(v), 1);
        CHECK(sent_count == before + 1 && sent_len[before] == vn && memcmp(sent_payload[before], v, vn) == 0,
              "po rozmowie prosba wariantem 1 (nie kolejnym)");
        k3346_tel_session_reset();
        CHECK(k3346_tel_next_variant() == 0, "reset sesji kasuje zapamietany wariant");
        n = link_packet(pkt, (const uint8_t *)"\x40\x40\x00\x06\x1d\x02", 6);
        k3346_tel_observe(pkt, n, 640000);          /* lacze gotowe dla testu polecen */
    }

    /* polecenie z Javy -> 0x415A */
    f = NULL;
    {
        FILE *c = fopen("/tmp/k3346-tel-test-cmd", "w");
        int before = sent_count;
        fputs("dial +001", c);      /* Java w trakcie zapisu: bez '\n' */
        fclose(c);
        k3346_tel_tick(&ctx, 499000, fake_send);
        CHECK(sent_count == before && access("/tmp/k3346-tel-test-cmd", F_OK) == 0,
              "niepelne polecenie (bez nowej linii) czeka - bez polowy numeru");
        c = fopen("/tmp/k3346-tel-test-cmd", "w");
        fputs("dial +00123\n", c);
        fclose(c);
        k3346_tel_tick(&ctx, 500000, fake_send);
        CHECK(sent_count == before + 1 && before < 16 && sent_msg[before] == 0x415A &&
              memcmp(sent_payload[before] + 9, "+00123", 6) == 0, "dial z pliku -> 0x415A");
    }
    CHECK(access("/tmp/k3346-tel-test-cmd", F_OK) != 0, "plik polecenia skasowany");

    /* zniekształcone wejscie: 200000 losowych pakietow bez awarii */
    for (i = 0; i < 200000; i++) {
        size_t len = 9 + (seed % 200), j;
        for (j = 0; j < len; j++) { seed = seed * 1103515245u + 12345u; pkt[j] = (uint8_t)(seed >> 16); }
        pkt[9] = 0x40; pkt[10] = 0x40; pkt[13] = 0x41; pkt[14] = (i & 1) ? 0x71 : 0x55;
        k3346_tel_observe(pkt, len, 70000 + i);
    }
    CHECK(1, "200000 losowych ramek 0x41xx bez awarii");

    printf("WYNIK_TEL: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
