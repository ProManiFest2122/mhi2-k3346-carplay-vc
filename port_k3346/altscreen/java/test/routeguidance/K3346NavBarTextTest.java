package com.luka.carplay.routeguidance;

/* Tekst kapsuly VC: godzina przyjazdu, czas jazdy, ulica. Bez auta. */
public final class K3346NavBarTextTest {

    private static final long MIN = 60000L;
    private static final long HOUR = 60L * MIN;
    private static final long DAY = 24L * HOUR;

    private static int ok;
    private static int fail;

    private static void eq(String name, String want, String got) {
        if (want.equals(got)) {
            ok++;
            System.out.println("  OK   " + name);
        } else {
            fail++;
            System.out.println("  FAIL " + name + " oczekiwano=[" + want + "] jest=[" + got + "]");
        }
    }

    /* nakladka rysuje wlasna ramke czasu */
    private static void times(String name, int mode, boolean call, int slot, boolean want) {
        eq(name, String.valueOf(want), String.valueOf(K3346NavBarText.timesOnBar(mode, call, slot, true)));
    }

    /* nakladka nie moze narysowac ramki: czas nie moze zniknac */
    private static void noPill(String name, int mode, boolean call, int slot, boolean want) {
        eq(name, String.valueOf(want), String.valueOf(K3346NavBarText.timesOnBar(mode, call, slot, false)));
    }

    private static void pill(String name, int mode, boolean ready, boolean valid, String arrival,
                             String remaining, String distance, String wantLeft, String wantRight) {
        String[] p = K3346NavBarText.etaPill(mode, ready, valid, arrival, remaining, distance);
        eq(name + ": dwie linie", "2", String.valueOf(p.length));
        eq(name + ": godzina", wantLeft, p[0]);
        eq(name + ": reszta", wantRight, p[1]);
    }

    private static void capsule(String name, int slot, boolean draw, String arrival,
                                String remaining, String distance, String street, String road,
                                String wantField, String wantLeft, String wantRight) {
        K3346NavBarText.Capsule c =
            K3346NavBarText.capsule(slot, draw, arrival, remaining, distance, street, road);
        eq(name + ": pole", wantField, c.field);
        eq(name + ": lewa", wantLeft, c.left);
        eq(name + ": prawa", wantRight, c.right);
    }

