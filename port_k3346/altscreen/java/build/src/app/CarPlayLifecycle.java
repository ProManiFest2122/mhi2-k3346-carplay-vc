package org.mib2high.carplay.rgi;

import java.lang.reflect.Field;

import org.osgi.framework.BundleContext;
import org.osgi.framework.ServiceReference;
import org.osgi.util.tracker.ServiceTracker;
import org.osgi.util.tracker.ServiceTrackerCustomizer;

import com.luka.carplay.framework.Log;
import com.luka.carplay.routeguidance.RouteGuidance;

import de.audi.app.terminalmode.device.IActiveDeviceStateListener;
import de.audi.app.terminalmode.device.IDeviceManager;
import de.audi.app.terminalmode.device.TMDevice;

/**
 * K2161 TerminalMode lifecycle adapter.
 *
 * Exact K2161 chain:
 *   ITerminalModeUpdateService
 *     -> implementation object de.audi.app.terminalmode.TMInterAppHandler
 *     -> private IDeviceManager deviceManager
 *     -> IActiveDeviceStateListener
 *
 * The private field is accessed reflectively because exact K2161 exposes no
 * public getter. Every identity/type check fails closed.
 *
 * Lifecycle semantics intentionally match Luka's established CarPlay behavior:
 *   CarPlay + active + selected -> RouteGuidance.start()
 *   non-CarPlay active-device callback -> RouteGuidance.stop()
 *   CarPlay temporarily inactive/deselected -> keep RouteGuidance alive
 */
