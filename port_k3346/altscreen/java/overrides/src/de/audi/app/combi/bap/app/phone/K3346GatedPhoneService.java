/*
 * K3346 Etap 5.2 - rozmowy CarPlay na karcie telefonu VC.
 *
 * Port luka-dev mib2q-carplay-rgi com.luka.carplay.telephony.GatedPhoneService
 * (bramka, mct 5 -> 3, Tel_State 15, FctID 22/23/24, cache i odtworzenie
 * wartosci fabrycznych). Roznice na K3346:
 *  - zrodlo rozmow: obiekt PPS /ramdisk/pps/iap2/telephony, ktory stock
 *    sterownik iAP2 (mm-ipod) wypelnia z TelephonyCallStateInformation 0x4155
 *    (w K3346 DSICarplayListenerImpl.updateCallState to pusty return);
 *    sygnal/operator z tego samego obiektu (TelephonyUpdate 0x4158);
 *  - odbierz/rozlacz z VC (Etap 5.4; "Odbierz" w oknie dzwonienia wymaga flagi
 *    ACCEPT_CALL i opcji ACCEPT dzwoniacej rozmowy): zamiast iAP2 AcceptCall/EndCall Luki
 *    fabryczna droga przycisku sluchawki na kierownicy (K3346CallKeys:
 *    ACCEPT 13 / END 14); gdy po FALLBACK_MS stan rozmowy w PPS sie nie
 *    zmienil -> FLASH 20 (dziala w aucie). Odrzucenie dzwoniacej rozmowy bez
 *    rezerwy FLASH (FLASH by ja odebral). Akcja, ktora kombi wyslalo z powodu
 *    przycisku sluchawki na kierownicy, nie jest powtarzana (stock flash juz
 *    ja wykonal) - inaczej odebranie + ponowne przelaczenie = "call failed";
 *  - bez list (ostatnie/ulubione), wybierania, wyciszania i zawieszania z VC.
 * Bramka = werdykt stock: FctID 14 z mobileConnectionType 5 wysyla
 * BAPPropertyTelFSGSetup tylko przy aktywnym CarPlay. Poza bramka wszystko
 * przechodzi 1:1. Kazdy blad watku PPS jest polykany (log K3346Diag).
 * Java 1.4.
 */
package de.audi.app.combi.bap.app.phone;

import de.audi.app.terminalmode.k3346.K3346CallKeys;
import de.audi.app.terminalmode.k3346.K3346Diag;
import de.audi.atip.interapp.combi.bap.phone.CombiBAPServicePhone;
import de.audi.atip.interapp.combi.bap.phone.data.CallStartTime;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallInfo;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallState;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPFavoriteNumberEntry;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPhoneMobileBatteryLevel;
import de.audi.atip.interapp.combi.bap.phone.data.FsgSetup;

import java.io.FileInputStream;

public class K3346GatedPhoneService implements CombiBAPServicePhone {
    private static final int MCT_HFP = 3, MCT_APPLE_LINK = 5;
    private static final int TEL_STATE_PHONE_ON_NO_SIM = 15;
    private static final int SLOTS = 7;
    private static final int NO_DURATION = CALL_DURATION_NOT_AVAILABLE;

    /* iAP2 TelephonyCallStateInformation / dio_manager.json kolejnosc wartosci */
    static final int IAP_DISCONNECTED = 0, IAP_ACTIVE = 1, IAP_HOLDING = 2, IAP_CONNECTING = 3;
    static final int IAP_INCOMING = 0;
    /* BAP CallState jak stock TelBAPCall.getCallState */
    static final int BAP_RINGING = 1, BAP_ACTIVE = 2, BAP_DIALING = 3, BAP_HELD = 5;
    private static final int BAP_CALL_TYPE_VOICE = 1;

    static final String PPS_TELEPHONY = "/ramdisk/pps/iap2/telephony";
    private static final long POLL_MS = 300;

    /* Etap 6: ostatnie polaczenia z hooka mm-ipod (iAP2 0x4171) i dzwonienie (0x415A) */
    static String RECENTS_FILE = "/tmp/k3346-tel-recents.txt";
    static String CMD_FILE = "/tmp/k3346-tel-cmd";
    /* BAP callMode jak stock BAPPropertyTelCombinedNumbers (Luka): */
    static final int CALLMODE_MISSED = 1, CALLMODE_RECEIVED = 2, CALLMODE_DIALED = 3;

