package de.audi.app.terminalmode.k3346;

import java.io.File;
import java.io.FileOutputStream;

/* Test logiki okladki (bez HMI i bez iPhone'a): atrapa narzedzia pobierania. */
public class K3346CoverArtTest {
    static int fails = 0;
    static void ok(boolean c, String n) { if (!c) fails++; System.out.println((c ? "  OK   " : "  FAIL ") + n); }

    static String pps(String title, String xfer, int size, int elapsed) {
        return "[n]@nowplaying\nMediaItemAlbumTitle::Album\nMediaItemArtist::Artysta\n"
            + "MediaItemArtworkFileTransferIdentifier::" + xfer + "\nMediaItemArtworkSize::" + size
            + "\nMediaItemTitle::" + title + "\nPlaybackAppName::Spotify\nPlaybackElapsedTimeInMilliseconds::" + elapsed + "\n";
    }

    static int fetches = 0, notifies = 0;
    static boolean fetchOk = true;
    static byte[] content = new byte[]{(byte)0xff, (byte)0xd8, (byte)0xff, 1, 2, 3};
    static File dir;

    public static void main(String[] a) throws Exception {
        dir = new File(System.getProperty("java.io.tmpdir"), "k3346cover" + System.currentTimeMillis());
        dir.mkdirs();
        K3346CoverArt.fetcher = new K3346CoverArt.Fetcher() {
            public String fetch(String base) {
                fetches++;
                if (!fetchOk) return null;
                try {
                    File f = new File(dir, new File(base).getName() + ".jpg");
                    FileOutputStream o = new FileOutputStream(f);
                    o.write(content);
                    o.close();
                    return f.getPath();
                } catch (Exception e) { return null; }
            }
        };
        K3346CoverArt c = new K3346CoverArt();
        c.setListener(new K3346CoverArt.Listener() { public void onCoverArt() { notifies++; } });

        String p = pps("Utwor A", "132", 98258, 1000);
        ok(K3346CoverArt.field(p, "MediaItemTitle").equals("Utwor A") && K3346CoverArt.field(p, "MediaItemArtworkSize").equals("98258"),
           "parser PPS: tytul i rozmiar okladki");
        ok(K3346CoverArt.field("[n]@nowplaying\nMediaItemTitle:s:X\n", "MediaItemTitle").equals("X"), "parser PPS: kodowanie s");
        ok(K3346CoverArt.field(p, "Brak").equals(""), "parser PPS: brak pola -> pusty");

        c.onPps(p);
        ok(fetches == 1 && notifies == 1, "nowy utwor z okladka -> pobranie + powiadomienie");
        ok(c.pathFor("Utwor A") != null && c.idFor("Utwor A") != 0, "okladka dla biezacego tytulu");
        ok(c.pathFor("Utwor B") == null && c.idFor("Utwor B") == 0, "inny tytul -> brak okladki (bez starej przy nowym utworze)");
        int idA = c.idFor("Utwor A");
        c.onPps(pps("Utwor A", "132", 98258, 1500));
        ok(fetches == 1 && notifies == 1, "zmiana tylko czasu odtwarzania -> bez ponownego pobierania");

        c.onPps(pps("Utwor B", "", 0, 0));
        ok(fetches == 1 && c.pathFor("Utwor B") == null, "utwor bez okladki -> nic nie pobiera, brak okladki");

        content = new byte[]{(byte)0xff, (byte)0xd8, (byte)0xff, 9, 9, 9};
        c.onPps(pps("Utwor C", "134", 5000, 0));
        ok(fetches == 2 && notifies == 2 && c.idFor("Utwor C") != 0 && c.idFor("Utwor C") != idA, "kolejna okladka -> nowe id");

        fetchOk = false;
        c.onPps(pps("Utwor D", "135", 5000, 0));
        ok(fetches == 3 && c.pathFor("Utwor D") == null && c.needsRetry(), "nieudane pobranie -> ponowienie zaplanowane");
        fetchOk = true;
        c.onPps(pps("Utwor D", "135", 5000, 100));
        ok(fetches == 4 && c.pathFor("Utwor D") != null && !c.needsRetry(), "ponowienie udane -> okladka");

        fetchOk = false;
        for (int i = 0; i < 8; i++) c.onPps(pps("Utwor E", "136", 5000, i));
        ok(fetches == 4 + 6 && !c.needsRetry(), "maksymalnie 6 prob na utwor");
        fetchOk = true;

        int n0 = notifies;
        c.onPps(pps("Utwor D", "135", 5000, 0));
        ok(c.pathFor("Utwor D") != null && notifies == n0, "powrot do utworu D z ta sama okladka -> bez zbednego powiadomienia");

        c.start(); c.stop();
        ok(c.pathFor("Utwor D") == null, "stop (koniec CarPlay) -> okladka wyczyszczona");
        System.out.println("WYNIK_COVER: " + (fails == 0 ? "PASS" : "FAIL " + fails));
    }
}
