/* Audi K2161: confirmed factory DSIBAP service 50 / FctID 54.
 * State bit 0: 0 = BIG gauges (slot 1), 1 = SMALL gauges (slot 0).
 * Parse the LIVE 2# listener only: 3#/4# are duplicate indications,
 * while 0# requests and acknowledge records are not gauge state.
 * This header is source code; no binary patching is permitted. */
#ifndef ALTSCREEN_GAUGE_BAP54_H
#define ALTSCREEN_GAUGE_BAP54_H
#include <string.h>
#include <stdio.h>
static int altscreen_bap54_layout_slot(const char *line, int *layout_slot,
                                       unsigned int *raw_state)
{
    static const char prefix[] = "2#org.dsi.ifc.bap.DSIBAPListener#indication#";
    unsigned long timestamp = 0;
    int service = -1, function_id = -1, a = -1, b = -1, value = -1;
    char tail = 0;
    int n;
    if (line == NULL || layout_slot == NULL || raw_state == NULL ||
        strncmp(line, prefix, sizeof(prefix) - 1) != 0)
        return 0;
    n = sscanf(line + sizeof(prefix) - 1,
               "%lu|int:%d|int:%d|int:%d|int:%d|int:%d%c",
               &timestamp, &service, &function_id, &a, &b, &value, &tail);
    /* DSITracer uses an optional '@' record terminator. */
    if (n != 6 && !(n == 7 && tail == '@'))
        return 0;
    if (service != 50 || function_id != 54 || a != 0 || b != 0 ||
        value < 0 || value > 255)
        return 0;
    *raw_state = (unsigned int)value;
    *layout_slot = (value & 1) ? 0 : 1;
    return 1;
}
#endif
