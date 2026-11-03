package com.luka.carplay.routeguidance;

/*
 * Text of the VC NavStatus bar while a route is active. The VC field is one line
 * of about 359 px, it cuts the end, and all four parts do not fit:
 *   big clocks    11:49 · 12 min · 1,2 km
 *   small clocks  the same, while capsule_text is 0 in the overlay conf; with 1
 *                 the overlay draws "11:49 · 12 min" and "1,2 km" at the ends of
 *                 the bar and the field keeps the street (too dark, run-53)
 *   no overlay    11:49 · 12 min · Street
 * No HMI dependencies.
 */
final class K3346NavBarText {

    private static final String SEPARATOR = " · ";
    private static final int TIME_FORMAT_12H = 1;
    private static final int STREET_MAX_CHARS = 22;
    private static final String NO_STREET = "·";
    private static final int ARRIVED_METERS = 200;

    static final int SLOT_NONE = -1;
    static final int SLOT_SMALL = 0;
    static final int SLOT_BIG = 1;
    private static final int ETA_MAP = 1;   /* K3346CapsuleFile.ETA_MAP */

    static final class Capsule {
        final String field;
        final String left;
        final String right;

        Capsule(String field, String left, String right) {
            this.field = field;
            this.left = left;
            this.right = right;
        }
    }

    private K3346NavBarText() {
    }

    /* localMs is already shifted to local time: no JVM time zone is involved. */
    static String arrival(long localMs, int timeFormat) {
        if (localMs < 0L) {
            return "";
        }
        long minutes = localMs / 60000L;
        int minute = (int)(minutes % 60L);
        int hour = (int)((minutes / 60L) % 24L);
        StringBuffer out = new StringBuffer();
        if (timeFormat == TIME_FORMAT_12H) {
            int hour12 = hour % 12;
            out.append(hour12 == 0 ? 12 : hour12);
            appendMinute(out, minute);
            out.append(hour >= 12 ? " PM" : " AM");
        } else {
            if (hour < 10) out.append('0');
            out.append(hour);
            appendMinute(out, minute);
        }
        return out.toString();
    }

    private static void appendMinute(StringBuffer out, int minute) {
        out.append(':');
        if (minute < 10) out.append('0');
        out.append(minute);
    }

    static String remaining(long seconds) {
        if (seconds < 0L) {
            return "";
        }
        long minutes = (seconds + 59L) / 60L;
        if (minutes < 60L) {
            return minutes + " min";
        }
        long rest = minutes % 60L;
        return (minutes / 60L) + " h " + (rest < 10L ? "0" : "") + rest;
    }

    /* With big clocks the call card takes the place of the ETA box, so the time moves
     * here for the length of the call. Same table: native/eta_mode.h. */
    static boolean timesOnBar(int etaMode, boolean callVisible, int slot) {
        return etaMode != ETA_MAP || (callVisible && slot == SLOT_BIG);
    }

    /* iOS sends 0 before it has a route time (run-52, distance not known yet).
     * Near the destination 0 is true. */
    static boolean timeBogus(long seconds, int distDestM) {
        return seconds == 0L && !(distDestM > 0 && distDestM <= ARRIVED_METERS);
    }

    static String distance(int meters, boolean metric) {
        if (meters <= 0) {
            return "";
        }
        if (!metric) {
            long tenths = (meters * 10000L + 804672L) / 1609344L;
            if (tenths < 1L) {
                tenths = 1L;
            }
            return tenths < 100L
                ? (tenths / 10L) + "." + (tenths % 10L) + " mi"
                : ((tenths + 5L) / 10L) + " mi";
        }
        if (meters < 975) {
            long rounded = (meters + 25L) / 50L * 50L;
            return (rounded < 50L ? 50L : rounded) + " m";
        }
        long tenths = (meters + 50L) / 100L;
        return tenths < 100L
            ? (tenths / 10L) + "," + (tenths % 10L) + " km"
            : ((meters + 500L) / 1000L) + " km";
    }

    static String limitStreet(String street, int maxChars) {
        String trimmed = street == null ? "" : street.trim();
        if (trimmed.length() <= maxChars) {
            return trimmed;
        }
        int end = maxChars;
        /* never half of a surrogate pair */
        char last = trimmed.charAt(end - 1);
        if (last >= 0xD800 && last <= 0xDBFF) {
            end--;
        }
        return trimmed.substring(0, end).trim() + "…";
    }

    /* road is the plain text for "no times known": it may carry a turn arrow. */
    static Capsule capsule(int slot, boolean draw, String arrival, String remaining,
                           String distance, String street, String road) {
        String times = compose(arrival, remaining, "");
        if (times.length() == 0) {
            return new Capsule(road, "", "");
        }
        if (slot == SLOT_SMALL && draw) {
            String field = limitStreet(street, STREET_MAX_CHARS);
            /* the VC puts its own word in an empty field ("offroad") */
            return new Capsule(field.length() == 0 ? NO_STREET : field, times, distance);
        }
        boolean layoutKnown = slot == SLOT_SMALL || slot == SLOT_BIG;
        if (!layoutKnown || distance.length() == 0) {
            return new Capsule(compose(arrival, remaining, street), "", "");
        }
        return new Capsule(compose(arrival, remaining, distance), "", "");
    }

    static String compose(String arrival, String remaining, String street) {
        StringBuffer out = new StringBuffer();
        append(out, arrival);
        append(out, remaining);
        append(out, street);
        return out.toString();
    }

    private static void append(StringBuffer out, String part) {
        if (part == null) {
            return;
        }
        String trimmed = part.trim();
        if (trimmed.length() == 0) {
            return;
        }
        if (out.length() > 0) {
            out.append(SEPARATOR);
        }
        out.append(trimmed);
    }
}
