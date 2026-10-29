/*
 * Host test for overlay_fbo.h - which FBO format the panel renders into.
 *
 * The GL measurement itself needs the car.  What it feeds - the decision -
 * does not, and the decision is where the one property that matters lives:
 *
 *     the panel is composited ONLY when some format really kept alpha.
 *
 * Break that and the cluster gets an opaque black slab over the CarPlay map,
 * which is what the car showed on 2026-10-28.  So besides the hand-picked
 * cases the test walks every combination of verdicts for the three
 * candidates (4^3 = 64) and checks the property on each one.
 *
 * Built with the container's native gcc:  bash overlay/tests/run_host_tests.sh
 */

#include <stdio.h>
#include <string.h>

#include "../overlay_fbo.h"

static int g_pass, g_fail;

static void check_int(int got, int want, const char *what)
{
    if (got == want) g_pass++;
    else { g_fail++; printf("  FAIL: %s (got %d, want %d)\n", what, got, want); }
}

#define OK   OVERLAY_FBO_ALPHA_OK
#define LOST OVERLAY_FBO_ALPHA_LOST
#define INC  OVERLAY_FBO_INCOMPLETE
#define UNT  OVERLAY_FBO_UNTESTED

static void t_today(void)
{
    overlay_fbo_verdict_t v[3] = { OK, UNT, UNT };
    printf("RGBA8 keeps alpha: nothing changes from before\n");
    check_int(overlay_fbo_pick(v, 3), 0, "RGBA8 chosen");
    check_int(overlay_fbo_has_alpha(v, 3), 1, "panel composited");
}

static void t_falls_through(void)
{
    overlay_fbo_verdict_t a[3] = { LOST, OK, UNT };
    overlay_fbo_verdict_t b[3] = { INC, LOST, OK };
    printf("a format that loses alpha is passed over\n");
    check_int(overlay_fbo_pick(a, 3), 1, "RGBA8 lost -> RGBA4");
    check_int(overlay_fbo_has_alpha(a, 3), 1, "and composited");
    check_int(overlay_fbo_pick(b, 3), 2, "incomplete, lost -> RGB5A1");
    check_int(overlay_fbo_has_alpha(b, 3), 1, "and composited");
}

static void t_order_is_preference(void)
{
    overlay_fbo_verdict_t v[3] = { OK, OK, OK };
    printf("several pass: the first one wins, not the last\n");
    check_int(overlay_fbo_pick(v, 3), 0, "RGBA8 over RGBA4 over RGB5A1");
}

static void t_none_keeps_alpha(void)
{
    overlay_fbo_verdict_t lost[3] = { LOST, LOST, LOST };
    overlay_fbo_verdict_t mixed[3] = { INC, LOST, LOST };
    overlay_fbo_verdict_t dead[3] = { INC, INC, INC };
    printf("nothing keeps alpha: render somewhere, show nothing\n");
    check_int(overlay_fbo_pick(lost, 3), -1, "no format chosen");
    check_int(overlay_fbo_has_alpha(lost, 3), 0, "panel NOT composited");
    check_int(overlay_fbo_pick_fallback(lost, 3), 0, "renderer still gets RGBA8");
    check_int(overlay_fbo_pick_fallback(mixed, 3), 1, "first complete one");
    check_int(overlay_fbo_pick_fallback(dead, 3), -1, "nothing complete at all");
    check_int(overlay_fbo_has_alpha(dead, 3), 0, "and nothing composited");
}

static void t_degenerate(void)
{
    overlay_fbo_verdict_t unt[3] = { UNT, UNT, UNT };
    printf("untested and empty inputs never count as a pass\n");
    check_int(overlay_fbo_pick(unt, 3), -1, "untested is not a pass");
    check_int(overlay_fbo_pick_fallback(unt, 3), -1, "untested is not usable");
    check_int(overlay_fbo_pick(NULL, 3), -1, "NULL list");
    check_int(overlay_fbo_pick(unt, 0), -1, "empty list");
    check_int(overlay_fbo_has_alpha(NULL, 0), 0, "no list, no alpha");
}

