/*
 * The list across a route reset, with the sequence the phone really sent in run-60.
 * Built and run by overlay/tests/run_host_tests.sh.
 */
#include <assert.h>
#include <stdio.h>

#include "../routeguidance/rgd_list_restore.h"

static const uint16_t LIST[2] = {0, 1};

static rgd_saved_list_t kept(void)
{
    rgd_saved_list_t s;

    memset(&s, 0, sizeof(s));
    rgd_saved_list_keep(&s, LIST, 2, 8);
    return s;
}

int main(void)
{
    rgd_saved_list_t s;
    uint16_t out[RGD_SAVED_LIST_MAX];
    uint16_t many[RGD_SAVED_LIST_MAX + 1];
    unsigned i;

    /* run-60, in order */
    memset(&s, 0, sizeof(s));
    /* rx=3: state 0 with nothing yet */
    rgd_saved_list_keep(&s, NULL, 0, 0);
    assert(!s.valid);
    /* rx=20: state 0, the cache has count=8 and the list 0,1 */
    rgd_saved_list_keep(&s, LIST, 2, 8);
    assert(s.valid);
    /* rx=21: state 0 again, the cache is empty now: the list stays */
    rgd_saved_list_keep(&s, NULL, 0, 0);
    assert(s.valid && s.n == 2);
    /* rx=22: state 3, count 8, no list, the maneuvers are not here yet */
    assert(rgd_saved_list_take(&s, 0, 1, 8, 0, out) == 0);
    assert(s.valid);
    /* rx=29: state 1, no list, maneuver 0 cached */
    assert(rgd_saved_list_take(&s, 0, 1, 8, 1, out) == 2);
    assert(out[0] == 0 && out[1] == 1);
    /* once */
    assert(!s.valid);
    assert(rgd_saved_list_take(&s, 0, 1, 8, 1, out) == 0);

    /* a list from the phone wins, an empty one too */
    s = kept();
    out[0] = 99;
    assert(rgd_saved_list_take(&s, 1, 1, 8, 1, out) == 0);
    assert(!s.valid && out[0] == 99);

    /* route not active: wait */
    s = kept();
    assert(rgd_saved_list_take(&s, 0, 0, 8, 1, out) == 0);
    assert(s.valid);

    /* another count: another route */
    s = kept();
    assert(rgd_saved_list_take(&s, 0, 1, 7, 1, out) == 0);
    assert(rgd_saved_list_take(&s, 0, 1, 0, 1, out) == 0);
    assert(s.valid);

    /* route end, hard clear, disconnect */
    s = kept();
    rgd_saved_list_clear(&s);
    assert(rgd_saved_list_take(&s, 0, 1, 8, 1, out) == 0);

    /* nothing worth keeping */
    memset(&s, 0, sizeof(s));
    rgd_saved_list_keep(&s, LIST, 2, 0);
    assert(!s.valid);
    rgd_saved_list_keep(&s, LIST, 0, 8);
    assert(!s.valid);
    for (i = 0; i < RGD_SAVED_LIST_MAX + 1; i++)
        many[i] = (uint16_t)i;
    rgd_saved_list_keep(&s, many, RGD_SAVED_LIST_MAX + 1, 8);
    assert(!s.valid);

    /* the longest one fits */
    rgd_saved_list_keep(&s, many, RGD_SAVED_LIST_MAX, 8);
    assert(rgd_saved_list_take(&s, 0, 1, 8, 1, out) == RGD_SAVED_LIST_MAX);
    assert(out[RGD_SAVED_LIST_MAX - 1] == RGD_SAVED_LIST_MAX - 1);

    /* a newer list replaces the kept one */
    s = kept();
    many[0] = 5;
    rgd_saved_list_keep(&s, many, 1, 9);
    assert(rgd_saved_list_take(&s, 0, 1, 9, 1, out) == 1);
    assert(out[0] == 5);

    puts("rgd list restore OK");
    return 0;
}
