package com.luka.carplay.routeguidance;

import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;

import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;

/*
 * Opcja "HUD CarPlay" (zgloszenie #15) na calym BAPBridge: prawdziwa klasa, a w
 * miejscu fabrycznego serwisu BAP atrapa, ktora zapisuje, co poszloby do zegarow.
 * Bez auta.  Plik opcji to prawdziwa sciezka /tmp/k3346-hud-mode (kontener testow).
 *
 *   (bez argumentow)   testy trybow Wl., Wyl. i Fabryczny
 *   trace <tryb>       sam zapis wywolan dla stalego scenariusza, bez asercji i bez
 *                      nowych klas: do porownania ze zrodlami poprzedniej wersji
 */
public final class K3346HudBridgeTest {

    private static final String MODE_FILE = "/tmp/k3346-hud-mode";
    private static final int FOLLOW_STREET = 11;
    private static final int RIGHT_TURN = 2;
    private static final int LEFT_TURN = 1;

    private static final int START = RouteGuidance.State.DIRTY_ROUTE_STATE
        | RouteGuidance.State.DIRTY_MANEUVER_COUNT | RouteGuidance.State.DIRTY_MANEUVER_LIST
        | RouteGuidance.State.DIRTY_MANEUVER_ICON | RouteGuidance.State.DIRTY_MANEUVER_TEXT
        | RouteGuidance.State.DIRTY_DIST_MAN;
    private static final int DIST = RouteGuidance.State.DIRTY_DIST_MAN;
    private static final int NEXT = RouteGuidance.State.DIRTY_MANEUVER_LIST
        | RouteGuidance.State.DIRTY_MANEUVER_ICON | RouteGuidance.State.DIRTY_MANEUVER_TEXT
        | RouteGuidance.State.DIRTY_DIST_MAN;
    /* RouteGuidance po zmianie opcji w trakcie trasy */
    private static final int RESEND = RouteGuidance.State.DIRTY_MANEUVER_ICON
        | RouteGuidance.State.DIRTY_DIST_MAN | RouteGuidance.State.DIRTY_MANEUVER_STATE;

    private static final List<String> calls = new ArrayList<String>();
    private static int ok;
    private static int fail;

    private static void check(String name, boolean cond) {
        if (cond) {
            ok++;
            System.out.println("  OK   " + name);
        } else {
            fail++;
            System.out.println("  FAIL " + name);
        }
    }

    private static void eq(String name, String want, String got) {
        if (want == null ? got == null : want.equals(got)) {
            ok++;
            System.out.println("  OK   " + name);
        } else {
            fail++;
            System.out.println("  FAIL " + name + " oczekiwano=[" + want + "] jest=[" + got + "]");
        }
    }

    private static String format(Object arg) {
        if (arg instanceof CombiBAPNaviManeuverDescriptor[]) {
            CombiBAPNaviManeuverDescriptor[] d = (CombiBAPNaviManeuverDescriptor[])arg;
            StringBuffer b = new StringBuffer("[");
            for (int i = 0; i < d.length; i++) {
                b.append(i == 0 ? "" : " ").append(d[i].mainElement).append(':').append(d[i].direction)
                    .append(':').append(d[i].zLevelGuidance).append(':')
                    .append(d[i].sideStreets == null ? -1 : d[i].sideStreets.length);
            }
            return b.append(']').toString();
        }
        if (arg instanceof Object[]) {
            return "[" + ((Object[])arg).length + "]";
        }
        return String.valueOf(arg);
    }

    private static CombiBAPServiceNavi service() {
        return (CombiBAPServiceNavi)Proxy.newProxyInstance(
            CombiBAPServiceNavi.class.getClassLoader(), new Class[] {CombiBAPServiceNavi.class},
            new InvocationHandler() {
                public Object invoke(Object proxy, Method m, Object[] args) {
                    StringBuffer b = new StringBuffer(m.getName()).append('(');
                    for (int i = 0; args != null && i < args.length; i++) {
                        b.append(i == 0 ? "" : ",").append(format(args[i]));
                    }
                    synchronized (calls) {
                        calls.add(b.append(')').toString());
                    }
                    Class r = m.getReturnType();
                    if (r == Boolean.TYPE) {
                        return Boolean.FALSE;
                    }
                    if (r == Integer.TYPE) {
                        return Integer.valueOf(0);
                    }
                    if (r == Long.TYPE) {
                        return Long.valueOf(0);
                    }
                    return null;
                }
            });
    }

