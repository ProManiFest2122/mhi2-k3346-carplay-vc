#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../native/gauge_bap54.h"
#include "../native/gauge_layout_urls.h"
#include "../native/eta_mode.h"
#define IND(v) "2#org.dsi.ifc.bap.DSIBAPListener#indication#123|int:50|int:54|int:0|int:0|int:" v
#define SMALL "maps:/car/instrumentcluster?maneuverLayout=leftAligned"
#define SMALL_ETA "maps:/car/instrumentcluster?showETA=yes&maneuverLayout=leftAligned"
#define BIG "maps:/car/instrumentcluster?maneuverLayout=topAligned"
#define BIG_ETA "maps:/car/instrumentcluster?showETA=yes&maneuverLayout=topAligned"

static void put(const char *path, const char *data, size_t len)
{
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    assert(fwrite(data, 1, len, f) == len);
    fclose(f);
}

static void test_eta_mode(void)
{
    char path[64];
    snprintf(path, sizeof(path), "/tmp/eta_mode_test.%d", (int)getpid());

    remove(path);
    assert(k3346_eta_mode_read(path, K3346_ETA_BOTH) == K3346_ETA_CAPSULE); /* default */

    put(path, "0\n", strlen("0\n"));
    assert(k3346_eta_mode_read(path, K3346_ETA_MAP) == K3346_ETA_CAPSULE);
    put(path, "1\n", strlen("1\n"));
    assert(k3346_eta_mode_read(path, K3346_ETA_CAPSULE) == K3346_ETA_MAP);
    put(path, "2\n", strlen("2\n"));
    assert(k3346_eta_mode_read(path, K3346_ETA_CAPSULE) == K3346_ETA_BOTH);

    /* caught half written or damaged: the last good value stays */
    put(path, "", strlen(""));
    assert(k3346_eta_mode_read(path, K3346_ETA_MAP) == K3346_ETA_MAP);
    put(path, "1", strlen("1"));
    assert(k3346_eta_mode_read(path, K3346_ETA_BOTH) == K3346_ETA_BOTH);
    put(path, "3\n", strlen("3\n"));
    assert(k3346_eta_mode_read(path, K3346_ETA_MAP) == K3346_ETA_MAP);
    put(path, "/\n", strlen("/\n"));
    assert(k3346_eta_mode_read(path, K3346_ETA_MAP) == K3346_ETA_MAP);
    put(path, "1x", strlen("1x"));
    assert(k3346_eta_mode_read(path, K3346_ETA_BOTH) == K3346_ETA_BOTH);
    put(path, "1\n\n", strlen("1\n\n"));
    assert(k3346_eta_mode_read(path, K3346_ETA_BOTH) == K3346_ETA_BOTH);
    remove(path);

    /* an open failure that is not "no file" is no answer either */
    assert(k3346_eta_mode_read("/proc/self/mem/nope", K3346_ETA_MAP) == K3346_ETA_MAP);

    assert(!k3346_eta_on_map(K3346_ETA_CAPSULE));
    assert(k3346_eta_on_map(K3346_ETA_MAP));
    assert(k3346_eta_on_map(K3346_ETA_BOTH));

    /* rc29: is the PHONE asked for its box?  Option, call, slot (0 small, 1 big, -1
     * unknown), and "the app sends no route guidance".  With route guidance never: the
     * overlay draws our own box. */
    {
        int mode, call, slot;
        for (mode = K3346_ETA_CAPSULE; mode <= K3346_ETA_BOTH; ++mode)
            for (call = 0; call < 2; ++call)
                for (slot = -1; slot < 2; ++slot)
                    assert(!k3346_eta_map_effective(mode, call, slot, 0));
    }
    /* without route guidance: the table of rc10, the call card takes the big gauges */
    assert(!k3346_eta_map_effective(K3346_ETA_CAPSULE, 0, 0, 1));
    assert(!k3346_eta_map_effective(K3346_ETA_CAPSULE, 0, 1, 1));
    assert(!k3346_eta_map_effective(K3346_ETA_CAPSULE, 1, 0, 1));
    assert(!k3346_eta_map_effective(K3346_ETA_CAPSULE, 1, 1, 1));
    assert(k3346_eta_map_effective(K3346_ETA_MAP, 0, 0, 1));
    assert(k3346_eta_map_effective(K3346_ETA_MAP, 0, 1, 1));
    assert(k3346_eta_map_effective(K3346_ETA_MAP, 1, 0, 1));
    assert(!k3346_eta_map_effective(K3346_ETA_MAP, 1, 1, 1));
    assert(k3346_eta_map_effective(K3346_ETA_BOTH, 0, 0, 1));
    assert(k3346_eta_map_effective(K3346_ETA_BOTH, 0, 1, 1));
    assert(k3346_eta_map_effective(K3346_ETA_BOTH, 1, 0, 1));
    assert(!k3346_eta_map_effective(K3346_ETA_BOTH, 1, 1, 1));
    assert(k3346_eta_map_effective(K3346_ETA_MAP, 1, -1, 1));
}

