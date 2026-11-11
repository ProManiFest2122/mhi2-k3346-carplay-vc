package com.luka.carplay.framework;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;

/* The Java end of the bus on a real socket.  Frames waiting while the handler is busy,
 * which is what a route start looks like: 29 frames in 0.55 s, 150 ms each (car run-70). */
public final class CarplayBusTest {

    private static int ok;
    private static int fail;

    private static final Object LOCK = new Object();
    private static final List GOT = new ArrayList();
    private static boolean holdFirst;
    private static boolean holding;
    private static boolean released;

    private static final CarplayBus.Listener LISTENER = new CarplayBus.Listener() {
        public void onFrame(int type, int flags, byte[] payload, int len) {
            synchronized (LOCK) {
                GOT.add(tag(type, new String(payload, 0, len)));
                LOCK.notifyAll();
                if (holdFirst) {
                    holdFirst = false;
                    holding = true;
                    LOCK.notifyAll();
                    while (!released) {
                        try {
                            LOCK.wait();
                        } catch (InterruptedException e) {
                            return;
                        }
                    }
                }
            }
        }
    };

    private static String tag(int type, String text) {
        if (type != CarplayBus.EVT_RGD_UPDATE) return "C";
        int at = text.indexOf("diag_rx_seq:n:");
        if (at >= 0) {
            int from = at + "diag_rx_seq:n:".length();
            return "S" + text.substring(from, text.indexOf('\n', from));
        }
        return text.indexOf("disconnect_reason") >= 0 ? "D" : "E";
    }

    /* The real text, rgd_hook.c: write_bus_snapshot_from_cache and the two short frames. */
    private static byte[] snap(int n) throws Exception {
        return frame(CarplayBus.EVT_RGD_UPDATE, "@routeguidance\ndiag_session:n:1\ndiag_rx_seq:n:" + n
            + "\nroute_state:n:1\nmaneuver_count:n:12\nmaneuver_list:s:0,1\n");
    }

    private static byte[] clear() throws Exception {
        return frame(CarplayBus.EVT_RGD_UPDATE,
            "@routeguidance\nroute_state:n:0\nmaneuver_count:n:0\ndisconnect_reason:s:session\n");
    }

    private static byte[] end() throws Exception {
        return frame(CarplayBus.EVT_RGD_UPDATE, "@routeguidance\nroute_state:n:0\nmaneuver_count:n:0\n");
    }

    private static byte[] cover() throws Exception {
        return frame(CarplayBus.EVT_COVERART, "@coverart\ndiag_rx_seq:n:1\n");
    }

    private static byte[] frame(int type, String text) throws Exception {
        byte[] p = text.getBytes("UTF-8");
        ByteArrayOutputStream b = new ByteArrayOutputStream();
        DataOutputStream d = new DataOutputStream(b);
        d.writeInt(CarplayBus.MAGIC);
        d.writeInt(1);
        d.writeShort(type);
        d.writeByte(0);
        d.writeByte(0);
        d.writeInt(p.length);
        d.write(p);
        return b.toByteArray();
    }

    private static byte[] join(byte[][] parts) throws Exception {
        ByteArrayOutputStream b = new ByteArrayOutputStream();
        for (int i = 0; i < parts.length; i++) b.write(parts[i]);
        return b.toByteArray();
    }

    private static void waitForCount(int count) throws Exception {
        long until = System.currentTimeMillis() + 3000;
        synchronized (LOCK) {
            while (GOT.size() < count && System.currentTimeMillis() < until) LOCK.wait(100);
        }
        Thread.sleep(150);   /* time for anything that should not be there */
    }

    private static void reset(boolean hold) {
        synchronized (LOCK) {
            GOT.clear();
            holdFirst = hold;
            holding = false;
            released = false;
        }
    }

    private static void check(String name, String want) {
        String got;
        synchronized (LOCK) {
            got = GOT.toString();
        }
        if (want.equals(got)) {
            ok++;
            System.out.println("  OK   " + name + " " + got);
        } else {
            fail++;
            System.out.println("  FAIL " + name + " oczekiwano=" + want + " jest=" + got);
        }
    }

