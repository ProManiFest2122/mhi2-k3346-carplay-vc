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
 *  - bez list (ostatnie/ulubione) i bez akcji z VC (odbierz/rozlacz/wybierz):
 *    wymagaja wstrzykiwania iAP2 - Etap 5.4. Kombi nie dostaje flag tych
 *    funkcji, wiec nie oferuje przyciskow, ktore nic by nie zrobily.
 * Bramka = werdykt stock: FctID 14 z mobileConnectionType 5 wysyla
 * BAPPropertyTelFSGSetup tylko przy aktywnym CarPlay. Poza bramka wszystko
 * przechodzi 1:1. Kazdy blad watku PPS jest polykany (log K3346Diag).
 * Java 1.4.
 */
package de.audi.app.combi.bap.app.phone;

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
        support[MOBILE_SERVICE_SUPPORT_REGISTER_STATE] = true;
        support[MOBILE_SERVICE_SUPPORT_NETWORK_PROVIDER] = true;
        support[MOBILE_SERVICE_SUPPORT_SIGNAL_QUALITY] = true;
        real.updateMobileServiceSupport(support);
        publishCalls();
        publishComm();
    }

    private void publishCalls() {
        CombiBAPCallState[] states = new CombiBAPCallState[SLOTS];
        CombiBAPCallInfo[] infos = new CombiBAPCallInfo[SLOTS];
        CallStartTime[] durations = noDurations();
        int st = bapState(call);
        if (st != 0) {
            states[0] = new CombiBAPCallState(st, BAP_CALL_TYPE_VOICE, 0);
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
    public void updateCombinedNumbers(CombiBAPCallStackEntry[] e) { real.updateCombinedNumbers(e); }
    public void updateRingToneMuteState(boolean b) { real.updateRingToneMuteState(b); }
    public void updateAutomaticRedialActive(boolean b) { real.updateAutomaticRedialActive(b); }
    public void updateAutomaticRedialExtendedInfo(int i, String s, String s1, int j) { real.updateAutomaticRedialExtendedInfo(i, s, s1, j); }
    public void updateSupportedServiceNumbers(boolean a, boolean b, boolean c, boolean d) { real.updateSupportedServiceNumbers(a, b, c, d); }
    public void updateFavoriteList(CombiBAPFavoriteNumberEntry[] e) { real.updateFavoriteList(e); }
}
