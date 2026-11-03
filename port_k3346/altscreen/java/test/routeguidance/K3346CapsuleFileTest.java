package com.luka.carplay.routeguidance;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;

/* Files shared with vc_overlay: what capsule_text.h expects. Bez auta. */
public final class K3346CapsuleFileTest {

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

    private static String read(File f) throws Exception {
        FileInputStream in = new FileInputStream(f);
        byte[] buf = new byte[512];
        int got = in.read(buf);
        in.close();
        return got <= 0 ? "" : new String(buf, 0, got, "UTF-8");
    }

    private static void put(File f, String text) throws Exception {
        FileOutputStream out = new FileOutputStream(f);
        out.write(text.getBytes("US-ASCII"));
        out.close();
    }

    private static String repeat(int n) {
        StringBuffer b = new StringBuffer();
        for (int i = 0; i < n; i++) {
            b.append('x');
        }
        return b.toString();
    }

    public static void main(String[] argv) throws Exception {
        File dir = File.createTempFile("capsule", "");
        dir.delete();
        dir.mkdir();
        File text = new File(dir, "text");
        File slot = new File(dir, "slot");

        System.out.println("=== sciezki wspolne z kodem natywnym");
        eq("karta", "/tmp/k3346-card-mode", K3346CapsuleFile.CARD_MODE_PATH);
        eq("karta: brak", "1", String.valueOf(K3346CapsuleFile.CARD_NONE));
        eq("bez wskazowek", "/tmp/k3346-no-rgi", K3346CapsuleFile.NO_RGI_PATH);

        eq("strzalka", "/tmp/k3346-arrows-mode", K3346CapsuleFile.ARROWS_MODE_PATH);
        eq("mapa", "/tmp/k3346-map-mode", K3346CapsuleFile.MAP_MODE_PATH);
        eq("hud", "/tmp/k3346-hud-mode", K3346CapsuleFile.HUD_MODE_PATH);

        System.out.println("=== wlacz/wylacz");
        eq("0", "0", String.valueOf(K3346CapsuleFile.parseOnOff("0\n")));
        eq("1", "1", String.valueOf(K3346CapsuleFile.parseOnOff("1\n")));
        eq("2 nie", String.valueOf(K3346CapsuleFile.SLOT_NOT_READY), String.valueOf(K3346CapsuleFile.parseOnOff("2\n")));
        eq("bez nowej linii nie", String.valueOf(K3346CapsuleFile.SLOT_NOT_READY), String.valueOf(K3346CapsuleFile.parseOnOff("1")));
        eq("zla nowa linia nie", String.valueOf(K3346CapsuleFile.SLOT_NOT_READY), String.valueOf(K3346CapsuleFile.parseOnOff("1x")));
        eq("null nie", String.valueOf(K3346CapsuleFile.SLOT_NOT_READY), String.valueOf(K3346CapsuleFile.parseOnOff(null)));
        File onoff = new File(dir, "onoff");
        eq("brak pliku to wl, mimo poprzedniego", "false", String.valueOf(K3346CapsuleFile.readOff(onoff.getPath(), true)));
        K3346CapsuleFile.writeEtaMode(onoff.getPath(), 1);
        eq("plik 1 to wyl", "true", String.valueOf(K3346CapsuleFile.readOff(onoff.getPath(), false)));
        K3346CapsuleFile.writeEtaMode(onoff.getPath(), 0);
        eq("plik 0 to wl", "false", String.valueOf(K3346CapsuleFile.readOff(onoff.getPath(), true)));
        K3346CapsuleFile.writeEtaMode(onoff.getPath(), 7);
        eq("zly plik: poprzednie wyl", "true", String.valueOf(K3346CapsuleFile.readOff(onoff.getPath(), true)));
        eq("zly plik: poprzednie wl", "false", String.valueOf(K3346CapsuleFile.readOff(onoff.getPath(), false)));
        onoff.delete();
        eq("rgi, strzalka wl", "true", String.valueOf(K3346CapsuleFile.vcRgiEnabled("rgi", false)));
        eq("map, strzalka wl", "true", String.valueOf(K3346CapsuleFile.vcRgiEnabled("map", false)));
        eq("off, strzalka wl", "false", String.valueOf(K3346CapsuleFile.vcRgiEnabled("off", false)));
        eq("rgi, strzalka wyl", "false", String.valueOf(K3346CapsuleFile.vcRgiEnabled("rgi", true)));
        eq("off, strzalka wyl", "false", String.valueOf(K3346CapsuleFile.vcRgiEnabled("off", true)));

        System.out.println("=== flaga");
        File flag = new File(dir, "flag");
        eq("brak pliku", "false", String.valueOf(K3346CapsuleFile.exists(flag.getPath())));
        eq("zdjecie nieustawionej", "true", String.valueOf(K3346CapsuleFile.writeFlag(flag.getPath(), false)));
        eq("ustawienie", "true", String.valueOf(K3346CapsuleFile.writeFlag(flag.getPath(), true)));
        eq("plik jest", "true", String.valueOf(K3346CapsuleFile.exists(flag.getPath())));
        eq("drugie ustawienie", "true", String.valueOf(K3346CapsuleFile.writeFlag(flag.getPath(), true)));
        eq("zdjecie", "true", String.valueOf(K3346CapsuleFile.writeFlag(flag.getPath(), false)));
        eq("pliku nie ma", "false", String.valueOf(K3346CapsuleFile.exists(flag.getPath())));
        eq("zla sciezka", "false", String.valueOf(
            K3346CapsuleFile.writeFlag(new File(dir, "brak/flag").getPath(), true)));

        System.out.println("=== tekst");
        eq("zapis", "true", String.valueOf(
            K3346CapsuleFile.writeText(text.getPath(), "14:25 · 3 min", "1,2 km")));
        eq("format pliku", "14:25 · 3 min\n1,2 km\nEND\n", read(text));
        K3346CapsuleFile.writeText(text.getPath(), "", "");
        eq("czyszczenie", "\n\nEND\n", read(text));
        K3346CapsuleFile.writeText(text.getPath(), null, null);
        eq("null jak puste", "\n\nEND\n", read(text));
        K3346CapsuleFile.writeText(text.getPath(), "a\nb", "c\rd");
        eq("nowa linia w tekscie", "a b\nc d\nEND\n", read(text));
        K3346CapsuleFile.writeText(text.getPath(), "dlugi tekst", "");
        K3346CapsuleFile.writeText(text.getPath(), "krotki", "");
        eq("nadpisanie obcina stara tresc", "krotki\n\nEND\n", read(text));

        eq("95 bajtow wchodzi", "true", String.valueOf(
            K3346CapsuleFile.writeText(text.getPath(), repeat(95), "")));
        eq("96 bajtow z lewej odrzucone", "false", String.valueOf(
            K3346CapsuleFile.writeText(text.getPath(), repeat(96), "")));
        eq("96 bajtow z prawej odrzucone", "false", String.valueOf(
            K3346CapsuleFile.writeText(text.getPath(), "", repeat(96))));
        eq("odrzucone nie rusza pliku", repeat(95) + "\n\nEND\n", read(text));
        /* bajty, nie znaki: te same znaki w UTF-8 sa dluzsze */
        eq("limit w bajtach UTF-8", "false", String.valueOf(
            K3346CapsuleFile.writeText(text.getPath(), repeat(94) + "ł", "")));
        eq("zla sciezka", "false", String.valueOf(
            K3346CapsuleFile.writeText(new File(dir, "brak/plik").getPath(), "a", "b")));

        System.out.println("=== slot");
        eq("brak pliku", K3346CapsuleFile.SLOT_MISSING, K3346CapsuleFile.readSlot(slot.getPath()));
        put(slot, "0 1\n");
        eq("male, rysuje", 2, K3346CapsuleFile.readSlot(slot.getPath()));
        put(slot, "1 1\n");
        eq("duze, rysuje", 3, K3346CapsuleFile.readSlot(slot.getPath()));
        put(slot, "0 0\n");
        eq("male, nie rysuje", 0, K3346CapsuleFile.readSlot(slot.getPath()));
        put(slot, "1 0\n");
        eq("duze, nie rysuje", 1, K3346CapsuleFile.readSlot(slot.getPath()));
        put(slot, "");
        eq("plik zlapany pusty", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.readSlot(slot.getPath()));
        put(slot, "0 ");
        eq("plik uciety", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.readSlot(slot.getPath()));

        eq("null", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseSlot(null));
        eq("bez nowej linii", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseSlot("0 1x"));
        eq("bez spacji", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseSlot("0x1\n"));
        eq("slot poza zakresem", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseSlot("2 1\n"));
        eq("draw poza zakresem", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseSlot("0 2\n"));
        eq("za dlugi", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseSlot("0 1 \n"));

        System.out.println("=== tryb ETA");
        File mode = new File(dir, "mode");
        eq("brak pliku", K3346CapsuleFile.SLOT_MISSING, K3346CapsuleFile.readEtaMode(mode.getPath()));
        eq("zapis", "true", String.valueOf(K3346CapsuleFile.writeEtaMode(mode.getPath(), 1)));
        eq("format", "1\n", read(mode));
        eq("mapa", K3346CapsuleFile.ETA_MAP, K3346CapsuleFile.readEtaMode(mode.getPath()));
        K3346CapsuleFile.writeEtaMode(mode.getPath(), 2);
        eq("obie", K3346CapsuleFile.ETA_BOTH, K3346CapsuleFile.readEtaMode(mode.getPath()));
        K3346CapsuleFile.writeEtaMode(mode.getPath(), 0);
        eq("kapsula", K3346CapsuleFile.ETA_CAPSULE, K3346CapsuleFile.readEtaMode(mode.getPath()));
        put(mode, "");
        eq("zlapany pusty", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.readEtaMode(mode.getPath()));
        eq("null", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseEtaMode(null));
        eq("bez nowej linii", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseEtaMode("1"));
        eq("poza zakresem", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseEtaMode("3\n"));
        eq("ponizej zakresu", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseEtaMode("/\n"));
        eq("za dlugi", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseEtaMode("1\n\n"));
        eq("znak zamiast nowej linii", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseEtaMode("1x"));
        eq("zla sciezka", "false", String.valueOf(
            K3346CapsuleFile.writeEtaMode(new File(dir, "brak/plik").getPath(), 1)));

        System.out.println("=== karta rozmowy");
        File call = new File(dir, "call");
        eq("brak pliku", K3346CapsuleFile.CALL_NONE, K3346CapsuleFile.readCallCard(call.getPath()));
        put(call, "R\nAnna\nDzwoni\nEND\n");
        eq("dzwoni", K3346CapsuleFile.CALL_VISIBLE, K3346CapsuleFile.readCallCard(call.getPath()));
        put(call, "D\nAnna\nX\nEND\n");
        eq("laczenie", K3346CapsuleFile.CALL_VISIBLE, K3346CapsuleFile.readCallCard(call.getPath()));
        put(call, "A\nAnna\nX\nEND\n");
        eq("rozmowa", K3346CapsuleFile.CALL_VISIBLE, K3346CapsuleFile.readCallCard(call.getPath()));
        put(call, "H\nAnna\nX\nEND\n");
        eq("wstrzymana", K3346CapsuleFile.CALL_VISIBLE, K3346CapsuleFile.readCallCard(call.getPath()));
        put(call, "\n\n\nEND\n");
        eq("koniec rozmowy", K3346CapsuleFile.CALL_NONE, K3346CapsuleFile.readCallCard(call.getPath()));
        put(call, "");
        eq("zlapany pusty", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.readCallCard(call.getPath()));
        put(call, "A\nAnna");
        eq("uciety", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.readCallCard(call.getPath()));
        eq("null", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseCallCard(null));
        eq("bez znacznika", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseCallCard("A\nAnna\nX\nEN"));
        eq("zly stan", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseCallCard("X\nAnna\nX\nEND\n"));
        eq("za dlugi stan", K3346CapsuleFile.SLOT_NOT_READY, K3346CapsuleFile.parseCallCard("AA\nAnna\nX\nEND\n"));

        call.delete();
        mode.delete();
        text.delete();
        slot.delete();
        dir.delete();
        System.out.println("WYNIK_CAPSULE_FILE: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }
}
