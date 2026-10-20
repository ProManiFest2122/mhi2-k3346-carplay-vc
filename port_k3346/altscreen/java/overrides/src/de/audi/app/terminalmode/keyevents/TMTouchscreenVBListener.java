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

    public TMTouchscreenVBListener(ITerminalLogger iTerminalLogger, ITerminalModeConfiguration iTerminalModeConfiguration, HMIService hMIService) {
        super(iTerminalLogger, iTerminalModeConfiguration, hMIService);
        K3346Diag.log("K3346_DPAD TMTouchscreenVBListener constructed");
    }

    /* Kontroler CarPlay jest aktywny -> touchpad jako D-pad. */
    private ITerminalModeDSIKeyEventsController k3346CarPlayController() {
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
