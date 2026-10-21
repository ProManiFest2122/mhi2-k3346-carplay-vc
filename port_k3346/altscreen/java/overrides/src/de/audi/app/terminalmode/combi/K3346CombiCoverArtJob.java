/*
 * K3346 Etap 5.3: ponowne wyslanie utworu z nowa okladka na dyspozytorze
 * Terminal Mode (tam, gdzie stock wysyla dane utworu). Java 1.4.
 */
package de.audi.app.terminalmode.combi;

import de.audi.app.terminalmode.dsi.AbstractDispatcherRunnable;
import de.audi.app.terminalmode.k3346.K3346Diag;

final class K3346CombiCoverArtJob extends AbstractDispatcherRunnable {
    private final TerminalModeBapCombi owner;

    K3346CombiCoverArtJob(TerminalModeBapCombi owner) {
        super("K3346CombiCoverArtJob");
        this.owner = owner;
    }

    public void run() {
        try {
            owner.k3346ResendTrack();
        } catch (Throwable t) {
            K3346Diag.log("K3346_COVER resend error " + t);
        }
    }
}
