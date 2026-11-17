package com.luka.carplay.routeguidance;

/*
 * What the MMI option "CarPlay HUD" asks of the maneuver output (issue #15).
 *
 * The maneuver descriptor, its distance and its state travel in one BAP stream to
 * the instrument cluster, which feeds the head-up display.  The car's own setting
 * "Navigation messages: manoeuvres" is applied there, to what it is sent.
 *
 *   ON       the next maneuver at any distance, no bargraph: 2.0.0 and before.
 *   OFF      no HUD content (BAPBridge.setHudDisplayContent).  On the one HUD it
 *            was tried on (Q7 4M) this does not remove the navigation.
 *   FACTORY  what the factory navigation sends, recorded in a car (run-03 of that
 *            issue, three maneuvers): far from the turn a follow-the-road arrow
 *            with state 1; near the turn the real maneuver; on the last metres the
 *            distance bargraph with state 3, falling about once a second to zero,
 *            and state 4 when little of it is left.  No blinking.
 *            The far/near switch and its distances are from
 *            luka-dev/mib2q-carplay-rgi ("Guidance Maneuver Preview: Follow road
 *            when far"); the bar is fed by K3346HudBar.
 *
 * Pure logic, no stock classes: tested on a PC by K3346HudStyleTest.
 */
final class K3346HudStyle {

    static final int MODE_ON = 0;
    static final int MODE_OFF = 1;
    static final int MODE_FACTORY = 2;

    /* The bar runs over this share of the prepare distance: 300 m, 600 m on a
     * motorway.  The factory navigation started it at about 300 m in town. */
    static final int BAR_PERCENT_OF_PREPARE = 20;
    /* The factory navigation moves the bar about once a second. */
    static final int BAR_INTERVAL_MS = 1000;
    /* It reports "action" (state 4) from about this much of the bar down. */
    static final int ACTION_PERCENT = 13;

    static final int STATE_FOLLOW = 1;
    static final int STATE_PREPARE = 2;
    static final int STATE_DISTANCE = 3;
    static final int STATE_ACTION = 4;

    private K3346HudStyle() {
    }

    /* The mode file of K3346EtaRow: no file is ON, a half-written or unknown one
     * keeps what we had. */
    static int readMode(String path, int previous) {
        return K3346CapsuleFile.readCallCardMode(path, previous);
    }

    static boolean off(int mode) {
        return mode == MODE_OFF;
    }

    /* A follow-the-road arrow instead of the real maneuver. */
    static boolean farStraight(int mode, boolean showManeuver, boolean nowApproach) {
        return mode == MODE_FACTORY && showManeuver && !nowApproach;
    }

    static int barRange(int prepareThresholdM) {
        return prepareThresholdM <= 0 ? -1 : (prepareThresholdM * BAR_PERCENT_OF_PREPARE) / 100;
    }

    /* The bargraph runs from rangeM down to the maneuver and stays on at zero until
     * the maneuver is over, as the factory navigation leaves it.  A negative distance
     * is "none". */
    static boolean barOn(int mode, double distM, int rangeM) {
        return mode == MODE_FACTORY && rangeM > 0 && distM >= 0.0 && distM <= rangeM;
    }

    /* 100 at the start of the range, 0 at the maneuver. */
    static int barPercent(double distM, int rangeM) {
        if (rangeM <= 0 || distM <= 0.0) {
            return 0;
        }
        int percent = (int)((distM * 100.0) / rangeM);
        return percent > 100 ? 100 : percent;
    }

    /* The maneuver state of the factory navigation for what the HUD is shown. */
    static int state(boolean farStraight, boolean barOn, int barPercent) {
        if (farStraight) {
            return STATE_FOLLOW;
        }
        if (!barOn) {
            return STATE_PREPARE;
        }
        return barPercent <= ACTION_PERCENT ? STATE_ACTION : STATE_DISTANCE;
    }
}
