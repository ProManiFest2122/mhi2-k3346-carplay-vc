/*
 * K3346 Etap 5.3: okladka biezacego utworu CarPlay dla VC.
 *
 * Stock K3346 nie pobiera okladki w CarPlay (dio_manager nie importuje
 * iap2_nowplay_getartwork), choc demon iAP2 ja ma (PPS nowplaying:
 * MediaItemArtworkFileTransferIdentifier / MediaItemArtworkSize). Watek co
 * POLL_MS czyta PPS /ramdisk/pps/iap2/nowplaying tylko przy aktywnym CarPlay;
 * przy nowym utworze z okladka uruchamia /mnt/app/root/k3346-coverart-fetch
 * (sciezka sondy 5.3, PASS w aucie) z limitem czasu i zapamietuje plik + id
 * (CRC32 obrazka). TerminalModeBapCombi dolacza okladke do danych utworu, a
 * AppConnectorTerminalMode wysyla ja do VC (DSIKombiPictureServer).
 * Okladka nalezy do tytulu z PPS - inny tytul = brak okladki (bez starej
 * okladki przy nowym utworze). Kazdy blad jest polykany (log K3346Diag).
 * Java 1.4.
 */
package de.audi.app.terminalmode.k3346;

import java.io.FileInputStream;
import java.io.InputStream;
import java.util.zip.CRC32;

public final class K3346CoverArt {
    public interface Listener {
        void onCoverArt();
    }

    /** Pobranie okladki do pliku; w tescie atrapa. Zwraca pelna sciezke albo null. */
    interface Fetcher {
        String fetch(String basePath);
    }

    static final String PPS_NOWPLAYING = "/ramdisk/pps/iap2/nowplaying";
    static final String FETCH_TOOL = "/mnt/app/root/k3346-coverart-fetch";
    static final String ART_DIR = "/var/app/icab/tmp/37";
    private static final long POLL_MS = 500, FETCH_TIMEOUT_MS = 10000;
    private static final int MAX_TRIES = 6, MAX_LOG = 60;

    private static final K3346CoverArt INSTANCE = new K3346CoverArt();

    static Fetcher fetcher = new Fetcher() {
        public String fetch(String basePath) {
            return runFetchTool(basePath);
        }
    };

    private Listener listener;
    private Thread poller;
    private volatile boolean polling;

    /* stan PPS i okladki */
    private String lastKey;
    private int tries;
    private String artTitle;
    private String artPath;
    private int artId;
    private long artCrc;
    private int slot;
    private int logged;
    private String pendingKey;

    K3346CoverArt() {
    }

    public static K3346CoverArt get() {
        return INSTANCE;
    }

    public synchronized void setListener(Listener l) {
        listener = l;
    }

    /** Okladka dla tego tytulu albo null (inny utwor / brak okladki). */
    public synchronized String pathFor(String title) {
        return artPath != null && same(artTitle, title) ? artPath : null;
    }

    public synchronized int idFor(String title) {
        return artPath != null && same(artTitle, title) ? artId : 0;
    }

    public synchronized void start() {
        if (poller != null) return;
        polling = true;
        log("K3346_COVER start");
        poller = new Thread("K3346CoverArt") {
            public void run() {
                String last = null;
                while (polling) {
                    try {
                        String text = readText(PPS_NOWPLAYING);
                        if (text != null && (!text.equals(last) || needsRetry())) {
                            last = text;
                            onPps(text);
                        }
                    } catch (Throwable t) {
                        /* brak PPS przed pierwszym utworem - normalne */
                    }
                    try {
                        Thread.sleep(POLL_MS);
                    } catch (InterruptedException e) {
                        break;
                    }
                }
            }
        };
        poller.setDaemon(true);
        poller.start();
    }

    public synchronized void stop() {
        if (poller == null) return;
        polling = false;
        poller.interrupt();
        poller = null;
        clear();
        log("K3346_COVER stop");
    }

    /* nieudane pobranie: demon moze jeszcze odbierac okladke z iPhone'a */
    synchronized boolean needsRetry() {
        return pendingKey != null && !pendingKey.equals(lastKey) && tries > 0 && tries < MAX_TRIES;
    }

    private void clear() {
        lastKey = null;
        pendingKey = null;
        tries = 0;
        artTitle = null;
        artPath = null;
        artId = 0;
        artCrc = 0;
    }