    private static Field field(String name) throws Exception {
        Field f = BAPBridge.class.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }

    private static BAPBridge bridge() throws Exception {
        BAPBridge b = new BAPBridge();
        field("appConnectorNavi").set(b, service());
        field("initialized").setBoolean(b, true);
        return b;
    }

    private static void writeMode(int mode) throws Exception {
        FileOutputStream out = new FileOutputStream(MODE_FILE);
        out.write(new byte[] {(byte)('0' + mode), '\n'});
        out.close();
    }

    /* applyHudMode() przez refleksje: w poprzedniej wersji zwracalo void. */
    private static Object applyMode(BAPBridge b) throws Exception {
        Method m = BAPBridge.class.getDeclaredMethod("applyHudMode", new Class[0]);
        m.setAccessible(true);
        return m.invoke(b, new Object[0]);
    }

    private static RouteGuidance.State route() {
        RouteGuidance.State s = new RouteGuidance.State();
        s.routeState = 1;
        s.maneuverCount = 1;
        s.maneuverOrder = new int[] {0};
        s.mType[0] = RIGHT_TURN;
        s.mTurnAngle[0] = 90;
        s.mJunctionType[0] = 0;
        s.mDrivingSide[0] = 0;
        s.mVer[0] = 1;
        s.mName[0] = "Storgata";
        s.currentRoad = "E6";
        return s;
    }

    private static void frame(BAPBridge b, RouteGuidance.State s, int dist, int dirty) {
        s.distManeuverM = dist;
        s.dirtyMask = dirty;
        b.update(s);
        s.clearDirty();
    }

    private static void nextManeuver(BAPBridge b, RouteGuidance.State s, int dist) {
        s.maneuverOrder = new int[] {1};
        s.mType[1] = LEFT_TURN;
        s.mTurnAngle[1] = -90;
        s.mJunctionType[1] = 0;
        s.mDrivingSide[1] = 0;
        s.mVer[1] = 1;
        s.mName[1] = "Kirkeveien";
        frame(b, s, dist, NEXT);
    }

    private static int count() {
        synchronized (calls) {
            return calls.size();
        }
    }

    /* Ostatnie wywolanie o tej nazwie, od pozycji from. */
    private static String last(String name, int from) {
        synchronized (calls) {
            for (int i = calls.size() - 1; i >= from; i--) {
                String c = calls.get(i);
                if (c.startsWith(name + "(")) {
                    return c;
                }
            }
        }
        return null;
    }

    private static int countOf(String name, int from) {
        int n = 0;
        synchronized (calls) {
            for (int i = from; i < calls.size(); i++) {
                if (calls.get(i).startsWith(name + "(")) {
                    n++;
                }
            }
        }
        return n;
    }

    private static boolean any(String needle, int from) {
        synchronized (calls) {
            for (int i = from; i < calls.size(); i++) {
                if (calls.get(i).indexOf(needle) >= 0) {
                    return true;
                }
            }
        }
        return false;
    }

    private static int mainOf(String descriptorCall) {
        if (descriptorCall == null) {
            return -1;
        }
        int a = descriptorCall.indexOf('[') + 1;
        return Integer.parseInt(descriptorCall.substring(a, descriptorCall.indexOf(':', a)));
    }

    private static boolean blinkThreadAlive() {
        Thread[] all = new Thread[Thread.activeCount() + 8];
        int n = Thread.enumerate(all);
        for (int i = 0; i < n; i++) {
            if ("K3346HudBlink".equals(all[i].getName()) && all[i].isAlive()) {
                return true;
            }
        }
        return false;
    }

    /* Staly scenariusz: daleko, dojazd, skret, nastepny manewr daleko, koniec trasy. */
    private static void scenario(BAPBridge b) {
        RouteGuidance.State s = route();
        frame(b, s, 146000, START);
        frame(b, s, 145000, DIST);
        frame(b, s, 1600, DIST);
        frame(b, s, 1500, DIST);
        frame(b, s, 800, DIST);
        frame(b, s, 225, DIST);
        frame(b, s, 100, DIST);
        frame(b, s, 40, DIST);
        frame(b, s, 30, DIST);
        nextManeuver(b, s, 3000);
        frame(b, s, 2900, DIST);
        s.maneuverCount = 0;
        s.routeState = 0;
        s.maneuverOrder = new int[0];
        frame(b, s, 0, RouteGuidance.State.DIRTY_ROUTE_STATE | RouteGuidance.State.DIRTY_MANEUVER_COUNT
            | RouteGuidance.State.DIRTY_MANEUVER_LIST);
    }

