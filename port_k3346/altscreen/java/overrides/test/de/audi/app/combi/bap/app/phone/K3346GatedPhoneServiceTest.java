package de.audi.app.combi.bap.app.phone;

import de.audi.atip.interapp.combi.bap.phone.CombiBAPServicePhone;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallInfo;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallState;
import de.audi.atip.interapp.combi.bap.phone.data.FsgSetup;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;

/* Test bramki i parsera PPS bez HMI: atrapa CombiBAPServicePhone nagrywa wywolania. */
public class K3346GatedPhoneServiceTest {
    static int fails = 0;
    static List calls = new ArrayList();
    static void ok(boolean c, String n) { if (!c) fails++; System.out.println((c ? "  OK   " : "  FAIL ") + n); }
    static Object last(String m) {
        for (int i = calls.size() - 1; i >= 0; i--) { Object[] e = (Object[])calls.get(i); if (e[0].equals(m)) return e[1]; }
        return null;
    }
    static FsgSetup fsg(int mct) {
        return FsgSetup.builder().setHfpConnectionPossible(false).setAppleLinkConnectionPossible(true).setMobileConnectionType(mct).build();
    }
    public static void main(String[] a) throws Exception {
        CombiBAPServicePhone real = (CombiBAPServicePhone)Proxy.newProxyInstance(
            CombiBAPServicePhone.class.getClassLoader(), new Class[]{CombiBAPServicePhone.class},
            new InvocationHandler() { public Object invoke(Object p, Method m, Object[] args) {
                calls.add(new Object[]{m.getName(), args}); return null; } });

        /* parser: format liczbowy i tekstowy */
        K3346PpsCall n = K3346PpsCall.parse("@telephony\nCallStatePhoneNumber::+00000000001\nCallStateCallerName::Jan Kowalski\nCallStateStatus:n:3\nCallStateDirection:n:0\nUniqueCallID::abc\nTelephonySignalStrength:n:4\nTelephonyRegistrationStatus:n:4\nMobileOperator::Play\n");
        ok(n.status == 3 && n.direction == 0 && n.number.equals("+00000000001") && n.name.equals("Jan Kowalski"), "parser: liczby");
        ok(n.signal == 4 && n.registration == 4 && n.operator.equals("Play"), "parser: sygnal/rejestracja/operator");
        K3346PpsCall t = K3346PpsCall.parse("@telephony\nCallStateStatus::Active\nCallStateDirection::Outgoing\nTelephonySignalStrength::3 bars\nTelephonyRegistrationStatus::Roaming\nTelephonyAirplaneModeStatus:b:false\n");
        ok(t.status == 1 && t.direction == 1 && t.signal == 3 && t.registration == 5 && !t.airplane, "parser: nazwy z dio_manager.json");
        ok(K3346PpsCall.parse("smieci bez dwukropkow\n:\n").status == 0, "parser: smieci -> brak rozmowy");
        ok(K3346GatedPhoneService.bapState(n) == K3346GatedPhoneService.BAP_RINGING, "przychodzaca + Connecting -> BAP ringing");

        K3346GatedPhoneService g = new K3346GatedPhoneService(real);
        /* poza bramka: przezroczysto */
        g.updateFsgSetup(fsg(3)); g.updateFSGOperationState(5, false, false);
        ok(((FsgSetup)((Object[])last("updateFsgSetup"))[0]).getMobileConnectionType() == 3, "bez CarPlay: FsgSetup przechodzi bez zmian");
        ok(((Integer)((Object[])last("updateFSGOperationState"))[0]).intValue() == 5, "bez CarPlay: stan stock przechodzi");

        /* bramka ON */
        calls.clear();
        g.updateFsgSetup(fsg(5));
        ok(((FsgSetup)((Object[])last("updateFsgSetup"))[0]).getMobileConnectionType() == 3, "CarPlay: mct 5 -> 3 (HFP), znika komunikat CarPlay");
        ok(((Integer)((Object[])last("updateFSGOperationState"))[0]).intValue() == 15, "CarPlay: Tel_State 15");
        boolean[] sup = (boolean[])((Object[])last("updateMobileServiceSupport"))[0];
        ok(sup[CombiBAPServicePhone.MOBILE_SERVICE_SUPPORT_CALL_STATE] && sup[CombiBAPServicePhone.MOBILE_SERVICE_SUPPORT_ACCEPT_CALL]
           && sup[CombiBAPServicePhone.MOBILE_SERVICE_SUPPORT_HANGUP_CALL] && sup[CombiBAPServicePhone.MOBILE_SERVICE_SUPPORT_DIAL_NUMBER]
           && sup[CombiBAPServicePhone.MOBILE_SERVICE_SUPPORT_COMBINED_NUMBERS],
           "CarPlay: stan rozmow, odbierz/rozlacz, ostatnie + wybieranie z listy (Etap 6)");
        g.updateFSGOperationState(5, false, false);
        ok(((Integer)((Object[])last("updateFSGOperationState"))[0]).intValue() == 15, "CarPlay: stock Tel_State zatrzymany (w cache)");

        /* rozmowa przychodzaca -> aktywna -> koniec */
        g.onPps(n);
        CombiBAPCallState[] st = (CombiBAPCallState[])((Object[])last("updateCallStates"))[0];
        CombiBAPCallInfo[] in = (CombiBAPCallInfo[])((Object[])last("updateCallInfo"))[0];
        ok(st[0] != null && st[0].getCallState() == 1 && in[0].getPbName().equals("Jan Kowalski") && in[0].getTelNumber().equals("+00000000001"), "VC: dzwoni Jan Kowalski +00000000001");
        ok(st[0].isCallOptionAcceptCallEnabled(), "VC: dzwoniaca rozmowa z opcja Odbierz");
        ok(((Integer)((Object[])last("updateSignalQuality"))[0]).intValue() == 80, "VC: sygnal 4 kreski -> 80%");
        n.status = 1; g.onPps(n);
        st = (CombiBAPCallState[])((Object[])last("updateCallStates"))[0];
        ok(st[0].getCallState() == 2 && !st[0].isCallOptionAcceptCallEnabled(), "VC: rozmowa aktywna, bez opcji Odbierz");
        n.status = 0; g.onPps(n);
        st = (CombiBAPCallState[])((Object[])last("updateCallStates"))[0];
        ok(st[0] == null, "VC: po rozlaczeniu brak rozmowy");
        K3346PpsCall same = K3346PpsCall.parse("CallStateStatus:n:1\nCallStateDirection:n:1\nCallStatePhoneNumber::123\nCallStateCallerName::123\n");
        g.onPps(same); in = (CombiBAPCallInfo[])((Object[])last("updateCallInfo"))[0];
        ok(in[0].getPbName().equals(""), "VC: nazwa = numer -> pusta nazwa (jak stock)");

        /* 5.4: akcje z VC */
        /* naciskanie testowane synchronicznie (pressUnlessSteering); odroczone watki
         * z onVcAction widza "przycisk kierownicy teraz" (steer = MAX) i nic nie naciskaja */
        final StringBuffer keys = new StringBuffer();
        final long[] steer = new long[]{Long.MAX_VALUE};
        K3346GatedPhoneService.keys = new K3346GatedPhoneService.KeySink() {
            public boolean press(int k) { keys.append(k).append(','); return true; }
            public long lastSteeringKeyMs() { return steer[0]; } };
        final int A = K3346GatedPhoneService.ACT_ACCEPT, H = K3346GatedPhoneService.ACT_HANGUP;
        final int RING = K3346GatedPhoneService.BAP_RINGING, ACT = K3346GatedPhoneService.BAP_ACTIVE;
        K3346PpsCall ring = K3346PpsCall.parse("CallStateStatus:n:3\nCallStateDirection:n:0\nCallStatePhoneNumber::+00000000001\nUniqueCallID::u1\n");
        String ck = K3346GatedPhoneService.callKey(ring);
        long now = System.currentTimeMillis();
        g.onPps(ring); keys.setLength(0); calls.clear();
        ok(K3346GatedPhoneService.onVcAction(A) && ((Integer)((Object[])last("acceptCallResult"))[0]).intValue() == 0
           && keys.length() == 0, "VC odbierz przy dzwonieniu -> wynik OK od razu, nacisniecie odroczone");
        steer[0] = 0;
        ok(g.pressUnlessSteering(A, RING, ck, now) && keys.toString().equals("1,"), "bez przycisku kierownicy -> ACCEPT(13)");
        keys.setLength(0); steer[0] = now - 200;
        ok(!g.pressUnlessSteering(A, RING, ck, now) && keys.length() == 0,
           "przycisk sluchawki tuz przed BAP accept -> nic (stock flash juz odebral; bylo 'call failed')");
        steer[0] = now + 300;
        ok(!g.pressUnlessSteering(H, RING, ck, now) && keys.length() == 0, "przycisk sluchawki tuz po BAP hangup -> nic");
        steer[0] = now - 5000;
        ok(g.pressUnlessSteering(H, RING, ck, now) && keys.toString().equals("2,"), "stary przycisk (5 s) -> VC odrzuc = END(14)");
        keys.setLength(0); steer[0] = 0;
        ok(g.fallbackIfUnchanged(RING, ck) && keys.toString().equals("3,"), "nadal dzwoni po ACCEPT -> rezerwa FLASH(20)");
        ring.status = 1; g.onPps(ring); keys.setLength(0);
        ok(!g.fallbackIfUnchanged(RING, ck) && keys.length() == 0, "odebrane po ACCEPT -> bez FLASH (nie rozlacza)");
        ok(!g.pressUnlessSteering(A, RING, ck, now) && keys.length() == 0, "stan zmienil sie przed nacisnieciem -> nic");
        ok(g.pressUnlessSteering(H, ACT, ck, now) && keys.toString().equals("2,"), "VC odloz przy rozmowie -> END(14)");
        keys.setLength(0);
        ok(g.fallbackIfUnchanged(ACT, ck) && keys.toString().equals("3,"), "rozmowa trwa po END -> rezerwa FLASH(20)");
        ring.status = 0; g.onPps(ring); keys.setLength(0);
        ok(!g.fallbackIfUnchanged(ACT, ck) && keys.length() == 0, "rozlaczone po END -> bez FLASH");
        steer[0] = Long.MAX_VALUE; calls.clear();
        ok(K3346GatedPhoneService.onVcAction(A) && ((Integer)((Object[])last("acceptCallResult"))[0]).intValue() == 1,
           "odbierz bez rozmowy -> wynik FAILED");
        Thread.sleep(1000);
        ok(keys.length() == 0, "odroczone watki przy przycisku kierownicy nic nie nacisnely");

        /* Etap 6 v4: rozmowy z sesji CarPlay -> wpisy na gorze (dotad: 2 odebrane powyzej) */
        K3346PpsCall out = K3346PpsCall.parse("CallStateStatus:n:3\nCallStateDirection:n:1\nCallStatePhoneNumber::+00000000005\nUniqueCallID::o1\n");
        g.onPps(out); out.status = 0; g.onPps(out);
        K3346PpsCall miss = K3346PpsCall.parse("CallStateStatus:n:3\nCallStateDirection:n:0\nCallStatePhoneNumber::+00000000006\nUniqueCallID::m1\n");
        g.onPps(miss); miss.status = 0; g.onPps(miss);
        de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[] vc = g.recentsForVc();
        ok(vc.length == 4 && vc[0].getTelNumber().equals("+00000000006") && vc[0].getCallMode() == K3346GatedPhoneService.CALLMODE_MISSED
           && vc[1].getCallMode() == K3346GatedPhoneService.CALLMODE_DIALED && vc[2].getCallMode() == K3346GatedPhoneService.CALLMODE_RECEIVED
           && vc[3].getPbName().equals("Jan Kowalski") && vc[0].getPosID() == 0 && vc[3].getPosID() == 3,
           "rozmowy sesji: nieodebrana, wybrana, odebrane - najnowsza na gorze, posID 0..3");
        ok(vc[0].getYear() != 1970, "rozmowa sesji bez daty 1970 (zegar MMX nieustawiony)");

        /* lista z BT zapamietana; czyszczenie MMI po starcie CarPlay (102 -> 66 -> 31 -> 0) pomijane */
        de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[] bt =
            new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[102];
        for (int i = 0; i < bt.length; i++)
            bt[i] = new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry(i, "K" + i, "+0000000" + (1000 + i), 0, 2);
        g.updateCombinedNumbers(bt);
        g.updateCombinedNumbers(new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[66]);
        g.updateCombinedNumbers(new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[31]);
        calls.clear();
        g.updateCombinedNumbers(new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[0]);
        vc = (de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[])((Object[])last("updateCombinedNumbers"))[0];
        ok(vc.length == 50 && vc[0].getTelNumber().equals("+00000000006") && vc[4].getTelNumber().equals("+00000001000")
           && vc[4].getPosID() == 4 && vc[49].getPosID() == 49,
           "CarPlay: VC = 4 rozmowy sesji + lista z BT (max 50), czyszczenie MMI pominiete");

        /* ulubione z BT zapamietane */
        de.audi.atip.interapp.combi.bap.phone.data.CombiBAPFavoriteNumberEntry[] fav =
            new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPFavoriteNumberEntry[]{
                new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPFavoriteNumberEntry(0, "Ulub", "+00000000007")};
        g.updateFavoriteList(fav);
        calls.clear();
        g.updateFavoriteList(new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPFavoriteNumberEntry[0]);
        ok(((Object[])last("updateFavoriteList"))[0] == fav, "CarPlay: ulubione z BT zostaja mimo czyszczenia MMI");

        de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[] stockList =
            new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[]{
                new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry(0, "Stock", "+00000000009", 0, 2)};

        /* Etap 6: ostatnie polaczenia z pliku hooka + wybieranie */
        calls.clear();
        g.onRecents("K3346_TEL_RECENTS v1\nn=3\n0\t2\t1777100628\t2\t+00000000001\tJan Kowalski\n"
            + "1\t3\t1777000000\t1\t+00000000002\t+00 000 000 002\n2\t1\t0\t1\t+00000000003\tAnna\n");
        de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[] rec =
            (de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[])((Object[])last("updateCombinedNumbers"))[0];
        ok(rec.length == 3 && rec[0].getPbName().equals("Jan Kowalski") && rec[0].getTelNumber().equals("+00000000001")
           && rec[0].getCallMode() == K3346GatedPhoneService.CALLMODE_DIALED && rec[0].getYear() >= 2026,
           "ostatnie: wpis 0 wybrane, nazwa, numer, data");
        ok(rec[1].getPbName().equals("") && rec[1].getCallMode() == K3346GatedPhoneService.CALLMODE_MISSED,
           "ostatnie: nazwa = sformatowany numer -> pusta, nieodebrane");
        ok(rec[2].getCallMode() == K3346GatedPhoneService.CALLMODE_RECEIVED, "ostatnie: odebrane");
        calls.clear();
        g.updateCombinedNumbers(stockList);
        ok(((de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[])((Object[])last("updateCombinedNumbers"))[0]).length == 3,
           "lista iAP2 niepusta: ma pierwszenstwo przed lista MMI");
        java.io.File cmdDir = new java.io.File(System.getProperty("java.io.tmpdir"));
        K3346GatedPhoneService.CMD_FILE = new java.io.File(cmdDir, "k3346-tel-cmd-test").getPath();
        new java.io.File(K3346GatedPhoneService.CMD_FILE).delete();
        calls.clear();
        ok(K3346GatedPhoneService.onVcDial("+00 000-000 001") && ((Integer)((Object[])last("dialNumberResult"))[0]).intValue() == 0,
           "VC wybierz z listy przy CarPlay -> wynik OK");
        String cmd = K3346CoverArtTestHelper.read(K3346GatedPhoneService.CMD_FILE);
        ok("dial +00000000001\n".equals(cmd), "plik polecenia dla hooka: dial + same cyfry");
        calls.clear();
        ok(K3346GatedPhoneService.onVcDial("abc") && ((Integer)((Object[])last("dialNumberResult"))[0]).intValue() == 1,
           "wybieranie bez cyfr -> FAILED");

        /* bramka OFF -> odtworzenie stock */
        calls.clear();
        g.updateFsgSetup(fsg(3));
        ok(((Integer)((Object[])last("updateFSGOperationState"))[0]).intValue() == 5, "koniec CarPlay: odtworzony stock Tel_State 5");
        ok(((FsgSetup)((Object[])last("updateFsgSetup"))[0]).getMobileConnectionType() == 3, "koniec CarPlay: stock FsgSetup");
        ok(K3346GatedPhoneService.wrap(g) == g, "wrap nie nawarstwia wrapperow");
        keys.setLength(0);
        ok(!K3346GatedPhoneService.onVcAction(H) && keys.length() == 0,
           "bez CarPlay: odloz z VC idzie do stock (HFP)");
        ok(K3346GatedPhoneListener.wrap(null) == null, "listener: null zostaje null");
        ok(!K3346GatedPhoneService.onVcDial("+00000000001"), "bez CarPlay: wybieranie idzie do stock");
        de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[] sc =
            (de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[])((Object[])last("updateCombinedNumbers"))[0];
        ok(sc == stockList, "koniec CarPlay: lista ostatnich stock odtworzona");

        /* rozmowy z sesji zostaja po ponownym podlaczeniu kabla; swieza lista z BT usuwa duplikat */
        g.onRecents("K3346_TEL_RECENTS v1\nn=0\n");
        g.updateFsgSetup(fsg(5));
        vc = g.recentsForVc();
        ok(vc.length > 4 && vc[0].getTelNumber().equals("+00000000006"), "ponowny CarPlay: rozmowy sesji nadal na gorze");
        g.updateFsgSetup(fsg(3));
        de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[] fresh =
            new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry[]{
                new de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry(0, "", "+00 000 000 006", 0,
                    K3346GatedPhoneService.CALLMODE_MISSED)};
        g.updateCombinedNumbers(fresh);
        g.updateFsgSetup(fsg(5));
        vc = g.recentsForVc();
        ok(!vc[0].getTelNumber().equals("+00000000006") && vc[0].getCallMode() == K3346GatedPhoneService.CALLMODE_DIALED,
           "swieza lista z BT ma te rozmowe -> wpis sesji usuniety (bez duplikatu)");
        g.updateFsgSetup(fsg(3));
        K3346GatedPhoneService.CMD_FILE = new java.io.File(cmdDir, "k3346-tel-cmd-test2").getPath();
        ok(K3346GatedPhoneService.writeCommand("dial +1\n") && "dial +1\n".equals(K3346CoverArtTestHelper.read(K3346GatedPhoneService.CMD_FILE)),
           "polecenie zapisane wprost (bez renameTo)");
        System.out.println("WYNIK_PHONE: " + (fails == 0 ? "PASS" : "FAIL " + fails));
        System.exit(fails == 0 ? 0 : 1);
    }
}
