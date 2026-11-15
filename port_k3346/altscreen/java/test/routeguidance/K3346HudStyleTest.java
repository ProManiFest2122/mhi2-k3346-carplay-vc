package com.luka.carplay.routeguidance;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.InputStreamReader;

/*
 * Opcja "HUD CarPlay" (zgloszenie #15): co idzie do zegarow w kazdym trybie.
 * Bez auta.  Z argumentem: plik z odleglosciami do manewru w metrach, po jednej
 * w linii (wyciete z logu z auta) - przebieg przez logike w trybie Wl. i Fabryczny.
 */
public final class K3346HudStyleTest {

    private static final int ON = K3346HudStyle.MODE_ON;
    private static final int OFF = K3346HudStyle.MODE_OFF;
    private static final int FACTORY = K3346HudStyle.MODE_FACTORY;

    /* BAPBridge: prog przygotowania i strefa akcji (15% progu). */
    private static final int CITY_PREPARE = 1500;
    private static final int CITY_ACTION = 225;
    private static final int HIGHWAY_ACTION = 450;

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

    private static void eq(String name, int want, int got) {
        eq(name, String.valueOf(want), String.valueOf(got));
    }

    private static void eq(String name, boolean want, boolean got) {
        eq(name, String.valueOf(want), String.valueOf(got));
    }

