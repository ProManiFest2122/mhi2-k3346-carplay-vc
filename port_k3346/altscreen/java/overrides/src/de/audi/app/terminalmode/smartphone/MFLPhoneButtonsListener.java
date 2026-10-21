/*
 * K3346 Etap 5.4 - odbierz/rozlacz z VC.
 *
 * Baza: fabryczna klasa K3346 (CFR 0.152 z work/lsd.jar). Jedyna zmiana:
 * konstruktor zapisuje fabryczny kontroler przyciskow telefonu w
 * K3346CallKeys, zeby akcje z karty telefonu VC szly ta sama droga co
 * przycisk sluchawki na kierownicy; keyTyped notuje moment nacisniecia
 * (K3346CallKeys.noteMflKey), zeby wrapper VC nie dublowal akcji, ktora kombi
 * wysyla rownolegle przez BAP. Obsluga przyciskow bez zmian.
 */
package de.audi.app.terminalmode.smartphone;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.ITerminalModeComponent;
import de.audi.app.terminalmode.ITerminalModeConfiguration;
import de.audi.app.terminalmode.k3346.K3346CallKeys;
import de.audi.atip.hmi.model.DefaultButtonListener;
import de.audi.atip.log.LogChannel;

public class MFLPhoneButtonsListener
implements ITerminalModeComponent {
    private static final String LOGCLASS = "MFLPhoneButtonsListener";
    private final LogChannel lc;
    private final ITerminalModeConfiguration configuration;
    private final IContext context;
    private final IPhoneCallController phoneCallController;

    public MFLPhoneButtonsListener(LogChannel logChannel, IContext iContext, IDSISmartphoneManager iDSISmartphoneManager, ITerminalModeConfiguration iTerminalModeConfiguration) {
        this.lc = logChannel;
        this.configuration = iTerminalModeConfiguration;
        this.context = iContext;
        this.phoneCallController = iDSISmartphoneManager.getPhoneCallController();
        try {
            K3346CallKeys.setController(this.phoneCallController);
        } catch (Throwable t) {
            /* K3346: bez uchwytu VC nie odbiera/rozlacza; przyciski stock dzialaja */
        }
    }

    public void init() {
        this.context.getButtonModel(3200047).setButtonListener(new DefaultButtonListener(){

            public void keyTyped(int n, int n2, int n3) {
                MFLPhoneButtonsListener.k3346NoteKey();
                MFLPhoneButtonsListener.this.lc.log(10000000, "<<- [%1.keyTyped] HOOK", (Object)MFLPhoneButtonsListener.LOGCLASS);
                if (MFLPhoneButtonsListener.this.configuration.hasBothPhoneMFLKeys()) {
                    MFLPhoneButtonsListener.this.phoneCallController.hook();
                } else {
                    MFLPhoneButtonsListener.this.phoneCallController.flash();
                }
            }
        });
        this.context.getButtonModel(3200048).setButtonListener(new DefaultButtonListener(){

            public void keyTyped(int n, int n2, int n3) {
                MFLPhoneButtonsListener.k3346NoteKey();
                MFLPhoneButtonsListener.this.lc.log(10000000, "<<- [%1.keyTyped] HANGUP", (Object)MFLPhoneButtonsListener.LOGCLASS);
                if (MFLPhoneButtonsListener.this.configuration.hasBothPhoneMFLKeys()) {
                    MFLPhoneButtonsListener.this.phoneCallController.hangup();
                } else {
                    MFLPhoneButtonsListener.this.phoneCallController.flash();
                }
            }
        });
    }

    /* K3346: moment nacisniecia przycisku sluchawki (bez wplywu na obsluge stock). */
    static void k3346NoteKey() {
        try {
            K3346CallKeys.noteMflKey();
        } catch (Throwable t) {
            /* diagnostyka/deduplikacja nigdy nie blokuje przycisku */
        }
    }

    public void deinit() {
        this.context.getButtonModel(3200048).setButtonListener(null);
        this.context.getButtonModel(3200047).setButtonListener(null);
    }
}
