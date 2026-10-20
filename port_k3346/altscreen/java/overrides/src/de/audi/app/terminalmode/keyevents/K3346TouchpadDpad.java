/*
 * K3346 Etap 5.1 - touchpad MMI jako D-pad w CarPlay.
 *
 * Port 1:1 logiki luka-dev mib2q-carplay-rgi com.luka.carplay.input.
 * TouchpadController (pojedynczy palec -> tyki kierunku, prog zalezny od
 * predkosci: szybko 150, wolno 200 jednostek pada; martwa strefa 30).
 * Roznica: tyk nie idzie bezposrednio do DSICarplay, tylko przez fabryczny
 * kontroler klawiszy CarPlay jako para PRESSED/RELEASED klawisza SOFTKEY_*,
 * ktory fabryczny getKeyId() mapuje na DSICarplay.BUTTON_UP/RIGHT/DOWN/LEFT
 * (7/6/8/5). Klawisze SOFTKEY_* nie sa joystickiem - nie zostawiaja stanu.
 * Java 1.4.
 */
package de.audi.app.terminalmode.keyevents;

import de.audi.app.terminalmode.k3346.K3346Diag;

final class K3346TouchpadDpad {
    private static final int DPAD_THRESHOLD_FAST = 150;
    private static final int DPAD_THRESHOLD_SLOW = 200;
    private static final int SPEED_FAST = 300;
    private static final int SPEED_SLOW = 100;
    private static final int DPAD_DEAD_ZONE = 30;

    static final int LEFT = 1;
    static final int RIGHT = 2;
    static final int UP = 3;
    static final int DOWN = 4;

    interface Sink {
        void postDpad(int direction);
    }

    private int lastX;
    private int lastY;
    private boolean down;
    private int accumDx;
    private int accumDy;
    private boolean armed;
    private long lastSampleMs;
    private int ticksLogged;

    synchronized void onTouch(int x, int y, Sink sink) {
        long now = System.currentTimeMillis();
        if (!down) {
            lastX = x;
            lastY = y;
            down = true;
            accumDx = 0;
            accumDy = 0;
            armed = false;
            lastSampleMs = now;
            return;
        }
        int dx = x - lastX;
        int dy = y - lastY;
        lastX = x;
        lastY = y;
        accumDx += dx;
        accumDy += dy;
        if (!armed) {
            int absX = accumDx < 0 ? -accumDx : accumDx;
            int absY = accumDy < 0 ? -accumDy : accumDy;
            if (absX < DPAD_DEAD_ZONE && absY < DPAD_DEAD_ZONE) {
                lastSampleMs = now;
                return;
            }
            armed = true;
        }
        int dtMs = (int)(now - lastSampleMs);
        if (dtMs <= 0) dtMs = 1;
        if (dtMs > 200) dtMs = 200;
        int stepDist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        int speed = stepDist * 100 / dtMs;
        lastSampleMs = now;
        int threshold;
        if (speed >= SPEED_FAST) {
            threshold = DPAD_THRESHOLD_FAST;
        } else if (speed <= SPEED_SLOW) {
            threshold = DPAD_THRESHOLD_SLOW;
        } else {
            int range = SPEED_FAST - SPEED_SLOW;
            int pos = speed - SPEED_SLOW;
            threshold = DPAD_THRESHOLD_FAST + (range - pos) * (DPAD_THRESHOLD_SLOW - DPAD_THRESHOLD_FAST) / range;
        }
        while (true) {
            int absX = accumDx < 0 ? -accumDx : accumDx;
            int absY = accumDy < 0 ? -accumDy : accumDy;
            if (absX < threshold && absY < threshold) break;
            if (absX >= threshold) {
                if (accumDx < 0) { emit(LEFT, sink);  accumDx += threshold; }
                else             { emit(RIGHT, sink); accumDx -= threshold; }
            }
            if (absY >= threshold) {
                if (accumDy < 0) { emit(UP, sink);    accumDy += threshold; }
                else             { emit(DOWN, sink);  accumDy -= threshold; }
            }
        }
    }

    synchronized void onTouchEnd() {
        down = false;
        accumDx = 0;
        accumDy = 0;
        armed = false;
        lastSampleMs = 0;
    }

    private void emit(int direction, Sink sink) {
        if (ticksLogged < 20) {
            ticksLogged++;
            K3346Diag.log("K3346_DPAD tick dir=" + direction);
        }
        try {
            sink.postDpad(direction);
        } catch (Throwable t) {
            K3346Diag.log("K3346_DPAD sink error " + t);
        }
    }
}
