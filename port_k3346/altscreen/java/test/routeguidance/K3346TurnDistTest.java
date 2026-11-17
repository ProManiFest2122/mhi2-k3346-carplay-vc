package com.luka.carplay.routeguidance;

/*
 * Liczba metrow do skretu, ktora nakladka pisze na zegarach, gdy zegary chowaja
 * swoja (2.0.1-rc4, tryb "HUD CarPlay: Fabryczny" na ostatnich metrach). Czysta
 * logika, bez auta.
 */
public final class K3346TurnDistTest {

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

    private static String show(String[] p) {
        return p.length + ":" + p[0] + "|" + p[1];
    }

    public static void main(String[] argv) {
        System.out.println("=== kiedy piszemy: tylko ze slupkiem i tylko gdy zegary maja nasz kafelek");
        eq("slupek, kafelek", "true", String.valueOf(K3346TurnDist.shown(true, true)));
        eq("bez slupka zegary pisza same", "false", String.valueOf(K3346TurnDist.shown(false, true)));
        eq("strzalki wylaczone: nic", "false", String.valueOf(K3346TurnDist.shown(true, false)));
        eq("ani slupka, ani kafelka", "false", String.valueOf(K3346TurnDist.shown(false, false)));

        System.out.println("=== metry: wartosc BAP jest w dziesiatych czesciach (auto, run-10)");
        eq("2000 -> 200 m", "2:200|m", show(K3346TurnDist.parts(2000, K3346TurnDist.UNIT_M)));
        eq("3000 -> 300 m", "2:300|m", show(K3346TurnDist.parts(3000, K3346TurnDist.UNIT_M)));
        eq("900 -> 90 m", "2:90|m", show(K3346TurnDist.parts(900, K3346TurnDist.UNIT_M)));
        eq("220 -> 22 m (zdjecie zegarow)", "2:22|m", show(K3346TurnDist.parts(220, K3346TurnDist.UNIT_M)));
        eq("100 -> 10 m", "2:10|m", show(K3346TurnDist.parts(100, K3346TurnDist.UNIT_M)));
        eq("10 -> 1 m", "2:1|m", show(K3346TurnDist.parts(10, K3346TurnDist.UNIT_M)));
        eq("6000 -> 600 m (autostrada)", "2:600|m", show(K3346TurnDist.parts(6000, K3346TurnDist.UNIT_M)));
        eq("metry bez ulamka: 225 -> 23", "2:23|m", show(K3346TurnDist.parts(225, K3346TurnDist.UNIT_M)));
        eq("cos zostalo, nie 0 m: 4 -> 1", "2:1|m", show(K3346TurnDist.parts(4, K3346TurnDist.UNIT_M)));

        System.out.println("=== brak odleglosci");
        eq("-1 (niewazna)", "2:|", show(K3346TurnDist.parts(-1, K3346TurnDist.UNIT_M)));
        eq("0", "2:|", show(K3346TurnDist.parts(0, K3346TurnDist.UNIT_M)));
        eq("nieznana jednostka 255", "2:|", show(K3346TurnDist.parts(2000, 255)));
        eq("nieznana jednostka 5", "2:|", show(K3346TurnDist.parts(2000, 5)));
        eq("ujemna jednostka", "2:|", show(K3346TurnDist.parts(2000, -1)));

        System.out.println("=== pozostale jednostki BAP (BAPDistanceFormatter)");
        eq("km z ulamkiem, przecinek", "2:1,2|km", show(K3346TurnDist.parts(12, K3346TurnDist.UNIT_KM)));
        eq("km pelne", "2:2|km", show(K3346TurnDist.parts(20, K3346TurnDist.UNIT_KM)));
        eq("km od 10 bez ulamka", "2:15|km", show(K3346TurnDist.parts(154, K3346TurnDist.UNIT_KM)));
        eq("jardy", "2:300|yd", show(K3346TurnDist.parts(3000, K3346TurnDist.UNIT_YD)));
        eq("stopy", "2:500|ft", show(K3346TurnDist.parts(5000, K3346TurnDist.UNIT_FT)));
        eq("mile z ulamkiem, kropka", "2:0.2|mi", show(K3346TurnDist.parts(2, K3346TurnDist.UNIT_MI)));
        eq("mile pelne", "2:3|mi", show(K3346TurnDist.parts(30, K3346TurnDist.UNIT_MI)));
        eq("nazwa jednostki 0", "m", K3346TurnDist.unit(0));
        eq("nazwa jednostki 1", "km", K3346TurnDist.unit(1));
        eq("nazwa jednostki 2", "yd", K3346TurnDist.unit(2));
        eq("nazwa jednostki 3", "ft", K3346TurnDist.unit(3));
        eq("nazwa jednostki 4", "mi", K3346TurnDist.unit(4));

        System.out.println("=== calosc");
        eq("slupek, kafelek, 200 m", "2:200|m", show(K3346TurnDist.label(true, true, 2000, 0)));
        eq("bez slupka: pusto", "2:|", show(K3346TurnDist.label(false, true, 2000, 0)));
        eq("bez kafelka: pusto", "2:|", show(K3346TurnDist.label(true, false, 2000, 0)));
        eq("slupek bez odleglosci: pusto", "2:|", show(K3346TurnDist.label(true, true, -1, 0)));
        String[] a = K3346TurnDist.none();
        a[0] = "x";
        eq("none() nie jest wspolna tablica", "2:|", show(K3346TurnDist.none()));

        System.out.println("WYNIK_TURN_DIST: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }
}