    /* first frame, the handler stays in it, the rest arrive, the handler comes back. */
    private static void burst(String name, byte[] first, byte[] rest, int count, String want)
            throws Exception {
        reset(true);
        Socket s = new Socket(CarplayBus.HOST, CarplayBus.PORT);
        OutputStream out = s.getOutputStream();
        out.write(first);
        out.flush();
        long until = System.currentTimeMillis() + 3000;
        synchronized (LOCK) {
            while (!holding && System.currentTimeMillis() < until) LOCK.wait(100);
        }
        out.write(rest);
        out.flush();
        Thread.sleep(200);   /* all of it in the socket */
        synchronized (LOCK) {
            released = true;
            LOCK.notifyAll();
        }
        waitForCount(count);
        s.close();
        Thread.sleep(100);
        check(name, want);
    }

    public static void main(String[] argv) throws Exception {
        CarplayBus bus = CarplayBus.getInstance();
        bus.on(CarplayBus.EVT_RGD_UPDATE, LISTENER);
        bus.on(CarplayBus.EVT_COVERART, LISTENER);
        bus.start();
        Thread.sleep(300);

        /* run-70: the whole burst is one update */
        byte[][] many = new byte[49][];
        for (int i = 0; i < many.length; i++) many[i] = snap(i + 2);
        burst("seria zwija sie do najnowszej", snap(1), join(many), 2, "[S1, S50]");

        /* the disconnect resets Java, so it counts where it is */
        burst("rozlaczenie zostaje na miejscu", snap(1),
            join(new byte[][] {snap(2), clear(), snap(3), snap(4)}), 4, "[S1, S2, D, S4]");

        /* a short frame has no whole state in it */
        burst("koniec trasy zostaje", snap(1),
            join(new byte[][] {snap(2), snap(3), end(), snap(4), snap(5)}), 4, "[S1, S3, E, S5]");
        burst("koniec trasy na koncu", snap(1), join(new byte[][] {snap(2), end()}), 3, "[S1, S2, E]");
        burst("krotkie obok siebie", snap(1), join(new byte[][] {clear(), clear(), end()}), 4,
            "[S1, D, D, E]");

        /* other types are not ours to drop, and do not get merged across */
        burst("inny typ dzieli serie", snap(1), join(new byte[][] {snap(2), cover(), snap(3)}), 4,
            "[S1, S2, C, S3]");
        burst("inny typ nie jest pomijany", snap(1), join(new byte[][] {cover(), cover()}), 3,
            "[S1, C, C]");

        /* the stream dies behind a good frame: the frame still counts, what follows does not */
        ByteArrayOutputStream junk = new ByteArrayOutputStream();
        junk.write(snap(2));
        junk.write(new byte[16]);
        junk.write(snap(3));
        burst("zly naglowek po dobrej ramce", snap(1), junk.toByteArray(), 2, "[S1, S2]");

        /* half a frame in the socket: wait for the rest of it */
        reset(true);
        Socket s = new Socket(CarplayBus.HOST, CarplayBus.PORT);
        OutputStream out = s.getOutputStream();
        out.write(snap(1));
        out.flush();
        synchronized (LOCK) {
            while (!holding) LOCK.wait(100);
        }
        byte[] three = snap(3);
        out.write(snap(2));
        out.write(three, 0, 20);
        out.flush();
        Thread.sleep(200);
        synchronized (LOCK) {
            released = true;
            LOCK.notifyAll();
        }
        Thread.sleep(100);
        out.write(three, 20, three.length - 20);
        out.flush();
        waitForCount(2);
        s.close();
        Thread.sleep(100);
        check("pol ramki w gniezdzie", "[S1, S3]");

        /* frames one by one: nothing is held back and nothing lost */
        reset(false);
        s = new Socket(CarplayBus.HOST, CarplayBus.PORT);
        out = s.getOutputStream();
        for (int i = 1; i <= 3; i++) {
            out.write(snap(i));
            out.flush();
            waitForCount(i);
        }
        s.close();
        Thread.sleep(100);
        check("pojedyncze ramki od razu", "[S1, S2, S3]");

        /* the next connection starts clean */
        reset(false);
        s = new Socket(CarplayBus.HOST, CarplayBus.PORT);
        out = s.getOutputStream();
        out.write(snap(7));
        out.flush();
        waitForCount(1);
        s.close();
        check("nastepne polaczenie", "[S7]");

        System.out.println("WYNIK: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }
}
