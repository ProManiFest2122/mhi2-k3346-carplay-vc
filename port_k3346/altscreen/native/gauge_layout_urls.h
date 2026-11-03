#ifndef ALTSCREEN_GAUGE_LAYOUT_URLS_H
#define ALTSCREEN_GAUGE_LAYOUT_URLS_H

/* v8S: retain v8P composite map+card surface; change only maneuver justification.
 * Apple cluster URL query parsing accepts canonical mixed-case leftAligned/topAligned.
 * BAP FctID 54: 1=small gauges/FULL map => views 0/1;
 *               0=big gauges/SMALL map => views 2/3.
 * showETA defaults to disabled when absent (RE luka-dev, MHI2Q, iOS 26.6 and 27). */
#define ALTSCREEN_URL_SMALL_GAUGES \
    "maps:/car/instrumentcluster?maneuverLayout=leftAligned"
#define ALTSCREEN_URL_BIG_GAUGES \
    "maps:/car/instrumentcluster?maneuverLayout=topAligned"
#define ALTSCREEN_URL_SMALL_GAUGES_ETA \
    "maps:/car/instrumentcluster?showETA=yes&maneuverLayout=leftAligned"
#define ALTSCREEN_URL_BIG_GAUGES_ETA \
    "maps:/car/instrumentcluster?showETA=yes&maneuverLayout=topAligned"

/* Map only, no maneuver card (RE luka-dev); the path is the part that hides it. */
#define ALTSCREEN_URL_BIG_GAUGES_NO_CARD "maps:/car/instrumentcluster/map"
#define ALTSCREEN_URL_BIG_GAUGES_NO_CARD_ETA "maps:/car/instrumentcluster/map?showETA=yes"
#define ALTSCREEN_URL_SMALL_GAUGES_NO_CARD \
    "maps:/car/instrumentcluster/map?maneuverLayout=leftAligned"
#define ALTSCREEN_URL_SMALL_GAUGES_NO_CARD_ETA \
    "maps:/car/instrumentcluster/map?showETA=yes&maneuverLayout=leftAligned"

static const char *altscreen_gauge_url_for_view(int view, int eta_on_map, int no_card)
{
    if (view == 2 || view == 3) {
        if (no_card)
            return eta_on_map ? ALTSCREEN_URL_BIG_GAUGES_NO_CARD_ETA : ALTSCREEN_URL_BIG_GAUGES_NO_CARD;
        return eta_on_map ? ALTSCREEN_URL_BIG_GAUGES_ETA : ALTSCREEN_URL_BIG_GAUGES;
    }
    if (no_card)
        return eta_on_map ? ALTSCREEN_URL_SMALL_GAUGES_NO_CARD_ETA : ALTSCREEN_URL_SMALL_GAUGES_NO_CARD;
    return eta_on_map ? ALTSCREEN_URL_SMALL_GAUGES_ETA : ALTSCREEN_URL_SMALL_GAUGES;
}

#endif