    public static void main(String[] argv) {
        /* nie wielokrotnosc 10 dni: inaczej "godzina modulo 240" wyglada jak modulo 24 */
        long epochDay = 20393L * DAY;

        System.out.println("=== godzina 24 h");
        eq("11:49", "11:49", K3346NavBarText.arrival(epochDay + 11 * HOUR + 49 * MIN, 0));
        eq("zera wiodace", "07:05", K3346NavBarText.arrival(epochDay + 7 * HOUR + 5 * MIN, 0));
        eq("polnoc", "00:00", K3346NavBarText.arrival(epochDay, 0));
        eq("sekundy nie zaokraglaja", "23:59", K3346NavBarText.arrival(epochDay + 23 * HOUR + 59 * MIN + 59000L, 0));
        eq("ujemny czas pusty", "", K3346NavBarText.arrival(-1L, 0));

        System.out.println("=== godzina 12 h");
        eq("rano", "9:05 AM", K3346NavBarText.arrival(epochDay + 9 * HOUR + 5 * MIN, 1));
        eq("po poludniu", "1:30 PM", K3346NavBarText.arrival(epochDay + 13 * HOUR + 30 * MIN, 1));
        eq("polnoc", "12:00 AM", K3346NavBarText.arrival(epochDay, 1));
        eq("poludnie", "12:00 PM", K3346NavBarText.arrival(epochDay + 12 * HOUR, 1));

        System.out.println("=== czas jazdy");
        eq("0 s", "0 min", K3346NavBarText.remaining(0L));
        eq("1 s w gore", "1 min", K3346NavBarText.remaining(1L));
        eq("59 s", "1 min", K3346NavBarText.remaining(59L));
        eq("60 s", "1 min", K3346NavBarText.remaining(60L));
        eq("61 s w gore", "2 min", K3346NavBarText.remaining(61L));
        eq("59 min", "59 min", K3346NavBarText.remaining(59L * 60L));
        eq("60 min", "1 h 00", K3346NavBarText.remaining(60L * 60L));
        eq("65 min", "1 h 05", K3346NavBarText.remaining(65L * 60L));
        eq("125 min", "2 h 05", K3346NavBarText.remaining(125L * 60L));
        eq("11 h 30", "11 h 30", K3346NavBarText.remaining(690L * 60L));
        eq("ujemny pusty", "", K3346NavBarText.remaining(-1L));

        System.out.println("=== sklad tekstu");
        eq("wszystko", "11:49 · 12 min · <ulica>",
            K3346NavBarText.compose("11:49", "12 min", "<ulica>"));
        eq("bez ulicy", "11:49 · 12 min", K3346NavBarText.compose("11:49", "12 min", ""));
        eq("ulica null", "11:49 · 12 min", K3346NavBarText.compose("11:49", "12 min", null));
        eq("ulica ze spacji", "11:49 · 12 min", K3346NavBarText.compose("11:49", "12 min", "   "));
        eq("sama godzina i ulica", "11:49 · <ulica>", K3346NavBarText.compose("11:49", "", "<ulica>"));
        eq("sam czas i ulica", "12 min · <ulica>", K3346NavBarText.compose("", "12 min", "<ulica>"));
        eq("sama ulica", "<ulica>", K3346NavBarText.compose("", "", "<ulica>"));
        eq("nic", "", K3346NavBarText.compose("", "", ""));

        System.out.println("=== czas niewiarygodny");
        eq("0 s, dystans nieznany", "true", String.valueOf(K3346NavBarText.timeBogus(0L, -1)));
        eq("0 s, dystans 0", "true", String.valueOf(K3346NavBarText.timeBogus(0L, 0)));
        eq("0 s, daleko", "true", String.valueOf(K3346NavBarText.timeBogus(0L, 201)));
        eq("0 s, u celu", "false", String.valueOf(K3346NavBarText.timeBogus(0L, 200)));
        eq("0 s, tuz u celu", "false", String.valueOf(K3346NavBarText.timeBogus(0L, 1)));
        eq("1 s, dystans nieznany", "false", String.valueOf(K3346NavBarText.timeBogus(1L, -1)));
        eq("brak czasu to nie zero", "false", String.valueOf(K3346NavBarText.timeBogus(-1L, -1)));

        System.out.println("=== czas w kapsule: tabela z native/eta_mode.h");
        times("Kapsula, male, bez rozmowy", 0, false, 0, true);
        times("Kapsula, duze, bez rozmowy", 0, false, 1, true);
        times("Kapsula, male, rozmowa", 0, true, 0, true);
        times("Kapsula, duze, rozmowa", 0, true, 1, true);
        times("Mapa, male, bez rozmowy", 1, false, 0, false);
        times("Mapa, duze, bez rozmowy", 1, false, 1, false);
        times("Mapa, male, rozmowa", 1, true, 0, false);
        times("Mapa, duze, rozmowa", 1, true, 1, true);
        times("Obie, male, bez rozmowy", 2, false, 0, true);
        times("Obie, duze, bez rozmowy", 2, false, 1, true);
        times("Obie, male, rozmowa", 2, true, 0, true);
        times("Obie, duze, rozmowa", 2, true, 1, true);
        times("Mapa, uklad nieznany, rozmowa", 1, true, -1, false);
        /* rc29: bez ramki z nakladki czas jest na pasku w kazdym trybie */
        noPill("bez ramki, Mapa, male", 1, false, 0, true);
        noPill("bez ramki, Mapa, duze", 1, false, 1, true);
        noPill("bez ramki, Mapa, uklad nieznany", 1, false, -1, true);
        noPill("bez ramki, Kapsula, male", 0, false, 0, true);
        noPill("bez ramki, Obie, duze, rozmowa", 2, true, 1, true);

        System.out.println("=== wlasna ramka czasu (overlay/eta_pill.h)");
        pill("Mapa", 1, true, true, "06:43", "15 min", "12 km", "06:43", "15 min · 12 km");
        pill("Obie", 2, true, true, "06:43", "15 min", "12 km", "06:43", "15 min · 12 km");
        pill("Kapsula: bez ramki", 0, true, true, "06:43", "15 min", "12 km", "", "");
        pill("nakladka nie rysuje", 1, false, true, "06:43", "15 min", "12 km", "", "");
        pill("czas niewiarygodny", 1, true, false, "06:43", "0 min", "12 km", "", "");
        pill("brak godziny", 1, true, true, "", "15 min", "12 km", "", "");
        pill("godzina null", 1, true, true, null, "15 min", "12 km", "", "");
        pill("godzina ze spacji", 2, true, true, "  ", "15 min", "12 km", "", "");
        pill("bez dystansu", 1, true, true, "06:43", "15 min", "", "06:43", "15 min");
        pill("bez czasu jazdy", 1, true, true, "06:43", "", "12 km", "06:43", "12 km");
        pill("sama godzina", 1, true, true, "06:43", "", "", "06:43", "");
        pill("ponad godzine", 2, true, true, "13:05", "1 h 05", "123 km", "13:05", "1 h 05 · 123 km");
        pill("12 h", 1, true, true, " 1:30 PM ", "15 min", "0.6 mi", "1:30 PM", "15 min · 0.6 mi");
        pill("tryb spoza zakresu", 3, true, true, "06:43", "15 min", "12 km", "", "");

        System.out.println("=== dystans km");
        eq("brak", "", K3346NavBarText.distance(-1, true));
        eq("zero", "", K3346NavBarText.distance(0, true));
        eq("1 m", "50 m", K3346NavBarText.distance(1, true));
        eq("74 m", "50 m", K3346NavBarText.distance(74, true));
        eq("75 m", "100 m", K3346NavBarText.distance(75, true));
        eq("600 m", "600 m", K3346NavBarText.distance(600, true));
        eq("974 m", "950 m", K3346NavBarText.distance(974, true));
        eq("975 m", "1,0 km", K3346NavBarText.distance(975, true));
        eq("1000 m", "1,0 km", K3346NavBarText.distance(1000, true));
        eq("1249 m", "1,2 km", K3346NavBarText.distance(1249, true));
        eq("1250 m", "1,3 km", K3346NavBarText.distance(1250, true));
        eq("9949 m", "9,9 km", K3346NavBarText.distance(9949, true));
        eq("9950 m", "10 km", K3346NavBarText.distance(9950, true));
        eq("12499 m", "12 km", K3346NavBarText.distance(12499, true));
        eq("12500 m", "13 km", K3346NavBarText.distance(12500, true));
        eq("512 km", "512 km", K3346NavBarText.distance(512000, true));

        System.out.println("=== dystans mile");
        eq("brak", "", K3346NavBarText.distance(0, false));
        eq("1 m", "0.1 mi", K3346NavBarText.distance(1, false));
        eq("1 mila", "1.0 mi", K3346NavBarText.distance(1609, false));
        eq("2,5 mili", "2.5 mi", K3346NavBarText.distance(4023, false));
        eq("9,9 mili", "9.9 mi", K3346NavBarText.distance(15933, false));
        eq("10 mil", "10 mi", K3346NavBarText.distance(16093, false));
        eq("100 mil", "100 mi", K3346NavBarText.distance(160934, false));

        System.out.println("=== ulica w polu VC");
        eq("krotka", "<ulica>", K3346NavBarText.limitStreet("  <ulica> ", 22));
        eq("null", "", K3346NavBarText.limitStreet(null, 22));
        eq("rowno limit", "1234567890", K3346NavBarText.limitStreet("1234567890", 10));
        eq("o znak za dluga", "1234567890…", K3346NavBarText.limitStreet("12345678901", 10));
        eq("bez spacji przed wielokropkiem", "Aleja…", K3346NavBarText.limitStreet("Aleja Jana", 6));
        eq("bez polowy pary znakow", "ab…", K3346NavBarText.limitStreet("ab🚗cd", 3));
        eq("cala para zostaje", "ab🚗…", K3346NavBarText.limitStreet("ab🚗cd", 4));

        System.out.println("=== kapsula");
        capsule("male, rysujemy", K3346NavBarText.SLOT_SMALL, true,
            "14:25", "3 min", "1,2 km", "<ulica>", "x",
            "<ulica>", "14:25 · 3 min", "1,2 km");
        capsule("male, bez ulicy: pole nie jest puste", K3346NavBarText.SLOT_SMALL, true,
            "14:25", "3 min", "1,2 km", "", "x",
            "·", "14:25 · 3 min", "1,2 km");
        capsule("male, bez dystansu", K3346NavBarText.SLOT_SMALL, true,
            "14:25", "3 min", "", "<ulica>", "x",
            "<ulica>", "14:25 · 3 min", "");
        capsule("male, dluga ulica", K3346NavBarText.SLOT_SMALL, true,
            "14:25", "3 min", "1,2 km", "Aleja Jana Pawła II nr 12345", "x",
            "Aleja Jana Pawła II nr…", "14:25 · 3 min", "1,2 km");
        capsule("male, sama godzina", K3346NavBarText.SLOT_SMALL, true,
            "14:25", "", "1,2 km", "<ulica>", "x",
            "<ulica>", "14:25", "1,2 km");
        capsule("male, nie rysujemy", K3346NavBarText.SLOT_SMALL, false,
            "14:25", "3 min", "1,2 km", "<ulica>", "x",
            "14:25 · 3 min · 1,2 km", "", "");
        capsule("duze", K3346NavBarText.SLOT_BIG, true,
            "14:25", "3 min", "1,2 km", "<ulica>", "x",
            "14:25 · 3 min · 1,2 km", "", "");
        capsule("duze, bez dystansu", K3346NavBarText.SLOT_BIG, true,
            "14:25", "3 min", "", "<ulica>", "x",
            "14:25 · 3 min · <ulica>", "", "");
        capsule("brak nakladki", K3346NavBarText.SLOT_NONE, false,
            "14:25", "3 min", "1,2 km", "<ulica>", "x",
            "14:25 · 3 min · <ulica>", "", "");
        capsule("bez czasu, male", K3346NavBarText.SLOT_SMALL, true,
            "", "", "1,2 km", "<ulica>", "→ <ulica>",
            "→ <ulica>", "", "");
        capsule("bez czasu, duze", K3346NavBarText.SLOT_BIG, true,
            "", "", "1,2 km", "<ulica>", "→ <ulica>",
            "→ <ulica>", "", "");

        System.out.println("WYNIK_NAV_BAR: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }
}
