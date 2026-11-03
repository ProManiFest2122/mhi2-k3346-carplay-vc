package com.luka.carplay.routeguidance;

import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;

/* Bramka paska VC: wysokosc, skala i kompas ukryte, ulica nietknieta. Bez auta. */
public final class K2161GatedCombiServiceTest {

    private static final String HIDDEN_ALTITUDE = "updateAltitude(32767,2)";
    private static final String HIDDEN_SCALE = "updateMapScale(15,false,65535,255,false)";
    private static final String HIDDEN_COMPASS = "updateCompassInfo(-1,-1)";

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

    private static final class Recorder implements InvocationHandler {
        final List<String> calls = new ArrayList<String>();
        boolean throwOnAltitude;

        public Object invoke(Object proxy, Method m, Object[] args) {
            if (m.getDeclaringClass() == Object.class) {
                return m.getName().equals("hashCode") ? (Object)Integer.valueOf(1) : "real";
            }
            StringBuilder s = new StringBuilder(m.getName()).append('(');
            for (int i = 0; args != null && i < args.length; i++) {
                if (i > 0) s.append(',');
                s.append(args[i]);
            }
            calls.add(s.append(')').toString());
            if (throwOnAltitude && m.getName().equals("updateAltitude")) {
                throw new IllegalStateException("test");
            }
            return null;
        }
    }

    private static Recorder rec;

    private static K2161GatedCombiService gate() {
        rec = new Recorder();
        CombiBAPServiceNavi real = (CombiBAPServiceNavi)Proxy.newProxyInstance(
            K2161GatedCombiServiceTest.class.getClassLoader(),
            new Class[] { CombiBAPServiceNavi.class }, rec);
        return new K2161GatedCombiService(real);
    }

    public static void main(String[] argv) {
        System.out.println("=== bez ukrycia: wszystko przechodzi");
        K2161GatedCombiService g = gate();
        g.updateAltitude(75, 0);
        g.updateMapScale(1, true, 100, 0, true);
        g.updateCompassInfo(90, 4);
        check("trzy wartosci bez zmian", rec.calls.toString().equals(
            "[updateAltitude(75,0), updateMapScale(1,true,100,0,true), updateCompassInfo(90,4)]"));

        System.out.println("=== wejscie w ukrycie");
        rec.calls.clear();
        g.setNavStatusHidden(true);
        check("flaga wlaczona", g.isNavStatusHidden());
        check("wysyla trzy wartosci ukrywajace", rec.calls.size() == 3
            && rec.calls.contains(HIDDEN_ALTITUDE)
            && rec.calls.contains(HIDDEN_SCALE)
            && rec.calls.contains(HIDDEN_COMPASS));
        rec.calls.clear();
        g.setNavStatusHidden(true);
        check("powtorne wejscie nic nie wysyla", rec.calls.isEmpty());

        System.out.println("=== w trakcie ukrycia");
        g.updateAltitude(80, 0);
        g.updateCompassInfo(180, 8);
        check("wysokosc i kompas nie przechodza", rec.calls.isEmpty());
        g.updateMapScale(2, false, 200, 0, false);
        g.updateMapScale(3, false, 500, 0, false);
        check("skala: status ukryty przy kazdym wywolaniu (odpowiedz na SetGet)",
            rec.calls.size() == 2 && HIDDEN_SCALE.equals(rec.calls.get(0))
            && HIDDEN_SCALE.equals(rec.calls.get(1)));

        System.out.println("=== tekst ulicy nietkniety");
        rec.calls.clear();
        g.updateCurrentPositionInfo("Ulica");
        g.updateTurnToInfo("A", "B");
        check("ulica przechodzi przy ukryciu", rec.calls.toString().equals(
            "[updateCurrentPositionInfo(Ulica), updateTurnToInfo(A,B)]"));

        System.out.println("=== tekst paska podczas trasy CarPlay");
        rec.calls.clear();
        g.setRouteGuidanceBlocked(true);
        g.updateCurrentPositionInfo("Stock");
        g.updateTurnToInfo("A", "B");
        check("fabryczny tekst nie przechodzi przy trasie", rec.calls.isEmpty());
        g.setRouteGuidanceBlocked(false);
        g.updateCurrentPositionInfo("Stock");
        g.updateTurnToInfo("A", "B");
        check("po trasie znow przechodzi", rec.calls.toString().equals(
            "[updateCurrentPositionInfo(Stock), updateTurnToInfo(A,B)]"));

        System.out.println("=== wyjscie odtwarza OSTATNIE wartosci");
        rec.calls.clear();
        g.setNavStatusHidden(false);
        check("flaga wylaczona", !g.isNavStatusHidden());
        check("ostatnie wartosci, nie pierwsze", rec.calls.size() == 3
            && rec.calls.contains("updateAltitude(80,0)")
            && rec.calls.contains("updateMapScale(3,false,500,0,false)")
            && rec.calls.contains("updateCompassInfo(180,8)"));
        rec.calls.clear();
        g.updateAltitude(81, 0);
        check("po wyjsciu znow przechodzi", rec.calls.toString().equals("[updateAltitude(81,0)]"));

        System.out.println("=== bez wczesniejszych wartosci");
        g = gate();
        g.setNavStatusHidden(true);
        rec.calls.clear();
        g.setNavStatusHidden(false);
        check("wyjscie nic nie wysyla", rec.calls.isEmpty());

        System.out.println("=== wyjatek nie przerywa pozostalych zapisow");
        g = gate();
        rec.throwOnAltitude = true;
        g.setNavStatusHidden(true);
        check("wejscie: skala i kompas mimo wyjatku", rec.calls.contains(HIDDEN_SCALE)
            && rec.calls.contains(HIDDEN_COMPASS));
        g.updateAltitude(75, 0);
        g.updateCompassInfo(90, 4);
        rec.calls.clear();
        g.setNavStatusHidden(false);
        check("wyjscie: kompas mimo wyjatku", rec.calls.contains("updateCompassInfo(90,4)"));

        System.out.println("WYNIK_NAV_STATUS: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }
}
