/*
 * K3346 Etap 5.1 - Siri krotkim wcisnieciem PTT przy CarPlay.
 *
 * Baza: fabryczna klasa K3346 (CFR 0.152 z work/lsd.jar). W K3346 nie ma typu
 * SiriAction - ICarplayDSIController.requestSIRIAction(int): 0 PREWARM,
 * 1 START (wcisniecie), 2 STOP (puszczenie).
 *
 * Zmiana (wzor: luka-dev mib2q-carplay-rgi, CarPlaySpeechRequestHandler):
 * cancelPrewarm() jest wolane przez SDSAppHandler po KROTKIM wcisnieciu PTT
 * (gdy TerminalModeSDSHandler.isTerminalModeSpeechActive() = true, co nasza
 * podmiana zapewnia przy CarPlay). Zamiast samego STOP odtwarzamy fabryczna
 * sciezke DLUGIEGO PTT: SDSAppHandler.pttLongDetected() (START + wlasciciel
 * SPEECH = urzadzenie, czyli mikrofon do telefonu) i pttReleasedAfterLongPress()
 * (STOP). Sam START w aucie Luki dawal animacje Siri bez dzwieku.
 * Kazdy blad -> zachowanie fabryczne (STOP), nigdy wyjatek do HMI.
 */
package de.audi.app.terminalmode.smartphone.carplay;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.dsi.carplay.ICarplayDSIController;
import de.audi.app.terminalmode.k3346.K3346Diag;
import de.audi.app.terminalmode.osgi.IServiceManager;
import de.audi.app.terminalmode.smartphone.ISpeechRequestHandler;
import de.audi.atip.interapp.ISDSKeyInterceptor;
import org.osgi.framework.ServiceReference;

public class CarPlaySpeechRequestHandler
implements ISpeechRequestHandler {
    private final ICarplayDSIController dioDSIController;
    /* K3346: kontekst do znalezienia SDSAppHandler (ISDSKeyInterceptor) */
    private final IContext k3346Context;

    public CarPlaySpeechRequestHandler(IContext iContext, ICarplayDSIController iCarplayDSIController) {
        this.dioDSIController = iCarplayDSIController;
        this.k3346Context = iContext;
    }

    public void prewarm() {
        this.dioDSIController.requestSIRIAction(0);
    }

    public void cancelPrewarm() {
        if (this.k3346StartSiriViaLongPressPath()) {
            return;
        }
        this.dioDSIController.requestSIRIAction(2);
    }

    public void startSpeechSession() {
        this.dioDSIController.requestSIRIAction(1);
    }

    public void abortActiveSpeechSession() {
        this.dioDSIController.requestSIRIAction(1);
        this.dioDSIController.requestSIRIAction(2);
    }

    public void pttReleasedAfterLongPress() {
        this.dioDSIController.requestSIRIAction(2);
    }

    private boolean k3346StartSiriViaLongPressPath() {
        try {
            if (this.k3346Context == null) {
                K3346Diag.log("K3346_SIRI no_context -> stock STOP");
                return false;
            }
            IServiceManager serviceManager = this.k3346Context.getServiceManager();
            if (serviceManager == null) {
                K3346Diag.log("K3346_SIRI no_service_manager -> stock STOP");
                return false;
            }
            ServiceReference[] refs = serviceManager.getServiceReferences(ISDSKeyInterceptor.class);
            if (refs == null) {
                K3346Diag.log("K3346_SIRI no_interceptor_refs -> stock STOP");
                return false;
            }
            for (int i = 0; i < refs.length; i++) {
                Object service = serviceManager.getService(refs[i]);
                if (service == null) {
                    continue;
                }
                if (!"de.audi.app.terminalmode.SDSAppHandler".equals(service.getClass().getName())) {
                    continue;
                }
                ISDSKeyInterceptor interceptor = (ISDSKeyInterceptor)service;
                interceptor.pttLongDetected();
                interceptor.pttReleasedAfterLongPress();
                K3346Diag.log("K3346_SIRI short_ptt -> long_ptt_path started");
                return true;
            }
            K3346Diag.log("K3346_SIRI SDSAppHandler not found among " + refs.length + " -> stock STOP");
        } catch (Throwable t) {
            K3346Diag.log("K3346_SIRI error " + t + " -> stock STOP");
        }
        return false;
    }
}
