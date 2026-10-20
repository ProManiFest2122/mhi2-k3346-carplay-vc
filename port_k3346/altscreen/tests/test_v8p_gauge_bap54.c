/* Run on host; imports exactly the header used in QNX v8N native build. */
#include <assert.h>
#include <stdio.h>
#include "../native/gauge_bap54.h"
#define EV(t,v) "2#org.dsi.ifc.bap.DSIBAPListener#indication#" t "|int:50|int:54|int:0|int:0|int:" v
int main(void) {
    const char *samples[] = {
        EV("46779","0"), EV("51794","1"), EV("53223","0"), EV("55200","1"),
        EV("194754","0"), EV("195615","1"), EV("196602","0"), EV("197264","1"),
        EV("414641","0"), EV("415449","1"), EV("416226","0"), EV("416967","1")
    };
    int slot = -1, n;
    unsigned int raw = 99;
    for (n=0; n<12; ++n) {
        assert(altscreen_bap54_layout_slot(samples[n],&slot,&raw));
        assert(slot == (n%2==0 ? 1 : 0)); /* 0 means BIG, 1 means SMALL */
        assert(raw == (unsigned int)(n%2));
    }
    assert(!altscreen_bap54_layout_slot("3#org.dsi.ifc.bap.DSIBAPListener#indication#123|int:50|int:54|int:0|int:0|int:1", &slot,&raw));
    assert(!altscreen_bap54_layout_slot("4#org.dsi.ifc.bap.DSIBAPListener#indication#123|int:50|int:54|int:0|int:0|int:1", &slot,&raw));
    assert(!altscreen_bap54_layout_slot("0#org.dsi.ifc.bap.DSIBAP#request#123|int:50|int:54|int:0|int:7|int:1", &slot,&raw));
    assert(!altscreen_bap54_layout_slot("2#org.dsi.ifc.bap.DSIBAPListener#acknowledge#123|int:50|int:54|int:0", &slot,&raw));
    assert(!altscreen_bap54_layout_slot(EV("123","-1"),&slot,&raw));
    assert(!altscreen_bap54_layout_slot(EV("123","256"),&slot,&raw));
    assert(!altscreen_bap54_layout_slot("2#org.dsi.ifc.bap.DSIBAPListener#indication#123|int:50|int:23|int:0|int:0|int:1",&slot,&raw));
    assert(altscreen_bap54_layout_slot(EV("123","3"),&slot,&raw) && slot==0); /* menu bits ignored */
    assert(altscreen_bap54_layout_slot(EV("123","2"),&slot,&raw) && slot==1);
    assert(altscreen_bap54_layout_slot(EV("123","1") "@",&slot,&raw) && slot==0);
    puts("V8N_BAP54_PARSER=PASS 12 VEHICLE EVENTS + DUPLICATES + MENU BITMASK");
    return 0;
}
