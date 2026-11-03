/*
 * The maneuver list across a route reset.
 *
 * run-60: right after a replug the phone sent the whole route (list 0,1), then
 * state=0, then the route again without the list, and never sent the list after
 * that.  Our reset had dropped it, so there was no current maneuver: a dark tile
 * with no arrow for the rest of the session, while run-58 and run-59 got the list
 * in every update.  Why the phone sent two rounds is not known.
 *
 * So the list is kept over a reset, and comes back only when the route is active
 * again, has the same number of maneuvers, the first one is cached, and the phone
 * has not said anything about the list since.  A list from the phone, even an
 * empty one, always wins.
 *
 * Plain data, no locking: the caller holds its own lock.
 */
#ifndef RGD_LIST_RESTORE_H
#define RGD_LIST_RESTORE_H

#include <stdint.h>
#include <string.h>

#define RGD_SAVED_LIST_MAX 16

typedef struct {
    int valid;
    uint16_t maneuver_count;
    uint16_t n;
    uint16_t list[RGD_SAVED_LIST_MAX];
} rgd_saved_list_t;

static void rgd_saved_list_clear(rgd_saved_list_t *s)
{
    s->valid = 0;
}

/* A reset with no list to keep leaves the saved one alone: the phone sends them
 * in a row, and the second finds an empty cache. */
static void rgd_saved_list_keep(rgd_saved_list_t *s, const uint16_t *list, unsigned n,
                                uint16_t maneuver_count)
{
    if (n == 0 || n > RGD_SAVED_LIST_MAX || maneuver_count == 0)
        return;
    memcpy(s->list, list, n * sizeof(s->list[0]));
    s->n = (uint16_t)n;
    s->maneuver_count = maneuver_count;
    s->valid = 1;
}

/* Number of entries written to out, 0 for none. */
static unsigned rgd_saved_list_take(rgd_saved_list_t *s, int has_list, int route_active,
                                    uint16_t maneuver_count, int first_cached, uint16_t *out)
{
    unsigned n;

    if (!s->valid)
        return 0;
    if (has_list) {
        s->valid = 0;
        return 0;
    }
    if (!route_active || !first_cached || maneuver_count != s->maneuver_count)
        return 0;
    n = s->n;
    memcpy(out, s->list, n * sizeof(s->list[0]));
    s->valid = 0;
    return n;
}

#endif