static void test_call_visible(void)
{
    char path[64], big[300];
    snprintf(path, sizeof(path), "/tmp/call_visible_test.%d", (int)getpid());

    remove(path);
    assert(k3346_call_visible_read(path, 1) == 0);   /* no file, no card */

    put(path, "R\nAnna\nDzwoni\nEND\n", strlen("R\nAnna\nDzwoni\nEND\n"));
    assert(k3346_call_visible_read(path, 0) == 1);
    put(path, "D\nAnna\nX\nEND\n", strlen("D\nAnna\nX\nEND\n"));
    assert(k3346_call_visible_read(path, 0) == 1);
    put(path, "A\nAnna\nX\nEND\n", strlen("A\nAnna\nX\nEND\n"));
    assert(k3346_call_visible_read(path, 0) == 1);
    put(path, "H\nAnna\nX\nEND\n", strlen("H\nAnna\nX\nEND\n"));
    assert(k3346_call_visible_read(path, 0) == 1);
    put(path, "\n\n\nEND\n", strlen("\n\n\nEND\n"));
    assert(k3346_call_visible_read(path, 1) == 0);

    /* caught half written or damaged: the last answer stays, whichever it was */
    put(path, "", strlen(""));
    assert(k3346_call_visible_read(path, 1) == 1);
    assert(k3346_call_visible_read(path, 0) == 0);
    put(path, "A\nAnna", strlen("A\nAnna"));
    assert(k3346_call_visible_read(path, 0) == 0);
    put(path, "A\nAnna\nX\nEN", strlen("A\nAnna\nX\nEN"));
    assert(k3346_call_visible_read(path, 0) == 0);
    put(path, "X\nAnna\nX\nEND\n", strlen("X\nAnna\nX\nEND\n"));
    assert(k3346_call_visible_read(path, 0) == 0);
    put(path, "AA\nAnna\nX\nEND\n", strlen("AA\nAnna\nX\nEND\n"));
    assert(k3346_call_visible_read(path, 0) == 0);
    memset(big, 'x', sizeof(big));
    memcpy(big + sizeof(big) - 5, "\nEND\n", 5);
    big[0] = 'A'; big[1] = '\n';
    put(path, big, sizeof(big));
    assert(k3346_call_visible_read(path, 0) == 0);
    remove(path);

    assert(k3346_call_visible_read("/proc/self/mem/nope", 1) == 1);
}

