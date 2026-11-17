package com.luka.carplay.routeguidance;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

/*
 * Opcja "HUD CarPlay" (zgloszenie #15): co idzie do zegarow w kazdym trybie, i
 * slupek dojazdu trybu Fabryczny (K3346HudBar). Bez auta.
 *
 * Dane: etykiety odleglosci z Map Google z jazdy zglaszajacego (run-03, log Javy,
 * czas w ms od startu, odleglosc w metrach) - przepisane, nie wymyslone.
 * Z argumentem: plik "ms metry" z calej jazdy (wyciety z logu) - przebieg.
 */
public final class K3346HudStyleTest {

    private static final int ON = K3346HudStyle.MODE_ON;
    private static final int OFF = K3346HudStyle.MODE_OFF;
    private static final int FACTORY = K3346HudStyle.MODE_FACTORY;

    /* Skret przy 696-757 s, ok. 45 km/h: etykieta przychodzi pol kroku przed swoja wartoscia. */
    private static final long[][] DRIVE = {
        {682000, 700}, {688800, 600}, {695800, 500}, {701800, 400}, {708900, 300}, {716800, 200},
        {724100, 100}, {728800, 90}, {730800, 80}, {732700, 70}, {735800, 60}, {738800, 50},
        {742100, 40}, {744700, 30}, {747700, 20}};
    private static final long DRIVE_END_MS = 757300;   /* etykieta 0 */

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

    private static void check(String name, boolean cond) {
        eq(name, true, cond);
    }

    private static String d1(double v) {
        return String.valueOf(Math.round(v * 10.0) / 10.0);
    }

    /* Slupek co sekunde przez jazde z etykietami: zwraca kolejne wartosci (tylko gdy wlaczony). */
    private static List<Integer> run(long[][] labels, long endMs, int rangeM, StringBuffer notes) {
        K3346HudBar bar = new K3346HudBar();
        List<Integer> out = new ArrayList<Integer>();
        int next = 0;
        double before = -1.0;
        boolean ahead = false;
        for (long t = labels[0][0]; t <= endMs; t += 1000) {
            while (next < labels.length && labels[next][0] <= t) {
                if (next > 0) {
                    /* tuz przed nowa etykieta: nie wolno byc dalej niz mowi telefon */
                    double e = bar.estimate(labels[next][0]);
                    double start = labels[next][1] + (labels[next - 1][1] - labels[next][1]) / 2.0;
                    if (e < start - 0.001) {
                        ahead = true;
                    }
                }
                bar.label((int)labels[next][1], labels[next][0]);
                next++;
            }
            int shown = bar.percent(t, rangeM);
            double est = bar.estimate(t);
            if (before >= 0.0 && est > before + 0.001) {
                notes.append("ROSNIE ");
            }
            before = est;
            if (shown >= 0) {
                out.add(Integer.valueOf(shown));
            }
        }
        if (ahead) {
            notes.append("PRZED_TELEFONEM ");
        }
        return out;
    }

    private static int maxDrop(List<Integer> v) {
        int m = 0;
        for (int i = 1; i < v.size(); i++) {
            m = Math.max(m, v.get(i - 1).intValue() - v.get(i).intValue());
        }
        return m;
    }

    private static int rises(List<Integer> v) {
        int n = 0;
        for (int i = 1; i < v.size(); i++) {
            if (v.get(i).intValue() > v.get(i - 1).intValue()) {
                n++;
            }
        }
        return n;
    }

    /* Slupek liczony wprost z etykiety, jak w 2.0.1-rc1 (zakres 225 m): do porownania. */
    private static int maxDropLabelOnly(long[][] labels, int rangeM) {
        int prev = 100;   /* pierwszy skok liczy sie od pelnego slupka */
        int m = 0;
        for (int i = 0; i < labels.length; i++) {
            int d = (int)labels[i][1];
            if (d > 0 && d <= rangeM) {
                int p = (d * 100) / rangeM;
                m = Math.max(m, prev - p);
                prev = p;
            }
        }
        return m;
    }

