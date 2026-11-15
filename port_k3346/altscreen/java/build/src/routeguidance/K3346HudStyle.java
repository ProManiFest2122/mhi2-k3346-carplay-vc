package com.luka.carplay.routeguidance;

/*
 * What the MMI option "CarPlay HUD" asks of the maneuver output (issue #15).
 *
 * The maneuver descriptor, its distance and its state travel in one BAP stream to
 * the instrument cluster, which feeds the head-up display.  The car's own setting
 * "Navigation messages: manoeuvres" is applied there, to what it is sent.
 *
 *   ON       the next maneuver at any distance, no bargraph: 2.0.0 and before.
 *   OFF      no HUD content (BAPBridge.setHudDisplayContent).
 *   FACTORY  far from the turn a follow-the-road arrow, the real maneuver from the
 *            prepare distance on, and the distance bargraph on the last metres,
 *            blinking at the very end.  This is what luka-dev/mib2q-carplay-rgi
 *            sends with "Guidance Maneuver Preview: Follow road when far"
 *            (BAPBridge.java showFollowStraight, getBargraphDenominatorM and the
 *            action blink); the numbers below are his.
 *
 * Pure logic, no stock classes: tested on a PC by K3346HudStyleTest.
 */
final class K3346HudStyle {

    static final int MODE_ON = 0;
    static final int MODE_OFF = 1;
    static final int MODE_FACTORY = 2;

    /* The bargraph blinks below this share of its range. */
    static final int BLINK_PERCENT = 20;
    static final int BLINK_INTERVAL_MS = 600;

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

    /* The bargraph runs from denominatorM down to the maneuver. */
    static boolean barOn(int mode, int distM, int denominatorM) {
        return mode == MODE_FACTORY && denominatorM > 0 && distM > 0 && distM <= denominatorM;
    }

    /* 100 at the start of the range, 0 at the maneuver. */
    static int barPercent(int distM, int denominatorM) {
        if (denominatorM <= 0 || distM <= 0) {
            return 0;
        }
        int percent = (int)(((long)distM * 100L) / denominatorM);
        return percent > 100 ? 100 : percent;
    }

    static boolean blinking(int mode, int distM, int denominatorM) {
        return barOn(mode, distM, denominatorM) && barPercent(distM, denominatorM) < BLINK_PERCENT;
    }

    /* One blink phase: the bar full or empty, still switched on. */
    static int blinkPercent(boolean full) {
        return full ? 100 : 0;
    }
}
