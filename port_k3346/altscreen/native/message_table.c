/* K3346 port (MHI2_ER_AUG22_K3346 / MU1438): stock ipod-drvr-iap2.so
 * SHA-256 0ac13f43d3fb259eee328c272fa1211bb558b6e2cd97c6e38ef38e4e71469d51.
 * Offsets re-derived from the driver's own .symtab/.dynsym, relocations and
 * table contents (tools/derive_k3346_iap2_offsets.py); K2161 values in comments.
 * Runtime checks below remain fail-closed. */
#include <stddef.h>
#include <stdint.h>

#include "message_table.h"
#include "diagnostics.h"

enum {
    K2161_CTRL_MSG_TABLE_OFFSET = 0x000ace80, /* iap2_ctrl_msg_table; K2161: 0x000ab050 */

    K2161_RGI_GROUP = 0x52,
    K2161_RGI_GROUP_COUNT = 5,

    /*
     * Exact stock K2161 reference entries used to verify that the
     * validated driver base, table offset and structure layout all match.
     */
    K2161_GROUP50_NAME_OFFSET     = 0x0002518c, /* 'Now Playing'; K2161: 0x000237dc */
    K2161_GROUP50_MESSAGES_OFFSET = 0x000ae30c, /* K2161: 0x000ac4dc */
    K2161_GROUP68_NAME_OFFSET     = 0x000251bc, /* 'Human Interface Device'; K2161: 0x000237e8 */
    K2161_GROUP68_MESSAGES_OFFSET = 0x000ae36c, /* K2161: 0x000ac50c */

    K2161_NP_START_NAME_OFFSET    = 0x00025554, /* 'StartNowPlayingUpdates'; K2161: 0x00023b80 */
    K2161_NP_START_PARAMS_OFFSET  = 0x00028918, /* params_nowplay_start; K2161: 0x00026bd8 */
    K2161_NP_UPDATE_NAME_OFFSET   = 0x0002556c, /* 'NowPlayingUpdate'; K2161: 0x00023b98 */
    K2161_NP_UPDATE_PARAMS_OFFSET = 0x00028938 /* params_nowplay_upd; K2161: 0x00026bf8 */
};

struct k2161_msg_info {
    const char *name;
    uint32_t flags;
    const void *params;
};

struct k2161_msg_group {
    const char *name;
    const struct k2161_msg_info *messages;
    uint32_t count;
};

/*
 * Stock direction behavior confirmed from the exact K2161 table:
 *
 *   flags 0x00000001 = accessory -> device message
 *   flags 0x00000002 = device -> accessory message
 *
 * Example from stock group 0x50:
 *   StartNowPlayingUpdates = 1
 *   NowPlayingUpdate       = 2
 *
 * RGI 5201/5202/5204 are device -> accessory.
 */
static const struct k2161_msg_info rgi_messages[K2161_RGI_GROUP_COUNT] = {
    { NULL, 0U, NULL },
    { "RouteGuidanceUpdate",         0x00000002U, NULL }, /* 0x5201 */
    { "RouteGuidanceManeuverUpdate", 0x00000002U, NULL }, /* 0x5202 */
    { NULL, 0U, NULL },
    { "RouteGuidanceLaneGuidance",   0x00000002U, NULL }  /* 0x5204 */
};

static const char rgi_group_name[] = "Route Guidance";

static struct k2161_msg_group *installed_slot;
static void *installed_driver_base;
static volatile int msgtable_installed;

/*
 * Exact live K2161 ARM layout.
 */
typedef char k2161_msg_info_size_must_be_12[
    sizeof(struct k2161_msg_info) == 12 ? 1 : -1
];

typedef char k2161_msg_group_size_must_be_12[
    sizeof(struct k2161_msg_group) == 12 ? 1 : -1
];

int k2161_msgtable_ready(void)
{
    if (!msgtable_installed ||
        installed_driver_base == NULL ||
        installed_slot == NULL)
        return 0;

    /*
     * Do a live readback, not just a remembered boolean.
     */
    if (installed_slot->name != rgi_group_name ||
        installed_slot->messages != rgi_messages ||
        installed_slot->count != K2161_RGI_GROUP_COUNT)
        return 0;

    return 1;
}

