/*
 * K3346 Etap 6: ostatnie polaczenia CarPlay (iAP2 0x4170/0x4171) i dzwonienie
 * z listy (0x415A) w mm-ipod. Formaty wiadomosci i parser wg luka-dev
 * hook/telephony/tel_hook.c (GPL-3.0; bajty 0x4170 = zweryfikowane w aucie
 * zadanie xPaiiN). Tylko w bibliotece mm-ipod (makro K3346_TEL_LISTS).
 */
#ifndef K3346_TEL_H
#define K3346_TEL_H

#include <stddef.h>
#include <stdint.h>

#define K3346_TEL_LIST_MAX 20
#define K3346_TEL_TEXT_MAX 64

#define K3346_TEL_MSG_IDENT_START    0x1D00
#define K3346_TEL_MSG_IDENT_ACCEPTED 0x1D02
#define K3346_TEL_MSG_INITIATE_CALL  0x415A
#define K3346_TEL_MSG_START_LISTS    0x4170
#define K3346_TEL_MSG_LIST_UPDATE    0x4171

typedef struct {
    uint16_t index;
    char number[K3346_TEL_TEXT_MAX];
    char name[K3346_TEL_TEXT_MAX];
    uint8_t type;           /* 1 odebrane, 2 wybrane, 3 nieodebrane, 0 inne */
    uint64_t timestamp;     /* s od 1970 */
    uint8_t occurrences;
} k3346_tel_entry;

typedef struct {
    size_t n;
    int count_set;
    uint16_t count;
    k3346_tel_entry e[K3346_TEL_LIST_MAX];
} k3346_tel_list;

/* Wysylka wiadomosci przez stock sterownik (send_hook.c); 0 = OK. */
typedef int (*k3346_tel_send_fn)(void *ctx, uint16_t msgid, const uint8_t *payload, size_t len);

/* ---- czyste funkcje (testy na PC) ---- */
size_t k3346_tel_build_start_lists(uint8_t *out, size_t max);
size_t k3346_tel_build_initiate_call(uint8_t *out, size_t max, const char *number);
int k3346_tel_parse_list_update(const uint8_t *payload, size_t len, k3346_tel_list *recents_upd);
void k3346_tel_merge(k3346_tel_list *last, const k3346_tel_list *upd);
size_t k3346_tel_format(const k3346_tel_list *l, char *out, size_t max);
int k3346_tel_parse_command(const char *text, char *number, size_t number_max);

/* ---- sesja (hook) ---- */
void k3346_tel_session_reset(void);
/* surowe dane lacza (naglowek 9 B + ramka kontrolna) z observer.c */
void k3346_tel_observe(const uint8_t *transport, size_t n, uint64_t now_ms);
/* watek sterownika po stock 0x2700: 0x4170 raz na sesje, polecenia z Javy */
void k3346_tel_tick(void *ctx, uint64_t now_ms, k3346_tel_send_fn send);

#endif
