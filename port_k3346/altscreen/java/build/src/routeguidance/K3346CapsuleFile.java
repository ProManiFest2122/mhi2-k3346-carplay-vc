package com.luka.carplay.routeguidance;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

/*
 * The files shared with vc_overlay, format in overlay/capsule_text.h.
 * /tmp on QNX has no rename(), so the text goes out in one write and ends with
 * a marker: the overlay ignores a file caught half written.
 */
final class K3346CapsuleFile {

    static final String TEXT_PATH = "/tmp/vc-capsule-text";
    static final String SLOT_PATH = "/tmp/vc-capsule-slot";
    /* The MMI option, native/eta_mode.h and overrides K3346EtaRow. */
    static final String ETA_MODE_PATH = "/tmp/k3346-eta-mode";
    static final int ETA_CAPSULE = 0;
    static final int ETA_MAP = 1;
    static final int ETA_BOTH = 2;
    /* The maneuver card option, same owners. */
    static final String CARD_MODE_PATH = "/tmp/k3346-card-mode";
    static final int CARD_FULL = 0;
    static final int CARD_NONE = 1;
    /* "1\n" while Apple Maps guides, written by mm-ipod: native/nav_app.h. */
    static final String NAV_APPLE_PATH = "/tmp/k3346-nav-apple";
    /* On/off options: 0 on, 1 off.  Arrows: overlay too.  Map: the session script. */
    static final String ARROWS_MODE_PATH = "/tmp/k3346-arrows-mode";
    static final String MAP_MODE_PATH = "/tmp/k3346-map-mode";
    static final String HUD_MODE_PATH = "/tmp/k3346-hud-mode";
    /* The VC skin, 0 Classic, 1 Sport: native/eta_mode.h and the overlay. */
    static final String SKIN_PATH = "/tmp/vc-skin";
    /* The layout the stock HMI holds, as a slot: overlay_layout.h. */
    static final String VIEW_SIZE_PATH = "/tmp/vc-view-size";
    /* Present while the app sends no route guidance, native/eta_mode.h. */
    static final String NO_RGI_PATH = "/tmp/k3346-no-rgi";
    /* Written by the phone part, format in overlay/call_card.h. */
    static final String CALL_CARD_PATH = "/tmp/vc-call-card";
    static final int CALL_NONE = 0;
    static final int CALL_VISIBLE = 1;
    /* The MMI option "Call card on VC" (K3346EtaRow): 0 smoked, 1 light, 2 off.  With
     * the card off a call leaves the times where they are: native/eta_mode.h. */
    static final String CALLCARD_MODE_PATH = "/tmp/k3346-callcard-mode";
    static final int CALLCARD_OFF = 2;
    /* Our own arrival-time box on the map, overlay/eta_pill.h: the text in the format
     * of writeText(), and the overlay's word that it can draw it. */
    static final String ETA_PILL_PATH = "/tmp/vc-eta-pill";
    static final String ETA_PILL_OK_PATH = "/tmp/vc-eta-pill-ok";

    /* One overlay line is below this many bytes, or the file is refused. */
    private static final int LINE_MAX = 96;
    private static final int CALL_CARD_READ_MAX = 512;

    /* IViewSizeManager counts the map: 1 small map = big gauges = slot 1, 2 large map =
     * small gauges = slot 0.  Anything else says nothing (car run-64, run-65). */
    static int viewSizeSlot(int size) {
        return size == 1 ? 1 : (size == 2 ? 0 : -1);
    }

    /* readSlot results; otherwise slot + 2 * draw. */
    static final int SLOT_MISSING = -1;
    static final int SLOT_NOT_READY = -2;

    private K3346CapsuleFile() {
    }

    /* null when a line is too long for the overlay. */
    static byte[] encodeText(String left, String right) throws IOException {
        byte[] l = line(left).getBytes("UTF-8");
        byte[] r = line(right).getBytes("UTF-8");
        if (l.length >= LINE_MAX || r.length >= LINE_MAX) {
            return null;
        }
        ByteArrayOutputStream out = new ByteArrayOutputStream(l.length + r.length + 6);
        out.write(l);
        out.write('\n');
        out.write(r);
        out.write('\n');
        out.write("END\n".getBytes("UTF-8"));
        return out.toByteArray();
    }

    private static String line(String s) {
        return s == null ? "" : s.replace('\n', ' ').replace('\r', ' ');
    }

    static boolean writeText(String path, String left, String right) {
        FileOutputStream out = null;
        try {
            byte[] data = encodeText(left, right);
            if (data == null) {
                return false;
            }
            out = new FileOutputStream(path);
            out.write(data);
            return true;
        } catch (IOException e) {
            return false;
        } finally {
            if (out != null) {
                try {
                    out.close();
                } catch (IOException e) {
                    /* nothing more to do with this file */
                }
            }
        }
    }

