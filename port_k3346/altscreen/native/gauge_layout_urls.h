#ifndef ALTSCREEN_GAUGE_LAYOUT_URLS_H
#define ALTSCREEN_GAUGE_LAYOUT_URLS_H

/* v8S: retain v8P composite map+card surface; change only maneuver justification.
 * Apple cluster URL query parsing accepts canonical mixed-case leftAligned/topAligned.
 * BAP FctID 54: 1=small gauges/FULL map => views 0/1;
 *               0=big gauges/SMALL map => views 2/3. */
#define ALTSCREEN_URL_SMALL_GAUGES \
    "maps:/car/instrumentcluster?maneuverLayout=leftAligned"
#define ALTSCREEN_URL_BIG_GAUGES \
    "maps:/car/instrumentcluster?maneuverLayout=topAligned"

static const char *altscreen_gauge_url_for_view(int view)
{
    return (view == 2 || view == 3) ? ALTSCREEN_URL_BIG_GAUGES :
                                      ALTSCREEN_URL_SMALL_GAUGES;
}

#endif
