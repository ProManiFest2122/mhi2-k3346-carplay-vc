/*
 * Exhaustive dump of the C maneuver mapper, for diffing against the Java one.
 *
 * The C mapper (native/routeguidance/maneuver_map.c) feeds the cluster overlay;
 * the Java mapper (java/build/src/routeguidance/ManeuverMapper.java) feeds the
 * VC over BAP.  They must agree, or the arrow we draw would contradict the
 * distance and street name the VC prints next to it.
 *
 * Both dumpers walk the same grid and print the same format; the runner diffs
 * them byte for byte.
 */

#include <stdio.h>

#include "../../native/routeguidance/maneuver_map.h"

int main(void)
{
    /* Sentinels first, then every whole degree.  The sentinels are the ones
     * that have actually bitten: +/-1000 means "no angle" from rgd_tlv and -1
     * is the unset slot default, and treating either as a real angle once
     * fabricated a U-turn at an ordinary roundabout exit. */
    static const int extra_angles[] = { -1000, 1000, -1 };
    int type, junction, side, i, angle;

    for (type = -2; type <= 55; type++) {
        for (junction = 0; junction <= 2; junction++) {
            for (side = 0; side <= 1; side++) {
                for (i = 0; i < (int)(sizeof extra_angles / sizeof extra_angles[0]); i++) {
                    int main_el = 0, dir = 0;
                    k3346_maneuver_map(type, extra_angles[i], junction, side,
                                       &main_el, &dir);
                    printf("%d %d %d %d %d %d\n", type, extra_angles[i],
                           junction, side, main_el, dir);
                }
                for (angle = -180; angle <= 180; angle++) {
                    int main_el = 0, dir = 0;
                    k3346_maneuver_map(type, angle, junction, side, &main_el, &dir);
                    printf("%d %d %d %d %d %d\n", type, angle,
                           junction, side, main_el, dir);
                }
            }
        }
    }
    return 0;
}
