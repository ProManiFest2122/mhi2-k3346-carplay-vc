/* Identify na atrapach: PPID profilu Map 30 FPS i to, ze nie moze zepsuc Identify. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../native/ident_hook.c"

/* atrapy tego, co w aucie daje status.c */
static int status_success, status_maps_calls, status_maps_rc = 99;
void k2161_status_ident_attempt(void *ctx) { (void)ctx; }
void k2161_status_ident_success(void *ctx) { (void)ctx; status_success++; }
void k2161_status_ident_hook_ready(void) { }
void k2161_status_ident_maps_profile(void *ctx, int rc)
{
    (void)ctx;
    status_maps_calls++;
    status_maps_rc = rc;
}

#define MAX_CALLS 8
static struct { uint16_t id; unsigned char data[128]; size_t len; } sent[MAX_CALLS];
static int sent_n, fail_rgi, fail_ppid, builder_rc;

static int fake_addparam(void *ctx, void *pkt, uint16_t id, const void *data, size_t len)
{
    (void)ctx; (void)pkt;
    assert(sent_n < MAX_CALLS && len <= sizeof(sent[0].data));
    sent[sent_n].id = id;
    memcpy(sent[sent_n].data, data, len);
    sent[sent_n].len = len;
    sent_n++;
    if (id == 0x001e && fail_rgi) return -1;
    if (id == 0x0022 && fail_ppid) return -1;
    return 0;
}

static int fake_builder(void *ctx, void *pkt)
{
    (void)ctx; (void)pkt;
    return builder_rc;
}

static void reset(void)
{
    sent_n = 0; fail_rgi = 0; fail_ppid = 0; builder_rc = 0;
    status_success = 0; status_maps_calls = 0; status_maps_rc = 99;
    packet_addparam_blob = fake_addparam;
    original_gps_builder = fake_builder;
}

int main(void)
{
    /* zwykly Identify: RGI, potem PPID, dokladne bajty */
    reset();
    assert(k2161_wrapped_gps_builder(NULL, NULL) == 1);
    assert(sent_n == 2);
    assert(sent[0].id == 0x001e);
    assert(sent[0].len == sizeof(rgi_component_payload));
    assert(memcmp(sent[0].data, rgi_component_payload, sent[0].len) == 0);
    assert(sent[1].id == 34);
    assert(sent[1].len == 17);
    assert(memcmp(sent[1].data, "bf889dfdaea54096\0", 17) == 0);
    assert(status_success == 1);
    assert(status_maps_calls == 1 && status_maps_rc == 0);

    /* PPID odrzucony: Identify przechodzi bez zmian, blad jest w logu */
    reset();
    fail_ppid = 1;
    assert(k2161_wrapped_gps_builder(NULL, NULL) == 1);
    assert(sent_n == 2);
    assert(status_success == 1);
    assert(status_maps_calls == 1 && status_maps_rc == -1);

    /* blad RGI: jak dotad, bez proby dopisania PPID */
    reset();
    fail_rgi = 1;
    assert(k2161_wrapped_gps_builder(NULL, NULL) == -1);
    assert(sent_n == 1 && sent[0].id == 0x001e);
    assert(status_maps_calls == 0);

    /* fabryczny builder zawiodl: nie dopisujemy niczego */
    reset();
    builder_rc = -1;
    assert(k2161_wrapped_gps_builder(NULL, NULL) == -1);
    assert(sent_n == 0);

    /* bez fabrycznego buildera: jak dotad */
    reset();
    original_gps_builder = NULL;
    assert(k2161_wrapped_gps_builder(NULL, NULL) == -1);
    assert(sent_n == 0);

    /* builder zwraca wynik dodatni: nadal 1 i oba parametry */
    reset();
    builder_rc = 5;
    assert(k2161_wrapped_gps_builder(NULL, NULL) == 1);
    assert(sent_n == 2);

    puts("WYNIK_IDENT_MAPS: PASS");
    return 0;
}
