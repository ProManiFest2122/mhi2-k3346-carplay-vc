/* K3346 port (MHI2_ER_AUG22_K3346 / MU1438): stock ipod-drvr-iap2.so
 * SHA-256 0ac13f43d3fb259eee328c272fa1211bb558b6e2cd97c6e38ef38e4e71469d51.
 * Offsets re-derived from the driver's own .symtab/.dynsym, relocations and
 * table contents (tools/derive_k3346_iap2_offsets.py); K2161 values in comments.
 * Runtime checks below remain fail-closed. */
/* K2161 writable Identification builder-table hook. */
#include <stdint.h>
#include <stddef.h>
#include "ident_hook.h"
#include "status.h"

typedef int (*ident_builder_fn)(void *ctx, void *pkt);
typedef int (*packet_addparam_blob_fn)(void *ctx, void *pkt, uint16_t parameter_id,
                                      const void *data, size_t data_length);
enum {
    K2161_IDENT_ID_OFFSET = 0x000a9ba0, /* K2161: 0x000a7d80 */
    K2161_IDENT_BUILDER_OFFSET = 0x000a9ba4, /* K2161: 0x000a7d84 */
    K2161_GPS_BUILDER_OFFSET = 0x0000cb70, /* ident_info_gpscomp; K2161: 0x0000a11c */
    K2161_ADDPARAM_BLOB_OFFSET = 0x0001e2c4 /* packet_addparam_blob; K2161: 0x0001d0d8 */
};
static ident_builder_fn original_gps_builder;
static ident_builder_fn *gps_builder_slot;
static packet_addparam_blob_fn packet_addparam_blob;
static void *installed_driver_base;
static volatile int hook_installed;
static const uint8_t rgi_component_payload[82] = {
    0x00,0x06,0x00,0x00,0x00,0x10,
    0x00,0x22,0x00,0x01,
    0x52,0x6f,0x75,0x74,0x65,0x47,0x75,0x69,0x64,0x61,0x6e,0x63,
    0x65,0x44,0x69,0x73,0x70,0x6c,0x61,0x79,0x43,0x6f,0x6d,0x70,
    0x6f,0x6e,0x65,0x6e,0x74,0x00,
    0x00,0x06,0x00,0x02,0x01,0x00, 0x00,0x06,0x00,0x03,0x01,0x00,
    0x00,0x06,0x00,0x04,0x01,0x00, 0x00,0x06,0x00,0x05,0x01,0x00,
    0x00,0x06,0x00,0x06,0x00,0x06, 0x00,0x06,0x00,0x07,0x01,0x00,
    0x00,0x06,0x00,0x08,0x00,0x06
};

/*
 * AltScreen note:
 *
 * The earlier dev experiment also synthesized IdentificationInformation
 * parameter 24 (WirelessCarPlayTransportComponent) with ThemeAssets support.
 * That is intentionally NOT emitted here anymore.  The native type-111 work
 * now follows the observed working architecture: keep iAP2 identification
 * stock/observed and negotiate the alternate display in the AirPlay layer.
 *
 * RouteGuidanceDisplayComponent (parameter 30) remains required for the
 * already-working semantic RGI feature and is unchanged.
 */

static int append_rgi_component(void *ctx, void *pkt) {
    int rc;
    k2161_status_ident_attempt(ctx);
    if (!packet_addparam_blob) return -1;
    rc = packet_addparam_blob(ctx, pkt, 0x001e, rgi_component_payload,
                              sizeof(rgi_component_payload));
    if (rc == 0) k2161_status_ident_success(ctx);
    return rc;
}

int k2161_wrapped_gps_builder(void *ctx, void *pkt) {
    int rc;
    if (!original_gps_builder) return -1;
    rc = original_gps_builder(ctx, pkt);
    if (rc == -1) return -1;
    if (append_rgi_component(ctx, pkt) != 0) return -1;
    return 1;
}

int k2161_ident_try_install_base(void *driver_base) {
    uint8_t *base = (uint8_t *)driver_base;
    ident_builder_fn expected, current;
    packet_addparam_blob_fn helper;

    if (!base) return 0;
    if (hook_installed && installed_driver_base == base) return 1;

    if (*(volatile uint32_t *)(base + K2161_IDENT_ID_OFFSET) != 0x0016)
        return -1;

    gps_builder_slot =
        (ident_builder_fn *)(base + K2161_IDENT_BUILDER_OFFSET);

    expected =
        (ident_builder_fn)(base + K2161_GPS_BUILDER_OFFSET);

    current = *(volatile ident_builder_fn *)gps_builder_slot;

    if (current == k2161_wrapped_gps_builder) {
        hook_installed = 1;
        installed_driver_base = base;
        return 1;
    }

    if (current != expected)
        return -1;

    /*
     * driver_base has already been independently validated by the
     * dual-export resolver before this function is called.
     */
    helper = (packet_addparam_blob_fn)
        (base + K2161_ADDPARAM_BLOB_OFFSET);

    packet_addparam_blob = helper;
    original_gps_builder = expected;
    installed_driver_base = base;

    /*
     * K2161 ARMv7: naturally aligned 32-bit function-pointer store.
     * Validate immediately before and after replacement.
     */
    if (*(volatile ident_builder_fn *)gps_builder_slot != expected)
        return -1;

    *(volatile ident_builder_fn *)gps_builder_slot =
        k2161_wrapped_gps_builder;

    if (*(volatile ident_builder_fn *)gps_builder_slot !=
            k2161_wrapped_gps_builder) {
        original_gps_builder = NULL;
        packet_addparam_blob = NULL;
        installed_driver_base = NULL;
        gps_builder_slot = NULL;
        return -1;
    }

    hook_installed = 1;
    k2161_status_ident_hook_ready();
    return 1;
}

int k2161_ident_restore(void) {
    if (!hook_installed || !gps_builder_slot || !original_gps_builder) return 0;
    if (*(volatile ident_builder_fn *)gps_builder_slot != k2161_wrapped_gps_builder)
        return -1;
    *(volatile ident_builder_fn *)gps_builder_slot = original_gps_builder;
    hook_installed = 0;
    return 1;
}
