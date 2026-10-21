package de.audi.app.combi.bap.app.phone;

/* Pomocnik testow: odczyt malego pliku tekstowego. */
final class K3346CoverArtTestHelper {
    static String read(String path) {
        try {
            java.io.FileInputStream in = new java.io.FileInputStream(path);
            byte[] b = new byte[512];
            int n = in.read(b);
            in.close();
            return n > 0 ? new String(b, 0, n, "UTF-8") : "";
        } catch (Exception e) {
            return null;
        }
    }
}
