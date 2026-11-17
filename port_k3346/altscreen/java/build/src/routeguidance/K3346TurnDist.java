package com.luka.carplay.routeguidance;

/*
 * The distance to the maneuver as the overlay writes it over the VC (2.0.1-rc4).
 *
 * The VC prints that distance itself, above our maneuver tile, from BAP FctID 18.
 * When the same message carries the bargraph ("CarPlay HUD: Factory", the last
 * metres before the turn), the VC takes its number away: the factory navigation
 * draws a bar in the tile there, and the tile is ours (car, 2.0.1-rc3, run-10).
 * So for as long as the bargraph is on, the overlay prints the number in the
 * place the VC left empty (overlay/turn_dist.h), from the very values that went
 * out in FctID 18.
 *
 * The value is in tenths of the unit; the units are those BAPDistanceFormatter
 * (stock lsd.jar, convertFormattedDistanceUnit2BapUnit) hands to the cluster.
 * Metres are what the bar range gives in a metric car; the other units follow
 * the same rule and were not seen on a VC.
 *
 * Pure logic, no stock classes: tested on a PC by K3346TurnDistTest.
 */
final class K3346TurnDist {

    static final int UNIT_M = 0;
    static final int UNIT_KM = 1;
    static final int UNIT_YD = 2;
    static final int UNIT_FT = 3;
    static final int UNIT_MI = 4;

    private static final String[] NONE = {"", ""};

    private K3346TurnDist() {
    }

    /* Only while the VC hides its own number and shows our tile at all. */
    static boolean shown(boolean bargraphOn, boolean vcTile) {
        return bargraphOn && vcTile;
    }

    static String unit(int unit) {
        switch (unit) {
            case UNIT_M:  return "m";
            case UNIT_KM: return "km";
            case UNIT_YD: return "yd";
            case UNIT_FT: return "ft";
            case UNIT_MI: return "mi";
            default:      return "";
        }
    }

    /* The number and its unit; two empty strings for "nothing to print". */
    static String[] parts(int value, int unit) {
        String u = unit(unit);
        if (value <= 0 || u.length() == 0) {
            return none();
        }
        int whole = value / 10;
        int tenth = value % 10;
        boolean decimal = (unit == UNIT_KM || unit == UNIT_MI) && whole < 10 && tenth != 0;
        String number;
        if (decimal) {
            number = whole + (unit == UNIT_KM ? "," : ".") + tenth;
        } else {
            /* to the nearest whole unit, never "0 m" for something that is left */
            int rounded = (value + 5) / 10;
            number = String.valueOf(rounded < 1 ? 1 : rounded);
        }
        return new String[] {number, u};
    }

    static String[] none() {
        return new String[] {NONE[0], NONE[1]};
    }

    static String[] label(boolean bargraphOn, boolean vcTile, int value, int unit) {
        return shown(bargraphOn, vcTile) ? parts(value, unit) : none();
    }
}