static void test_layout_slot(void)
{
    char path[64];
    snprintf(path, sizeof(path), "/tmp/layout_slot_test.%d", (int)getpid());

    remove(path);
    assert(k3346_layout_slot_read(path) == -1);
    put(path, "0 1\n", strlen("0 1\n"));
    assert(k3346_layout_slot_read(path) == 0);
    put(path, "1 0\n", strlen("1 0\n"));
    assert(k3346_layout_slot_read(path) == 1);
    put(path, "", 0);
    assert(k3346_layout_slot_read(path) == -1);
    put(path, "1 ", 2);
    assert(k3346_layout_slot_read(path) == -1);
    put(path, "2 1\n", 4);
    assert(k3346_layout_slot_read(path) == -1);
    put(path, "1x1\n", 4);
    assert(k3346_layout_slot_read(path) == -1);
    put(path, "1 1x", 4);
    assert(k3346_layout_slot_read(path) == -1);
    put(path, "1 1\nX", 5);
    assert(k3346_layout_slot_read(path) == -1);
    remove(path);
}

static void test_card_option(void)
{
    const char *path = "/tmp/k3346_card_option_test";
    int view;

    assert(!strcmp(K3346_CARD_MODE_PATH, "/tmp/k3346-card-mode"));
    assert(!strcmp(K3346_NO_RGI_PATH, "/tmp/k3346-no-rgi"));
    /* card option: the file */
    put(path, "1\n", 2);
    assert(k3346_card_mode_read(path, 0) == 1);
    put(path, "0\n", 2);
    assert(k3346_card_mode_read(path, 1) == 0);
    put(path, "2\n", 2);
    assert(k3346_card_mode_read(path, 1) == 1);
    assert(k3346_card_mode_read(path, 0) == 0);
    put(path, "1", 1);
    assert(k3346_card_mode_read(path, 0) == 0);
    put(path, "1x", 2);
    assert(k3346_card_mode_read(path, 0) == 0);
    put(path, "1\n\n", 3);
    assert(k3346_card_mode_read(path, 0) == 0);
    unlink(path);
    assert(k3346_card_mode_read(path, 1) == K3346_CARD_FULL);
    /* card option: an app without route guidance keeps its card */
    assert(k3346_card_effective(K3346_CARD_NONE, 0) == K3346_CARD_NONE);
    assert(k3346_card_effective(K3346_CARD_NONE, 1) == K3346_CARD_FULL);
    assert(k3346_card_effective(K3346_CARD_FULL, 1) == K3346_CARD_FULL);
    /* skin */
    assert(!strcmp(K3346_SKIN_PATH, "/tmp/vc-skin"));
    put(path, "1\n", 2);
    assert(k3346_skin_read(path, 0) == 1);
    put(path, "0\n", 2);
    assert(k3346_skin_read(path, 1) == 0);
    put(path, "2\n", 2);
    assert(k3346_skin_read(path, 1) == 1 && k3346_skin_read(path, 0) == 0);
    put(path, "1", 1);
    assert(k3346_skin_read(path, 0) == 0);
    put(path, "1x", 2);
    assert(k3346_skin_read(path, 0) == 0);
    put(path, "1\n\n", 3);
    assert(k3346_skin_read(path, 0) == 0);
    unlink(path);
    assert(k3346_skin_read(path, 1) == 0);
    assert(k3346_wire_view(2, 1, 0) == 4 && k3346_wire_view(3, 1, 0) == 4);
    assert(k3346_wire_view(2, 0, 0) == 2 && k3346_wire_view(3, 0, 0) == 3);
    assert(k3346_wire_view(0, 1, 0) == 0 && k3346_wire_view(1, 1, 0) == 1);
    /* Apple Maps: its own areas with a route; on the big gauges only without the card,
     * and never on the Sport skin */
    {
        int sport, area;
        for (sport = 0; sport < 2; ++sport) {
            assert(k3346_wire_view(1, sport, K3346_APPLE_FREE) == K3346_VIEW_APPLE_FREE);
            assert(k3346_wire_view(1, sport, K3346_APPLE_CARD) == K3346_VIEW_APPLE_CARD);
            assert(k3346_wire_view(1, sport, K3346_APPLE_NONE) == 1);
            /* another app without the card: view 1 as with it (rc30) */
            assert(k3346_wire_view(1, sport, K3346_OTHER_FREE) == 1);
            for (area = K3346_APPLE_NONE; area <= K3346_OTHER_FREE; ++area) {
                assert(k3346_wire_view(0, sport, area) == 0);
                assert(k3346_wire_view(2, sport, area) == (sport ? 4 : 2));
            }
            assert(k3346_wire_view(3, sport, K3346_APPLE_NONE) == (sport ? 4 : 3));
            assert(k3346_wire_view(3, sport, K3346_APPLE_CARD) == (sport ? 4 : 3));
            assert(k3346_wire_view(3, sport, K3346_APPLE_FREE) == (sport ? 4 : K3346_VIEW_APPLE_BIG));
            /* rc30: another app without the card ends above our boxes on the big gauges */
            assert(k3346_wire_view(3, sport, K3346_OTHER_FREE) == (sport ? 4 : 8));
        }
        assert(K3346_VIEW_OTHER_BIG == 8);
        assert(K3346_VIEW_APPLE_FREE == 5 && K3346_VIEW_APPLE_CARD == 6);
        assert(K3346_VIEW_APPLE_BIG == 7);
        assert(k3346_apple_area(1, K3346_CARD_NONE) == K3346_APPLE_FREE);
        assert(k3346_apple_area(1, K3346_CARD_FULL) == K3346_APPLE_CARD);
        /* other apps: by the card, which matters on the big gauges only (rc30) */
        assert(k3346_apple_area(0, K3346_CARD_NONE) == K3346_OTHER_FREE);
        assert(k3346_apple_area(0, K3346_CARD_FULL) == K3346_APPLE_NONE);
        assert(K3346_APPLE_NONE == 0 && K3346_OTHER_FREE == 3);
        assert(K3346_APPLE_FREE != K3346_APPLE_CARD && K3346_APPLE_FREE != K3346_APPLE_NONE &&
               K3346_OTHER_FREE != K3346_APPLE_FREE && K3346_OTHER_FREE != K3346_APPLE_CARD);
    }
    /* card option: URLs.  No ETA box: the same plain path everywhere. */
    for (view = 0; view < 4; ++view)
        assert(!strcmp(altscreen_gauge_url_for_view(view, 0, 1), "maps:/car/instrumentcluster/map"));
    /* with the box the two layouts always differ */
    for (view = 0; view < 2; ++view) {
        assert(!strcmp(altscreen_gauge_url_for_view(view, 1, 1),
                       "maps:/car/instrumentcluster/map?showETA=yes&maneuverLayout=leftAligned"));
        assert(!strcmp(altscreen_gauge_url_for_view(view + 2, 1, 1),
                       "maps:/car/instrumentcluster/map?showETA=yes&maneuverLayout=topAligned"));
    }
    /* no key of our own in any of them (rc21 had one, it brought nothing back) */
    for (view = 0; view < 4; ++view) {
        assert(!strstr(altscreen_gauge_url_for_view(view, 1, 1), "k3346"));
        assert(!strstr(altscreen_gauge_url_for_view(view, 1, 0), "k3346"));
    }
}

