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

    private static boolean barThreadAlive() {
        Thread[] all = new Thread[Thread.activeCount() + 8];
        int n = Thread.enumerate(all);
        for (int i = 0; i < n; i++) {
            if ("K3346HudBar".equals(all[i].getName()) && all[i].isAlive()) {
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
        check("wl: bez watku slupka", !barThreadAlive());
    }

    /* Wartosci slupka z wywolan FctID 18 od pozycji from (tylko z wlaczonym slupkiem). */
    private static List<Integer> bars(int from) {
        List<Integer> out = new ArrayList<Integer>();
        synchronized (calls) {
            for (int i = from; i < calls.size(); i++) {
                String c = calls.get(i);
                int k = c.indexOf(",true,");
                if (c.startsWith("updateDistanceToNextManeuver(") && k > 0) {
                    out.add(Integer.valueOf(c.substring(k + 6, c.length() - 1)));
                }
            }
        }
        return out;
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

    private static void testFactory() throws Exception {
        System.out.println("=== Fabryczny: prosto daleko od skretu, strzalka skretu od progu");
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
        eq("146 km: stan Follow (1), jak fabryczna daleko od skretu", "updateManeuverState(1)", last("updateManeuverState", 0));

        int mark = count();
        frame(b, s, 145000, DIST);
        frame(b, s, 1600, DIST);
        eq("daleko: deskryptor nie jest wysylany co aktualizacje", "0",
            String.valueOf(countOf("updateManeuverDescriptor", mark)));
        check("1600 m: dalej bez slupka", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));
        check("daleko: bez watku slupka", !barThreadAlive());

        mark = count();
        frame(b, s, 1500, DIST);
        check("1500 m: prawdziwy manewr", mainOf(last("updateManeuverDescriptor", mark)) > 0
            && mainOf(last("updateManeuverDescriptor", mark)) != FOLLOW_STREET);
        eq("1500 m: stan Prepare (2)", "updateManeuverState(2)", last("updateManeuverState", mark));
        check("1500 m: jeszcze bez slupka", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));

        System.out.println("=== slupek: etykiety telefonu co 100 m, slupek idzie sam co sekunde");
        frame(b, s, 500, DIST);
        Thread.sleep(2000);
        frame(b, s, 400, DIST);
        check("400 m: poza zasiegiem slupka, bez watku", !barThreadAlive());
        Thread.sleep(2000);
        mark = count();
        frame(b, s, 300, DIST);
        /* etykieta 300 przychodzi przy ok. 350 m: slupka jeszcze nie ma */
        check("etykieta 300: jeszcze bez slupka", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));
        eq("etykieta 300: stan dalej Prepare", "updateManeuverState(2)", last("updateManeuverState", mark));
        Thread.sleep(300);
        check("etykieta 300: watek slupka pracuje", barThreadAlive());
        Thread.sleep(2300);
        List<Integer> v = bars(mark);
        check("bez nowej etykiety slupek sam sie wlacza (" + v + ")", v.size() >= 1);
        check("zaczyna od prawie pelnego", v.size() >= 1 && v.get(0).intValue() >= 83);
        check("stan Distance (3) razem ze slupkiem, jak fabryczna", any("updateManeuverState(3)", mark));
        check("liczba metrow dalej z telefonu (300 m)", last("updateDistanceToNextManeuver", mark).startsWith("updateDistanceToNextManeuver(3000,0,"));
        int at200 = count();
        frame(b, s, 200, DIST);
        /* dawniej od 225 m szedl stan Action (4); fabryczna trzyma Distance (3), poki slupka jest duzo */
        eq("etykieta 200: stan dalej Distance (3), nie Action", "updateManeuverState(3)", last("updateManeuverState", at200));
        Thread.sleep(2300);
        check("etykieta 200, slupek powyzej 13%: nigdzie stanu 4", !any("updateManeuverState(4)", mark));
        frame(b, s, 100, DIST);
        Thread.sleep(6500);   /* slupek schodzi najwyzej 8 punktow na sekunde */
        v = bars(mark);
        check("slupek zszedl do granicy etykiety 100 (" + v + ")", v.get(v.size() - 1).intValue() <= 33);
        int quietFrom = count();
        Thread.sleep(2500);
        eq("auto stoi przy etykiecie 100: nic nie wysyla, nic nie miga", String.valueOf(quietFrom), String.valueOf(count()));
        int[] fine = {90, 80, 70, 60, 50, 40, 30, 20, 10};
        for (int i = 0; i < fine.length; i++) {
            frame(b, s, fine[i], DIST);
            Thread.sleep(450);
        }
        Thread.sleep(1500);
        v = bars(mark);
        System.out.println("  slupek: " + v);
        eq("slupek nigdy nie rosnie (bez mrugania)", "0", String.valueOf(rises(v)));
        check("co najmniej 10 roznych krokow (jest " + v.size() + ")", v.size() >= 10);
        check("konczy na zerze albo tuz nad", v.get(v.size() - 1).intValue() <= 3);
        check("na zerze slupek dalej wlaczony", last("updateDistanceToNextManeuver", mark).indexOf(",true,") > 0);
        check("stan Action (4) na koncu slupka", "updateManeuverState(4)".equals(last("updateManeuverState", mark)));
        check("stan 3 przed stanem 4", calls.indexOf("updateManeuverState(3)") >= 0
            && calls.lastIndexOf("updateManeuverState(3)") < calls.lastIndexOf("updateManeuverState(4)"));
        check("w zasiegu slupka nigdy z powrotem Prepare", !anyAfter("updateManeuverState(2)", calls.indexOf("updateManeuverState(3)")));

        mark = count();
        nextManeuver(b, s, 3000);
        eq("nastepny manewr daleko: prosta strzalka", String.valueOf(FOLLOW_STREET),
            String.valueOf(mainOf(last("updateManeuverDescriptor", mark))));
        check("nastepny manewr: slupek zgaszony", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));
        eq("nastepny manewr: stan Follow", "updateManeuverState(1)", last("updateManeuverState", mark));
        Thread.sleep(1300);
        int quiet = count();
        Thread.sleep(1300);
        eq("po manewrze watek slupka nic nie wysyla", String.valueOf(quiet), String.valueOf(count()));
        check("po manewrze watek slupka skonczyl", !barThreadAlive());

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

        System.out.println("=== watek slupka konczy sie z koncem wskazowek i ze zmiana trybu");
        writeMode(2);
        applyMode(b);
        frame(b, s, 2000, RESEND);
        frame(b, s, 1400, DIST);
        frame(b, s, 300, DIST);
        Thread.sleep(200);
        check("300 m: watek slupka pracuje", barThreadAlive());
        b.onStop();
        Thread.sleep(1300);
        check("po onStop watek skonczyl", !barThreadAlive());
        frame(b, s, 300, RESEND);
        Thread.sleep(200);
        check("300 m po wznowieniu: pracuje znowu", barThreadAlive());
        writeMode(0);
        applyMode(b);
        Thread.sleep(1300);
        check("po przelaczeniu na Wl. watek skonczyl", !barThreadAlive());
        mark = count();
        frame(b, s, 200, RESEND);
        check("Wl. w zasiegu slupka: bez slupka", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));
        writeMode(2);
        applyMode(b);
        frame(b, s, 200, RESEND);
        Thread.sleep(200);
        check("znowu Fabryczny: watek pracuje", barThreadAlive());
        mark = count();
        b.onShutdown();
        Thread.sleep(1300);
        check("po onShutdown watek skonczyl", !barThreadAlive());
        check("onShutdown: odleglosc wyczyszczona bez slupka", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));
        eq("onShutdown: tryb do odczytania od nowa", "0", String.valueOf(field("hudMode").getInt(b)));
        writeMode(1);
        applyMode(b);
        eq("nastepny start: wyl zastosowane ponownie", "true", String.valueOf(field("hudSuppressed").getBoolean(b)));
    }

    private static String slurp(String path) throws Exception {
        File f = new File(path);
        if (!f.exists()) {
            return null;
        }
        java.io.FileInputStream in = new java.io.FileInputStream(f);
        byte[] buf = new byte[256];
        int n = in.read(buf);
        in.close();
        return n <= 0 ? "" : new String(buf, 0, n, "UTF-8").replace('\n', '|');
    }

    private static String none() {
        return "||END|";
    }

    private static void put(String path, String text) throws Exception {
        FileOutputStream out = new FileOutputStream(path);
        out.write(text.getBytes("US-ASCII"));
        out.close();
    }

    /* Liczba metrow dla nakladki (2.0.1-rc4): zegary chowaja swoja razem ze slupkiem. */
    private static void testTurnDist() throws Exception {
        System.out.println("=== liczba metrow dla nakladki: tylko gdy do zegarow idzie slupek");
        final String file = K3346CapsuleFile.TURN_DIST_PATH;
        final String none = "||END|";
        new File(file).delete();
        writeMode(2);
        BAPBridge b = bridge();
        applyMode(b);
        RouteGuidance.State s = route();
        calls.clear();
        frame(b, s, 5000, START);
        eq("daleko: plik jest i jest pusty", none, slurp(file));
        frame(b, s, 1400, DIST);
        eq("1400 m, strzalka skretu bez slupka: pusty", none, slurp(file));
        int mark = count();
        frame(b, s, 250, DIST);
        check("250 m: do zegarow idzie slupek", last("updateDistanceToNextManeuver", mark).indexOf(",true,") > 0);
        eq("250 m: nakladka dostaje 250 m", "250|m|END|", slurp(file));
        frame(b, s, 200, DIST);
        eq("200 m: nakladka dostaje 200 m", "200|m|END|", slurp(file));
        Thread.sleep(1300);
        eq("watek slupka nie zmienia liczby", "200|m|END|", slurp(file));

        System.out.println("=== plik zniknal (skrypt sesji): ta sama liczba wraca");
        new File(file).delete();
        frame(b, s, 200, DIST);
        eq("po skasowaniu pliku: znowu 200 m", "200|m|END|", slurp(file));
        frame(b, s, 90, DIST);
        eq("90 m", "90|m|END|", slurp(file));

        System.out.println("=== koniec slupka: liczba znika");
        mark = count();
        nextManeuver(b, s, 3000);
        check("nastepny manewr daleko: bez slupka", last("updateDistanceToNextManeuver", mark).endsWith(",false,0)"));
        eq("nastepny manewr: pusty", none, slurp(file));
        frame(b, s, 250, DIST);
        eq("znow 250 m przed skretem", "250|m|END|", slurp(file));
        writeMode(0);
        applyMode(b);
        frame(b, s, 250, RESEND);
        eq("tryb Wl.: zegary pisza same, plik pusty", none, slurp(file));
        writeMode(2);
        applyMode(b);
        frame(b, s, 250, RESEND);
        eq("znow Fabryczny: 250 m", "250|m|END|", slurp(file));

        System.out.println("=== strzalki na zegarach wylaczone: nie ma kafelka, nie ma liczby");
        Field rgi = field("VC_RGI_ENABLED");
        boolean before = rgi.getBoolean(null);
        rgi.setBoolean(null, false);
        try {
            mark = count();
            frame(b, s, 240, DIST);
            check("bez strzalek slupek dla HUD dalej idzie", last("updateDistanceToNextManeuver", mark).indexOf(",true,") > 0);
            eq("bez strzalek: plik pusty", none, slurp(file));
        } finally {
            rgi.setBoolean(null, before);
        }
        frame(b, s, 230, DIST);
        eq("strzalki z powrotem: 230 m", "230|m|END|", slurp(file));

        b.onShutdown();
        eq("onShutdown: pusty", none, slurp(file));
        Thread.sleep(1200);
        check("po onShutdown watek skonczyl", !barThreadAlive());
        eq("po onShutdown dalej pusty", none, slurp(file));

        System.out.println("=== tryb Wl. od poczatku: jeden pusty zapis, potem nic");
        new File(file).delete();
        writeMode(0);
        BAPBridge c = bridge();
        applyMode(c);
        RouteGuidance.State t = route();
        frame(c, t, 5000, START);
        eq("Wl.: pierwszy zapis czysci to, co moglo zostac", none, slurp(file));
        new File(file).delete();
        frame(c, t, 200, DIST);
        frame(c, t, 100, DIST);
        eq("Wl.: pliku nikt wiecej nie pisze", null, slurp(file));
        c.onShutdown();
        new File(file).delete();
    }

    /* Wysokosc na VC wlaczona: czas dojazdu dalej idzie na pasek wedlug opcji (rc3: znikal). */
    private static void testBarWithAltitude() throws Exception {
        System.out.println("=== wysokosc na VC wlaczona a czas dojazdu na pasku (auto, run-10)");
        final String alt = K3346CapsuleFile.ALTITUDE_MODE_PATH;
        final String eta = K3346CapsuleFile.ETA_MODE_PATH;
        final String pillOk = K3346CapsuleFile.ETA_PILL_OK_PATH;
        final int bar = START | RouteGuidance.State.DIRTY_TIME_REMAINING
            | RouteGuidance.State.DIRTY_CURRENT_ROAD | RouteGuidance.State.DIRTY_DIST_DEST;
        try {
            new File(alt).delete();
            new File(K3346CapsuleFile.SLOT_PATH).delete();
            put(eta, "0\n");            /* Kapsula */
            put(pillOk, "1\n");         /* nakladka umie narysowac ramke na mapie */
            writeMode(0);
            BAPBridge b = bridge();
            applyMode(b);
            RouteGuidance.State s = route();
            s.currentRoad = "Glowna";
            s.timeRemainingSeconds = 720;
            s.distDestM = 9800;
            calls.clear();
            frame(b, s, 5000, bar);
            b.refreshBarIfChanged(s);
            String off = (String)field("lastBarText").get(b);
            check("wysokosc wyl., Kapsula: czas na pasku (" + off + ")", off != null && off.indexOf("12 min") >= 0);

            put(alt, "1\n");
            b.pollAltitude();
            b.refreshBarIfChanged(s);
            eq("wysokosc wl., Kapsula: pasek bez zmiany, czas zostaje", off, (String)field("lastBarText").get(b));

            put(eta, "1\n");            /* Mapa */
            b.refreshBarIfChanged(s);
            eq("wysokosc wl., Mapa: na pasku ulica", "Glowna", (String)field("lastBarText").get(b));
            eq("wysokosc wl., Mapa: ulica poszla do zegarow", "updateCurrentPositionInfo(Glowna)",
                last("updateCurrentPositionInfo", 0));

            put(eta, "2\n");            /* Obie */
            b.refreshBarIfChanged(s);
            eq("wysokosc wl., Obie: czas wraca na pasek", off, (String)field("lastBarText").get(b));

            put(alt, "0\n");
            b.pollAltitude();
            b.refreshBarIfChanged(s);
            eq("wysokosc wyl., Obie: bez zmiany", off, (String)field("lastBarText").get(b));

            System.out.println("=== nakladka pisze na koncach paska (capsule_text 1, male zegary)");
            put(K3346CapsuleFile.SLOT_PATH, "0 1\n");
            put(eta, "0\n");
            b.refreshBarIfChanged(s);
            eq("wysokosc wyl.: w polu ulica", "Glowna", (String)field("lastBarText").get(b));
            eq("wysokosc wyl.: czasy na koncach paska", "12 min|9,8 km|END|", slurp(K3346CapsuleFile.TEXT_PATH));
            put(alt, "1\n");
            b.pollAltitude();
            b.refreshBarIfChanged(s);
            eq("wysokosc wl.: czasy w polu, obok kompasu i wysokosci", "12 min · 9,8 km",
                (String)field("lastBarText").get(b));
            eq("wysokosc wl.: konce paska wolne", none(), slurp(K3346CapsuleFile.TEXT_PATH));
            put(alt, "0\n");
            b.pollAltitude();
            b.refreshBarIfChanged(s);
            eq("wysokosc znowu wyl.: czasy wracaja na konce", "12 min|9,8 km|END|", slurp(K3346CapsuleFile.TEXT_PATH));
            b.onShutdown();
        } finally {
            new File(alt).delete();
            new File(eta).delete();
            new File(pillOk).delete();
            new File(K3346CapsuleFile.ETA_PILL_PATH).delete();
            new File(K3346CapsuleFile.TEXT_PATH).delete();
            new File(K3346CapsuleFile.SLOT_PATH).delete();
        }
    }

    private static boolean anyAfter(String call, int from) {
        if (from < 0) {
            return false;
        }
        synchronized (calls) {
            for (int i = from + 1; i < calls.size(); i++) {
                if (calls.get(i).equals(call)) {
                    /* po nastepnym manewrze Prepare jest znow na miejscu */
                    for (int k = from + 1; k < i; k++) {
                        if (calls.get(k).startsWith("updateManeuverDescriptor(")) {
                            return false;
                        }
                    }
                    return true;
                }
            }
        }
        return false;
    }

    public static void main(String[] argv) throws Exception {
        if (argv.length == 2 && "trace".equals(argv[0])) {
            trace(Integer.parseInt(argv[1]));
            System.exit(0);
        }
        try {
            testOn();
            testFactory();
            testTurnDist();
            testBarWithAltitude();
        } finally {
            new File(MODE_FILE).delete();
            new File(K3346CapsuleFile.TURN_DIST_PATH).delete();
        }
        System.out.println("WYNIK_HUD_BRIDGE: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }
}
