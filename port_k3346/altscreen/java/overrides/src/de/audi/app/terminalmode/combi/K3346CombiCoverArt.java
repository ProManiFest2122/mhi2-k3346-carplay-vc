/*
 * K3346 Etap 5.3: sluchacz K3346CoverArt -> TerminalModeBapCombi (bez klas
 * anonimowych, zeby nazwy $N podmienionej klasy zostaly fabryczne). Java 1.4.
 */
package de.audi.app.terminalmode.combi;

import de.audi.app.terminalmode.k3346.K3346CoverArt;
import de.audi.app.terminalmode.k3346.K3346Diag;

final class K3346CombiCoverArt implements K3346CoverArt.Listener {
    private final TerminalModeBapCombi owner;

    K3346CombiCoverArt(TerminalModeBapCombi owner) {
        this.owner = owner;
    }

    public void onCoverArt() {
        try {
            owner.k3346OnCoverArt();
        } catch (Throwable t) {
            K3346Diag.log("K3346_COVER dispatch error " + t);
        }
    }
}
