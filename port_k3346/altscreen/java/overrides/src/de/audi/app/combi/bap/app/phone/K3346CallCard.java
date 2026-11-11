package de.audi.app.combi.bap.app.phone;

import de.audi.app.terminalmode.k3346.K3346Diag;
import de.audi.app.terminalmode.k3346.K3346HmiLanguage;
import java.io.FileOutputStream;
import java.io.IOException;

/*
 * Tells vc_overlay what the call card shows, format in overlay/call_card.h:
 *   <state>\n<name>\n<label>\nEND\n
 * /tmp on QNX has no rename(), so the text goes out in one write and ends with a
 * marker. The caller's name stays out of the logs: it is personal data.
 */
final class K3346CallCard {
    private static final int NAME_MAX_BYTES = 127;
    private static final String NO_CALL = "\n\n\nEND\n";

    static String path = "/tmp/vc-call-card";

    private static String last = null;

    private K3346CallCard() {
    }

    static void update(K3346PpsCall c) {
        try {
            write(text(K3346GatedPhoneService.bapState(c), c.name, c.number, K3346HmiLanguage.lang()));
        } catch (Throwable t) {
            K3346Diag.log("K3346_CALL_CARD update_failed " + t.getClass().getName());
        }
    }

    static void clear() {
        try {
            write(NO_CALL);
        } catch (Throwable t) {
            K3346Diag.log("K3346_CALL_CARD clear_failed " + t.getClass().getName());
        }
    }

    /* A forgotten text forces the next write, for the host test. */
    static void forget() {
        last = null;
    }

    /* The columns of K3346HmiLanguage.LANGS: en, pl, de, ru, cs, fr, it, es.  The
     * overlay draws them with the MMI's AudiTypeDisplay, which has all their letters
     * (Cyrillic too, checked 2026-11-11).  Short: the state of a ringing or a dialling
     * call shares the 320 px of the card with the name, which is cut to fit; the
     * unknown caller is the name itself, beside the clock (preview 2026-11-11). */
    static final String[] RINGING = {"Incoming call", "Dzwoni", "Anruf", "Входящий",
        "Příchozí hovor", "Appel entrant", "In arrivo", "Entrante"};
    static final String[] DIALING = {"Calling", "Łączenie", "Verbinden", "Вызов",
        "Vytáčení", "Appel sortant", "Chiamata", "Llamando"};
    static final String[] ACTIVE = {"In call", "Trwa rozmowa", "Im Gespräch", "Разговор",
        "Hovor", "En communication", "In chiamata", "En llamada"};
    static final String[] HELD = {"On hold", "Wstrzymana", "Gehalten", "Удержание",
        "Přidržený", "En attente", "In attesa", "En espera"};
    static final String[] UNKNOWN = {"Unknown", "Nieznany", "Unbekannt", "Неизвестный",
        "Neznámé číslo", "Inconnu", "Sconosciuto", "Desconocido"};

    static String text(int bapState, String name, String number, boolean polish) {
        return text(bapState, name, number, polish ? "pl" : "en");
    }

    static String text(int bapState, String name, String number, String lang) {
        String state;
        String[] label;
        switch (bapState) {
            case K3346GatedPhoneService.BAP_RINGING:
                state = "R";
                label = RINGING;
                break;
            case K3346GatedPhoneService.BAP_DIALING:
                state = "D";
                label = DIALING;
                break;
            case K3346GatedPhoneService.BAP_ACTIVE:
                state = "A";
                label = ACTIVE;
                break;
            case K3346GatedPhoneService.BAP_HELD:
                state = "H";
                label = HELD;
                break;
            default:
                return NO_CALL;
        }
        return state + "\n" + limit(shownName(name, number, lang)) + "\n"
            + K3346HmiLanguage.pick(label, lang) + "\nEND\n";
    }

    static String shownName(String name, String number, boolean polish) {
        return shownName(name, number, polish ? "pl" : "en");
    }

    static String shownName(String name, String number, String lang) {
        String n = oneLine(name);
        if (n.length() > 0) {
            return n;
        }
        String d = oneLine(number);
        if (d.length() > 0) {
            return d;
        }
        return K3346HmiLanguage.pick(UNKNOWN, lang);
    }

    private static String oneLine(String s) {
        return s == null ? "" : s.replace('\n', ' ').replace('\r', ' ').trim();
    }

    /* The overlay refuses a longer line. Whole characters only. */
    static String limit(String s) {
        int bytes = 0;
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            int size;
            if (c < 0x80) {
                size = 1;
            } else if (c < 0x800) {
                size = 2;
            } else if (c >= 0xD800 && c <= 0xDBFF) {
                size = 4;
            } else {
                size = 3;
            }
            if (bytes + size > NAME_MAX_BYTES) {
                return s.substring(0, i);
            }
            bytes += size;
            if (size == 4) {
                i++;
            }
        }
        return s;
    }

    private static void write(String text) throws IOException {
        if (text.equals(last)) {
            return;
        }
        FileOutputStream out = new FileOutputStream(path);
        try {
            out.write(text.getBytes("UTF-8"));
        } finally {
            out.close();
        }
        last = text;
        K3346Diag.log("K3346_CALL_CARD state=" + (text.charAt(0) == '\n' ? "none" : text.substring(0, 1)));
    }
}
