package de.audi.app.terminalmode.keyevents;

/* Test logiki D-pada (bez HMI). Uruchamiany na PC: build_overrides + java. */
public class K3346TouchpadDpadTest {
    static StringBuffer out = new StringBuffer();
    static int fails = 0;
    static K3346TouchpadDpad.Sink sink = new K3346TouchpadDpad.Sink() {
        public void postDpad(int d) { out.append("LRUD".charAt(d - 1)); }
    };
    static void check(String name, String expected) {
        String got = out.toString(); out.setLength(0);
        boolean ok = got.equals(expected);
        if (!ok) fails++;
        System.out.println((ok ? "  OK   " : "  FAIL ") + name + " got=" + got + " expected=" + expected);
    }
    static void sleep(int ms) { try { Thread.sleep(ms); } catch (InterruptedException e) {} }
    static void drag(K3346TouchpadDpad p, int x0, int y0, int dx, int dy, int steps, int ms) {
        p.onTouchEnd(); p.onTouch(x0, y0, sink);
        for (int i = 1; i <= steps; i++) { sleep(ms); p.onTouch(x0 + dx * i / steps, y0 + dy * i / steps, sink); }
        p.onTouchEnd();
    }
    public static void main(String[] a) {
        K3346TouchpadDpad p = new K3346TouchpadDpad();
        drag(p, 500, 400, 20, 20, 4, 30);    check("drobny ruch w martwej strefie -> brak tykow", "");
        drag(p, 500, 400, 0, -220, 10, 30);  check("powolny ruch w gore o 220 -> 1x UP", "U");
        drag(p, 500, 400, 0, 220, 10, 30);   check("powolny ruch w dol o 220 -> 1x DOWN", "D");
        drag(p, 300, 400, 420, 0, 14, 30);   check("powolny ruch w prawo o 420 -> 2x RIGHT", "RR");
        drag(p, 700, 400, -420, 0, 14, 30);  check("powolny ruch w lewo o 420 -> 2x LEFT", "LL");
        drag(p, 100, 400, 800, 0, 4, 30);    check("szybki swipe w prawo o 800 -> 5x RIGHT (prog 150)", "RRRRR");
        p.onTouchEnd(); p.onTouch(500, 400, sink); sleep(30); p.onTouch(500, 380, sink); p.onTouchEnd();
        p.onTouch(900, 700, sink); p.onTouchEnd(); check("nowe dotkniecie w innym miejscu -> brak skoku", "");
        System.out.println("WYNIK_DPAD: " + (fails == 0 ? "PASS" : "FAIL " + fails));
        System.exit(fails == 0 ? 0 : 1);
    }
}