    /* akcje z VC (K3346GatedPhoneListener) */
    static final int ACT_ACCEPT = 1, ACT_HANGUP = 2;
    private static final int RESULT_OK = 0, RESULT_FAILED = 1;
    private static final long FALLBACK_MS = 2500;
    /* przycisk sluchawki: kombi wysyla rownolegle BAP, a stock MFL robi flash */
    private static final long MFL_SETTLE_MS = 700, MFL_WINDOW_MS = 1500;

    /** Przyciski telefonu; w tescie atrapa. */
    interface KeySink {
        boolean press(int key);
        long lastSteeringKeyMs();
    }

    static KeySink keys = new KeySink() {
        public boolean press(int key) {
            return K3346CallKeys.press(key);
        }
        public long lastSteeringKeyMs() {
            return K3346CallKeys.lastMflKeyMs();
        }
    };

    private static K3346GatedPhoneService instance;

    final CombiBAPServicePhone real;
    private boolean gated;

    /* ostatnie wartosci stock dla funkcji objetych bramka */
    private FsgSetup stockFsg;
    private boolean haveStockOp;
    private int stockTelState;
    private boolean stockPrivacy, stockEnhancedPrivacy;
    private boolean[] stockSupport;
    private CombiBAPCallState[] stockStates;
    private boolean stockEcallPending;
    private CombiBAPCallInfo[] stockInfos;
    private CallStartTime[] stockDurations;
    private int stockSignal = -1, stockRegState, stockNetType, stockDataType;
    private boolean haveStockRegState, haveStockProvider;
    private int stockProvNameValid, stockProvIdValid;
    private String stockProvName, stockProvId;
    private CombiBAPCallStackEntry[] stockCombined;

    /* Etap 6: lista ostatnich CarPlay */
    private CombiBAPCallStackEntry[] recents = new CombiBAPCallStackEntry[0];
    private String recentsText;

    /* stan CarPlay z PPS */
    private K3346PpsCall call = K3346PpsCall.NONE;
    private String activeId;
    private long activeSince;
    private int signalPct, regState;
    private String carrier = "";

    private Thread poller;
    private volatile boolean polling;

    public K3346GatedPhoneService(CombiBAPServicePhone r) {
        this.real = r;
        synchronized (K3346GatedPhoneService.class) {
            instance = this;
        }
    }

    /** Owija raz; ponowna rejestracja nie nawarstwia wrapperow. */
    public static Object wrap(Object service) {
        if (service instanceof K3346GatedPhoneService || !(service instanceof CombiBAPServicePhone)) return service;
        return new K3346GatedPhoneService((CombiBAPServicePhone)service);
    }

    /* ---------------- bramka ---------------- */

    public synchronized void updateFsgSetup(FsgSetup f) {
        stockFsg = f;
        boolean carplay = f != null && f.getMobileConnectionType() == MCT_APPLE_LINK;
        if (carplay != gated) {
            gated = carplay;
            K3346Diag.log("K3346_PHONE gate " + (gated ? "ON (CarPlay -> VC calls)" : "OFF (stock)"));
            if (gated) {
                publishCarPlay();
                startPolling();
            } else {
                stopPolling();
                call = K3346PpsCall.NONE;
                activeId = null;
                replayStock();
            }
            return;
        }
        real.updateFsgSetup(gated ? asHfp(f) : f);
    }

    public synchronized void updateFSGOperationState(int telState, boolean privacy, boolean enhanced) {
        haveStockOp = true;
        stockTelState = telState; stockPrivacy = privacy; stockEnhancedPrivacy = enhanced;
        if (!gated) real.updateFSGOperationState(telState, privacy, enhanced);
    }

    public synchronized void updateMobileServiceSupport(boolean[] b) {
        stockSupport = b;
        if (!gated) real.updateMobileServiceSupport(b);
    }

    public synchronized void updateCallStates(CombiBAPCallState[] s, boolean ecallPending) {
        stockStates = s; stockEcallPending = ecallPending;
        if (!gated) real.updateCallStates(s, ecallPending);
    }

    public synchronized void updateCallInfo(CombiBAPCallInfo[] i) {
        stockInfos = i;
        if (!gated) real.updateCallInfo(i);
    }