int k2161_msgtable_try_install_base(void *driver_base)
{
    uint8_t *base = (uint8_t *)driver_base;
    struct k2161_msg_group *table;
    struct k2161_msg_group *slot50;
    struct k2161_msg_group *slot52;
    struct k2161_msg_group *slot68;
    const struct k2161_msg_info *np;

    if (base == NULL)
        return 0;

    if (msgtable_installed)
        return installed_driver_base == base &&
               k2161_msgtable_ready() ? 1 : -1;

    table = (struct k2161_msg_group *)
        (base + K2161_CTRL_MSG_TABLE_OFFSET);

    slot50 = &table[0x50];
    slot52 = &table[K2161_RGI_GROUP];
    slot68 = &table[0x68];

    /*
     * Verify two known populated stock groups around 0x52.
     * This confirms:
     *   - exact table offset
     *   - 12-byte group layout
     *   - relocated pointers
     *   - expected exact K2161 binary layout
     */
    if (slot50->name !=
            (const char *)(base + K2161_GROUP50_NAME_OFFSET) ||
        slot50->messages !=
            (const struct k2161_msg_info *)
                (base + K2161_GROUP50_MESSAGES_OFFSET) ||
        slot50->count != 4U) {

        rgi_diag_log("MSGTABLE",
                    "msgtable_verify=FAIL reason=group50_mismatch");
        return -1;
    }

    if (slot68->name !=
            (const char *)(base + K2161_GROUP68_NAME_OFFSET) ||
        slot68->messages !=
            (const struct k2161_msg_info *)
                (base + K2161_GROUP68_MESSAGES_OFFSET) ||
        slot68->count != 4U) {

        rgi_diag_log("MSGTABLE",
                    "msgtable_verify=FAIL reason=group68_mismatch");
        return -1;
    }

    /*
     * Verify the inner message-info layout and direction flags using
     * stock Now Playing entries.
     */
    np = slot50->messages;

    if (np[0].name !=
            (const char *)(base + K2161_NP_START_NAME_OFFSET) ||
        np[0].flags != 0x00000001U ||
        np[0].params !=
            (const void *)(base + K2161_NP_START_PARAMS_OFFSET)) {

        rgi_diag_log("MSGTABLE",
                    "msgtable_verify=FAIL reason=msginfo_layout_start");
        return -1;
    }

    if (np[1].name !=
            (const char *)(base + K2161_NP_UPDATE_NAME_OFFSET) ||
        np[1].flags != 0x00000002U ||
        np[1].params !=
            (const void *)(base + K2161_NP_UPDATE_PARAMS_OFFSET)) {

        rgi_diag_log("MSGTABLE",
                    "msgtable_verify=FAIL reason=msginfo_layout_update");
        return -1;
    }

    rgi_diag_log("MSGTABLE",
                "msgtable_probe slot=%p name=%p messages=%p count=%u",
                slot52,
                slot52->name,
                slot52->messages,
                (unsigned)slot52->count);

    /*
     * Exact stock state for group 0x52 is three zero words.
     * Anything else means stop.
     */
    if (slot52->name != NULL ||
        slot52->messages != NULL ||
        slot52->count != 0U) {

        rgi_diag_log("MSGTABLE",
                    "msgtable_verify=FAIL reason=stock_slot52_mismatch");
        return -1;
    }

    /*
     * Publish count LAST.
     *
     * Stock lookup checks count before using the message array, so the
     * entry cannot become visible in a half-written state.
     */
    slot52->name = rgi_group_name;
    slot52->messages = rgi_messages;

    __sync_synchronize();

    slot52->count = K2161_RGI_GROUP_COUNT;

    __sync_synchronize();

    installed_slot = slot52;
    installed_driver_base = base;

    if (slot52->name != rgi_group_name ||
        slot52->messages != rgi_messages ||
        slot52->count != K2161_RGI_GROUP_COUNT) {

        slot52->count = 0U;
        __sync_synchronize();

        slot52->messages = NULL;
        slot52->name = NULL;

        installed_slot = NULL;
        installed_driver_base = NULL;

        rgi_diag_log("MSGTABLE",
                    "msgtable_verify=FAIL reason=write_verify");
        return -1;
    }

    msgtable_installed = 1;

    rgi_diag_log("MSGTABLE",
                "msgtable_patch msg=5201 flags=00000002");
    rgi_diag_log("MSGTABLE",
                "msgtable_patch msg=5202 flags=00000002");
    rgi_diag_log("MSGTABLE",
                "msgtable_patch msg=5204 flags=00000002");
    rgi_diag_log("MSGTABLE",
                "msgtable_verify=PASS slot=%p count=%u",
                slot52, (unsigned)slot52->count);

    return 1;
}
