/*
 * K3346 Etap 5.1 - touchpad MMI jako D-pad w CarPlay.
 *
 * Baza: fabryczna klasa K3346 (CFR 0.152 z work/lsd.jar). Ekran dotykowy bez
 * zmian. Zmienione tylko touchPadPressed/Moved/Released, i tylko gdy aktualny
 * kontroler klawiszy to kontroler CarPlay
 * (CarplayDSILifecycleController$TerminalModeDSIKeyEventsController): wtedy
 * pojedynczy palec na touchpadzie nie idzie jako surowe wspolrzedne (iOS w
 * trybie pokretla tylko podswietla element), tylko jako tyki D-pad przez
 * fabryczny updateKey(SOFTKEY_*, PRESSED/RELEASED) -> DSICarplay
 * BUTTON_UP/RIGHT/DOWN/LEFT. Natywny tryb trackpada celowo nie jest wlaczany
 * (u Luki crash Apple Music na iOS 27.2). Android Auto i brak CarPlay -
 * zachowanie fabryczne. Kazdy blad -> sciezka fabryczna.
 *
 * 1.0.1 (issue #6, Q7 4M): tryb z /mnt/app/root/k3346-touchpad.conf,
 * "touchpad=dpad" (domyslnie, takze bez pliku) albo "touchpad=stock" -
 * touchpad zawsze sciezka fabryczna, takze w CarPlay. Przystawki typu
 * Carlinkit (Android udajacy CarPlay) rysuja kursor z surowych wspolrzednych,
 * ktorych D-pad nie przekazuje. W trybie dpad zakres wspolrzednych pada
 * idzie do logu, bo progi Luki sa dobrane do malego pada na pokretle, a Q7
 * ma inny.
 */
package de.audi.app.terminalmode.keyevents;

import de.audi.app.terminalmode.ITerminalLogger;
import de.audi.app.terminalmode.ITerminalModeConfiguration;
import de.audi.app.terminalmode.k3346.K3346Diag;
import de.audi.atip.hmi.HMIService;

