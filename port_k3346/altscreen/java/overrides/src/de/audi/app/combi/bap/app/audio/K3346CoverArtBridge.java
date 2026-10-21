/*
 * K3346 Etap 5.3: most Terminal Mode (TerminalModeBapCombi) -> modul kombi
 * (AppConnectorTerminalMode). Fabryczne API AppConnectorTerminalMode zostaje
 * bez zmian, wiec wysylka okladki idzie przez metode pakietowa konektora,
 * zarejestrowanego tu w konstruktorze. Java 1.4.
 */
package de.audi.app.combi.bap.app.audio;

import de.audi.app.terminalmode.k3346.K3346Diag;

public final class K3346CoverArtBridge {
    private static volatile AppConnectorTerminalMode connector;

    private K3346CoverArtBridge() {
    }

    static void setConnector(AppConnectorTerminalMode c) {
        connector = c;
    }

    /** Wysyla okladke CarPlay do VC; false = brak konektora / CarPlay nie ma fokusu audio. */
    public static boolean push(String path, int id) {
        AppConnectorTerminalMode c = connector;
        if (c == null) {
            K3346Diag.log("K3346_COVER push - brak konektora kombi");
            return false;
        }
        try {
            return c.k3346PushCoverArt(path, id);
        } catch (Throwable t) {
            K3346Diag.log("K3346_COVER push error " + t);
            return false;
        }
    }

    /** Koniec sesji CarPlay: okladka nie odpowiada juz na prosby VC. */
    public static void clear() {
        AppConnectorTerminalMode c = connector;
        if (c != null) {
            try {
                c.k3346ClearCoverArt();
            } catch (Throwable t) {
                /* nic */
            }
        }
    }
}
