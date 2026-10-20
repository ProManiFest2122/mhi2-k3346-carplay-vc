package org.mib2high.carplay.rgi;

import java.io.File;
import java.io.FileWriter;

import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;
import org.osgi.framework.ServiceReference;
import org.osgi.util.tracker.ServiceTracker;
import org.osgi.util.tracker.ServiceTrackerCustomizer;

import com.luka.carplay.framework.Log;
import com.luka.carplay.routeguidance.RouteGuidance;

import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;

/**
 * OSGi entry point for CarPlay route-guidance integration.
 * Real RGI events drive RouteGuidance ownership; no synthetic maneuvers are generated.
 */
public final class Activator implements BundleActivator, ServiceTrackerCustomizer {
    private static final String TAG = "CarPlayRGI";
    private static final String SERVICE_NAME =
        "de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi";
    private static final String LOG_PATH =
        "/net/rcc/dev/shmem/mib2high-carplay-rgi.log";
    private static final String READY_PATH =
        "/net/rcc/dev/shmem/mib2high-carplay-rgi.ok";

    private BundleContext context;
    private ServiceTracker tracker;
    private ServiceReference heldReference;
    private Object heldService;
    private RouteGuidance routeGuidance;
    private CarPlayLifecycle carPlayLifecycle;
    private boolean stopping;

    public synchronized void start(BundleContext ctx) throws Exception {
        if (ctx == null) throw new IllegalArgumentException("context=null");

        Log.setPath(LOG_PATH);
        Log.setLevel(Log.LEVEL_DEBUG);
        Log.i(TAG, "BUNDLE_START");

        context = ctx;

        carPlayLifecycle = new CarPlayLifecycle(ctx);
        carPlayLifecycle.open();
        stopping = false;
        tracker = new ServiceTracker(ctx, SERVICE_NAME, this);
        tracker.open();
        Log.i(TAG, "TRACKER_OPEN service=" + SERVICE_NAME);
    }

    public synchronized void stop(BundleContext ctx) throws Exception {
        Log.i(TAG, "BUNDLE_STOP_BEGIN");
        stopping = true;

        ServiceTracker closing = tracker;
        tracker = null;
        if (closing != null) {
            try {
                closing.close();
            } catch (Throwable t) {
                Log.e(TAG, "TRACKER_CLOSE_FAILED", t);
            }
        }

        if (carPlayLifecycle != null) {
            try {
                carPlayLifecycle.close();
            } catch (Throwable t) {
                Log.e(TAG, "CARPLAY_LIFECYCLE_CLOSE_FAILED", t);
            }
            carPlayLifecycle = null;
        }

        release("bundle_stop");
        context = null;
        Log.i(TAG, "BUNDLE_STOP_COMPLETE");
    }

    public synchronized Object addingService(ServiceReference ref) {
        if (stopping || context == null || ref == null) return null;
        if (heldService != null) {
            Log.w(TAG, "SERVICE_ADD_IGNORED already_held=1");
            return null;
        }

        Object service = null;
        try {
            service = context.getService(ref);
            if (!(service instanceof CombiBAPServiceNavi)) {
                Log.w(TAG, "SERVICE_REJECTED class=" +
                    (service == null ? "null" : service.getClass().getName()));
                if (service != null) context.ungetService(ref);
                return null;
            }

            RouteGuidance rg = new RouteGuidance();
            if (!rg.init(service)) {
                context.ungetService(ref);
                Log.e(TAG, "ROUTE_GUIDANCE_INIT_FAILED");
                return null;
            }

            /* RouteGuidance start/stop is owned by exact K2161 TerminalMode lifecycle. */
            heldReference = ref;
            heldService = service;
            routeGuidance = rg;
            if (carPlayLifecycle != null) {
                carPlayLifecycle.setRouteGuidance(rg);
            }
            writeReadyMarker(service.getClass().getName());
            Log.i(TAG, "SERVICE_ACQUIRED class=" + service.getClass().getName());
            return service;
        } catch (Throwable t) {
            Log.e(TAG, "SERVICE_ACQUIRE_FAILED", t);
            if (service != null) {
                try { context.ungetService(ref); } catch (Throwable ignored) {}
            }
            return null;
        }
    }

    public synchronized void modifiedService(ServiceReference ref, Object service) {
        if (sameReference(heldReference, ref)) {
            Log.i(TAG, "SERVICE_MODIFIED held=1");
        }
    }

    public synchronized void removedService(ServiceReference ref, Object service) {
        if (sameReference(heldReference, ref)) {
            release("service_removed");
        }
    }

    private synchronized void release(String reason) {
        Log.i(TAG, "RELEASE_BEGIN reason=" + reason);

        if (routeGuidance != null) {
            try {
                routeGuidance.stop();
            } catch (Throwable t) {
                Log.e(TAG, "ROUTE_GUIDANCE_STOP_FAILED", t);
            }
            routeGuidance = null;
            if (carPlayLifecycle != null) {
                carPlayLifecycle.setRouteGuidance(null);
            }
        }

        if (heldReference != null && context != null) {
            try {
                context.ungetService(heldReference);
            } catch (Throwable t) {
                Log.e(TAG, "SERVICE_UNGET_FAILED", t);
            }
        }

        heldReference = null;
        heldService = null;

        try {
            File marker = new File(READY_PATH);
            if (marker.exists() && !marker.delete()) {
                Log.w(TAG, "READY_MARKER_REMOVE_FAILED");
            }
        } catch (Throwable t) {
            Log.e(TAG, "READY_MARKER_REMOVE_EXCEPTION", t);
        }

        Log.i(TAG, "RELEASE_COMPLETE reason=" + reason);
    }

    private void writeReadyMarker(String implementationClass) {
        FileWriter out = null;
        try {
            out = new FileWriter(READY_PATH, false);
            out.write("MIB2HIGH_CARPLAY_RGI_READY\n");
            out.write("mode=production\n");
            out.write("service=" + implementationClass + "\n");
            out.flush();
        } catch (Throwable t) {
            Log.e(TAG, "READY_MARKER_WRITE_FAILED", t);
        } finally {
            if (out != null) {
                try { out.close(); } catch (Throwable ignored) {}
            }
        }
    }

    private static boolean sameReference(ServiceReference a, ServiceReference b) {
        return a == b || (a != null && a.equals(b));
    }
}
