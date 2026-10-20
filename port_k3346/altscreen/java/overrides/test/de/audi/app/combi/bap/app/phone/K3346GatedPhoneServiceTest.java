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
        ok(sup[CombiBAPServicePhone.MOBILE_SERVICE_SUPPORT_CALL_STATE] && !sup[CombiBAPServicePhone.MOBILE_SERVICE_SUPPORT_ACCEPT_CALL]
           && !sup[CombiBAPServicePhone.MOBILE_SERVICE_SUPPORT_HANGUP_CALL] && !sup[CombiBAPServicePhone.MOBILE_SERVICE_SUPPORT_DIAL_NUMBER],
           "CarPlay: wsparcie stanu rozmow, bez odbierz/rozlacz/wybierz");
        g.updateFSGOperationState(5, false, false);
        ok(((Integer)((Object[])last("updateFSGOperationState"))[0]).intValue() == 15, "CarPlay: stock Tel_State zatrzymany (w cache)");

        /* rozmowa przychodzaca -> aktywna -> koniec */
        g.onPps(n);
        CombiBAPCallState[] st = (CombiBAPCallState[])((Object[])last("updateCallStates"))[0];
        CombiBAPCallInfo[] in = (CombiBAPCallInfo[])((Object[])last("updateCallInfo"))[0];
        ok(st[0] != null && st[0].getCallState() == 1 && in[0].getPbName().equals("Jan Kowalski") && in[0].getTelNumber().equals("+00000000001"), "VC: dzwoni Jan Kowalski +00000000001");
        ok(((Integer)((Object[])last("updateSignalQuality"))[0]).intValue() == 80, "VC: sygnal 4 kreski -> 80%");
        n.status = 1; g.onPps(n);
        st = (CombiBAPCallState[])((Object[])last("updateCallStates"))[0];
        ok(st[0].getCallState() == 2, "VC: rozmowa aktywna");
        n.status = 0; g.onPps(n);
        st = (CombiBAPCallState[])((Object[])last("updateCallStates"))[0];
        ok(st[0] == null, "VC: po rozlaczeniu brak rozmowy");
        K3346PpsCall same = K3346PpsCall.parse("CallStateStatus:n:1\nCallStateDirection:n:1\nCallStatePhoneNumber::123\nCallStateCallerName::123\n");
        g.onPps(same); in = (CombiBAPCallInfo[])((Object[])last("updateCallInfo"))[0];
        ok(in[0].getPbName().equals(""), "VC: nazwa = numer -> pusta nazwa (jak stock)");

        /* bramka OFF -> odtworzenie stock */
        calls.clear();
        g.updateFsgSetup(fsg(3));
        ok(((Integer)((Object[])last("updateFSGOperationState"))[0]).intValue() == 5, "koniec CarPlay: odtworzony stock Tel_State 5");
        ok(((FsgSetup)((Object[])last("updateFsgSetup"))[0]).getMobileConnectionType() == 3, "koniec CarPlay: stock FsgSetup");
        ok(K3346GatedPhoneService.wrap(g) == g, "wrap nie nawarstwia wrapperow");
        System.out.println("WYNIK_PHONE: " + (fails == 0 ? "PASS" : "FAIL " + fails));
        System.exit(fails == 0 ? 0 : 1);
    }
}