    public synchronized void updateCallDurations(CallStartTime[] d) {
        stockDurations = d;
        if (!gated) real.updateCallDurations(d);
    }

    public synchronized void updateSignalQuality(int q) {
        stockSignal = q;
        if (!gated) real.updateSignalQuality(q);
    }

    public synchronized void updateRegisterState(int reg, int net, int data) {
        haveStockRegState = true; stockRegState = reg; stockNetType = net; stockDataType = data;
        if (!gated) real.updateRegisterState(reg, net, data);
    }

    public synchronized void updateNetworkProvider(int nameValid, String name, int idValid, String id) {
        haveStockProvider = true;
        stockProvNameValid = nameValid; stockProvName = name; stockProvIdValid = idValid; stockProvId = id;
        if (!gated) real.updateNetworkProvider(nameValid, name, idValid, id);
    }

    /* ---------------- akcje z VC (5.4) ---------------- */

    /** Akcja z karty telefonu VC; false = bramka zamknieta, obsluguje stock. */
    static boolean onVcAction(int action) {
        K3346GatedPhoneService g;
        synchronized (K3346GatedPhoneService.class) {
            g = instance;
        }
        try {
            return g != null && g.handle(action);
        } catch (Throwable t) {
            K3346Diag.log("K3346_PHONE action " + action + " error " + t + " -> stock");
            return false;
        }
    }

    synchronized boolean handle(int action) {
        if (!gated) return false;
        int st = bapState(call);
        boolean ok;
        if (action == ACT_ACCEPT) {
            ok = st == BAP_RINGING;
            real.acceptCallResult(ok ? RESULT_OK : RESULT_FAILED);
        } else if (action == ACT_HANGUP) {
            ok = st != 0;
            real.hangupCallResult(ok ? RESULT_OK : RESULT_FAILED);
        } else {
            return false;
        }
        K3346Diag.log("K3346_PHONE vc " + actionName(action) + " state=" + st + " result=" + (ok ? "OK" : "FAILED"));
        if (ok) schedulePress(action, st, callKey(call), System.currentTimeMillis());
        return true;
    }

    /* Nacisniecie odroczone: przycisk sluchawki na kierownicy dociera do HMI
     * (stock flash) i do kombi (BAP) mniej wiecej rownoczesnie. */
    private void schedulePress(final int action, final int pressedState, final String key, final long actionMs) {
        Thread t = new Thread("K3346CallKeyPress") {
            public void run() {
                try {
                    Thread.sleep(MFL_SETTLE_MS);
                } catch (InterruptedException e) {
                    return;
                }
                try {
                    pressUnlessSteering(action, pressedState, key, actionMs);
                } catch (Throwable x) {
                    K3346Diag.log("K3346_PHONE press error " + x);
                }
            }
        };
        t.setDaemon(true);
        t.start();
    }

    /* pakietowa (test): true = przycisk nacisniety przez wrapper */
    synchronized boolean pressUnlessSteering(int action, int pressedState, String key, long actionMs) {
        if (keys.lastSteeringKeyMs() >= actionMs - MFL_WINDOW_MS) {
            K3346Diag.log("K3346_PHONE vc " + actionName(action) + " from steering key - stock flash handles it");
            return false;
        }
        if (!gated || !key.equals(callKey(call)) || bapState(call) != pressedState) {
            K3346Diag.log("K3346_PHONE vc " + actionName(action) + " skipped (call changed)");
            return false;
        }
        boolean pressed = keys.press(action == ACT_ACCEPT ? K3346CallKeys.ACCEPT : K3346CallKeys.END);
        if (pressed && (action == ACT_ACCEPT || pressedState != BAP_RINGING)) scheduleFallback(pressedState, key);
        return pressed;
    }

    private static String actionName(int action) {
        return action == ACT_ACCEPT ? "accept" : "hangup";
    }

    private void scheduleFallback(final int pressedState, final String key) {
        Thread t = new Thread("K3346CallKeyCheck") {
            public void run() {
                try {
                    Thread.sleep(FALLBACK_MS);
                } catch (InterruptedException e) {
                    return;
                }
                try {
                    fallbackIfUnchanged(pressedState, key);
                } catch (Throwable x) {
                    K3346Diag.log("K3346_PHONE fallback error " + x);
                }
            }
        };
        t.setDaemon(true);
        t.start();
    }