    private static void trace(int mode) throws Exception {
        writeMode(mode);
        BAPBridge b = bridge();
        applyMode(b);
        calls.clear();
        scenario(b);
        synchronized (calls) {
            for (int i = 0; i < calls.size(); i++) {
                System.out.println("CALL " + calls.get(i));
            }
        }
        new File(MODE_FILE).delete();
    }

    private static void testOn() throws Exception {
        System.out.println("=== Wl.: jak w 2.0.0");
        writeMode(0);
        BAPBridge b = bridge();
        eq("wl: bez zmiany trybu", "false", String.valueOf(applyMode(b)));
        calls.clear();
        scenario(b);
        check("wl: sa deskryptory", countOf("updateManeuverDescriptor", 0) > 0);
        check("wl: zadnej prostej strzalki zamiast manewru", !any("updateManeuverDescriptor([" + FOLLOW_STREET + ":", 0));
        check("wl: zaden slupek", !any(",true,", 0));
        check("wl: stan Follow daleko", any("updateManeuverState(1)", 0));
        check("wl: stan Prepare", any("updateManeuverState(2)", 0));
        check("wl: stan Action", any("updateManeuverState(4)", 0));
        Thread.sleep(900);
        check("wl: bez watku mrugania", !blinkThreadAlive());
    }

    private static void testFactory() throws Exception {
        System.out.println("=== Fabryczny: prosto daleko od skretu, slupek przy dojezdzie");
        writeMode(2);
        BAPBridge b = bridge();
        eq("fabryczny: zmiana trybu kaze wyslac trase", "true", String.valueOf(applyMode(b)));
        eq("fabryczny: drugi odczyt bez zmiany", "false", String.valueOf(applyMode(b)));
        RouteGuidance.State s = route();

        calls.clear();
        frame(b, s, 146000, START);
        eq("146 km: prosta strzalka", String.valueOf(FOLLOW_STREET),
            String.valueOf(mainOf(last("updateManeuverDescriptor", 0))));
        check("146 km: odleglosc bez slupka", last("updateDistanceToNextManeuver", 0).endsWith(",false,0)"));
        check("146 km: odleglosc wazna", !last("updateDistanceToNextManeuver", 0).startsWith("updateDistanceToNextManeuver(-1,"));
        eq("146 km: stan Follow", "updateManeuverState(1)", last("updateManeuverState", 0));

        int mark = count();
        frame(b, s, 145000, DIST);
        frame(b, s, 1600, DIST);
        eq("daleko: deskryptor nie jest wysylany co aktualizacje", "0",
            String.valueOf(countOf("updateManeuverDescriptor", mark)));
        check("1600 m: dalej bez slupka", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));

