#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../native/gauge_bap54.h"
#include "../native/gauge_layout_urls.h"
#define IND(v) "2#org.dsi.ifc.bap.DSIBAPListener#indication#123|int:50|int:54|int:0|int:0|int:" v
int main(void)
{
    int slot = -1, view;
    unsigned int raw = 99;
    assert(altscreen_bap54_layout_slot(IND("1"), &slot, &raw) && slot == 0);
    for (view = slot * 2; view <= slot * 2 + 1; ++view) {
        assert(strcmp(altscreen_gauge_url_for_view(view),
                      "maps:/car/instrumentcluster?maneuverLayout=leftAligned") == 0);
    }
    assert(altscreen_bap54_layout_slot(IND("0"), &slot, &raw) && slot == 1);
    for (view = slot * 2; view <= slot * 2 + 1; ++view) {
        assert(strcmp(altscreen_gauge_url_for_view(view),
                      "maps:/car/instrumentcluster?maneuverLayout=topAligned") == 0);
    }
    puts("V8R_GAUGE_ALIGN_URL_TEST=PASS");
    return 0;
}
