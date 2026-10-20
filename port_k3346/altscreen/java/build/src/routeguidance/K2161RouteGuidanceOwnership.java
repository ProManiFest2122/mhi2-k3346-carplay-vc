package com.luka.carplay.routeguidance;

import com.luka.carplay.framework.Log;

import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.tghu.navi.app.Navigation;
import de.audi.tghu.navi.app.cluster.ClusterService;
import de.esolutions.fw.util.commons.job.DispatcherBase;

public final class K2161RouteGuidanceOwnership {

    private static final long DISPATCH_TIMEOUT_MS = 10000L;

    private final CombiBAPServiceNavi real;

    private K2161GatedCombiService gate;
    private ClusterService cluster;
    private DispatcherBase dispatcher;

    private boolean installed;
    private boolean carPlayRouteActive;

    public K2161RouteGuidanceOwnership(CombiBAPServiceNavi real) {
        if (real == null) {
            throw new IllegalArgumentException("real=null");
        }

        this.real = real;
    }

    private void note(String s) {
        Log.i("RGI-OWN", s);
    }

    /*
     * Install the wrapper permanently, but initially allow
     * all native route-guidance traffic.
     */
    public synchronized boolean install()
        throws Exception {

        if (installed) {
            note("INSTALL_SKIPPED already_installed=1");
            return true;
        }

        Navigation nav = Navigation.getInstance();

        if (nav == null) {
            note("INSTALL_NOT_READY navigation=null");
            return false;
        }

        ClusterService cs = nav.getClusterService();

        if (cs == null) {
            note("INSTALL_NOT_READY cluster=null");
            return false;
        }

        DispatcherBase d = nav.getDispatcher();

        if (d == null) {
            note("INSTALL_NOT_READY dispatcher=null");
            return false;
        }

        final ClusterService installCluster = cs;

        final K2161GatedCombiService installGate =
            new K2161GatedCombiService(real);

        installGate.setRouteGuidanceBlocked(false);
        installGate.setMapPresentationBlocked(false);

        runOnDispatcherAndWait(
            d,
            new Runnable() {
                public void run() {
                    installCluster.setCombiBAPService(
                        installGate
                    );
                }
            }
        );

        cluster = cs;
        dispatcher = d;
        gate = installGate;

        installed = true;
        carPlayRouteActive = false;

        note("INSTALL_RESULT=PASS route_block=0");

        return true;
    }

    /*
     * FINAL RUNTIME API.
     *
     * true:
     *   actual CarPlay route is active
     *   -> block native HUD route-guidance writes.
     *
     * false:
     *   no CarPlay route
     *   -> allow native writes and immediately refresh
     *      stock state through updateAll().
     */
    public synchronized boolean setCarPlayRouteActive(
        boolean active
    ) throws Exception {

        if (!installed) {
            if (!install()) {
                note(
                    "ROUTE_STATE_NOT_READY requested=" +
                    (active ? "1" : "0")
                );

                return false;
            }
        }

        if (gate == null ||
            cluster == null ||
            dispatcher == null) {

            throw new IllegalStateException(
                "ownership state incomplete"
            );
        }

        if (carPlayRouteActive == active) {
            note(
                "ROUTE_STATE_UNCHANGED active=" +
                (active ? "1" : "0")
            );

            return true;
        }

        final boolean requested = active;

        final K2161GatedCombiService targetGate =
            gate;

        final ClusterService targetCluster =
            cluster;

        runOnDispatcherAndWait(
            dispatcher,
            new Runnable() {
                public void run() {

                    targetGate.setRouteGuidanceBlocked(
                        requested
                    );

                    /*
                     * Native Audi MAP/FSG presentation writes are a second
                     * writer to the VC. Block them for the same CarPlay
                     * ownership interval so MAP cannot replace factory RGI
                     * or defeat the HUD-only/no-navigation policy.
                     */
                    targetGate.setMapPresentationBlocked(
                        requested
                    );

                    /*
                     * On hand-back to native navigation,
                     * call the exact K2161 setter again.
                     *
                     * This invokes:
                     *
                     * CombiBAPListener.setCombiService()
                     * -> updateAll()
                     *
                     * while the gate is OPEN.
                     */
                    if (!requested) {
                        targetCluster.setCombiBAPService(
                            targetGate
                        );
                    }
                }
            }
        );

        carPlayRouteActive = active;

        note(
            "ROUTE_STATE_RESULT=PASS active=" +
            (active ? "1" : "0")
        );

        return true;
    }

    public synchronized boolean isCarPlayRouteActive() {
        return carPlayRouteActive;
    }

    /*
     * Bundle-stop safety.
     *
     * Reopen native RG first, then put the original raw
     * service back into ClusterService.
     */
    public synchronized void restoreBestEffort() {

        if (!installed) {
            note("RESTORE_SKIPPED installed=0");
            return;
        }

        try {
            final ClusterService restoreCluster =
                cluster;

            final CombiBAPServiceNavi restoreReal =
                real;

            if (gate != null) {
                gate.setRouteGuidanceBlocked(false);
                gate.setMapPresentationBlocked(false);
            }

            if (restoreCluster != null &&
                dispatcher != null) {

                runOnDispatcherAndWait(
                    dispatcher,
                    new Runnable() {
                        public void run() {
                            restoreCluster.setCombiBAPService(
                                restoreReal
                            );
                        }
                    }
                );
            }

            note("RESTORE_RESULT=PASS");

        } catch (Throwable t) {

            note(
                "RESTORE_RESULT=FAIL class=" +
                t.getClass().getName()
            );

        } finally {

            installed = false;
            carPlayRouteActive = false;

            gate = null;
            cluster = null;
            dispatcher = null;
        }
    }

    private void runOnDispatcherAndWait(
        DispatcherBase d,
        final Runnable action
    ) throws Exception {

        if (d == null) {
            throw new IllegalArgumentException(
                "dispatcher=null"
            );
        }

        if (action == null) {
            throw new IllegalArgumentException(
                "action=null"
            );
        }

        if (d.isDispatchThread()) {
            action.run();
            return;
        }

        final Object lock = new Object();

        final boolean[] complete =
            new boolean[] { false };

        final Throwable[] failure =
            new Throwable[] { null };

        Runnable dispatched =
            new Runnable() {
                public void run() {

                    synchronized (lock) {

                        try {
                            action.run();
                        } catch (Throwable t) {
                            failure[0] = t;
                        }

                        complete[0] = true;

                        lock.notifyAll();
                    }
                }
            };

        synchronized (lock) {

            d.execute(dispatched);

            long deadline =
                System.currentTimeMillis() +
                DISPATCH_TIMEOUT_MS;

            while (!complete[0]) {

                long remaining =
                    deadline -
                    System.currentTimeMillis();

                if (remaining <= 0L) {
                    throw new Exception(
                        "Navigation dispatcher timeout"
                    );
                }

                lock.wait(remaining);
            }
        }

        if (failure[0] != null) {

            if (failure[0] instanceof Exception) {
                throw (Exception)failure[0];
            }

            if (failure[0] instanceof Error) {
                throw (Error)failure[0];
            }

            throw new Exception(
                String.valueOf(failure[0])
            );
        }
    }
}