    /* pakietowa (test): po ACCEPT nadal dzwoni / po END nadal trwa ta sama
     * rozmowa -> iPhone nie przyjal przycisku 13/14, FLASH jak kierownica */
    synchronized boolean fallbackIfUnchanged(int pressedState, String key) {
        if (!gated || !key.equals(callKey(call))) return false;
        int st = bapState(call);
        boolean unchanged = pressedState == BAP_RINGING ? st == BAP_RINGING : st != 0 && st != BAP_RINGING;
        if (!unchanged) {
            K3346Diag.log("K3346_PHONE vc key worked (state " + pressedState + " -> " + st + ")");
            return false;
        }
        K3346Diag.log("K3346_PHONE vc key ignored (state " + st + ") -> FLASH");
        return keys.press(K3346CallKeys.FLASH);
    }

    static String callKey(K3346PpsCall c) {
        return c.uuid.length() > 0 ? c.uuid : c.number + "|" + c.name;
    }

    /* ---------------- PPS ---------------- */

    private void startPolling() {
        if (poller != null) return;
        polling = true;
        poller = new Thread("K3346PhonePps") {
            public void run() {
                String last = null;
                int rawLogged = 0;
                K3346Diag.log("K3346_PHONE pps poller started " + PPS_TELEPHONY);
                while (polling) {
                    try {
                        String rt = readPps(RECENTS_FILE);
                        if (rt != null && !rt.equals(recentsText)) onRecents(rt);
                    } catch (Throwable t) {
                        /* brak pliku do pierwszej listy z iPhone'a - normalne */
                    }
                    try {
                        String text = readPps(PPS_TELEPHONY);
                        if (text != null && !text.equals(last)) {
                            last = text;
                            if (rawLogged < 30) {
                                rawLogged++;
                                K3346Diag.log("K3346_PHONE pps " + text.replace('\n', '|'));
                            }
                            onPps(K3346PpsCall.parse(text));
                        }
                    } catch (Throwable t) {
                        if (rawLogged < 30) {
                            rawLogged++;
                            K3346Diag.log("K3346_PHONE pps error " + t);
                        }
                    }
                    try {
                        Thread.sleep(POLL_MS);
                    } catch (InterruptedException e) {
                        break;
                    }
                }
                K3346Diag.log("K3346_PHONE pps poller stopped");
            }
        };
        poller.setDaemon(true);
        poller.start();
    }

    private void stopPolling() {
        polling = false;
        if (poller != null) {
            poller.interrupt();
            poller = null;
        }
    }

