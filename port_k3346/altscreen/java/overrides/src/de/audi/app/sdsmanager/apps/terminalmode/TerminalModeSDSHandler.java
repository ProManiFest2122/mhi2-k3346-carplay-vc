/*
 * K3346 Etap 5.1 - Siri krotkim wcisnieciem PTT przy CarPlay.
 *
 * Baza: fabryczna klasa K3346 (CFR 0.152 z work/lsd.jar, MHI2_ER_AUG22_K3346 /
 * MU1438). Sonda 5.0 potwierdzila w aucie, ze J9 laduje ta klase z naszego
 * JAR-a (lsd/jars/ przed lsd.jxe).
 *
 * Zmiana (wzor: luka-dev mib2q-carplay-rgi, TerminalModeSDSHandler):
 * isTerminalModeSpeechActive() zwraca true takze wtedy, gdy aktywne urzadzenie
 * to CarPlay (TerminalModeDevice.CONN_METHOD_CARPLAY = 1). Wtedy fabryczny
 * SDSHMIListener.handlePTTShort() zamiast startowac asystenta Audi wola
 * notifyExternalSDSKeyListeners(1) -> SDSAppHandler.pttReleasedAfterShortPress
 * -> CarPlaySpeechRequestHandler.cancelPrewarm() (nasza podmiana uruchamia
 * tam Siri). Bez CarPlay - zachowanie fabryczne bez zmian.
 */
package de.audi.app.sdsmanager.apps.terminalmode;

import de.audi.app.sdsmanager.common.Logger;
import de.audi.app.terminalmode.k3346.K3346Diag;
import de.audi.atip.interapp.terminalmode.DefaultTerminalModeUpdateListener;
import de.audi.atip.interapp.terminalmode.TerminalModeAppState;
import de.audi.atip.interapp.terminalmode.TerminalModeDevice;
import de.audi.atip.log.LogChannel;

public class TerminalModeSDSHandler
extends DefaultTerminalModeUpdateListener
implements ITerminalModeSDSHandler {
    private final LogChannel lc = Logger.getHMILog();
    private volatile boolean terminalModeDeviceActive = false;
    private volatile boolean terminalModeSpeechActive = false;
    /* K3346: aktywne urzadzenie terminal mode jest polaczone przez CarPlay */
    private volatile boolean k3346CarPlayActive = false;

    static {
        K3346Diag.log("K3346_OVERRIDE_PROBE class_loaded TerminalModeSDSHandler (5.1 siri)");
    }

    public void updateActiveDeviceState(TerminalModeDevice terminalModeDevice) {
        boolean bl = terminalModeDevice.isActive();
        this.lc.log(10000000, "TerminalModeSDSHandler#updateActiveDeviceState: terminal mode device active: %1!", bl);
        if (!bl) {
            this.terminalModeSpeechActive = false;
        }
        this.terminalModeDeviceActive = bl;
        boolean carPlay = false;
        try {
            carPlay = bl && terminalModeDevice.getConnectionMethod() == TerminalModeDevice.CONN_METHOD_CARPLAY;
        } catch (Throwable t) {
            carPlay = false;
        }
        this.k3346CarPlayActive = carPlay;
        K3346Diag.log("K3346_SIRI updateActiveDeviceState active=" + bl + " carplay=" + carPlay);
    }

    public boolean isTerminalModeDeviceActive() {
        return this.terminalModeDeviceActive;
    }

    public void updateAppStates(TerminalModeAppState[] terminalModeAppStateArray) {
        for (int i2 = 0; i2 < terminalModeAppStateArray.length; ++i2) {
            TerminalModeAppState terminalModeAppState = terminalModeAppStateArray[i2];
            if (terminalModeAppState == null) continue;
            this.lc.log(10000000, "TerminalModeSDSHandler#updateAppStates: %1!", (Object)terminalModeAppState.toString());
            if (terminalModeAppState.getAppId() != 3 || !terminalModeAppState.isRunningOnDevice()) continue;
            this.terminalModeSpeechActive = true;
            return;
        }
        this.lc.log(10000000, "TerminalModeSDSHandler#updateAppStates: Currently no SDS running on terminal mode device!");
        this.terminalModeSpeechActive = false;
    }

    public boolean isTerminalModeSpeechActive() {
        if (this.terminalModeDeviceActive && this.terminalModeSpeechActive) {
            return true;
        }
        return this.terminalModeDeviceActive && this.k3346CarPlayActive;
    }
}
