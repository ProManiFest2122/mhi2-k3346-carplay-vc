/*
 * K3346 Etap 5.4: uchwyt na fabryczny kontroler przyciskow telefonu.
 *
 * MFLPhoneButtonsListener (przycisk sluchawki na kierownicy) dostaje od stock
 * DSISmartphoneManagerProxy kontroler, ktory zawsze wskazuje aktywny telefon
 * (przy CarPlay: CarPlayDSIManager -> postDSICarplayButtonEvent 13/14/20 ->
 * dio_manager -> HID Eso_Telephony do iPhone'a). Podmieniony
 * MFLPhoneButtonsListener zapisuje go tutaj, a wrapper telefonu kombi
 * (Etap 5.4: odbierz/rozlacz z VC) naciska te same przyciski. Kazdy blad jest
 * polykany (log K3346Diag). Java 1.4.
 */
package de.audi.app.terminalmode.k3346;

import de.audi.app.terminalmode.smartphone.IPhoneCallController;

public final class K3346CallKeys {
    /* DSICarplay.BUTTON_ACCEPT_CALL 13 / BUTTON_END_CALL 14 / BUTTON_FLASH_CALL 20 */
    public static final int ACCEPT = 1, END = 2, FLASH = 3;

    private static volatile IPhoneCallController controller;
    private static volatile long lastMflKeyMs;

    private K3346CallKeys() {
    }

    /* Przycisk sluchawki na kierownicy: kombi moze rownoczesnie wyslac te sama
     * akcje przez BAP (np. hangupCall) - wrapper VC jej wtedy nie powtarza. */
    public static void noteMflKey() {
        lastMflKeyMs = System.currentTimeMillis();
        K3346Diag.log("K3346_CALLKEY mfl key");
    }

    public static long lastMflKeyMs() {
        return lastMflKeyMs;
    }

    public static void setController(IPhoneCallController c) {
        controller = c;
        K3346Diag.log("K3346_CALLKEY controller " + (c != null ? "set" : "null"));
    }

    /** Naciska (wcisniecie + puszczenie) przycisk telefonu; true = wyslane. */
    public static boolean press(int key) {
        IPhoneCallController c = controller;
        if (c == null) {
            K3346Diag.log("K3346_CALLKEY press " + name(key) + " - brak kontrolera");
            return false;
        }
        try {
            switch (key) {
                case ACCEPT: c.hook(); break;
                case END: c.hangup(); break;
                case FLASH: c.flash(); break;
                default: return false;
            }
            K3346Diag.log("K3346_CALLKEY press " + name(key));
            return true;
        } catch (Throwable t) {
            K3346Diag.log("K3346_CALLKEY press " + name(key) + " error " + t);
            return false;
        }
    }

    public static String name(int key) {
        switch (key) {
            case ACCEPT: return "ACCEPT(13)";
            case END: return "END(14)";
            case FLASH: return "FLASH(20)";
            default: return "?" + key;
        }
    }
}