    private static void replay(String path) throws Exception {
        System.out.println("=== przebieg z auta: " + path);
        BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(path), "US-ASCII"));
        List<long[]> all = new ArrayList<long[]>();
        String line;
        long prev = Long.MIN_VALUE;
        while ((line = in.readLine()) != null) {
            line = line.trim();
            int sp = line.indexOf(' ');
            if (sp <= 0) {
                continue;
            }
            long ms = Long.parseLong(line.substring(0, sp));
            long d = Long.parseLong(line.substring(sp + 1).trim());
            if (d != prev) {
                all.add(new long[] {ms, d});
                prev = d;
            }
        }
        in.close();
        /* dojazdy: ciagi malejacych dodatnich etykiet, ktore schodza ponizej 100 m */
        int approaches = 0, worst = 0, worstOld = 0, risesAll = 0;
        boolean aheadAny = false;
        int i = 0;
        while (i < all.size()) {
            if (all.get(i)[1] <= 0) {
                i++;
                continue;
            }
            int j = i;
            while (j + 1 < all.size() && all.get(j + 1)[1] > 0 && all.get(j + 1)[1] < all.get(j)[1]) {
                j++;
            }
            if (all.get(j)[1] < 100 && j - i >= 4) {
                long[][] seg = new long[j - i + 1][];
                for (int k = i; k <= j; k++) {
                    seg[k - i] = all.get(k);
                }
                long end = (j + 1 < all.size()) ? all.get(j + 1)[0] : seg[seg.length - 1][0] + 5000;
                StringBuffer notes = new StringBuffer();
                List<Integer> v = run(seg, end, 300, notes);
                int old = maxDropLabelOnly(seg, 225);
                approaches++;
                worst = Math.max(worst, maxDrop(v));
                worstOld = Math.max(worstOld, old);
                risesAll += rises(v);
                if (notes.indexOf("PRZED_TELEFONEM") >= 0) {
                    aheadAny = true;
                }
                System.out.println("  dojazd od " + (seg[0][0] / 1000) + " s, etykiety " + seg[0][1] + "->" + seg[seg.length - 1][1]
                    + " m: krokow " + v.size() + ", najwiekszy " + maxDrop(v) + " pkt (z samej etykiety: " + old + " pkt) " + notes);
            }
            i = j + 1;
        }
        check("przebieg: sa dojazdy do skretu", approaches > 0);
        eq("przebieg: slupek nigdy nie rosnie", 0, risesAll);
        check("przebieg: nigdy przed telefonem", !aheadAny);
        check("przebieg: najwiekszy krok <= 8 pkt na sekunde (jest " + worst + "; z samej etykiety " + worstOld + ")", worst <= 8);
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
        eq("wyl, daleko", false, K3346HudStyle.farStraight(OFF, true, false));
        eq("nieznany tryb jak wl", false, K3346HudStyle.farStraight(7, true, false));

        System.out.println("=== zakres slupka: fabryczna zaczynala ok. 300 m przed skretem");
        eq("miasto", 300, K3346HudStyle.barRange(1500));
        eq("autostrada", 600, K3346HudStyle.barRange(3000));
        eq("brak progu", -1, K3346HudStyle.barRange(0));
        eq("fabryczny, poczatek zakresu", true, K3346HudStyle.barOn(FACTORY, 300.0, 300));
        eq("fabryczny, tuz przed zakresem", false, K3346HudStyle.barOn(FACTORY, 300.5, 300));
        eq("fabryczny, 1 m", true, K3346HudStyle.barOn(FACTORY, 1.0, 300));
        eq("fabryczny, 0 m: zostaje wlaczony na zerze, jak w fabrycznej", true, K3346HudStyle.barOn(FACTORY, 0.0, 300));
        eq("fabryczny, brak odleglosci", false, K3346HudStyle.barOn(FACTORY, -1.0, 300));
        eq("fabryczny, brak zakresu", false, K3346HudStyle.barOn(FACTORY, 100.0, -1));
        int onAnywhere = 0;
        for (int d = -5; d <= 5000; d++) {
            if (K3346HudStyle.barOn(ON, d, 300) || K3346HudStyle.barOn(OFF, d, 300) || K3346HudStyle.barOn(ON, d, 600)) {
                onAnywhere++;
            }
        }
        eq("wl i wyl: slupka nie ma na zadnej odleglosci", 0, onAnywhere);
        eq("300 z 300", 100, K3346HudStyle.barPercent(300.0, 300));
        eq("297 z 300 (fabryczna zaczynala od 99)", 99, K3346HudStyle.barPercent(297.0, 300));
        eq("150 z 300", 50, K3346HudStyle.barPercent(150.0, 300));
        eq("2,9 z 300", 0, K3346HudStyle.barPercent(2.9, 300));
        eq("ponad zakres obciete", 100, K3346HudStyle.barPercent(900.0, 300));
        eq("brak zakresu", 0, K3346HudStyle.barPercent(100.0, 0));

        System.out.println("=== stan manewru jak w fabrycznej: 1 daleko, 2 przygotowanie, 3 ze slupkiem, 4 na koncu");
        eq("daleko", 1, K3346HudStyle.state(true, false, 0));
        eq("strzalka skretu bez slupka", 2, K3346HudStyle.state(false, false, 0));
        eq("slupek 99", 3, K3346HudStyle.state(false, true, 99));
        eq("slupek 14", 3, K3346HudStyle.state(false, true, 14));
        eq("slupek 13 (fabryczna: 4 przy 13-14)", 4, K3346HudStyle.state(false, true, 13));
        eq("slupek 0", 4, K3346HudStyle.state(false, true, 0));
        eq("okres jak w fabrycznej: 1 s", 1000, K3346HudStyle.BAR_INTERVAL_MS);

        System.out.println("=== estymator: pierwsze etykiety");
        K3346HudBar b = new K3346HudBar();
        eq("bez etykiety", "-1.0", d1(b.estimate(1000)));
        eq("bez etykiety: brak dolnej granicy", "-1.0", d1(b.lowerBound()));
        b.label(500, 10000);
        eq("pierwsza etykieta: sama etykieta", "500.0", d1(b.estimate(10000)));
        eq("pierwsza etykieta: stoi, nic nie wiadomo o predkosci", "500.0", d1(b.estimate(40000)));
        eq("pierwsza etykieta: dolna granica to ona sama", "500.0", d1(b.lowerBound()));
        b.label(400, 18000);
        eq("druga: przychodzi pol kroku wczesniej", "450.0", d1(b.estimate(18000)));
        eq("druga: predkosc jeszcze nieznana, stoi", "450.0", d1(b.estimate(30000)));
        b.label(300, 26000);
        eq("trzecia: 350 m w chwili przyjscia", "350.0", d1(b.estimate(26000)));
        eq("trzecia: po 4 s przy 12,5 m/s", "300.0", d1(b.estimate(30000)));
        eq("trzecia: nie dalej niz punkt nastepnej etykiety", "250.0", d1(b.estimate(60000)));
        eq("ta sama etykieta ponownie nic nie zmienia", "250.0", d1(estimateAfterSame(b, 300, 61000)));

        System.out.println("=== estymator: gdzie nalezy sie nastepna etykieta");
        eq("200 po kroku 100", "150.0", d1(K3346HudBar.lower(200, 100)));
        eq("100 po kroku 100: potem etykiety co 10", "95.0", d1(K3346HudBar.lower(100, 100)));
        eq("90 po kroku 10", "85.0", d1(K3346HudBar.lower(90, 10)));
        eq("10 po kroku 10: potem juz sam skret", "0.0", d1(K3346HudBar.lower(10, 10)));
        eq("50 po kroku 50", "47.5", d1(K3346HudBar.lower(50, 50)));

        System.out.println("=== estymator: przypadki brzegowe");
        b = new K3346HudBar();
        b.label(300, 0); b.label(200, 8000); b.label(100, 16000);
        b.label(2600, 17000);
        eq("odleglosc urosla (nastepny manewr): od nowa", "2600.0", d1(b.estimate(17000)));
        b.label(0, 18000);
        eq("zero: brak etykiety", "-1.0", d1(b.estimate(18000)));
        b = new K3346HudBar();
        b.label(400, 0); b.label(300, 8000); b.label(200, 8100);
        eq("dwie etykiety w 0,1 s: bez predkosci", "250.0", d1(b.estimate(20000)));
        b = new K3346HudBar();
        b.label(400, 0); b.label(300, 8000); b.label(200, 8500);
        eq("skok GPS (200 m/s): bez predkosci", "250.0", d1(b.estimate(20000)));
        b = new K3346HudBar();
        b.label(400, 0); b.label(300, 8000); b.label(200, 10000);
        eq("50 m/s to jeszcze jazda", "150.0", d1(b.estimate(12000)));
        b = new K3346HudBar();
        b.label(400, 100000); b.label(300, 108000); b.label(200, 116000);
        eq("zegar cofniety: nie wiecej niz punkt startu", "250.0", d1(b.estimate(50000)));
        b = new K3346HudBar();
        b.label(1200, 0); b.label(600, 10000);
        eq("krok 600 m to nie krok: sama etykieta", "600.0", d1(b.estimate(10000)));
        b = new K3346HudBar();
        b.label(400, 0); b.label(300, 8000); b.label(200, 16000);
        double a1 = b.estimate(18000);
        b.label(100, 18500);   /* telefon szybciej niz szacunek */
        check("nowa etykieta nigdy nie podnosi szacunku", b.estimate(18500) <= a1 + 0.001);
        /* Drobne etykiety zaczynaja sie wczesniej, niz zalozono: po "200" przychodzi "190"
         * (przy 195 m), a szacunek jest juz na 175 m. Ma zostac na 175, nie wrocic na 195. */
        b = new K3346HudBar();
        b.label(400, 0); b.label(300, 8000); b.label(200, 16000);
        eq("6 s po etykiecie 200 przy 12,5 m/s", "175.0", d1(b.estimate(22000)));
        b.label(190, 22500);
        eq("etykieta 190 (195 m) nie cofa szacunku", "175.0", d1(b.estimate(22500)));
        eq("i slupek nie rosnie", 58, b.percent(22500, 300));

        System.out.println("=== prawdziwy dojazd z auta (run-03, 696-757 s), zakres 300 m, krok 1 s");
        StringBuffer notes = new StringBuffer();
        List<Integer> v = run(DRIVE, DRIVE_END_MS, 300, notes);
        System.out.println("  slupek: " + v);
        eq("uwagi", "", notes.toString().trim());
        check("zaczyna blisko pelnego (" + v.get(0) + ")", v.get(0).intValue() >= 95);
        eq("nigdy nie rosnie", 0, rises(v));
        check("najwiekszy krok na sekunde <= 8 (jest " + maxDrop(v) + "; z samej etykiety bylo " + maxDropLabelOnly(DRIVE, 225) + ")",
            maxDrop(v) <= 8);
        check("z samej etykiety skok byl >= 40", maxDropLabelOnly(DRIVE, 225) >= 40);
        check("co najmniej 30 krokow (jest " + v.size() + ")", v.size() >= 30);
        check("konczy nisko (" + v.get(v.size() - 1) + ")", v.get(v.size() - 1).intValue() <= 8);
        /* auto staje przy etykiecie 100: slupek stoi, nie miga i nie ucieka */
        b = new K3346HudBar();
        for (int i = 0; i <= 6; i++) {
            b.label((int)DRIVE[i][1], DRIVE[i][0]);
        }
        int p10 = b.percent(DRIVE[6][0] + 10000, 300);
        int p60 = b.percent(DRIVE[6][0] + 60000, 300);
        eq("auto stoi 10 s przy etykiecie 100: slupek na granicy etykiety", 31, p10);
        eq("auto stoi 60 s: ta sama wartosc, bez mrugania", p10, p60);

        System.out.println("=== tempo opadania: predkosc nieznana, etykieta daleko ponizej szacunku");
        b = new K3346HudBar();
        b.label(300, 0);
        eq("pierwsza etykieta 300: pelny slupek", 100, b.percent(0, 300));
        b.label(200, 1000);
        eq("po 1 s etykieta 200 (250 m = 83): schodzi najwyzej o 8", 92, b.percent(1000, 300));
        eq("po kolejnej sekundzie znow o 8", 84, b.percent(2000, 300));
        eq("i dochodzi do celu", 83, b.percent(3000, 300));
        eq("stoi na celu", 83, b.percent(30000, 300));
        b.label(100, 40000);
        eq("etykieta 100 po 37 s postoju (150 m = 50): od razu najwyzej tyle, ile wolno", 50, b.percent(40000, 300));
        b = new K3346HudBar();
        b.label(300, 0);
        b.percent(0, 300);
        b.label(200, 100);
        eq("wywolania czesciej niz co sekunde: co najmniej 1 punkt", 99, b.percent(100, 300));
        eq("poza zasiegiem: -1", -1, new K3346HudBar().percent(0, 300));
        b = new K3346HudBar();
        b.label(400, 0);
        eq("400 m przy zakresie 300: jeszcze nie", -1, b.percent(0, 300));
        eq("brak zakresu: -1", -1, b.percent(0, -1));

        if (argv.length > 0) {
            replay(argv[0]);
        }

        System.out.println("WYNIK_HUD_STYLE: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }

    private static double estimateAfterSame(K3346HudBar b, int label, long ms) {
        b.label(label, ms);
        return b.estimate(ms);
    }
}
