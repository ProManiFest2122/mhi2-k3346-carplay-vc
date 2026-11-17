package com.luka.carplay.routeguidance;

import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;

/*
 * Opcja MMI "Wysokosc na VC" (2.0.1) na prawdziwym BAPBridge: plik opcji ->
 * ukrycie albo odsloniecie wysokosci, skali i kompasu na pasku zegarow, i sygnal
 * do odswiezenia paska. Bez auta: bramka BAP nie jest tu zalozona (brak
 * Navigation), sprawdzane jest to, czego mostek od niej zazada.
 * Plik opcji to prawdziwa sciezka /tmp/k3346-altitude-mode (kontener testow).
 */
public final class K3346AltitudeBridgeTest {

    private static final String FILE = K3346CapsuleFile.ALTITUDE_MODE_PATH;
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

    private static Field field(String name) throws Exception {
        Field f = BAPBridge.class.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }

    private static void put(String text) throws Exception {
        FileOutputStream out = new FileOutputStream(FILE);
        out.write(text.getBytes("US-ASCII"));
        out.close();
    }

    private static String hidden(BAPBridge b) throws Exception {
        return String.valueOf(field("navStatusHiddenWanted").getBoolean(b));
    }

    private static String on(BAPBridge b) throws Exception {
        return String.valueOf(field("altitudeOn").getBoolean(b));
    }

    private static String dirty(BAPBridge b) throws Exception {
        return String.valueOf(field("altitudeDirty").getBoolean(b));
    }

    public static void main(String[] argv) throws Exception {
        try {
            new File(FILE).delete();
            BAPBridge b = new BAPBridge();
            eq("przed startem: nic nie ukryte", "false", hidden(b));

            System.out.println("=== start sesji bez pliku opcji: jak w 2.0, ukryte");
            b.applyAltitudeAtStart();
            eq("brak pliku: wysokosc wylaczona", "false", on(b));
            eq("brak pliku: pasek ukryty", "true", hidden(b));
            eq("brak pliku: pasek do odswiezenia", "true", dirty(b));
            field("altitudeDirty").setBoolean(b, false);
            b.pollAltitude();
            eq("bez zmiany pliku nic sie nie dzieje", "false", dirty(b));

            System.out.println("=== wlaczenie w trakcie sesji");
            put("1\n");
            b.pollAltitude();
            eq("plik 1: wysokosc wlaczona", "true", on(b));
            eq("plik 1: pasek odsloniety", "false", hidden(b));
            eq("plik 1: pasek do odswiezenia", "true", dirty(b));
            field("altitudeDirty").setBoolean(b, false);
            b.pollAltitude();
            eq("ponowny odczyt: bez zmiany", "false", dirty(b));
            eq("ponowny odczyt: dalej odsloniety", "false", hidden(b));

            System.out.println("=== plik zlapany w polowie zapisu");
            put("0");
            b.pollAltitude();
            eq("niedopisany plik: zostaje wlaczona", "true", on(b));
            eq("niedopisany plik: dalej odsloniety", "false", hidden(b));

            System.out.println("=== wylaczenie");
            put("0\n");
            b.pollAltitude();
            eq("plik 0: wylaczona", "false", on(b));
            eq("plik 0: pasek ukryty", "true", hidden(b));
            eq("plik 0: pasek do odswiezenia", "true", dirty(b));

            System.out.println("=== nastepna sesja stosuje opcje od nowa");
            put("1\n");
            b.pollAltitude();
            b.setNavStatusHidden(true);   /* cos innego ukrylo pasek miedzy sesjami */
            field("altitudeDirty").setBoolean(b, false);
            b.applyAltitudeAtStart();
            eq("start sesji z wlaczona: odsloniety mimo braku zmiany pliku", "false", hidden(b));
            eq("start sesji: pasek do odswiezenia", "true", dirty(b));

            System.out.println("=== plik zniknal (nowy start MMI przed wierszem opcji)");
            new File(FILE).delete();
            b.pollAltitude();
            eq("brak pliku: wraca do wylaczonej", "false", on(b));
            eq("brak pliku: ukryty", "true", hidden(b));

            /* czasy na pasku przy wlaczonej wysokosci: K3346HudBridgeTest.testBarWithAltitude */
        } finally {
            new File(FILE).delete();
        }
        System.out.println("WYNIK_ALTITUDE: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }
}