        mark = count();
        frame(b, s, 1500, DIST);
        check("1500 m: prawdziwy manewr", mainOf(last("updateManeuverDescriptor", mark)) > 0
            && mainOf(last("updateManeuverDescriptor", mark)) != FOLLOW_STREET);
        eq("1500 m: stan Prepare", "updateManeuverState(2)", last("updateManeuverState", mark));
        check("1500 m: jeszcze bez slupka", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));

        mark = count();
        frame(b, s, 800, DIST);
        eq("800 m: deskryptor bez zmian", "0", String.valueOf(countOf("updateManeuverDescriptor", mark)));
        check("800 m: bez slupka", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));

        mark = count();
        frame(b, s, 225, DIST);
        check("225 m: slupek pelny", last("updateDistanceToNextManeuver", mark).endsWith(",true,100)"));
        eq("225 m: stan Action", "updateManeuverState(4)", last("updateManeuverState", mark));
        frame(b, s, 100, DIST);
        check("100 m: slupek 44%", last("updateDistanceToNextManeuver", mark).endsWith(",true,44)"));
        check("100 m: bez watku mrugania", !blinkThreadAlive());

        mark = count();
        frame(b, s, 40, DIST);
        check("40 m: slupek wlaczony, faza pelna", last("updateDistanceToNextManeuver", mark).endsWith(",true,100)"));
        Thread.sleep(900);
        check("40 m: watek mrugania pracuje", blinkThreadAlive());
        check("40 m: faza pusta po 600 ms, slupek dalej wlaczony", any(",true,0)", mark));
        Thread.sleep(700);
        int ticks = countOf("updateDistanceToNextManeuver", mark);
        check("40 m: kolejne fazy (" + ticks + " wywolan)", ticks >= 3);
        check("mruganie: nigdy slupek wylaczony w strefie", !any(",false,", mark));

        mark = count();
        nextManeuver(b, s, 3000);
        eq("nastepny manewr daleko: prosta strzalka", String.valueOf(FOLLOW_STREET),
            String.valueOf(mainOf(last("updateManeuverDescriptor", mark))));
        check("nastepny manewr: slupek zgaszony", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));
        eq("nastepny manewr: stan Follow", "updateManeuverState(1)", last("updateManeuverState", mark));
        Thread.sleep(900);
        int quiet = count();
        Thread.sleep(900);
        eq("po manewrze watek mrugania nic nie wysyla", String.valueOf(quiet), String.valueOf(count()));
        check("po manewrze watek mrugania skonczyl", !blinkThreadAlive());

        System.out.println("=== zmiana opcji w trakcie trasy");
        writeMode(0);
        eq("fabryczny -> wl: wyslac ponownie", "true", String.valueOf(applyMode(b)));
        mark = count();
        frame(b, s, 2900, RESEND);
        check("wl daleko: prawdziwy manewr", mainOf(last("updateManeuverDescriptor", mark)) != FOLLOW_STREET
            && mainOf(last("updateManeuverDescriptor", mark)) > 0);
        writeMode(2);
        eq("wl -> fabryczny: wyslac ponownie", "true", String.valueOf(applyMode(b)));
        mark = count();
        frame(b, s, 2900, RESEND);
        eq("fabryczny daleko: znow prosta strzalka", String.valueOf(FOLLOW_STREET),
            String.valueOf(mainOf(last("updateManeuverDescriptor", mark))));
        writeMode(1);
        eq("fabryczny -> wyl: wyslac ponownie", "true", String.valueOf(applyMode(b)));
        eq("wyl zapamietane", "true", String.valueOf(field("hudSuppressed").getBoolean(b)));
        writeMode(0);
        eq("wyl -> wl: BAP bez zmian", "false", String.valueOf(applyMode(b)));
        eq("wl zapamietane", "false", String.valueOf(field("hudSuppressed").getBoolean(b)));

        System.out.println("=== mruganie konczy sie z koncem wskazowek");
        writeMode(2);
        applyMode(b);
        frame(b, s, 2000, RESEND);
        frame(b, s, 1400, DIST);
        frame(b, s, 30, DIST);
        Thread.sleep(200);
        check("30 m: watek mrugania pracuje", blinkThreadAlive());
        b.onStop();
        Thread.sleep(900);
        check("po onStop watek skonczyl", !blinkThreadAlive());
        frame(b, s, 30, RESEND);
        Thread.sleep(200);
        check("30 m po wznowieniu: mruga znowu", blinkThreadAlive());
        mark = count();
        b.onShutdown();
        Thread.sleep(900);
        check("po onShutdown watek skonczyl", !blinkThreadAlive());
        check("onShutdown: odleglosc wyczyszczona bez slupka", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));
        eq("onShutdown: tryb do odczytania od nowa", "0", String.valueOf(field("hudMode").getInt(b)));
        writeMode(1);
        applyMode(b);
        eq("nastepny start: wyl zastosowane ponownie", "true", String.valueOf(field("hudSuppressed").getBoolean(b)));
    }

    public static void main(String[] argv) throws Exception {
        if (argv.length == 2 && "trace".equals(argv[0])) {
            trace(Integer.parseInt(argv[1]));
            System.exit(0);
        }
        try {
            testOn();
            testFactory();
        } finally {
            new File(MODE_FILE).delete();
        }
        System.out.println("WYNIK_HUD_BRIDGE: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }
}
