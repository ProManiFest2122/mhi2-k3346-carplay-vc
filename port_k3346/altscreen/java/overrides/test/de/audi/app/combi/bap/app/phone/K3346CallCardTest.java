package de.audi.app.combi.bap.app.phone;

import de.audi.app.terminalmode.k3346.K3346HmiLanguage;
import java.io.File;
import java.io.FileInputStream;

/* Plik karty rozmowy: to, czego oczekuje overlay/call_card.h. Bez auta. */
public final class K3346CallCardTest {

    private static int ok;
    private static int fail;

    private static void eq(String name, Object want, Object got) {
        if (String.valueOf(want).equals(String.valueOf(got))) {
            ok++;
            System.out.println("  OK   " + name);
        } else {
            fail++;
            System.out.println("  FAIL " + name + " oczekiwano=[" + want + "] jest=[" + got + "]");
        }
    }

    private static String read(File f) throws Exception {
        if (!f.exists()) {
            return "(brak)";
        }
        FileInputStream in = new FileInputStream(f);
        byte[] buf = new byte[512];
        int got = in.read(buf);
        in.close();
        return got <= 0 ? "" : new String(buf, 0, got, "UTF-8");
    }

    private static String repeat(char c, int n) {
        StringBuffer b = new StringBuffer();
        for (int i = 0; i < n; i++) {
            b.append(c);
        }
        return b.toString();
    }

    private static int utf8(String s) throws Exception {
        return s.getBytes("UTF-8").length;
    }

    private static K3346PpsCall call(int status, int direction, String name, String number) {
        K3346PpsCall c = new K3346PpsCall();
        c.status = status;
        c.direction = direction;
        c.name = name;
        c.number = number;
        return c;
    }

    public static void main(String[] argv) throws Exception {
        final int RING = K3346GatedPhoneService.BAP_RINGING;
        final int DIAL = K3346GatedPhoneService.BAP_DIALING;
        final int ACTIVE = K3346GatedPhoneService.BAP_ACTIVE;
        final int HELD = K3346GatedPhoneService.BAP_HELD;

        System.out.println("=== tresc po polsku");
        eq("dzwoni", "R\nAnna\nDzwoni\nEND\n", K3346CallCard.text(RING, "Anna", "+48", true));
        eq("laczenie", "D\nAnna\nŁączenie\nEND\n", K3346CallCard.text(DIAL, "Anna", "+48", true));
        eq("rozmowa", "A\nAnna\nTrwa rozmowa\nEND\n", K3346CallCard.text(ACTIVE, "Anna", "+48", true));
        eq("wstrzymana", "H\nAnna\nWstrzymana\nEND\n", K3346CallCard.text(HELD, "Anna", "+48", true));
        eq("brak rozmowy", "\n\n\nEND\n", K3346CallCard.text(0, "Anna", "+48", true));

        System.out.println("=== tresc po angielsku");
        eq("dzwoni", "R\nAnna\nIncoming call\nEND\n", K3346CallCard.text(RING, "Anna", "+48", false));
        eq("laczenie", "D\nAnna\nCalling\nEND\n", K3346CallCard.text(DIAL, "Anna", "+48", false));
        eq("rozmowa", "A\nAnna\nIn call\nEND\n", K3346CallCard.text(ACTIVE, "Anna", "+48", false));
        eq("wstrzymana", "H\nAnna\nOn hold\nEND\n", K3346CallCard.text(HELD, "Anna", "+48", false));

        System.out.println("=== nazwa");
        eq("nazwa", "Anna", K3346CallCard.shownName("Anna", "+48", true));
        eq("bez nazwy numer", "+48", K3346CallCard.shownName("", "+48", true));
        eq("null numer", "+48", K3346CallCard.shownName(null, "+48", true));
        eq("spacje jak brak", "+48", K3346CallCard.shownName("   ", "+48", true));
        eq("nic pl", "Nieznany numer", K3346CallCard.shownName("", "", true));
        eq("nic en", "Unknown number", K3346CallCard.shownName("", null, false));
        eq("nowa linia w nazwie", "An na", K3346CallCard.shownName("An\nna", "", true));
        eq("powrot karetki w nazwie", "An na", K3346CallCard.shownName("An\rna", "", true));
        eq("nowa linia nie psuje pliku", "A\nAn na\nIn call\nEND\n",
            K3346CallCard.text(ACTIVE, "An\nna", "", false));

        System.out.println("=== limit bajtow");
        eq("127 ascii wchodzi", 127, utf8(K3346CallCard.limit(repeat('a', 127))));
        eq("128 ascii uciete", 127, utf8(K3346CallCard.limit(repeat('a', 128))));
        eq("polskie: bez polowy znaku", 126, utf8(K3346CallCard.limit(repeat('ą', 100))));
        eq("trzybajtowe", 126, utf8(K3346CallCard.limit(repeat('€', 100))));
        String pair = "🚗";
        StringBuffer pairs = new StringBuffer();
        for (int i = 0; i < 40; i++) {
            pairs.append(pair);
        }
        String cut = K3346CallCard.limit(pairs.toString());
        eq("emoji: cale pary", 124, utf8(cut));
        eq("emoji: parzysta dlugosc", 0, cut.length() % 2);
        eq("krotka bez zmian", "Anna", K3346CallCard.limit("Anna"));
        eq("miesci sie w overlayu", "true", String.valueOf(
            utf8(K3346CallCard.text(ACTIVE, repeat('ą', 200), "", true)) < 208));

        System.out.println("=== plik");
        File dir = File.createTempFile("callcard", "");
        dir.delete();
        dir.mkdir();
        File card = new File(dir, "card");
        K3346CallCard.path = card.getPath();

        K3346PpsCall ring = call(K3346GatedPhoneService.IAP_CONNECTING, K3346GatedPhoneService.IAP_INCOMING, "Anna", "+48");
        K3346CallCard.update(ring);
        eq("dzwonek w pliku", K3346CallCard.text(RING, "Anna", "+48", K3346HmiLanguage.isPolish()), read(card));

        /* zapis tylko po zmianie: skasowany plik nie wraca przy tym samym stanie */
        card.delete();
        K3346CallCard.update(ring);
        eq("ten sam stan: bez zapisu", "(brak)", read(card));

        K3346PpsCall active = call(K3346GatedPhoneService.IAP_ACTIVE, K3346GatedPhoneService.IAP_INCOMING, "Anna", "+48");
        K3346CallCard.update(active);
        eq("po odebraniu", K3346CallCard.text(ACTIVE, "Anna", "+48", false), read(card));

        K3346CallCard.update(call(K3346GatedPhoneService.IAP_DISCONNECTED, 0, "", ""));
        eq("rozlaczenie czysci", "\n\n\nEND\n", read(card));

        K3346CallCard.update(active);
        K3346CallCard.clear();
        eq("clear czysci", "\n\n\nEND\n", read(card));

        /* blad zapisu nie wychodzi do watku PPS, a nastepna proba pisze dalej */
        K3346CallCard.path = new File(dir, "brak/plik").getPath();
        K3346CallCard.update(active);
        K3346CallCard.path = card.getPath();
        K3346CallCard.update(active);
        eq("po bledzie zapis sie ponawia", K3346CallCard.text(ACTIVE, "Anna", "+48", false), read(card));
        K3346CallCard.clear();

        System.out.println("=== integracja z uslugą telefonu");
        K3346HmiLanguage.setFramework(null);
        eq("bez frameworku angielski", "false", String.valueOf(K3346HmiLanguage.isPolish()));

        card.delete();
        dir.delete();
        System.out.println("WYNIK_CALL_CARD_JAVA: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }
}