    /* pakietowa: watek PPS i test */
    void onPps(String text) {
        String title = field(text, "MediaItemTitle");
        String xfer = field(text, "MediaItemArtworkFileTransferIdentifier");
        int size = number(field(text, "MediaItemArtworkSize"));
        String key = xfer + "|" + title;
        String base;
        synchronized (this) {
            if (key.equals(lastKey)) return;
            if (xfer.length() == 0 || size <= 0) {
                /* utwor bez okladki: stara nie pasuje juz do tytulu */
                lastKey = key;
                tries = 0;
                return;
            }
            if (!key.equals(pendingKey)) {
                pendingKey = key;
                tries = 0;
            }
            tries++;
            slot ^= 1;
            base = ART_DIR + "/k3346_cover_" + slot;
        }
        String path = fetcher.fetch(base);
        Listener l = null;
        synchronized (this) {
            if (path == null) {
                log("K3346_COVER fetch failed try=" + tries + " size=" + size);
                if (tries >= MAX_TRIES) lastKey = key;
                return;
            }
            lastKey = key;
            tries = 0;
            long crc = crcOf(path);
            if (crc == artCrc && same(artTitle, title) && artPath != null) return;
            artTitle = title;
            artPath = path;
            artCrc = crc;
            int id = (int)crc;
            if (id == 0 || id == artId) id = artId + 1 == 0 ? 1 : artId + 1;
            artId = id;
            log("K3346_COVER new art id=" + id + " size=" + size + " " + path);
            l = listener;
        }
        if (l != null) {
            try {
                l.onCoverArt();
            } catch (Throwable t) {
                log("K3346_COVER listener error " + t);
            }
        }
    }

    /* ---------------- narzedzia ---------------- */

    static boolean same(String a, String b) {
        return (a == null ? "" : a.trim()).equals(b == null ? "" : b.trim());
    }

    /* PPS: linie "nazwa:kodowanie:wartosc" */
    static String field(String text, String name) {
        int from = 0;
        while (from < text.length()) {
            int end = text.indexOf('\n', from);
            if (end < 0) end = text.length();
            if (text.startsWith(name + ":", from)) {
                int c = text.indexOf(':', from + name.length() + 1);
                if (c >= 0 && c < end) return text.substring(c + 1, end).trim();
            }
            from = end + 1;
        }
        return "";
    }

    static int number(String s) {
        try {
            return Integer.parseInt(s.trim());
        } catch (Throwable t) {
            return 0;
        }
    }

    static String readText(String path) throws java.io.IOException {
        FileInputStream in = new FileInputStream(path);
        try {
            byte[] buf = new byte[8192];
            int total = 0;
            while (total < buf.length) {
                int n = in.read(buf, total, buf.length - total);
                if (n <= 0) break;
                total += n;
            }
            return new String(buf, 0, total, "UTF-8");
        } finally {
            in.close();
        }
    }

    static long crcOf(String path) {
        CRC32 crc = new CRC32();
        try {
            FileInputStream in = new FileInputStream(path);
            try {
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0) crc.update(buf, 0, n);
            } finally {
                in.close();
            }
        } catch (Throwable t) {
            return 0;
        }
        return crc.getValue();
    }

    /* Uruchamia narzedzie natywne; stdout "OK <bajty> <plik>". Limit czasu. */
    static String runFetchTool(String basePath) {
        final Process p;
        try {
            p = Runtime.getRuntime().exec(new String[]{FETCH_TOOL, basePath});
        } catch (Throwable t) {
            log("K3346_COVER exec error " + t);
            return null;
        }
        Thread killer = new Thread("K3346CoverArtTimeout") {
            public void run() {
                try {
                    Thread.sleep(FETCH_TIMEOUT_MS);
                    p.destroy();
                } catch (Throwable t) {
                    /* koniec przed limitem */
                }
            }
        };
        killer.setDaemon(true);
        killer.start();
        String out = "";
        try {
            p.getOutputStream().close();
            out = readAll(p.getInputStream());
            p.waitFor();
        } catch (Throwable t) {
            log("K3346_COVER tool error " + t);
        } finally {
            killer.interrupt();
            try { p.getErrorStream().close(); } catch (Throwable t) { /* nic */ }
        }
        out = out.trim();
        if (out.startsWith("OK ")) {
            int sp = out.indexOf(' ', 3);
            if (sp > 0) return out.substring(sp + 1).trim();
        }
        log("K3346_COVER tool: " + out);
        return null;
    }

    private static String readAll(InputStream in) throws java.io.IOException {
        StringBuffer sb = new StringBuffer();
        byte[] buf = new byte[256];
        int n;
        while ((n = in.read(buf)) > 0 && sb.length() < 2048) sb.append(new String(buf, 0, n));
        in.close();
        return sb.toString();
    }

    private static void log(String s) {
        synchronized (INSTANCE) {
            if (INSTANCE.logged >= MAX_LOG) return;
            INSTANCE.logged++;
        }
        K3346Diag.log(s);
    }
}