    static int parseSlot(String text) {
        if (text == null || text.length() != 4 || text.charAt(1) != ' ' || text.charAt(3) != '\n') {
            return SLOT_NOT_READY;
        }
        char slot = text.charAt(0);
        char draw = text.charAt(2);
        if ((slot != '0' && slot != '1') || (draw != '0' && draw != '1')) {
            return SLOT_NOT_READY;
        }
        return (slot - '0') + 2 * (draw - '0');
    }

    static int parseEtaMode(String text) {
        if (text == null || text.length() != 2 || text.charAt(1) != '\n') {
            return SLOT_NOT_READY;
        }
        char mode = text.charAt(0);
        return mode >= '0' && mode <= '2' ? mode - '0' : SLOT_NOT_READY;
    }

    /* CALL_VISIBLE, CALL_NONE or SLOT_NOT_READY. Only a complete file counts. */
    static int parseCallCard(String text) {
        if (text == null || text.length() < 6 || !text.endsWith("\nEND\n")) {
            return SLOT_NOT_READY;
        }
        char state = text.charAt(0);
        if (state == '\n') {
            return CALL_NONE;
        }
        if (text.charAt(1) == '\n'
                && (state == 'R' || state == 'D' || state == 'A' || state == 'H')) {
            return CALL_VISIBLE;
        }
        return SLOT_NOT_READY;
    }

    static int readCallCard(String path) {
        String text = readUpTo(path, CALL_CARD_READ_MAX);
        return text == null ? CALL_NONE : parseCallCard(text);
    }

    /* No file is the default, smoked (on); a half-written one keeps the previous. */
    static int readCallCardMode(String path, int previous) {
        int read = readEtaMode(path);
        if (read == SLOT_MISSING) {
            return 0;
        }
        return read == SLOT_NOT_READY ? previous : read;
    }

    /* A call makes room for our card only while the card is on. */
    static boolean callCardShown(boolean callVisible, int callCardMode) {
        return callVisible && callCardMode != CALLCARD_OFF;
    }

    static int readSlot(String path) {
        String text = readShort(path);
        return text == null ? SLOT_MISSING : parseSlot(text);
    }

    /* SLOT_MISSING, SLOT_NOT_READY or the mode. */
    static int readEtaMode(String path) {
        String text = readShort(path);
        return text == null ? SLOT_MISSING : parseEtaMode(text);
    }

    /* SLOT_NOT_READY unless the file is whole. */
    static int parseOnOff(String text) {
        if (text == null || text.length() != 2 || text.charAt(1) != '\n') {
            return SLOT_NOT_READY;
        }
        char mode = text.charAt(0);
        return mode == '0' || mode == '1' ? mode - '0' : SLOT_NOT_READY;
    }

    /* No file means on, as after a boot before the HMI row came up. */
    static boolean readOff(String path, boolean previous) {
        String text = readShort(path);
        if (text == null) {
            return false;
        }
        int mode = parseOnOff(text);
        return mode == SLOT_NOT_READY ? previous : mode == 1;
    }

    /* The VC reserves room for the tile in its gauge while it runs route guidance
     * (run-39..42), so hiding the tile alone would leave a dark box. */
    static boolean vcRgiEnabled(String policy, boolean arrowsOff) {
        return !"off".equals(policy) && !arrowsOff;
    }

    static boolean exists(String path) {
        return new File(path).exists();
    }

    /* Creates or removes the file; its content means nothing. */
    static boolean writeFlag(String path, boolean on) {
        File file = new File(path);
        if (!on) {
            return file.delete() || !file.exists();
        }
        FileOutputStream out = null;
        try {
            out = new FileOutputStream(file);
            return true;
        } catch (IOException e) {
            return false;
        } finally {
            if (out != null) {
                try {
                    out.close();
                } catch (IOException e) {
                    /* nothing more to do with this file */
                }
            }
        }
    }

    static boolean writeEtaMode(String path, int mode) {
        FileOutputStream out = null;
        try {
            out = new FileOutputStream(path);
            out.write(new byte[] {(byte)('0' + mode), '\n'});
            return true;
        } catch (IOException e) {
            return false;
        } finally {
            if (out != null) {
                try {
                    out.close();
                } catch (IOException e) {
                    /* nothing more to do with this file */
                }
            }
        }
    }

    /* null = no file; "" = unreadable or empty, which no parser accepts. */
    private static String readShort(String path) {
        return readUpTo(path, 8);
    }

    private static String readUpTo(String path, int max) {
        File file = new File(path);
        if (!file.exists()) {
            return null;
        }
        FileInputStream in = null;
        try {
            in = new FileInputStream(file);
            byte[] buf = new byte[max];
            int got = in.read(buf);
            return got <= 0 ? "" : new String(buf, 0, got, "US-ASCII");
        } catch (IOException e) {
            return "";
        } finally {
            if (in != null) {
                try {
                    in.close();
                } catch (IOException e) {
                    /* read-only, nothing to lose */
                }
            }
        }
    }
}