public class TMTouchscreenVBListener
extends TMVirtualButtonListener {
    private static final String LOGCLASS = "TMTouchscreenVBListener";
    private volatile TouchEvent firstFinger;
    private volatile TouchEvent lastPinchEvent;

    private static final String K3346_CARPLAY_KEY_CONTROLLER =
        "de.audi.app.terminalmode.dsi.carplay.CarplayDSILifecycleController$TerminalModeDSIKeyEventsController";
    private final K3346TouchpadDpad k3346Dpad = new K3346TouchpadDpad();
    private volatile boolean k3346Announced = false;

    private static final String K3346_TOUCHPAD_FILE = "/mnt/app/root/k3346-touchpad.conf";
    private static String k3346TouchpadSource = "default";
    /* true = touchpad jako D-pad w CarPlay (jak 1.0.0), false = fabrycznie */
    private static final boolean K3346_DPAD = !"stock".equals(k3346LoadTouchpadMode());

    /* Zakres wspolrzednych pada w trybie dpad - do strojenia progow (Q7). */
    private static final int K3346_RANGE_SAMPLES = 200;
    private int k3346Samples;
    private int k3346MinX = Integer.MAX_VALUE, k3346MaxX = Integer.MIN_VALUE;
    private int k3346MinY = Integer.MAX_VALUE, k3346MaxY = Integer.MIN_VALUE;

    /* "touchpad=<dpad|stock>"; '#' to komentarz.  FileInputStream, bo to API
     * jest sprawdzone na tym J9 (K3346GatedPhoneService).  Kazdy problem ->
     * dpad, czyli zachowanie 1.0.0. */
    private static String k3346LoadTouchpadMode() {
        java.io.FileInputStream in = null;
        try {
            in = new java.io.FileInputStream(K3346_TOUCHPAD_FILE);
            byte[] buf = new byte[256];
            int n = 0;
            int r;
            while (n < buf.length && (r = in.read(buf, n, buf.length - n)) > 0) {
                n += r;
            }
            String text = new String(buf, 0, n);
            int start = 0;
            while (start <= text.length()) {
                int end = text.indexOf('\n', start);
                if (end < 0) end = text.length();
                String line = text.substring(start, end).trim();
                start = end + 1;
                if (line.length() == 0 || line.charAt(0) == '#') continue;
                int eq = line.indexOf('=');
                if (eq < 0) continue;
                if (!"touchpad".equals(line.substring(0, eq).trim())) continue;
                String v = line.substring(eq + 1).trim().toLowerCase();
                if ("dpad".equals(v) || "stock".equals(v)) {
                    k3346TouchpadSource = "file";
                    return v;
                }
                k3346TouchpadSource = "file-invalid";
                return "dpad";
            }
            k3346TouchpadSource = "file-no-key";
        } catch (Throwable t) {
            k3346TouchpadSource = "default";
        } finally {
            if (in != null) {
                try { in.close(); } catch (Throwable ignored) { }
            }
        }
        return "dpad";
    }

    private void k3346NoteRange(int x, int y) {
        if (this.k3346Samples >= K3346_RANGE_SAMPLES) return;
        if (x < this.k3346MinX) this.k3346MinX = x;
        if (x > this.k3346MaxX) this.k3346MaxX = x;
        if (y < this.k3346MinY) this.k3346MinY = y;
        if (y > this.k3346MaxY) this.k3346MaxY = y;
        this.k3346Samples++;
        if (this.k3346Samples == 20 || this.k3346Samples == K3346_RANGE_SAMPLES) {
            K3346Diag.log("K3346_DPAD range samples=" + this.k3346Samples
                + " x=" + this.k3346MinX + ".." + this.k3346MaxX
                + " y=" + this.k3346MinY + ".." + this.k3346MaxY);
        }
    }

    public TMTouchscreenVBListener(ITerminalLogger iTerminalLogger, ITerminalModeConfiguration iTerminalModeConfiguration, HMIService hMIService) {
        super(iTerminalLogger, iTerminalModeConfiguration, hMIService);
        K3346Diag.log("K3346_DPAD TMTouchscreenVBListener constructed mode="
            + (K3346_DPAD ? "dpad" : "stock") + " source=" + k3346TouchpadSource);
    }

    /* Kontroler CarPlay jest aktywny -> touchpad jako D-pad. */
    private ITerminalModeDSIKeyEventsController k3346CarPlayController() {
        if (!K3346_DPAD) return null;      /* tryb stock: zawsze fabrycznie */
        try {
            ITerminalModeDSIKeyEventsController c = this.keyEventListener;
            if (c != null && K3346_CARPLAY_KEY_CONTROLLER.equals(c.getClass().getName())) {
                if (!this.k3346Announced) {
                    this.k3346Announced = true;
                    K3346Diag.log("K3346_DPAD carplay key controller active -> touchpad = dpad");
                }
                return c;
            }
        } catch (Throwable t) {
            /* sciezka fabryczna */
        }
        return null;
    }

    private static K3346TouchpadDpad.Sink k3346Sink(final ITerminalModeDSIKeyEventsController c) {
        return new K3346TouchpadDpad.Sink() {
            public void postDpad(int direction) {
                Key key;
                switch (direction) {
                    case K3346TouchpadDpad.UP:    key = Key.SOFTKEY_SOUTHWEST; break; /* -> 7 BUTTON_UP */
                    case K3346TouchpadDpad.RIGHT: key = Key.SOFTKEY_NORTHEAST; break; /* -> 6 BUTTON_RIGHT */
                    case K3346TouchpadDpad.DOWN:  key = Key.SOFTKEY_SOUTHEAST; break; /* -> 8 BUTTON_DOWN */
                    case K3346TouchpadDpad.LEFT:  key = Key.SOFTKEY_NORTHWEST; break; /* -> 5 BUTTON_LEFT */
                    default: return;
                }
                c.updateKey(key, KeyState.PRESSED);
                c.updateKey(key, KeyState.RELEASED);
            }
        };
    }

    public void touchPadPositionMoved(int n, int n2, int n3, int n4, int n5, int n6) {
        if (this.logger.isDebug2()) {
            this.logger.log(100000000, "[%1.touchPadPositionMoved]", (Object)LOGCLASS);
        }
        ITerminalModeDSIKeyEventsController c = this.k3346CarPlayController();
        if (c != null) {
            this.k3346NoteRange(n4, n5);
            this.k3346Dpad.onTouch(n4, n5, k3346Sink(c));
            return;
        }
        TouchEvent[] touchEventArray = new TouchEvent[]{new TouchEvent(n2, n3, n4, n5, false)};
        this.keyEventListener.updateTouchEvents(touchEventArray);
    }

    public void touchPadPressed(int n, int n2, int n3) {
        if (this.logger.isDebug2()) {
            this.logger.log(100000000, "[%1.touchPadPressed]", (Object)LOGCLASS);
        }
        ITerminalModeDSIKeyEventsController c = this.k3346CarPlayController();
        if (c != null) {
            /* jak fabrycznie: TouchEvent(true, n, n2, false) -> x = n, y = n2 */
            this.k3346NoteRange(n, n2);
            this.k3346Dpad.onTouchEnd();
            this.k3346Dpad.onTouch(n, n2, k3346Sink(c));
            return;
        }
        TouchEvent[] touchEventArray = new TouchEvent[]{new TouchEvent(true, n, n2, false)};
        this.keyEventListener.updateTouchEvents(touchEventArray);
    }

    public void touchPadReleased(int n, int n2, int n3) {
        if (this.logger.isDebug2()) {
            this.logger.log(100000000, "[%1.touchPadReleased]", (Object)LOGCLASS);
        }
        ITerminalModeDSIKeyEventsController c = this.k3346CarPlayController();
        if (c != null) {
            this.k3346Dpad.onTouchEnd();
            return;
        }
        TouchEvent[] touchEventArray = new TouchEvent[]{new TouchEvent(false, n, n2, false)};
        this.keyEventListener.updateTouchEvents(touchEventArray);
    }

    public void touchScreenMoved(int n, int n2, int n3, int n4, int n5, int n6) {
        if (!this.configuration.hasTouchscreen()) {
            return;
        }
        if (this.logger.isDebug2()) {
            this.logger.log(100000000, "[%1.touchScreenMoved] %2 %3", (Object)LOGCLASS, (Object)Integer.toString(n4), (Object)Integer.toString(n5));
        }
        TouchEvent[] touchEventArray = new TouchEvent[]{new TouchEvent(n2, n3, n4, n5, true)};
        this.keyEventListener.updateTouchEvents(touchEventArray);
    }

    public void touchScreenPressed(int n, int n2, int n3, int n4) {
        if (!this.configuration.hasTouchscreen()) {
            return;
        }
        if (this.logger.isDebug2()) {
            this.logger.log(100000000, "[%1.touchScreenPressed] %2 %3", (Object)LOGCLASS, (Object)Integer.toString(n2), (Object)Integer.toString(n3));
        }
        this.firstFinger = new TouchEvent(true, n2, n3, true);
        TouchEvent[] touchEventArray = new TouchEvent[]{this.firstFinger};
        this.keyEventListener.updateTouchEvents(touchEventArray);
    }

    public void touchScreenLongPressed(int n, int n2, int n3, int n4) {
        if (!this.configuration.hasTouchscreen()) {
            return;
        }
        if (this.logger.isDebug2()) {
            this.logger.log(100000000, "[%1.touchScreenLongPressed]", (Object)LOGCLASS);
        }
    }

    public void touchScreenReleased(int n, int n2, int n3, int n4) {
        TouchEvent[] touchEventArray;
        if (!this.configuration.hasTouchscreen()) {
            return;
        }
        if (this.logger.isDebug2()) {
            this.logger.log(100000000, "[%1.touchScreenReleased] %2 %3", (Object)LOGCLASS, (Object)Integer.toString(n2), (Object)Integer.toString(n3));
        }
        if (null == this.lastPinchEvent) {
            touchEventArray = new TouchEvent[]{new TouchEvent(false, n2, n3, true)};
        } else {
            touchEventArray = this.lastPinchEvent.getCurrentX() == n2 && this.lastPinchEvent.getCurrentY() == n3 ? new TouchEvent[]{new TouchEvent(false, this.firstFinger.getCurrentX(), this.firstFinger.getCurrentY(), true), new TouchEvent(false, this.lastPinchEvent.getCurrentX(), this.lastPinchEvent.getCurrentY(), true)} : new TouchEvent[]{new TouchEvent(false, n2, n3, true), new TouchEvent(false, this.lastPinchEvent.getCurrentX(), this.lastPinchEvent.getCurrentY(), true)};
            this.lastPinchEvent = null;
            this.firstFinger = null;
        }
        this.keyEventListener.updateTouchEvents(touchEventArray);
    }

    public void touchScreenDoubleClick(int n, int n2, int n3, int n4) {
        if (!this.configuration.hasTouchscreen()) {
            return;
        }
        if (this.logger.isDebug2()) {
            this.logger.log(100000000, "[%1.touchScreenDoubleClick]", (Object)LOGCLASS);
        }
    }

    public void touchScreenPinch(int n, float f2, int n2, int n3, int n4) {
        if (!this.configuration.hasTouchscreen()) {
            return;
        }
        if (this.logger.isDebug2()) {
            this.logger.log(100000000, "[%1.touchScreenPinch] %2 %3 %4", (Object)LOGCLASS, (Object)Float.toString(f2), (Object)Integer.toString(n2), (Object)Integer.toString(n3));
        }
        if (null == this.firstFinger) {
            return;
        }
        TouchEvent touchEvent = null == this.lastPinchEvent ? new TouchEvent(true, n2, n3, true) : new TouchEvent(this.lastPinchEvent.getCurrentX(), this.lastPinchEvent.getCurrentY(), n2, n3, true);
        TouchEvent[] touchEventArray = new TouchEvent[]{this.firstFinger, touchEvent};
        this.lastPinchEvent = touchEvent;
        this.keyEventListener.updateTouchEvents(touchEventArray);
    }

    public void touchScreenRotate(int n, short s, int n2) {
        if (!this.configuration.hasTouchscreen()) {
            return;
        }
        if (this.logger.isDebug2()) {
            this.logger.log(100000000, "[%1.touchScreenRotate] %2", (Object)LOGCLASS, (long)s);
        }
    }
}