public final class CarPlayLifecycle
        implements ServiceTrackerCustomizer, IActiveDeviceStateListener {

    private static final String TAG = "K2161TMLifecycle";
    private static final String SERVICE_NAME =
        "de.audi.atip.interapp.terminalmode.ITerminalModeUpdateService";
    private static final String IMPL_NAME =
        "de.audi.app.terminalmode.TMInterAppHandler";
    private static final String DEVICE_MANAGER_FIELD = "deviceManager";
    private static final String DEVICE_MANAGER_TYPE =
        "de.audi.app.terminalmode.device.IDeviceManager";

    private final BundleContext context;
    private ServiceTracker tracker;
    private ServiceReference heldReference;
    private Object heldService;
    private IDeviceManager deviceManager;
    private RouteGuidance routeGuidance;
    private boolean closed;

    public CarPlayLifecycle(BundleContext context) {
        if (context == null) throw new IllegalArgumentException("context=null");
        this.context = context;
    }

    public synchronized void open() {
        if (closed) throw new IllegalStateException("lifecycle already closed");
        if (tracker != null) return;

        tracker = new ServiceTracker(context, SERVICE_NAME, this);
        tracker.open();
        Log.i(TAG, "TRACKER_OPEN service=" + SERVICE_NAME);
    }

    public synchronized void close() {
        if (closed) return;
        closed = true;

        ServiceTracker t = tracker;
        tracker = null;

        if (t != null) {
            try {
                t.close();
            } catch (Throwable e) {
                Log.e(TAG, "TRACKER_CLOSE_FAILED", e);
            }
        }

        releaseService("close");
        routeGuidance = null;
        Log.i(TAG, "CLOSED");
    }

    public synchronized void setRouteGuidance(RouteGuidance rg) {
        routeGuidance = rg;
        if (rg == null) {
            Log.i(TAG, "ROUTE_GUIDANCE_REF cleared");
            return;
        }

        Log.i(TAG, "ROUTE_GUIDANCE_REF set");
        evaluateCurrentDevice("route_guidance_set");
    }

    public synchronized Object addingService(ServiceReference ref) {
        if (closed || ref == null) return null;
        if (heldService != null) return null;

        Object obj = null;
        try {
            obj = context.getService(ref);
            if (obj == null) {
                Log.e(TAG, "TM_SERVICE_NULL");
                return null;
            }

            String impl = obj.getClass().getName();
            if (!IMPL_NAME.equals(impl)) {
                Log.e(TAG, "TM_SERVICE_IMPL_MISMATCH class=" + impl);
                try { context.ungetService(ref); } catch (Throwable ignored) {}
                return null;
            }

            Field f = obj.getClass().getDeclaredField(DEVICE_MANAGER_FIELD);
            String fieldType = f.getType().getName();
            if (!DEVICE_MANAGER_TYPE.equals(fieldType)) {
                Log.e(TAG, "DEVICE_MANAGER_FIELD_TYPE_MISMATCH type=" + fieldType);
                try { context.ungetService(ref); } catch (Throwable ignored) {}
                return null;
            }

            f.setAccessible(true);
            Object dmObj = f.get(obj);
            if (!(dmObj instanceof IDeviceManager)) {
                Log.e(TAG, "DEVICE_MANAGER_INSTANCE_MISMATCH class=" +
                    (dmObj == null ? "null" : dmObj.getClass().getName()));
                try { context.ungetService(ref); } catch (Throwable ignored) {}
                return null;
            }

            IDeviceManager dm = (IDeviceManager)dmObj;
            dm.addActiveDeviceListener(this);

            heldReference = ref;
            heldService = obj;
            deviceManager = dm;

            Log.i(TAG, "TM_LIFECYCLE_ATTACH=PASS impl=" + impl);
            evaluateCurrentDevice("service_attach");
            return obj;

        } catch (Throwable e) {
            Log.e(TAG, "TM_LIFECYCLE_ATTACH=FAIL", e);
            if (obj != null) {
                try { context.ungetService(ref); } catch (Throwable ignored) {}
            }
            return null;
        }
    }

    public synchronized void modifiedService(ServiceReference ref, Object service) {
        // No action required. Active-device state arrives through IActiveDeviceStateListener.
    }

    public synchronized void removedService(ServiceReference ref, Object service) {
        if (sameReference(heldReference, ref)) {
            releaseService("service_removed");
        }
    }

    public synchronized void updateActiveDeviceState(TMDevice device) {
        handleDevice(device, "callback");
    }

    private void evaluateCurrentDevice(String source) {
        IDeviceManager dm = deviceManager;
        if (dm == null) {
            Log.i(TAG, "DEVICE_EVAL source=" + source + " result=no_device_manager");
            return;
        }

        try {
            TMDevice device = dm.getActiveDevice();
            if (device == null) {
                Log.i(TAG, "DEVICE_EVAL source=" + source + " result=null_active_device");
                return;
            }
            handleDevice(device, source);
        } catch (Throwable e) {
            Log.e(TAG, "DEVICE_EVAL_FAILED source=" + source, e);
        }
    }

    private void handleDevice(TMDevice device, String source) {
        if (device == null) {
            Log.i(TAG, "DEVICE_STATE source=" + source + " device=null");
            return;
        }

        try {
            boolean carplay = device.isCarplayDevice();
            boolean active = device.isActive();
            boolean selected = device.isSelected();

            Log.i(TAG, "DEVICE_STATE source=" + source +
                " carplay=" + carplay +
                " active=" + active +
                " selected=" + selected);

            if (carplay && active && selected) {
                startGuidance("carplay_active_selected");
            } else if (!carplay) {
                stopGuidance("active_device_not_carplay");
            } else {
                // CarPlay is still the device but temporarily inactive/deselected.
                // Preserve guidance, matching the established Luka lifecycle behavior.
                Log.i(TAG, "DEVICE_STATE transient_carplay_inactive_keep_guidance");
            }

        } catch (Throwable e) {
            Log.e(TAG, "DEVICE_STATE_FAILED source=" + source, e);
        }
    }

    private void startGuidance(String reason) {
        RouteGuidance rg = routeGuidance;
        if (rg == null) {
            Log.i(TAG, "RG_START_SKIP reason=" + reason + " no_route_guidance");
            return;
        }

        try {
            if (!rg.isRunning()) {
                rg.start();
                Log.i(TAG, "RG_START reason=" + reason);
            }
        } catch (Throwable e) {
            Log.e(TAG, "RG_START_FAILED reason=" + reason, e);
        }
    }

    private void stopGuidance(String reason) {
        RouteGuidance rg = routeGuidance;
        if (rg == null) {
            Log.i(TAG, "RG_STOP_SKIP reason=" + reason + " no_route_guidance");
            return;
        }

        try {
            if (rg.isRunning()) {
                rg.stop();
                Log.i(TAG, "RG_STOP reason=" + reason);
            }
        } catch (Throwable e) {
            Log.e(TAG, "RG_STOP_FAILED reason=" + reason, e);
        }
    }

    private void releaseService(String reason) {
        IDeviceManager dm = deviceManager;
        deviceManager = null;

        if (dm != null) {
            try {
                dm.removeActiveDeviceListener(this);
                Log.i(TAG, "TM_LISTENER_REMOVED reason=" + reason);
            } catch (Throwable e) {
                Log.e(TAG, "TM_LISTENER_REMOVE_FAILED reason=" + reason, e);
            }
        }

        ServiceReference ref = heldReference;
        heldReference = null;
        heldService = null;

        if (ref != null) {
            try {
                context.ungetService(ref);
            } catch (Throwable e) {
                Log.e(TAG, "TM_SERVICE_UNGET_FAILED reason=" + reason, e);
            }
        }

        // A TerminalMode service removal is a genuine lifecycle teardown boundary.
        stopGuidance("terminalmode_" + reason);
    }

    private static boolean sameReference(ServiceReference a, ServiceReference b) {
        return a == b || (a != null && a.equals(b));
    }
}