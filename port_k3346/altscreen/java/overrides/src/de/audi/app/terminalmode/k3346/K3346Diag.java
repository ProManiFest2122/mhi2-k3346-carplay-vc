/*
 * K3346 Etap 5: wspolny zapis diagnostyczny klas podmieniajacych fabryczne.
 * Plik /net/rcc/dev/shmem/k3346-override-probe.log (pamiec wspolna RCC, ta sama
 * co logi Javy v8s; zbiera go COLLECT). Limit linii, kazdy blad polykany -
 * diagnostyka nigdy nie zmienia dzialania HMI. Java 1.4.
 */
package de.audi.app.terminalmode.k3346;

import java.io.FileOutputStream;

public final class K3346Diag {
    private static final String PATH = "/net/rcc/dev/shmem/k3346-override-probe.log";
    private static final int MAX_LINES = 400;
    private static int lines = 0;

    private K3346Diag() {
    }

    public static synchronized void log(String line) {
        try {
            if (lines >= MAX_LINES) {
                return;
            }
            lines++;
            FileOutputStream out = new FileOutputStream(PATH, true);
            try {
                out.write((System.currentTimeMillis() + " " + line + "\n").getBytes());
            } finally {
                out.close();
            }
        } catch (Throwable t) {
            /* diagnostyka nigdy nie przerywa HMI */
        }
    }
}