static void t_names(void)
{
    printf("verdict names are distinct, for the log\n");
    check_int(strcmp(overlay_fbo_verdict_name(OK), overlay_fbo_verdict_name(LOST)) != 0,
              1, "OK differs from LOST");
    check_int(strcmp(overlay_fbo_verdict_name(LOST), overlay_fbo_verdict_name(INC)) != 0,
              1, "LOST differs from INCOMPLETE");
    check_int(strcmp(overlay_fbo_verdict_name(UNT), "untested") == 0, 1,
              "UNTESTED reads as untested");
}

/* Every combination.  The answers are recomputed the slow, obvious way and
 * compared - and the safety property is checked on its own, because it is the
 * one that decides whether the car gets a black slab. */
static void t_exhaustive(void)
{
    const overlay_fbo_verdict_t all[4] = { UNT, OK, LOST, INC };
    int i, j, k, n = 0;
    char what[96];

    printf("all 64 combinations of three verdicts\n");
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) for (k = 0; k < 4; k++) {
        overlay_fbo_verdict_t v[3];
        int want_pick = -1, want_fb = -1, c;

        v[0] = all[i]; v[1] = all[j]; v[2] = all[k];
        for (c = 2; c >= 0; c--) if (v[c] == OK)   want_pick = c;
        for (c = 2; c >= 0; c--) if (v[c] == LOST) want_fb = c;

        sprintf(what, "combo %d%d%d: pick", i, j, k);
        check_int(overlay_fbo_pick(v, 3), want_pick, what);
        sprintf(what, "combo %d%d%d: fallback", i, j, k);
        check_int(overlay_fbo_pick_fallback(v, 3), want_fb, what);
        sprintf(what, "combo %d%d%d: composited only if a format kept alpha", i, j, k);
        check_int(overlay_fbo_has_alpha(v, 3),
                  (v[0] == OK || v[1] == OK || v[2] == OK) ? 1 : 0, what);
        n++;
    }
    check_int(n, 64, "walked every combination");
}

/*
 * The second net: a tile opaque almost everywhere and lit almost nowhere is
 * the signature of a failure, never a picture.  The first case is literally
 * the car's readback of 2026-10-28 (alpha in 100.0% of the crop, colour in
 * 0.0%).  The rest pin the thresholds from both sides, so the guard can only
 * ever hide a broken tile - including the ones a working renderer produces on
 * its worst frames: a blank tile, a mid-fade first frame, a big opaque arrow.
 */
static void t_tile_degenerate(void)
{
    int a, r, wrong = 0;

    printf("a black slab is recognised, a picture never is\n");
    check_int(overlay_tile_is_degenerate(1000, 0), 1,
              "the car's readback: alpha 100%, colour 0% -> slab");
    check_int(overlay_tile_is_degenerate(990, 9), 1, "99.0% opaque, 0.9% lit -> slab");
    check_int(overlay_tile_is_degenerate(989, 0), 0, "98.9% opaque is not enough");
    check_int(overlay_tile_is_degenerate(1000, 10), 0, "1.0% lit is a picture");
    check_int(overlay_tile_is_degenerate(0, 0), 0, "blank transparent tile is fine");
    check_int(overlay_tile_is_degenerate(150, 150), 0, "mid-fade first frame is fine");
    check_int(overlay_tile_is_degenerate(180, 160), 0, "an arrow over the map is fine");
    check_int(overlay_tile_is_degenerate(1000, 1000), 0,
              "opaque AND lit is a picture, not the failure");

    /* The whole plane, every 10 per mille, against the rule written out. */
    for (a = 0; a <= 1000; a += 10)
        for (r = 0; r <= 1000; r += 10)
            if (overlay_tile_is_degenerate(a, r) != (a >= 990 && r < 10)) wrong++;
    check_int(wrong, 0, "the 101x101 grid agrees with the rule");
}

int main(void)
{
    t_today();
    t_falls_through();
    t_order_is_preference();
    t_none_keeps_alpha();
    t_degenerate();
    t_names();
    t_exhaustive();
    t_tile_degenerate();

    printf("\noverlay_fbo: %d OK, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