    static String readPps(String path) throws java.io.IOException {
        FileInputStream in = new FileInputStream(path);
        try {
            byte[] buf = new byte[4096];
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

    /* pakietowa: wolana przez watek PPS i test jednostkowy */
    synchronized void onPps(K3346PpsCall c) {
        call = c;
        signalPct = c.signal < 0 ? 0 : Math.min(c.signal, 5) * 20;
        regState = bapRegisterState(c.registration, c.airplane);
        carrier = c.operator;
        if (gated) {
            publishCalls();
            publishComm();
        }
    }

    /* ---------------- Etap 6: ostatnie polaczenia ---------------- */

    /* plik hooka: "K3346_TEL_RECENTS v1", "n=N", linie "i\ttyp\tts\tocc\tnumer\tnazwa" */
    synchronized void onRecents(String text) {
        recentsText = text;
        java.util.Vector v = new java.util.Vector();
        int from = 0;
        while (from < text.length()) {
            int end = text.indexOf('\n', from);
            if (end < 0) end = text.length();
            String line = text.substring(from, end);
            from = end + 1;
            String[] f = split(line, '\t');
            if (f.length < 6) continue;
            String num = f[4], name = f[5];
            if (num.length() == 0) continue;
            v.addElement(recent(v.size(), isNumber(name) ? "" : name, num,
                                callMode((int)toLong(f[1])), toLong(f[2])));
        }
        CombiBAPCallStackEntry[] r = new CombiBAPCallStackEntry[v.size()];
        v.copyInto(r);
        recents = r;
        K3346Diag.log("K3346_PHONE recents n=" + r.length);
        if (gated) real.updateCombinedNumbers(recents);
    }

    static long toLong(String s) {
        try {
            return Long.parseLong(s.trim());
        } catch (Throwable t) {
            return 0;
        }
    }

    static String[] split(String s, char sep) {
        java.util.Vector v = new java.util.Vector();
        int from = 0;
        while (true) {
            int i = s.indexOf(sep, from);
            if (i < 0) { v.addElement(s.substring(from)); break; }
            v.addElement(s.substring(from, i));
            from = i + 1;
        }
        String[] out = new String[v.size()];
        v.copyInto(out);
        return out;
    }

    static boolean isNumber(String s) {
        if (s == null || s.length() == 0) return false;
        for (int i = 0; i < s.length(); i++)
            if ("0123456789+*#-() ".indexOf(s.charAt(i)) < 0) return false;
        return true;
    }

    /* iAP2 Type 1 odebrane / 2 wybrane / 3 nieodebrane (Luka) */
    static int callMode(int iap2Type) {
        switch (iap2Type) {
            case 1: return CALLMODE_RECEIVED;
            case 2: return CALLMODE_DIALED;
            case 3: return CALLMODE_MISSED;
            default: return 0;
        }
    }

    static CombiBAPCallStackEntry recent(int pos, String name, String num, int mode, long ts) {
        if (ts <= 0) return new CombiBAPCallStackEntry(pos, name, num, 0, mode);
        java.util.Calendar c = java.util.Calendar.getInstance();
        c.setTime(new java.util.Date(ts * 1000L));
        return new CombiBAPCallStackEntry(pos, name, num, 0, mode,
            (short)c.get(java.util.Calendar.DAY_OF_MONTH), (short)(c.get(java.util.Calendar.MONTH) + 1),
            (short)c.get(java.util.Calendar.YEAR), (short)c.get(java.util.Calendar.HOUR_OF_DAY),
            (short)c.get(java.util.Calendar.MINUTE), (short)c.get(java.util.Calendar.SECOND),
            new de.audi.atip.interapp.phone.TelAdbEntryStruct());
    }

    /** Dzwonienie z listy VC; false = bramka zamknieta, obsluguje stock. */
    static boolean onVcDial(String number) {
        K3346GatedPhoneService g;
        synchronized (K3346GatedPhoneService.class) {
            g = instance;
        }
        try {
            return g != null && g.dial(number);
        } catch (Throwable t) {
            K3346Diag.log("K3346_PHONE dial error " + t + " -> stock");
            return false;
        }
    }

    synchronized boolean dial(String number) {
        if (!gated) return false;
        String n = number == null ? "" : number.trim();
        StringBuffer clean = new StringBuffer();
        for (int i = 0; i < n.length(); i++) {
            char ch = n.charAt(i);
            if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '*' || ch == '#') clean.append(ch);
        }
        boolean ok = clean.length() > 0 && clean.length() <= 32 && writeCommand("dial " + clean + "\n");
        real.dialNumberResult(ok ? RESULT_OK : RESULT_FAILED);
        K3346Diag.log("K3346_PHONE vc dial digits=" + clean.length() + " result=" + (ok ? "OK" : "FAILED"));
        return true;
    }

    static boolean writeCommand(String cmd) {
        java.io.File tmp = new java.io.File(CMD_FILE + ".tmp");
        try {
            java.io.FileOutputStream out = new java.io.FileOutputStream(tmp);
            try {
                out.write(cmd.getBytes("UTF-8"));
            } finally {
                out.close();
            }
            java.io.File dst = new java.io.File(CMD_FILE);
            dst.delete();
            return tmp.renameTo(dst);
        } catch (Throwable t) {
            K3346Diag.log("K3346_PHONE cmd write error " + t);
            return false;
        }
    }

    /* iAP2 RegistrationStatus -> BAP RegisterState (jak u Luki) */
    static int bapRegisterState(int iap2, boolean airplane) {
        if (airplane) return 0;
        switch (iap2) {
            case 2: return 2;
            case 3: return 3;
            case 4: return 1;
            case 5: return 4;
            default: return 0;
        }
    }

    static int bapState(K3346PpsCall c) {
        switch (c.status) {
            case IAP_ACTIVE: return BAP_ACTIVE;
            case IAP_HOLDING: return BAP_HELD;
            case IAP_CONNECTING: return c.direction == IAP_INCOMING ? BAP_RINGING : BAP_DIALING;
            default: return 0;
        }
    }

    /* ---------------- publikacja ---------------- */

    private static FsgSetup asHfp(FsgSetup f) {
        return FsgSetup.builder()
            .setInternalSimCardReaderPresent(f.isInternalSimCardReaderPresent())
            .setCableConnectionPossible(f.isCableConnectionPossible())
            .setHfpConnectionPossible(true)
            .setRsapConnectionPossible(f.isRsapConnectionPossible())
            .setAppleLinkConnectionPossible(f.isAppleLinkConnectionPossible())
            .setGoogleLinkConnectionPossible(f.isGoogleLinkConnectionPossible())
            .setMobileConnectionType(MCT_HFP)
            .build();
    }

    private void publishCarPlay() {
        real.updateFsgSetup(asHfp(stockFsg));
        real.updateFSGOperationState(TEL_STATE_PHONE_ON_NO_SIM, false, true);
        boolean[] support = new boolean[MOBILE_SERVICE_SUPPORT_MAX];
        support[MOBILE_SERVICE_SUPPORT_CALL_STATE] = true;
        support[MOBILE_SERVICE_SUPPORT_CALL_INFO] = true;
        support[MOBILE_SERVICE_SUPPORT_CALL_DURATION_SYNC] = true;
        /* ACCEPT_CALL + opcja rozmowy ACCEPT = "Odbierz" w oknie dzwonienia VC.
         * Przycisk sluchawki wysyla wtedy tez BAP acceptCall - tej kopii nie
         * powtarzamy (pressUnlessSteering), odbiera stock FLASH. */
        support[MOBILE_SERVICE_SUPPORT_ACCEPT_CALL] = true;
        support[MOBILE_SERVICE_SUPPORT_HANGUP_CALL] = true;
        support[MOBILE_SERVICE_SUPPORT_REGISTER_STATE] = true;
        support[MOBILE_SERVICE_SUPPORT_NETWORK_PROVIDER] = true;
        support[MOBILE_SERVICE_SUPPORT_SIGNAL_QUALITY] = true;
        support[MOBILE_SERVICE_SUPPORT_COMBINED_NUMBERS] = true;    /* Etap 6 */
        support[MOBILE_SERVICE_SUPPORT_DIAL_NUMBER] = true;         /* Etap 6: z listy */
        real.updateMobileServiceSupport(support);
        publishCalls();
        publishComm();
        real.updateCombinedNumbers(recents);
    }

    private void publishCalls() {
        CombiBAPCallState[] states = new CombiBAPCallState[SLOTS];
        CombiBAPCallInfo[] infos = new CombiBAPCallInfo[SLOTS];
        CallStartTime[] durations = noDurations();
        int st = bapState(call);
        if (st != 0) {
            states[0] = new CombiBAPCallState(st, BAP_CALL_TYPE_VOICE, 0);
            /* VC pokazuje Accept tylko dla rozmowy z ta opcja (RE Luki, callOptions 0x01) */
            states[0].setCallOptionAcceptCallEnabled(st == BAP_RINGING);
            infos[0] = new CombiBAPCallInfo(displayName(call), call.number, BAP_CALL_TYPE_VOICE, 0);
            if (st == BAP_ACTIVE || st == BAP_HELD) {
                long now = System.currentTimeMillis();
                String id = call.uuid.length() > 0 ? call.uuid : call.number;
                if (!id.equals(activeId)) { activeId = id; activeSince = now; }
                durations[0] = elapsed(now - activeSince);
            } else {
                activeId = null;
            }
        } else {
            activeId = null;
        }
        real.updateCallStates(states, false);
        real.updateCallInfo(infos);
        real.updateCallDurations(durations);
    }

    private void publishComm() {
        real.updateSignalQuality(signalPct);
        real.updateRegisterState(regState, 0, 0);
        int valid = carrier.length() > 0 ? 1 : 0;
        real.updateNetworkProvider(valid, carrier, valid, carrier);
    }

    private void replayStock() {
        if (stockFsg != null) real.updateFsgSetup(stockFsg);
        if (haveStockOp) real.updateFSGOperationState(stockTelState, stockPrivacy, stockEnhancedPrivacy);
        if (stockSupport != null) real.updateMobileServiceSupport(stockSupport);
        real.updateCallStates(stockStates, stockEcallPending);
        real.updateCallInfo(stockInfos);
        real.updateCallDurations(stockDurations != null ? stockDurations : noDurations());
        real.updateSignalQuality(stockSignal >= 0 ? stockSignal : 0);
        if (haveStockRegState) real.updateRegisterState(stockRegState, stockNetType, stockDataType);
        else real.updateRegisterState(0, 0, 0);
        if (haveStockProvider) real.updateNetworkProvider(stockProvNameValid, stockProvName, stockProvIdValid, stockProvId);
        else real.updateNetworkProvider(0, "", 0, "");
        real.updateCombinedNumbers(stockCombined != null ? stockCombined : new CombiBAPCallStackEntry[0]);
    }

    /* stock pokazuje pusta nazwe, gdy rowna numerowi (TelBAPCall.getDisplayName) */
    static String displayName(K3346PpsCall c) {
        return c.name.equals(c.number) ? "" : c.name;
    }

    private static CallStartTime elapsed(long ms) {
        int s = (int)(ms / 1000);
        return new CallStartTime(s / 3600, (s / 60) % 60, s % 60);
    }

    private static CallStartTime[] noDurations() {
        CallStartTime[] d = new CallStartTime[SLOTS];
        for (int i = 0; i < SLOTS; i++) d[i] = new CallStartTime(NO_DURATION);
        return d;
    }

    /* ---------------- przepuszczane 1:1 ---------------- */

    public void updateLockState(int i) { real.updateLockState(i); }
    public void updateDisconnectReason(int i) { real.updateDisconnectReason(i); }
    public void dialNumberResult(int i) { real.dialNumberResult(i); }
    public void dialNumberFromAdbEntryResult(int i) { real.dialNumberFromAdbEntryResult(i); }
    public void dialServiceResult(int i) { real.dialServiceResult(i); }
    public void confirmEmergencyCallResult(int i) { real.confirmEmergencyCallResult(i); }
    public void hangupCallResult(int i) { real.hangupCallResult(i); }
    public void acceptCallResult(int i) { real.acceptCallResult(i); }
    public void callHoldResult(int i) { real.callHoldResult(i); }
    public void resumeCallResult(int i) { real.resumeCallResult(i); }
    public void updateMicMuteState(boolean b) { real.updateMicMuteState(b); }
    public void releaseActiveCallAcceptWaitingCallResult(int i) { real.releaseActiveCallAcceptWaitingCallResult(i); }
    public void swapCallsResult(int i) { real.swapCallsResult(i); }
    public void callHoldAcceptWaitingCallResult(int i) { real.callHoldAcceptWaitingCallResult(i); }
    public void releaseAllCallsAcceptWaitingCallResult(int i) { real.releaseAllCallsAcceptWaitingCallResult(i); }
    public void setWaitingCallOnHoldResult(int i) { real.setWaitingCallOnHoldResult(i); }
    public void joinCallsResult(int i) { real.joinCallsResult(i); }
    public void splitCallResult(int i) { real.splitCallResult(i); }
    public void updateMobileBatteryLevel(CombiBAPhoneMobileBatteryLevel b) { real.updateMobileBatteryLevel(b); }
    public void updateMissedCallIndication(int a, int b) { real.updateMissedCallIndication(a, b); }
    public void updateMissedCalls(CombiBAPCallStackEntry[] e) { real.updateMissedCalls(e); }
    public void updateReceivedCalls(CombiBAPCallStackEntry[] e) { real.updateReceivedCalls(e); }
    public void updateDialedNumbers(CombiBAPCallStackEntry[] e) { real.updateDialedNumbers(e); }
    public synchronized void updateCombinedNumbers(CombiBAPCallStackEntry[] e) {
        stockCombined = e;
        if (!gated) real.updateCombinedNumbers(e);
    }
    public void updateRingToneMuteState(boolean b) { real.updateRingToneMuteState(b); }
    public void updateAutomaticRedialActive(boolean b) { real.updateAutomaticRedialActive(b); }
    public void updateAutomaticRedialExtendedInfo(int i, String s, String s1, int j) { real.updateAutomaticRedialExtendedInfo(i, s, s1, j); }
    public void updateSupportedServiceNumbers(boolean a, boolean b, boolean c, boolean d) { real.updateSupportedServiceNumbers(a, b, c, d); }
    public void updateFavoriteList(CombiBAPFavoriteNumberEntry[] e) { real.updateFavoriteList(e); }
}