    private static void replay(String path) throws Exception {
        System.out.println("=== przebieg z auta: " + path);
        BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(path), "US-ASCII"));
        int lines = 0, onStraight = 0, onBar = 0, facStraight = 0, facReal = 0, facBar = 0, facBlink = 0;
        int farthest = 0;
        boolean farthestStraight = false;
        String line;
        while ((line = in.readLine()) != null) {
            line = line.trim();
            if (line.length() == 0) {
                continue;
            }
            int dist = Integer.parseInt(line);
            if (dist <= 0) {
                continue;
            }
            lines++;
            boolean approach = dist <= CITY_PREPARE;
            if (K3346HudStyle.farStraight(ON, true, approach)) {
                onStraight++;
            }
            if (K3346HudStyle.barOn(ON, dist, CITY_ACTION)) {
                onBar++;
            }
            boolean straight = K3346HudStyle.farStraight(FACTORY, true, approach);
            if (straight) {
                facStraight++;
            } else {
                facReal++;
            }
            if (K3346HudStyle.barOn(FACTORY, dist, CITY_ACTION)) {
                facBar++;
            }
            if (K3346HudStyle.blinking(FACTORY, dist, CITY_ACTION)) {
                facBlink++;
            }
            if (dist > farthest) {
                farthest = dist;
                farthestStraight = straight;
            }
        }
        in.close();
        System.out.println("  aktualizacji z odlegloscia: " + lines + ", najdalszy manewr: " + farthest + " m");
        System.out.println("  Wl.:       prosta strzalka " + onStraight + ", slupek " + onBar);
        System.out.println("  Fabryczny: prosta strzalka " + facStraight + ", prawdziwy manewr " + facReal
            + ", slupek " + facBar + ", w tym mruganie " + facBlink);
        eq("przebieg: sa dane", true, lines > 0);
        eq("przebieg Wl.: nigdy prosta strzalka", 0, onStraight);
        eq("przebieg Wl.: nigdy slupek", 0, onBar);
        eq("przebieg Fabryczny: najdalszy manewr to prosta strzalka", farthest > CITY_PREPARE, farthestStraight);
        eq("przebieg Fabryczny: kazda aktualizacja ma jedno z dwojga", lines, facStraight + facReal);
    }

    public static void main(String[] argv) throws Exception {
        System.out.println("=== wartosci wspolne z wierszem MMI (K3346EtaRow)");
        eq("wl", 0, ON);
        eq("wyl", 1, OFF);
        eq("fabryczny", 2, FACTORY);
        eq("wyl to wyl", true, K3346HudStyle.off(OFF));
        eq("wl to nie wyl", false, K3346HudStyle.off(ON));
        eq("fabryczny to nie wyl", false, K3346HudStyle.off(FACTORY));

        System.out.println("=== prosta strzalka daleko od skretu");
        eq("fabryczny, manewr, daleko", true, K3346HudStyle.farStraight(FACTORY, true, false));
        eq("fabryczny, manewr, w strefie", false, K3346HudStyle.farStraight(FACTORY, true, true));
        eq("fabryczny, bez manewru", false, K3346HudStyle.farStraight(FACTORY, false, false));
        eq("wl, daleko: prawdziwy manewr jak w 2.0.0", false, K3346HudStyle.farStraight(ON, true, false));
        eq("wl, w strefie", false, K3346HudStyle.farStraight(ON, true, true));
        eq("wyl, daleko", false, K3346HudStyle.farStraight(OFF, true, false));
        eq("nieznany tryb jak wl", false, K3346HudStyle.farStraight(7, true, false));

        System.out.println("=== slupek: wlaczony");
        eq("fabryczny, poczatek strefy", true, K3346HudStyle.barOn(FACTORY, 225, CITY_ACTION));
        eq("fabryczny, metr przed strefa", false, K3346HudStyle.barOn(FACTORY, 226, CITY_ACTION));
        eq("fabryczny, 1 m", true, K3346HudStyle.barOn(FACTORY, 1, CITY_ACTION));
        eq("fabryczny, 0 m", false, K3346HudStyle.barOn(FACTORY, 0, CITY_ACTION));
        eq("fabryczny, brak odleglosci", false, K3346HudStyle.barOn(FACTORY, -1, CITY_ACTION));
        eq("fabryczny, brak strefy", false, K3346HudStyle.barOn(FACTORY, 100, -1));
        eq("fabryczny, strefa 0", false, K3346HudStyle.barOn(FACTORY, 100, 0));
        eq("fabryczny, autostrada 450", true, K3346HudStyle.barOn(FACTORY, 450, HIGHWAY_ACTION));
        eq("fabryczny, autostrada 451", false, K3346HudStyle.barOn(FACTORY, 451, HIGHWAY_ACTION));
        eq("fabryczny, krotki odcinek 80 m", true, K3346HudStyle.barOn(FACTORY, 80, 80));
        eq("wl, w strefie: bez slupka jak w 2.0.0", false, K3346HudStyle.barOn(ON, 100, CITY_ACTION));
        eq("wyl, w strefie", false, K3346HudStyle.barOn(OFF, 100, CITY_ACTION));
        int onAnywhere = 0;
        for (int d = -5; d <= 5000; d++) {
            if (K3346HudStyle.barOn(ON, d, CITY_ACTION) || K3346HudStyle.barOn(ON, d, HIGHWAY_ACTION)
                    || K3346HudStyle.blinking(ON, d, CITY_ACTION) || K3346HudStyle.barOn(OFF, d, CITY_ACTION)) {
                onAnywhere++;
            }
        }
        eq("wl i wyl: slupka nie ma na zadnej odleglosci", 0, onAnywhere);

        System.out.println("=== slupek: procent");
        eq("225 z 225", 100, K3346HudStyle.barPercent(225, 225));
        eq("112 z 225", 49, K3346HudStyle.barPercent(112, 225));
        eq("45 z 225", 20, K3346HudStyle.barPercent(45, 225));
        eq("44 z 225", 19, K3346HudStyle.barPercent(44, 225));
        eq("1 z 225", 0, K3346HudStyle.barPercent(1, 225));
        eq("0", 0, K3346HudStyle.barPercent(0, 225));
        eq("ujemna", 0, K3346HudStyle.barPercent(-3, 225));
        eq("ponad zakres obciete", 100, K3346HudStyle.barPercent(900, 225));
        eq("brak zakresu", 0, K3346HudStyle.barPercent(100, 0));
        eq("duze liczby bez przepelnienia", 50, K3346HudStyle.barPercent(30000000, 60000000));
        int last = 101;
        int rising = 0;
        for (int d = 225; d >= 1; d--) {
            int p = K3346HudStyle.barPercent(d, 225);
            if (p > last || p < 0 || p > 100) {
                rising++;
            }
            last = p;
        }
        eq("im blizej, tym mniej: nigdy nie rosnie", 0, rising);

        System.out.println("=== mruganie: ostatnie 20% slupka");
        eq("44 m: mruga", true, K3346HudStyle.blinking(FACTORY, 44, CITY_ACTION));
        eq("45 m: jeszcze nie", false, K3346HudStyle.blinking(FACTORY, 45, CITY_ACTION));
        eq("1 m: mruga", true, K3346HudStyle.blinking(FACTORY, 1, CITY_ACTION));
        eq("0 m: nie", false, K3346HudStyle.blinking(FACTORY, 0, CITY_ACTION));
        eq("poza slupkiem: nie", false, K3346HudStyle.blinking(FACTORY, 300, CITY_ACTION));
        eq("autostrada 89 m: mruga", true, K3346HudStyle.blinking(FACTORY, 89, HIGHWAY_ACTION));
        eq("autostrada 90 m: nie", false, K3346HudStyle.blinking(FACTORY, 90, HIGHWAY_ACTION));
        eq("wl: nigdy", false, K3346HudStyle.blinking(ON, 10, CITY_ACTION));
        eq("faza pelna", 100, K3346HudStyle.blinkPercent(true));
        eq("faza pusta", 0, K3346HudStyle.blinkPercent(false));
        eq("progi", 20, K3346HudStyle.BLINK_PERCENT);
        eq("okres", 600, K3346HudStyle.BLINK_INTERVAL_MS);

        if (argv.length > 0) {
            replay(argv[0]);
        }

        System.out.println("WYNIK_HUD_STYLE: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }
}