static void test_show_held(void)
{
    /* nothing sent yet in this session */
    assert(k3346_show_held(0, 5000) == 0);
    assert(k3346_show_held(0, 1000) == 0);
    assert(k3346_show_held(1000, 1000) == 1);
    assert(k3346_show_held(1000, 1499) == 1);
    assert(k3346_show_held(1000, 1500) == 0);
    assert(K3346_SHOW_MIN_INTERVAL_MS == 500ULL);
    assert(k3346_show_held(1000, 90000) == 0);
}

static void test_early_layout(void)
{
    char path[64];
    snprintf(path, sizeof(path), "/tmp/view_size_test.%d", (int)getpid());

    /* the file BAPBridge writes (K3346CapsuleFile.VIEW_SIZE_PATH) */
    assert(!strcmp(K3346_VIEW_SIZE_PATH, "/tmp/vc-view-size"));

    remove(path);
    assert(k3346_view_size_read(path) == -1);
    put(path, "0\n", strlen("0\n"));
    assert(k3346_view_size_read(path) == 0);
    put(path, "1\n", strlen("1\n"));
    assert(k3346_view_size_read(path) == 1);
    /* the raw HMI value would be 2: this file holds the slot */
    put(path, "2\n", strlen("2\n"));
    assert(k3346_view_size_read(path) == -1);
    put(path, "", 0);
    assert(k3346_view_size_read(path) == -1);
    put(path, "1", strlen("1"));
    assert(k3346_view_size_read(path) == -1);
    put(path, "1x", strlen("1x"));
    assert(k3346_view_size_read(path) == -1);
    put(path, "1\n\n", strlen("1\n\n"));
    assert(k3346_view_size_read(path) == -1);
    /* the slot file of the overlay has another shape */
    put(path, "1 0\n", strlen("1 0\n"));
    assert(k3346_view_size_read(path) == -1);
    remove(path);

    assert(k3346_layout_early(-1, -1) == -1);
    assert(k3346_layout_early(-1, 0) == 0);
    assert(k3346_layout_early(-1, 1) == 1);
    assert(k3346_layout_early(0, 1) == 0);
    assert(k3346_layout_early(1, 0) == 1);
    assert(k3346_layout_early(1, -1) == 1);
    assert(k3346_layout_early(0, -1) == 0);
}

