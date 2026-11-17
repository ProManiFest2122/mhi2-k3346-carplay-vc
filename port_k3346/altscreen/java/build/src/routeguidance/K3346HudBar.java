package com.luka.carplay.routeguidance;

/*
 * Where the car is between two distance updates of the phone (issue #15).
 *
 * The factory navigation moves the HUD bargraph about once a second by a few
 * points (car log run-03: 99, 94, 89, 83 ... 2, 0).  The phone gives the distance
 * to the maneuver in coarse labels: Google Maps every 100 m above 100 m and every
 * 10 m below, each rounded to the nearest label, so "200" arrives at about 250 m
 * (same log: 300@709 s, 200@717 s, 100@724 s, 90@729 s, at a steady 12.7 m/s).
 * A bar computed from the label alone jumps (88, 44, 40 ...).
 *
 * So: a label arrives where its range begins (label + half the step from the
 * label before); the speed comes from the last two arrivals; between labels the
 * estimate moves on with that speed, but never past the point where the next
 * label is due.  The estimate never rises within one maneuver.
 *
 * Pure logic, the time is passed in: tested on a PC by K3346HudStyleTest with the
 * labels of that drive.
 */
final class K3346HudBar {

    /* A step between two labels above this is not a step but another route. */
    static final int MAX_STEP_M = 500;
    /* Above this the last two arrivals were not a drive (a GPS jump). */
    static final double MAX_SPEED = 70.0;
    /* Below this many seconds between two labels the speed says nothing. */
    static final double MIN_INTERVAL_S = 0.3;
    /* The bar never falls faster than this many points a second.  Where the speed
     * is not known (a route that starts next to the turn, a car that stood still)
     * a new label can be far below the estimate; the bar then glides down to it.
     * The factory navigation moved 2 to 5 points a second. */
    static final double MAX_FALL_PER_S = 8.0;

    private int label = -1;
    private long labelMs;
    private double startM;
    private double lowerM;
    private double speed;
    private boolean arrived;   /* this label came in as a step, so its start is known */
    private double estM = -1.0;
    private int shown = -1;
    private long shownMs;

    void reset() {
        label = -1;
        speed = 0.0;
        arrived = false;
        estM = -1.0;
        shown = -1;
    }

    int label() {
        return label;
    }

    /* The lowest value the estimate can reach while this label holds, -1 without one. */
    double lowerBound() {
        return label < 0 ? -1.0 : lowerM;
    }

    /* A distance from the phone, in metres. */
    void label(int meters, long nowMs) {
        if (meters <= 0) {
            reset();
            return;
        }
        if (meters == label) {
            return;
        }
        int step = label - meters;
        if (label < 0 || step <= 0 || step > MAX_STEP_M) {
            /* the first label, or the distance grew: nothing is known but the label */
            label = meters;
            labelMs = nowMs;
            startM = meters;
            lowerM = meters;
            speed = 0.0;
            arrived = false;
            estM = -1.0;
            shown = -1;
            return;
        }
        double newStart = meters + step / 2.0;
        double dt = (nowMs - labelMs) / 1000.0;
        if (arrived && dt >= MIN_INTERVAL_S) {
            double v = (startM - newStart) / dt;
            speed = (v > 0.0 && v <= MAX_SPEED) ? v : 0.0;
        } else {
            speed = 0.0;
        }
        label = meters;
        labelMs = nowMs;
        startM = newStart;
        lowerM = lower(meters, step);
        arrived = true;
    }

    /* Where the next label is due.  A label as large as its own step is the last one
     * of that size (100 after 200): finer labels follow, a tenth of the step, unless
     * the step is already the finest one, then the maneuver itself is next. */
    static double lower(int meters, int step) {
        if (meters > step) {
            return meters - step / 2.0;
        }
        if (step > 10) {
            return meters - step / 20.0;
        }
        return 0.0;
    }

    /* The distance now, in metres; -1 without a label. */
    double estimate(long nowMs) {
        if (label < 0) {
            return -1.0;
        }
        double d = startM;
        if (speed > 0.0) {
            d = startM - speed * ((nowMs - labelMs) / 1000.0);
        }
        if (d > startM) {
            d = startM;   /* the unit's clock was set back in between */
        }
        if (d < lowerM) {
            d = lowerM;
        }
        if (estM >= 0.0 && d > estM) {
            d = estM;
        }
        estM = d;
        return d;
    }

    /* The bar to show now over rangeM: 100 at the start of the range, 0 at the
     * maneuver, -1 while the car is not within it.  It never rises and never falls
     * faster than MAX_FALL_PER_S. */
    int percent(long nowMs, int rangeM) {
        double est = estimate(nowMs);
        if (est < 0.0 || rangeM <= 0 || est > rangeM) {
            shown = -1;
            return -1;
        }
        int target = K3346HudStyle.barPercent(est, rangeM);
        if (shown < 0) {
            shown = target;
        } else if (target < shown) {
            double dt = (nowMs - shownMs) / 1000.0;
            int maxFall = dt > 0.0 ? (int)(MAX_FALL_PER_S * dt) : 0;
            if (maxFall < 1) {
                maxFall = 1;
            }
            shown = Math.max(target, shown - maxFall);
        }
        shownMs = nowMs;
        return shown;
    }
}