static void test_eta_held(void)
{
    /* no layout change yet, or none this session: at once */
    assert(k3346_eta_held(1, 0, 0, 3000, 5000) == 1);
    /* just changed: wait as long as asked */
    assert(k3346_eta_held(1, 0, 5000, 3000, 5000) == 0);
    assert(k3346_eta_held(1, 0, 5000, 3000, 5000 + 3000 - 1) == 0);
    assert(k3346_eta_held(1, 0, 5000, 3000, 5000 + 3000) == 1);
    assert(k3346_eta_held(1, 0, 5000, 12000, 5000 + 3000) == 0);
    assert(k3346_eta_held(1, 0, 5000, 12000, 5000 + 12000) == 1);
    /* a box on screen stays, and hiding never waits */
    assert(k3346_eta_held(1, 1, 5000, 3000, 5000) == 1);
    assert(k3346_eta_held(0, 1, 5000, 3000, 5000) == 0);
    assert(k3346_eta_held(0, 0, 5000, 3000, 5000) == 0);
    assert(K3346_ETA_AFTER_LAYOUT_MS == 12000ULL);
}

int main(void)
{
    int slot = -1, view;
    unsigned int raw = 99;
    assert(altscreen_bap54_layout_slot(IND("1"), &slot, &raw) && slot == 0);
    for (view = slot * 2; view <= slot * 2 + 1; ++view) {
        assert(strcmp(altscreen_gauge_url_for_view(view, 1, 0), SMALL_ETA) == 0);
        assert(strcmp(altscreen_gauge_url_for_view(view, 0, 0), SMALL) == 0);
    }
    assert(altscreen_bap54_layout_slot(IND("0"), &slot, &raw) && slot == 1);
    for (view = slot * 2; view <= slot * 2 + 1; ++view) {
        assert(strcmp(altscreen_gauge_url_for_view(view, 1, 0), BIG_ETA) == 0);
        assert(strcmp(altscreen_gauge_url_for_view(view, 0, 0), BIG) == 0);
    }
    test_eta_mode();
    test_call_visible();
    test_layout_slot();
    test_eta_held();
    test_card_option();
    test_show_held();
    test_early_layout();
    puts("V8R_GAUGE_ALIGN_URL_TEST=PASS");
    return 0;
}
