/*
 * CarPlay Route Guidance - BAP Bridge
 *
 * Translates real CarPlay route-guidance state to the exact K2161 BAP API.
 * Scope: factory HUD plus the cluster's shared/simple VC route-guidance presentation.
 * No custom Virtual Cockpit graphics renderer is used by this bridge.
 *
 */
package com.luka.carplay.routeguidance;

import com.luka.carplay.framework.Log;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationInfo;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviDestination;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;
import de.audi.atip.log.LogChannel;
import de.audi.atip.metrics.DateMetric;
import de.audi.atip.metrics.Distance;
import de.audi.tghu.navi.app.Navigation;
import de.audi.tghu.navi.app.cluster.BAPDistanceFormatter;
import de.audi.tghu.navi.app.cluster.ClusterService;
import de.audi.tghu.navi.app.command.DSIResponseContainer;
import java.lang.reflect.Field;
import de.audi.atip.interapp.combi.ddp2.CombiService;
import java.lang.reflect.Method;

public class BAPBridge {

    private static final String TAG = "BAPBridge";
    /* RGType sent to cluster: 0=RGI (BAP ManeuverDescriptor icons for HUD).
     * FPK has rgType=4 hardcoded in CombiBAPListener -- the BAP rgType=0 is for the
     * AppConnectorNavi FSG sync flow, not for view mode selection. */
    private static final int ACTIVE_RGTYPE = 0;  /* RGI -- native BAP HUD icons. */
    private static final boolean BAP_TRACE_ENABLED = true;

    /*
     * K2161 shares the maneuver descriptor/distance BAP transaction between HUD and
     * the simple VC/FPK route-guidance presentation.  This compile-time switch controls
     * whether we leave that VC presentation available or apply the existing best-effort
     * map-presentation suppression.  It intentionally does not gate the shared maneuver
     * descriptor/distance writes because the HUD requires those same writes.
     */
    /*
     * Virtual Cockpit policy, read once from VC_POLICY_FILE (K3346 1.0.1):
     *
     * rgi (default) = factory Audi RGI presentation in the VC: rgActive plus
     *                 favoredViewMode=RGI, reasserted while CarPlay guides.
     * map           = rgActive as in rgi, but favoredViewMode is left alone.
     *                 Issue #5: on VC H11/0379 favored=RGI turns the CarPlay
     *                 map black for the whole route; VC H15/0299 ignores it.
     * off           = HUD-only CarPlay guidance (VC_RGI_ENABLED=false below).
     *
     * A missing file or an unknown value means rgi, so units without the file
     * behave exactly as 1.0.0.
     */
    private static final String VC_POLICY_FILE = "/mnt/app/root/mib2high-vc.conf";
    private static String VC_POLICY_SOURCE = "default";
    private static final String VC_POLICY = loadVcPolicy();

    /*
     * true  = factory Audi RGI presentation in the VC.
     * false = HUD-only CarPlay guidance; VC is held in the factory
     *         non-navigation/COMPASS presentation.
     */
    private static boolean VC_RGI_ENABLED = !"off".equals(VC_POLICY);

    /* Only the rgi policy selects and holds favoredViewMode=RGI. */
    private static final boolean VC_SET_FAVORED = "rgi".equals(VC_POLICY);

    private static final int VC_VIEW_RGI = 1;

    /* Allow small rounding differences before treating the top-level distance as belonging
     * to a different (typically START_ROUTE) presentation state. */
    private static final int PRESENTATION_DISTANCE_TOLERANCE_M = 100;

    /* BAP supports three maneuver slots, but publish only the current one. */
    private static final int MAX_BAP_MANEUVERS = 1;

    /* Presentation thresholds used only for maneuver state / street text.
     * The real maneuver icon and numeric distance are published at all ranges. */
    private static final int CITY_PREPARE_THRESHOLD_M = 1500;
    private static final int HIGHWAY_PREPARE_THRESHOLD_M = 3000;
    private static final int HIGHWAY_STEP_THRESHOLD_M = 2000;
    private static final int ACTION_PERCENT_OF_PREPARE = 15;

    private CombiBAPServiceNavi appConnectorNavi;
    private final BAPDistanceFormatter distanceFormatter =
        new BAPDistanceFormatter(new SilentLogChannel());

    private boolean initialized = false;

    /* Approach state affects only maneuver-state emphasis and turn-to text. */
    private boolean inApproachZone = false;
    private int lastFirstManeuverIdx = -1;
    private int lastFirstManeuverVer = -1;
    private String latchedTurnToText = "";
    private final Object distanceToManeuverLock = new Object();
    private boolean hasLastDistM = false;
    private int lastDistM = 0;
    private K2161RouteGuidanceOwnership ownership;

    private ClusterService csRef;

    /*
     * Temporary VC presentation ownership.
     *
     * K2161's ClusterInputListener rejects RGI as a directly user-selectable
     * FPK mode, while ClusterViewMode itself supports the automatic RGI mode.
     * Save the existing favored state so CarPlay can temporarily override it
     * and restore it exactly when guidance is released.
     */
    private boolean vcPresentationOverrideActive = false;
    private int vcSavedFavoredViewMode = 0;
    private boolean vcSavedFavoredViewModeReceived = false;
    private boolean vcFactoryRgStatePropagated = false;

    /*
     * Used only by VC_RGI_ENABLED=false.
     * DDP2 exposes HUD content separately from RG state.
     */
    private boolean hudDisplayContentForced = false;

    private Field vcFavoredViewModeField;
    private Field vcFavoredViewModeReceivedField;

    private static final class SilentLogChannel extends LogChannel {
        public void log(int level, String pattern,
                        Object a, Object b, Object c, Object d,
                        long l1, long l2, long l3, int flags, Throwable t) {
            /* no-op */
        }
        public void log(int level, int messageId,
                        Object a, Object b, Object c, Object d,
                        long l1, long l2, long l3, int flags, Throwable t) {
            /* no-op */
        }
    }

    private static final class FormattedDistance {
        final int value;
        final int unit;

        FormattedDistance(int value, int unit) {
            this.value = value;
            this.unit = unit;
        }
    }

    /* One authoritative maneuver selection for descriptor, distance and presentation state. */
    private static final class PresentationManeuver {
        final int index;
        final int type;
        final int distanceM;
        final int liveDistanceM;
        final int slotDistanceM;
        final boolean skippedStartRoute;
        final boolean usedSlotDistance;

        PresentationManeuver(int index, int type, int distanceM,
                             int liveDistanceM, int slotDistanceM,
                             boolean skippedStartRoute, boolean usedSlotDistance) {
            this.index = index;
            this.type = type;
            this.distanceM = distanceM;
            this.liveDistanceM = liveDistanceM;
            this.slotDistanceM = slotDistanceM;
            this.skippedStartRoute = skippedStartRoute;
            this.usedSlotDistance = usedSlotDistance;
        }
    }

    /**
     * Send distance to maneuver through AppConnectorNavi using native formatter rules.
     */
    private void sendDistanceToManeuverRaw(int meters, boolean bargraphOn, int bargraph) throws Exception {
        synchronized (distanceToManeuverLock) {
            if (meters > 0) {
                hasLastDistM = true;
                lastDistM = meters;
            } else {
                hasLastDistM = false;
                lastDistM = 0;
            }
        }

        /* Keep the BAP distance bargraph disabled for direct RGI output. */
        FormattedDistance fd = formatDistanceToTurn(meters);
        traceBap("updateDistanceToNextManeuver",
            fd.value + "," + fd.unit + ",false,0");
        V8MFactoryTrace.distance("before", meters, fd.value, fd.unit, csRef);
        appConnectorNavi.updateDistanceToNextManeuver(fd.value, fd.unit, false, 0);
        V8MFactoryTrace.distance("after_returned", meters, fd.value, fd.unit, csRef);
    }

    private void sendDistanceToDestinationRaw(int meters, boolean isStopOver) {
        FormattedDistance fd = formatDistanceToDestination(meters);
        traceBap("updateDistanceToDestination", fd.value + "," + fd.unit + "," + isStopOver);
        appConnectorNavi.updateDistanceToDestination(fd.value, fd.unit, isStopOver);
    }

    private FormattedDistance formatDistanceToTurn(int meters) {
        if (meters <= 0) return new FormattedDistance(-1, 0);
        try {
            boolean metric = isMetricDistanceUnits();
            /* K2161 metric unit 0 is 0.1 m.  The stock turn formatter clamps
             * short values; preserve the actual CarPlay distance below 1 km. */
            if (metric && meters < 1000) {
                return new FormattedDistance(meters * 10, 0);
            }
            /* BAPDistanceFormatter$BAPDistance is public, but the outer class .class file
             * lacks the InnerClasses attribute (decompiler artifact), so javac can't
             * resolve BAPDistanceFormatter.BAPDistance as a type.  Use Object + getValue/getUnit. */
            Object d = distanceFormatter.formatDistanceToTurn(meters, metric);
            int v = ((Integer) d.getClass().getMethod("getValue", new Class[0]).invoke(d, new Object[0])).intValue();
            int u = ((Integer) d.getClass().getMethod("getUnit", new Class[0]).invoke(d, new Object[0])).intValue();
            return new FormattedDistance(v, u);
        } catch (Throwable t) {
            Log.w(TAG, "formatDistanceToTurn failed, using invalid distance: " + t.getMessage());
            return new FormattedDistance(-1, 0);
        }
    }

    private FormattedDistance formatDistanceToDestination(int meters) {
        if (meters <= 0) return new FormattedDistance(-1, 0);
        try {
            boolean metric = isMetricDistanceUnits();
            Object d = distanceFormatter.formatDistanceToDestination(meters, metric);
            int v = ((Integer) d.getClass().getMethod("getValue", new Class[0]).invoke(d, new Object[0])).intValue();
            int u = ((Integer) d.getClass().getMethod("getUnit", new Class[0]).invoke(d, new Object[0])).intValue();
            return new FormattedDistance(v, u);
        } catch (Throwable t) {
            Log.w(TAG, "formatDistanceToDestination failed, using invalid distance: " + t.getMessage());
            return new FormattedDistance(-1, 0);
        }
    }

    private static boolean isMetricDistanceUnits() {
        try {
            int unit = Distance.getSystemUnit();
            return unit == Distance.KM || unit == Distance.METERS || unit == Distance.NONE;
        } catch (Throwable t) {
            return true;
        }
    }

    private void traceBap(String call, String args) {
        if (!BAP_TRACE_ENABLED) return;
        Log.d(TAG, "[BAP] " + call + "(" + args + ")");
    }

    /* Reads "vc_policy=<rgi|map|off>" from VC_POLICY_FILE; '#' starts a comment.
     * Never throws: any problem falls back to rgi, the 1.0.0 behaviour. */
    private static String loadVcPolicy() {
        /* FileInputStream, not FileReader: it is the file API already proven
         * on this J9 (K3346GatedPhoneService, K3346CoverArt). */
        java.io.FileInputStream in = null;
        try {
            in = new java.io.FileInputStream(VC_POLICY_FILE);
            byte[] buf = new byte[512];
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
                if (!"vc_policy".equals(line.substring(0, eq).trim())) continue;
                String v = line.substring(eq + 1).trim().toLowerCase();
                if ("rgi".equals(v) || "map".equals(v) || "off".equals(v)) {
                    VC_POLICY_SOURCE = "file";
                    return v;
                }
                VC_POLICY_SOURCE = "file-invalid";
                return "rgi";
            }
            VC_POLICY_SOURCE = "file-no-key";
        } catch (Throwable t) {
            VC_POLICY_SOURCE = "default";
        } finally {
            if (in != null) {
                try { in.close(); } catch (Throwable ignored) { }
            }
        }
        return "rgi";
    }

    /** Capture the factory VC preference before CarPlay changes it. */
    private void captureVcPresentationState() throws Exception {
        if (vcPresentationOverrideActive) return;

        if (csRef == null || csRef.getClusterViewMode() == null) {
            throw new Exception("ClusterViewMode unavailable");
        }

        Object cvm = csRef.getClusterViewMode();
        Class cls = cvm.getClass();

        vcFavoredViewModeField =
            cls.getDeclaredField("favoredViewMode");
        vcFavoredViewModeReceivedField =
            cls.getDeclaredField("favoredViewModeReceived");

        vcFavoredViewModeField.setAccessible(true);
        vcFavoredViewModeReceivedField.setAccessible(true);

        vcSavedFavoredViewMode =
            vcFavoredViewModeField.getInt(cvm);
        vcSavedFavoredViewModeReceived =
            vcFavoredViewModeReceivedField.getBoolean(cvm);

        vcPresentationOverrideActive = true;

        Log.i(TAG,
            "VC state captured: favored=" + vcSavedFavoredViewMode
            + " received=" + vcSavedFavoredViewModeReceived);
    }

    /**
     * Select a factory K2161 ClusterViewMode directly.
     *
     * Do not route this through ClusterInputListener: on FPK that user-input
     * path validates RGI away. ClusterViewMode.setFavoredViewMode() is the
     * underlying OEM state-machine input and immediately refreshes the view.
     */
    private void setVcFavoredViewMode(int mode, String reason)
        throws Exception {
        V8KVcTrace.record("VC_SET_FAVORED_BEGIN mode=" + mode + " reason=" + reason);

        if (csRef == null || csRef.getClusterViewMode() == null) {
            throw new Exception("ClusterViewMode unavailable");
        }

        csRef.getClusterViewMode().setFavoredViewMode(mode);
        V8MFactoryTrace.snapshot("after_favored_view_mode", csRef);
        V8KVcTrace.record("VC_SET_FAVORED_RETURNED_OK mode=" + mode);

        Log.i(TAG,
            "VC favored mode=" + mode + " reason=" + reason);
    }

    /**
     * Prepare the VC before the forged HUD/RGI acceptance state is enabled.
     *
     * In HUD-only mode we move to COMPASS first so enabling the shared HUD
     * precondition cannot momentarily select MAP/RGI.
     */
    /**
     * Request HUD navigation content independently through the
     * factory DDP2 CombiService.
     *
     * Used only when VC RGI is disabled.
     */
    private boolean setHudDisplayContent(boolean enabled) {
        try {
            if (csRef == null
                    || csRef.getClusterViewMode() == null) {
                return false;
            }

            Object cvm = csRef.getClusterViewMode();

            Field f =
                cvm.getClass().getDeclaredField("combiService");

            f.setAccessible(true);

            CombiService service =
                (CombiService) f.get(cvm);

            if (service == null) {
                Log.w(TAG,
                    "DDP2 CombiService unavailable");
                return false;
            }

            service.updateHUDDisplayContent(enabled);
            hudDisplayContentForced = enabled;

            Log.i(TAG,
                "DDP2 HUD display content=" + enabled);

            return true;

        } catch (Throwable t) {
            Log.w(TAG,
                "DDP2 HUD display request failed: "
                + t.getClass().getName()
                + ": "
                + t.getMessage());

            return false;
        }
    }


    private void prepareVcPresentation() throws Exception {
        V8KVcTrace.record("VC_POLICY=" + VC_POLICY + " source=" + VC_POLICY_SOURCE);
        V8KVcTrace.record("PREPARE vcEnabled=" + VC_RGI_ENABLED);
        if (VC_RGI_ENABLED) {
            captureVcPresentationState();
            V8KVcTrace.record("CAPTURE_STATE_OK favored=" + vcSavedFavoredViewMode);
            return;
        }

        /*
         * HUD-only mode:
         *
         * Leave the factory VC presentation untouched.
         * In particular, do NOT force COMPASS.  COMPASS is itself
         * a navigation presentation on this FPK.
         */
        /* v8j verified behavior: no VC map suppression here. */

        Log.i(TAG,
            "VC RGI disabled: favored view untouched");
    }

    /**
     * Apply the requested dev.4 VC policy after K2161's shared RGI/HUD
     * acceptance flags and BAP RG status are valid.
     */
    private void configureVcPresentation() throws Exception {
        V8KVcTrace.record("CONFIGURE vcEnabled=" + VC_RGI_ENABLED
            + " csRef=" + (csRef != null));
        if (VC_RGI_ENABLED) {
            /*
             * Full dev4 factory-RGI path.
             */
            V8KVcTrace.record("UPDATE_RG_ACTIVE_BEGIN");
            csRef.updateRgActive(true);
            V8MFactoryTrace.snapshot("after_rg_active_true", csRef);
            V8KVcTrace.record("UPDATE_RG_ACTIVE_RETURNED_OK");
            vcFactoryRgStatePropagated = true;

            if (VC_SET_FAVORED) {
                setVcFavoredViewMode(
                    VC_VIEW_RGI,
                    "CarPlay factory RGI"
                );
                V8KVcTrace.record("SET_FAVORED_RGI_RETURNED_OK");
            } else {
                V8KVcTrace.record("SET_FAVORED_SKIPPED policy=" + VC_POLICY);
            }

            Log.i(TAG,
                "VC policy active: FACTORY_RGI");

            return;
        }

        /*
         * HUD-only path.
         *
         * No ClusterViewMode selection and no factory RG-active
         * propagation into the VC state machine.
         */
        /* v8j verified behavior: no VC map suppression here. */

        if (setHudDisplayContent(true)) {
            Log.i(TAG,
                "VC policy active: HUD_ONLY_DDP2");
        } else {
            Log.w(TAG,
                "HUD-only DDP2 request unavailable");
        }
    }

    /**
     * Reassert the selected presentation if another factory component changes
     * favoredViewMode while CarPlay owns route guidance.
     */
    private void maintainVcPresentation() {
        V8KVcTrace.record("MAINTAIN vcEnabled=" + VC_RGI_ENABLED
            + " override=" + vcPresentationOverrideActive);
        if (!VC_RGI_ENABLED || !VC_SET_FAVORED) {
            return;
        }

        if (!vcPresentationOverrideActive
                || csRef == null
                || vcFavoredViewModeField == null) {
            return;
        }

        try {
            Object cvm = csRef.getClusterViewMode();

            if (cvm == null) {
                return;
            }

            int current =
                vcFavoredViewModeField.getInt(cvm);

            if (current != VC_VIEW_RGI) {
                setVcFavoredViewMode(
                    VC_VIEW_RGI,
                    "reassert after external VC mode change"
                );
                V8KVcTrace.record("VC_MODE_REASSERT_RETURNED_OK");
            }

        } catch (Throwable t) {
            Log.w(TAG,
                "VC presentation maintenance failed: "
                + t.getClass().getName()
                + ": "
                + t.getMessage());
        }
    }

    /** Restore the exact favored VC state captured before CarPlay takeover. */
    private void restoreVcPresentation() {
        V8KVcTrace.record("RESTORE override=" + vcPresentationOverrideActive);
        if (!vcPresentationOverrideActive) return;

        try {
            if (csRef != null
                    && csRef.getClusterViewMode() != null) {

                Object cvm =
                    csRef.getClusterViewMode();

                csRef.getClusterViewMode().setFavoredViewMode(
                    vcSavedFavoredViewMode
                );
                V8KVcTrace.record("RESTORE_FAVORED_RETURNED_OK mode="
                    + vcSavedFavoredViewMode);

                /*
                 * setFavoredViewMode() marks the value as explicitly received.
                 * Restore that bookkeeping flag as well so shutdown leaves the
                 * stock state exactly as it was before CarPlay takeover.
                 */
                if (vcFavoredViewModeReceivedField != null) {
                    vcFavoredViewModeReceivedField.setBoolean(
                        cvm,
                        vcSavedFavoredViewModeReceived
                    );
                }

                Log.i(TAG,
                    "VC state restored: favored="
                    + vcSavedFavoredViewMode
                    + " received="
                    + vcSavedFavoredViewModeReceived);
            }
        } catch (Throwable t) {
            Log.w(TAG,
                "VC state restore failed: "
                + t.getClass().getName() + ": "
                + t.getMessage());
        } finally {
            vcPresentationOverrideActive = false;
            vcFavoredViewModeField = null;
            vcFavoredViewModeReceivedField = null;
        }
    }

    /**
     * Best-effort suppression of VC map presentation.  K2161's simple maneuver panel
     * can still interpret shared route-guidance BAP fields, so this is deliberately
     * separate from the maneuver descriptor/distance transaction required by HUD.
     */
    private void suppressVcMapPresentation() {
        try {
            traceBap("updateMapVisibility", "false,false");
            appConnectorNavi.updateMapVisibility(false, false);
        } catch (Throwable t) {
            Log.w(TAG, "VC map visibility suppression failed: "
                + t.getClass().getName() + ": " + t.getMessage());
        }

        try {
            traceBap("updateMapPresentation", "false,false,false");
            appConnectorNavi.updateMapPresentation(false, false, false);
            V8MFactoryTrace.snapshot("after_map_presentation_false", csRef);
        } catch (Throwable t) {
            Log.w(TAG, "VC map presentation suppression failed: "
                + t.getClass().getName() + ": " + t.getMessage());
        }
    }

    private void traceDescriptor(int outPos, int idx, int type, int main, int dir, int zLevel, byte[] sideStreets) {
        if (!BAP_TRACE_ENABLED) return;
        StringBuffer sb = new StringBuffer();
        sb.append("pos=").append(outPos)
          .append(" idx=").append(idx)
          .append(" type=").append(type)
          .append(" main=").append(main)
          .append(" dir=").append(dir)
          .append(" z=").append(zLevel)
          .append(" side=[");
        if (sideStreets != null) {
            for (int i = 0; i < sideStreets.length; i++) {
                if (i > 0) sb.append(',');
                sb.append(sideStreets[i] & 0xFF);
            }
        }
        sb.append(']');
        Log.d(TAG, "[BAP] descriptor " + sb.toString());
    }

    /* ============================================================
     * Initialization
     * ============================================================ */

    public boolean init(Object naviService) {
        if (initialized) return true;

        try {
            if (!(naviService instanceof CombiBAPServiceNavi)) {
                String cls = (naviService != null) ? naviService.getClass().getName() : "null";
                Log.e(TAG, "Init failed: service is not CombiBAPServiceNavi (" + cls + ")");
                return false;
            }
            this.appConnectorNavi = (CombiBAPServiceNavi) naviService;

            initialized = true;
            initClusterAccess(); /* installs the native gate open when Navigation is ready */
            Log.i(TAG, "Initialized successfully (raw AppConnectorNavi): " + naviService.getClass().getName());
            return true;

        } catch (Exception e) {
            Log.e(TAG, "Init failed", e);
            return false;
        }
    }

    /**
     * Get ClusterService via Navigation singleton and install native BAP gate.
     * Non-fatal - if this fails, native RG stream won't be blocked.
     */
    private void initClusterAccess() {
        try {
            Navigation navi = Navigation.getInstance();
            if (navi == null) {
                Log.w(TAG, "ClusterAccess: Navigation.getInstance() returned null");
                return;
            }
            ClusterService cs = navi.getClusterService();
            if (cs == null) {
                Log.w(TAG, "ClusterAccess: ClusterService is null");
                return;
            }
            this.csRef = cs;
            /* Install the exact K2161 dispatcher-safe ownership gate, initially open. */
            try {
                ownership = new K2161RouteGuidanceOwnership(appConnectorNavi);
                if (ownership.install()) {
                    Log.i(TAG, "Exact K2161 route-guidance ownership gate installed open");
                } else {
                    ownership = null;
                    Log.w(TAG, "K2161 ownership gate not ready");
                }
            } catch (Exception ex) {
                ownership = null;
                Log.w(TAG, "K2161 ownership gate install failed: " + ex.getMessage());
            }

            Log.i(TAG, "ClusterAccess init OK");
        } catch (Exception e) {
            Log.w(TAG, "ClusterAccess setup failed (non-fatal): " + e.getMessage());
        }
    }

    /* Preserve the real DSI rgActive value while CarPlay temporarily owns it. */
    private boolean rgActiveForced = false;
    private boolean rgActiveSaved = false;
    /**
     * Set the K2161 route-information acceptance state used by the HUD path.
     */
    private void forceClusterRouteInfoState(boolean active) {
        if (csRef == null) return;

        if (!active
                && !rgActiveForced
                && !vcFactoryRgStatePropagated) {
            return;
        }

        try {
            Navigation nav = Navigation.getInstance();
            DSIResponseContainer container = (nav != null) ? nav.getDsiResponseContainer() : null;
            if (container != null) {
                if (active) {
                    if (!rgActiveForced) {
                        rgActiveSaved = container.isRgActive();
                        rgActiveForced = true;
                    }
                    container.setRgActive(true);
                } else if (rgActiveForced) {
                    rgActiveForced = false;
                    container.setRgActive(rgActiveSaved);
                    Log.i(TAG, "rgActive overlay released (restored " + rgActiveSaved + ")");
                }
            }
        } catch (Exception e) {
            Log.d(TAG, "force container rgActive failed: " + e.getMessage());
        }

        if (active) {
            V8MFactoryTrace.snapshot("before_rgi_valid_on", csRef);
            try { csRef.updateRGIString(new short[]{1}); }
            catch (Exception e) { Log.d(TAG, "force rgiValid=true failed: " + e.getMessage()); }
            V8MFactoryTrace.snapshot("after_rgi_valid_on", csRef);
        } else {
            V8MFactoryTrace.snapshot("before_rgi_valid_off", csRef);
            try { csRef.updateRGIString(null); }
            catch (Exception e) { Log.d(TAG, "force rgiValid=false failed: " + e.getMessage()); }
            V8MFactoryTrace.snapshot("after_rgi_valid_off", csRef);

            /*
             * Only the VC-enabled path propagates ClusterService RG-active
             * state. Restore that factory state symmetrically on release.
             */
            if (vcFactoryRgStatePropagated) {
                try {
                    csRef.updateRgActive(rgActiveSaved);
                    Log.i(TAG,
                        "Factory VC RG state restored to "
                        + rgActiveSaved);
                } catch (Exception e) {
                    Log.w(TAG,
                        "Factory VC RG-state restore failed: "
                        + e.getMessage());
                } finally {
                    vcFactoryRgStatePropagated = false;
                }
            }
        }
    }


    /* ============================================================
     * Lifecycle
     * ============================================================ */

    public boolean onStart() {
        if (!initialized) return false;

        try {
            inApproachZone = false;
            lastFirstManeuverIdx = -1;
            lastFirstManeuverVer = -1;
            latchedTurnToText = "";
            synchronized (distanceToManeuverLock) {
                hasLastDistM = false;
                lastDistM = 0;
            }

            /*
             * Lazy-init cluster hooks (native stream gate).
             * Navigation singleton may not be available at init() time.
             */
            if (ownership == null) {
                initClusterAccess();
            }

            /* Native Audi navigation is never cancelled or stopped. */

            /* Block native route-guidance BAP stream during CarPlay RG. */
            if (ownership == null || !ownership.setCarPlayRouteActive(true)) {
                Log.w(TAG, "Route ownership not ready; no CarPlay BAP emitted");
                return false;
            }

            /*
             * Capture/prepare VC state before enabling the shared HUD
             * route-information acceptance overlay.
             */
            prepareVcPresentation();

            /*
             * Factory VC-RGI mode uses the proven K2161
             * rgActive/RGI-valid acceptance state.
             */
            if (VC_RGI_ENABLED) {
                forceClusterRouteInfoState(true);
            }

            /*
             * Guidance start -- K2161 HUD route-guidance BAP transaction.
             *
             * 1. RGStatus(1) - FctID 17 -> triggers startSync(0) for {17,39,23,18,49}
             * 2. Complete sync(0) window: rgType(39), descriptor(23), distance(18), exitView(49)
             */
            int clusterRgStatus =
                VC_RGI_ENABLED ? 1 : 0;

            traceBap(
                "updateRGStatusAndActiveRGType",
                clusterRgStatus + "," + ACTIVE_RGTYPE
            );

            sendRgStatusAndType(clusterRgStatus, ACTIVE_RGTYPE);

            /* Keep HUD active and configure whether VC may consume the shared RGI presentation. */
            configureVcPresentation();

            /* Sync(0) FctIDs: descriptor, distance, exitView */
            sendNoSymbol();                                                         /* FctID 23 */
            sendDistanceToManeuverRaw(0, false, 0);                                  /* FctID 18 */


            /* Non-sync FctIDs */
            traceBap("updateCurrentPositionInfo", "\"\"");
            try {
                appConnectorNavi.updateCurrentPositionInfo("");                       /* FctID 19 */
            } catch (Throwable t) {
                Log.w(TAG, "BAP FctID 19 failed during start: "
                    + t.getClass().getName() + ": " + t.getMessage());
            }

            Log.i(TAG, "Started (rgType=" + ACTIVE_RGTYPE + ", display=HUD)");
            return true;

        } catch (Throwable e) {
            Log.e(TAG, "onStart error: " + e.getClass().getName() + ": " + e.getMessage());

            if (!VC_RGI_ENABLED
                    && hudDisplayContentForced) {
                setHudDisplayContent(false);
            }

            try { forceClusterRouteInfoState(false); }
            catch (Throwable ignored) { }

            restoreVcPresentation();

            try {
                if (ownership != null && ownership.isCarPlayRouteActive()) {
                    ownership.setCarPlayRouteActive(false);
                }
            } catch (Throwable ignored) { }

            return false;
        }
    }

    public void onStop() {
        if (!initialized) return;

        try {
            /* Lightweight stop — reset internal state only.
             * No BAP teardown. iOS sends transient route_state=0
             * during maneuver transitions; full teardown causes HUD flicker. BAP teardown happens in onShutdown() on real disconnect. */
            inApproachZone = false;
            lastFirstManeuverIdx = -1;
            lastFirstManeuverVer = -1;
            latchedTurnToText = "";
            Log.d(TAG, "Stopped (lightweight, BAP kept alive)");
        } catch (Exception e) {
            Log.e(TAG, "onStop error", e);
        }
    }

    /**
     * Full shutdown — BAP teardown and native ownership hand-back.
     * Called on actual CarPlay disconnect or stop().
     */
    public void onShutdown() {
        if (!initialized) return;

        try {

            /*
             * Guidance stop — full BAP teardown:
             * 1. RGStatus(0) - triggers sync(0) for {17,39,23,18,49}
             * 2. Complete sync(0) window: descriptor(23), distance(18), exitView(49)
             * 3. Non-sync FctIDs last
             */
            traceBap("updateRGStatusAndActiveRGType", "0,0");
            sendRgStatusAndType(0, 0);
            sendNoSymbol();
            sendDistanceToManeuverRaw(0, false, 0);

            traceBap("updateManeuverState", "0");
            appConnectorNavi.updateManeuverState(0);
            traceBap("updateCurrentPositionInfo", "\"\"");
            try { appConnectorNavi.updateCurrentPositionInfo(""); } catch (Throwable t) {}
            sendDistanceToDestinationRaw(0, false);
            traceBap("updateTimeToDestination", "0,0,-1");
            appConnectorNavi.updateTimeToDestination(0, 0, -1);
            traceBap("updateLaneGuidance", "[],false");
            appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);

            if (!VC_RGI_ENABLED
                    && hudDisplayContentForced) {
                setHudDisplayContent(false);
            }

            /* Restore the real DSI rgActive value captured when CarPlay took ownership. */
            forceClusterRouteInfoState(false);

            /* Restore the user's pre-CarPlay VC presentation preference. */
            restoreVcPresentation();

            /* Clear CarPlay BAP first, then reopen native RG and force the
             * exact K2161 ClusterService setter/updateAll hand-back. */
            if (ownership != null && ownership.isCarPlayRouteActive()) {
                ownership.setCarPlayRouteActive(false);
            }

            Log.i(TAG, "Shutdown (full teardown)");
        } catch (Exception e) {
            Log.e(TAG, "onShutdown error", e);
        }
    }

    /** Full bundle/service teardown: hand native ClusterService its raw service back. */
    public synchronized void dispose() {
        onShutdown();
        if (ownership != null) {
            ownership.restoreBestEffort();
            ownership = null;
        }
        initialized = false;
        appConnectorNavi = null;
        csRef = null;
        Log.i(TAG, "Disposed; raw K2161 CombiBAPServiceNavi restored");
    }

    /* ============================================================
     * Update
     * ============================================================ */

    public void update(RouteGuidance.State s) {
        if (!initialized || s == null) return;

        int dirty = s.dirtyMask;
        if (dirty == 0) return;
        Log.d(TAG, "Update delta mask=0x" + Integer.toHexString(dirty));

        try {
            /*
             * Keep dev.4 VC policy authoritative during the active
             * CarPlay route.
             */
            maintainVcPresentation();

            /*
             * Explicit clear: count dropped to 0.  But only treat it as a real clear
             * if the route itself is inactive (routeState < 1).
             */
            boolean explicitClear = ((dirty & RouteGuidance.State.DIRTY_MANEUVER_COUNT) != 0)
                && (s.maneuverCount == 0)
                && (s.routeState <= 0);
            int[] idxs = getManeuverIndexList(s);
            boolean hasManeuverList = (idxs != null && idxs.length > 0);
            boolean hasAnyManeuver = (s.maneuverCount > 0);
            boolean shouldClearManeuver = (s.maneuverCount == 0) && (s.routeState <= 0);

            PresentationManeuver presentation = resolvePresentationManeuver(s);
            int firstIdx = presentation.index;
            int type0 = presentation.type;
            int distM = presentation.distanceM;
            boolean showManeuver = ManeuverMapper.isValidType(type0);

            if (presentation.usedSlotDistance) {
                Log.d(TAG, "Presentation distance aligned to selected maneuver idx=" + firstIdx
                    + " live=" + presentation.liveDistanceM + "m slot="
                    + presentation.slotDistanceM + "m");
            }

            /* Keep a lightweight approach state for Audi maneuver-state emphasis
             * and turn-to street text only.  It never gates the real icon or distance. */
            int rawStepM = (firstIdx >= 0 && s.mDistance != null
                    && firstIdx < s.mDistance.length) ? s.mDistance[firstIdx] : -1;
            boolean isHighway;
            if (rawStepM > 0) {
                isHighway = rawStepM > HIGHWAY_STEP_THRESHOLD_M;
            } else {
                isHighway = (type0 >= 0) && ManeuverMapper.isHighwayManeuver(type0);
            }
            int prepareThreshold = isHighway ? HIGHWAY_PREPARE_THRESHOLD_M : CITY_PREPARE_THRESHOLD_M;
            int actionThresholdM = getActionThresholdM(s, firstIdx, prepareThreshold);

            int currentFirstVer = (firstIdx >= 0 && s.mVer != null
                    && firstIdx < s.mVer.length) ? s.mVer[firstIdx] : -1;
            boolean primaryChanged = (firstIdx != lastFirstManeuverIdx)
                || (currentFirstVer != lastFirstManeuverVer);
            if (primaryChanged) {
                inApproachZone = false;
                lastFirstManeuverIdx = firstIdx;
                lastFirstManeuverVer = currentFirstVer;
            }

            boolean hasUsableDistance = distM > 0;
            boolean isArrival = (type0 == ManeuverMapper.MT_ARRIVE_END_OF_NAVIGATION
                || type0 == ManeuverMapper.MT_ARRIVE_AT_DESTINATION
                || type0 == ManeuverMapper.MT_ARRIVE_END_OF_DIRECTIONS
                || type0 == ManeuverMapper.MT_ARRIVE_DESTINATION_LEFT
                || type0 == ManeuverMapper.MT_ARRIVE_DESTINATION_RIGHT);
            boolean nowApproach = isArrival
                || (hasUsableDistance ? (distM <= prepareThreshold) : inApproachZone);
            boolean approachChanged = hasUsableDistance
                && (nowApproach != inApproachZone)
                && showManeuver && hasManeuverList;
            if (approachChanged) {
                dirty |= RouteGuidance.State.DIRTY_DIST_MAN
                       | RouteGuidance.State.DIRTY_LANE_GUIDANCE
                       | RouteGuidance.State.DIRTY_MANEUVER_TEXT
                       | RouteGuidance.State.DIRTY_MANEUVER_ICON;
                inApproachZone = nowApproach;
                latchedTurnToText = "";
                Log.i(TAG, "Approach zone " + (nowApproach ? "ENTER" : "EXIT")
                    + " (dist=" + distM + "m, highway=" + isHighway
                    + ", prepThr=" + prepareThreshold + ", actionThr=" + actionThresholdM + ")");
            }

            if (explicitClear || shouldClearManeuver) {
                latchedTurnToText = "";
            }

            /*
             * 1. Maneuver icons (FctID 23)
             */
            if ((dirty & (RouteGuidance.State.DIRTY_MANEUVER_ICON |
                          RouteGuidance.State.DIRTY_MANEUVER_LIST |
                          RouteGuidance.State.DIRTY_MANEUVER_COUNT)) != 0) {
                if (explicitClear) {
                    sendNoSymbol();
                } else if (hasManeuverList) {
                    if (showManeuver) {
                        sendManeuvers(s, firstIdx);
                    } else if (shouldClearManeuver) {
                        sendNoSymbol();
                    } else if (hasAnyManeuver) {
                        Log.d(TAG, "Slot data pending for list, keeping last icons (count=" + s.maneuverCount + ")");
                    }
                } else if (shouldClearManeuver) {
                    sendNoSymbol();
                } else if (hasAnyManeuver) {
                    Log.d(TAG, "Maneuver list missing (count=" + s.maneuverCount + "), keep last icons");
                }
            }

            /*
             * 2. ExitView is published atomically with every descriptor through
             *    updateManeuverDescriptorAndExitView(..., 0, 0), matching stock K2161.
             */
            /*
             * 3. Distance to maneuver (FctID 18)
             *
             * Sub-kilometer metric values are encoded directly; other values use the OEM formatter.
             */
            if ((dirty & (RouteGuidance.State.DIRTY_DIST_MAN |
                          RouteGuidance.State.DIRTY_MANEUVER_ICON |
                          RouteGuidance.State.DIRTY_MANEUVER_LIST |
                          RouteGuidance.State.DIRTY_MANEUVER_COUNT)) != 0) {
                boolean transientNoDistance = (distM <= 0)
                    && !shouldClearManeuver
                    && !explicitClear
                    && hasManeuverList
                    && hasAnyManeuver;
                if (transientNoDistance) {
                    boolean haveCached;
                    int cachedDistM;
                    synchronized (distanceToManeuverLock) {
                        haveCached = hasLastDistM;
                        cachedDistM = lastDistM;
                    }
                    if (haveCached) {
                        sendDistanceToManeuverRaw(cachedDistM, false, 0);
                    } else {
                        sendDistanceToManeuverRaw(0, false, 0);
                    }
                } else if (distM <= 0 || shouldClearManeuver) {
                    sendDistanceToManeuverRaw(0, false, 0);
                } else {
                    sendDistanceToManeuverRaw(distM, false, 0);
                }
            }

            /* 4. Street text (FctID 19 - CurrentPositionInfo) */
            if ((dirty & (RouteGuidance.State.DIRTY_CURRENT_ROAD |
                          RouteGuidance.State.DIRTY_MANEUVER_TEXT |
                          RouteGuidance.State.DIRTY_MANEUVER_LIST |
                          RouteGuidance.State.DIRTY_MANEUVER_ICON)) != 0) {
                String road;
                boolean turnTextMode = inApproachZone && !explicitClear && !shouldClearManeuver;
                if (turnTextMode) {
                    int idx = firstIdx;
                    String candidate = "";
                    if (idx >= 0) {
                        if (s.mExitInfo != null && idx < s.mExitInfo.length
                                && s.mExitInfo[idx] != null && s.mExitInfo[idx].length() > 0) {
                            candidate = keepLastColonPart(s.mExitInfo[idx]);
                        } else if (s.mAfterRoad != null && idx < s.mAfterRoad.length
                                && s.mAfterRoad[idx] != null && s.mAfterRoad[idx].length() > 0) {
                            candidate = keepLastColonPart(s.mAfterRoad[idx]);
                        } else if (s.mName != null && idx < s.mName.length
                                && s.mName[idx] != null && s.mName[idx].length() > 0) {
                            candidate = s.mName[idx];
                        }
                    }
                    if (candidate.length() > 0) {
                        String arrow = directionArrow(s, idx);
                        latchedTurnToText = arrow + " " + candidate;
                    }
                    road = latchedTurnToText;
                } else {
                    latchedTurnToText = "";
                    road = (s.currentRoad != null) ? s.currentRoad : "";
                }
                road = limitUtf8(road, 96);
                traceBap("updateCurrentPositionInfo", "\"" + road + "\"");
                appConnectorNavi.updateCurrentPositionInfo(road);
            }

            /*
             * 5. Maneuver state (FctID 24) - for HUD
             */
            if ((dirty & (RouteGuidance.State.DIRTY_MANEUVER_STATE |
                          RouteGuidance.State.DIRTY_MANEUVER_ICON |
                          RouteGuidance.State.DIRTY_MANEUVER_LIST |
                          RouteGuidance.State.DIRTY_MANEUVER_COUNT |
                          RouteGuidance.State.DIRTY_DIST_MAN)) != 0) {
                if (explicitClear) {
                    traceBap("updateManeuverState", "0");
                    appConnectorNavi.updateManeuverState(0);
                } else if (shouldClearManeuver || (hasManeuverList && showManeuver)) {
                    int bapState;
                    if (showManeuver) {
                        if (!nowApproach) {
                            bapState = 1;   /* Follow */
                        } else if (hasUsableDistance && actionThresholdM > 0 && distM <= actionThresholdM) {
                            bapState = 4;   /* Action */
                        } else {
                            bapState = 2;   /* Prepare */
                        }
                    } else {
                        bapState = 0;
                    }
                    traceBap("updateManeuverState", String.valueOf(bapState));
                    appConnectorNavi.updateManeuverState(bapState);
                } else if (hasManeuverList && hasAnyManeuver) {
                    traceBap("updateManeuverState", "1");
                    appConnectorNavi.updateManeuverState(1);
                } else if (hasAnyManeuver && !hasManeuverList) {
                    Log.d(TAG, "Maneuver state unchanged: list missing (count=" + s.maneuverCount + ")");
                }
            }

            /*
             * 6. Lane guidance (FctID 24)
             */
            int laneRecomputeMask = RouteGuidance.State.DIRTY_LANE_GUIDANCE
                | RouteGuidance.State.DIRTY_MANEUVER_LIST
                | RouteGuidance.State.DIRTY_MANEUVER_COUNT;
            if ((dirty & laneRecomputeMask) != 0) {
                /* Trust iOS's display intent — `laneGuidanceShowing` is the
                 * authoritative signal.  iOS sets it to 1 only when its own
                 * navigation manager (MNGuidanceManager._considerLaneGuidance)
                 * has decided to show lanes for the user, based on the lane
                 * event's startValidRouteCoordinate / endValidRouteCoordinate
                 * range.  This naturally covers:
                 *   - highway pre-positioning (shows 1-2 km early)
                 *   - active lane choice (right at the maneuver)
                 *   - hide on exit (showing→0 once past endValidRouteCoordinate)
                 * Do not add a local distance gate here; iOS owns lane visibility. */
                boolean wantLaneGuidance = !explicitClear
                    && !shouldClearManeuver
                    && s.laneGuidanceShowing == 1;
                if (wantLaneGuidance) {
                    sendLaneGuidance(s);
                } else {
                    traceBap("updateLaneGuidance", "[],false");
                    appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
                }
            }

            /* 7. Distance to destination (FctID 21) */
            if ((dirty & RouteGuidance.State.DIRTY_DIST_DEST) != 0) {
                sendDistanceToDestinationRaw(s.distDestM, false);
            }

            /* 8. Time to destination (FctID 22) - via AppConnectorNavi */
            if ((dirty & (RouteGuidance.State.DIRTY_TIME_REMAINING |
                          RouteGuidance.State.DIRTY_ETA)) != 0) {
                long eta = s.etaSeconds;
                int timeInfoType = 1;
                int timeFormat = getHuNavigationTimeFormat();
                long timeVal;

                if (eta >= 0) {
                    timeVal = eta;
                } else if (s.timeRemainingSeconds >= 0) {
                    long nowMs = getUtcMillis();
                    timeVal = (nowMs / 1000L) + s.timeRemainingSeconds;
                } else {
                    timeVal = -1;
                }

                /* JVM default TZ is UTC on MHI2. AppConnectorNavi uses
                 * GregorianCalendar(Date(epoch*1000)) which would show UTC.
                 * Convert UTC epoch to local epoch so calendar shows local time.
                 * Uses fw.convertUTCTimeToLocalTime() -- DST-aware, always correct. */
                if (timeVal >= 0) {
                    long utcMs = timeVal * 1000L;
                    long localMs = convertUtcToLocalMs(utcMs);
                    timeVal = localMs / 1000L;
                }

                traceBap("updateTimeToDestination", timeInfoType + "," + timeFormat + "," + timeVal);
                appConnectorNavi.updateTimeToDestination(timeInfoType, timeFormat, timeVal);
            }

            /* 9. Destination info (FctID 46) - via AppConnectorNavi */
            if ((dirty & RouteGuidance.State.DIRTY_DESTINATION) != 0) {
                String dest = (s.destination != null) ? s.destination : "";
                dest = limitUtf8(dest, 50);
                CombiBAPNaviDestination naviDest =
                    new CombiBAPNaviDestination("", "", dest, "", "", "", "");
                CombiBAPDestinationInfo destInfo = new CombiBAPDestinationInfo(naviDest);
                traceBap("updateDestinationInfo", "\"" + dest + "\"");
                appConnectorNavi.updateDestinationInfo(destInfo);
            }


            Log.d(TAG, "Update: dist=" + distM + "m maneuvers="
                + Math.min(s.maneuverCount, MAX_BAP_MANEUVERS)
                + " selectedIdx=" + firstIdx
                + " distSource=" + (presentation.usedSlotDistance ? "slot" : "live")
                + " startSkipped=" + presentation.skippedStartRoute);

        } catch (Exception e) {
            Log.e(TAG, "update error", e);
        }
    }

    /* ============================================================
     * Maneuver Sending (current maneuver for HUD)
     * ============================================================ */

    /**
     * Send FOLLOW_STREET descriptor for the K2161 startup sync transaction only.
     */
    private void sendFollowStreet() throws Exception {
        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[1];
        arr[0] = createDescriptor(ManeuverMapper.FOLLOW_STREET, ManeuverMapper.DIR_STRAIGHT, 0, new byte[0]);
        traceBap("updateManeuverDescriptorAndExitView", "count=1 FOLLOW_STREET exit=0,0");
        V8MFactoryTrace.descriptor("before_call", arr, 0, 0, appConnectorNavi, csRef);
        sendManeuverDescriptorAndExitView(arr, 0, 0);
        V8MFactoryTrace.descriptor("after_returned", arr, 0, 0, appConnectorNavi, csRef);
    }

    /**
     * K3346: AppConnectorNavi.updateRGStatusAndActiveRGType() does not exist.
     * Stock CombiBAPListener.updateRGStatusAndType() sends RGStatus first,
     * then ActiveRGType; keep that order.
     */
    private void sendRgStatusAndType(int rgStatus, int rgType) {
        appConnectorNavi.updateRGStatus(rgStatus);
        appConnectorNavi.updateActiveRGType(rgType);
    }

    /**
     * K3346: AppConnectorNavi.updateManeuverDescriptorAndExitView() is a
     * deprecated no-op (it only logs). Stock CombiBAPListener sends the
     * descriptor and the exit view as two separate BAP updates.
     */
    private void sendManeuverDescriptorAndExitView(
        CombiBAPNaviManeuverDescriptor[] arr, int exitVariant, int exitViewId) {
        appConnectorNavi.updateManeuverDescriptor(arr);
        appConnectorNavi.updateExitView(exitVariant, exitViewId);
    }

    /**
     * Send NO_SYMBOL descriptor.
     */
    private void sendNoSymbol() throws Exception {
        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[1];
        arr[0] = createDescriptor(0, 0, 0, new byte[0]);
        traceBap("updateManeuverDescriptorAndExitView", "count=1 NO_SYMBOL exit=0,0");
        V8MFactoryTrace.descriptor("before_call", arr, 0, 0, appConnectorNavi, csRef);
        sendManeuverDescriptorAndExitView(arr, 0, 0);
        V8MFactoryTrace.descriptor("after_returned", arr, 0, 0, appConnectorNavi, csRef);
    }

    /**
     * Send only the current (first valid) maneuver from iOS maneuverOrder.
     */
    private void sendManeuvers(RouteGuidance.State s, int idx) throws Exception {
        if (idx < 0) {
            if (s.maneuverCount == 0) sendNoSymbol();
            else Log.d(TAG, "No presentable authoritative maneuver yet; keeping last icon");
            return;
        }

        int[] mapped = ManeuverMapper.map(
            s.mType[idx], s.mTurnAngle[idx],
            s.mJunctionType[idx], s.mDrivingSide[idx]);
        int zLevel = (s.mZLevel != null && idx < s.mZLevel.length) ? s.mZLevel[idx] : 0;
        byte[] sideStreets;
        if (mapped[0] == ManeuverMapper.NO_INFO || mapped[0] == ManeuverMapper.NO_SYMBOL) {
            sideStreets = new byte[0];
        } else {
            sideStreets = SideStreets.calcSideStreetsBytes(
                s.mType[idx], s.mJunctionType[idx], s.mDrivingSide[idx],
                s.mJunctionAngles[idx], s.mExitAngle[idx]);
        }

        Log.d(TAG, "[BAP] mapinput idx=" + idx
            + " type=" + s.mType[idx]
            + " turnAngle=" + s.mTurnAngle[idx]
            + " junctionType=" + s.mJunctionType[idx]
            + " drivingSide=" + s.mDrivingSide[idx]
            + " distM=" + s.distManeuverM);
        traceDescriptor(0, idx, s.mType[idx], mapped[0], mapped[1], zLevel, sideStreets);

        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[1];
        arr[0] = createDescriptor(mapped[0], mapped[1], zLevel, sideStreets);
        traceBap("updateManeuverDescriptorAndExitView", "count=1 exit=0,0");
        V8MFactoryTrace.descriptor("before_call", arr, 0, 0, appConnectorNavi, csRef);
        sendManeuverDescriptorAndExitView(arr, 0, 0);
        V8MFactoryTrace.descriptor("after_returned", arr, 0, 0, appConnectorNavi, csRef);
        Log.d(TAG, "Sent 1 maneuver");
    }

    /* ============================================================
     * Lane Guidance (FctID 24)
     * ============================================================ */

    private void sendLaneGuidance(RouteGuidance.State s) throws Exception {
        int idx = resolveLaneGuidanceManeuverIndex(s);
        int count = laneCountForManeuver(s, idx);
        boolean hasRealData = hasLaneGuidanceForManeuver(s, idx);

        if (hasRealData) {
            sendRealLaneGuidance(s, idx, count);
        } else {
            traceBap("updateLaneGuidance", "[],false");
            appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
        }
    }

    private void sendRealLaneGuidance(RouteGuidance.State s, int idx, int count) throws Exception {
        int n = Math.min(count, 8);
        CombiBAPNaviLaneGuidanceData[] tmp = new CombiBAPNaviLaneGuidanceData[n];
        int out = 0;

        for (int i = 0; i < n; i++) {
            short lanePos = mapLanePosition(s, idx, i);
            short laneDir = mapLaneDirection(s, idx, i);
            if (!shouldEmitLane(s, idx, i, laneDir)) {
                continue;
            }
            byte[] laneSideStreets = mapLaneSideStreets(s, idx, i, laneDir);
            byte gi = mapGuidanceInfo(s, idx, i);
            Log.d(TAG, "  lane[" + i + "] pos=" + lanePos + " dir=0x"
                + Integer.toHexString(laneDir & 0xFF) + " gi=" + gi
                + " sideStreets=" + hexBytes(laneSideStreets));
            tmp[out++] = new CombiBAPNaviLaneGuidanceData(
                lanePos, laneDir, laneSideStreets, (short) 0,
                (byte) 0, (byte) 0, (byte) 0, gi);
        }

        if (out <= 0) {
            traceBap("updateLaneGuidance", "[],false");
            appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
            return;
        }

        CombiBAPNaviLaneGuidanceData[] arr = tmp;
        if (out != n) {
            arr = new CombiBAPNaviLaneGuidanceData[out];
            for (int i = 0; i < out; i++) arr[i] = tmp[i];
        }

        traceBap("updateLaneGuidance", "real,count=" + out
            + ",slot=" + idx
            + ",current=" + s.laneGuidanceIndex
            + ",mapped=" + s.laneGuidanceSlot);
        appConnectorNavi.updateLaneGuidance(true, arr);
    }

    private static boolean hasLaneGuidanceForManeuver(RouteGuidance.State s, int manIdx) {
        if (manIdx < 0) return false;
        if (hasLaneCacheForSlot(s, manIdx)) return true;
        if (s.mLaneDirections == null || manIdx >= s.mLaneDirections.length) return false;
        if (s.mLaneDirections[manIdx] == null) return false;
        return laneCountForManeuver(s, manIdx) > 0;
    }

    private static boolean hasLaneCacheForSlot(RouteGuidance.State s, int slot) {
        if (slot < 0) return false;
        if (s.lgLaneDirections == null || slot >= s.lgLaneDirections.length) return false;
        if (s.lgLaneDirections[slot] == null) return false;
        return laneCacheCountForSlot(s, slot) > 0;
    }

    /* Verify lg-cache slot's stored event id still matches the active idx.
     * Needed because lg cache slots are LRU-allocated independently of
     * maneuver slot numbers, so a slot can be remapped to a different
     * event after eviction. */
    private static boolean lgSlotMatches(RouteGuidance.State s, int slot, int idx) {
        if (slot < 0 || s.lgIndex == null || slot >= s.lgIndex.length) return false;
        return s.lgIndex[slot] == idx;
    }

    /* m-cache-only check (skips lg-cache).  Used in legacy fallback paths
     * where we need m-cache data without aliasing into lg-cache that
     * happens to occupy the same numeric slot. */
    private static boolean hasMCacheLaneForSlot(RouteGuidance.State s, int slot) {
        if (slot < 0) return false;
        if (s.mLaneDirections == null || slot >= s.mLaneDirections.length) return false;
        if (s.mLaneDirections[slot] == null) return false;
        int count = -1;
        if (s.mLaneCount != null && slot < s.mLaneCount.length) count = s.mLaneCount[slot];
        if (count <= 0) count = s.mLaneDirections[slot].length;
        return count > 0;
    }

    private static int laneCacheCountForSlot(RouteGuidance.State s, int slot) {
        if (slot < 0) return 0;
        int count = 0;
        if (s.lgLaneCount != null && slot < s.lgLaneCount.length) {
            count = s.lgLaneCount[slot];
        }
        if (count <= 0 && s.lgLaneDirections != null && slot < s.lgLaneDirections.length
            && s.lgLaneDirections[slot] != null) {
            count = s.lgLaneDirections[slot].length;
        }
        if (count <= 0 && s.lgLaneStatus != null && slot < s.lgLaneStatus.length
            && s.lgLaneStatus[slot] != null) {
            count = s.lgLaneStatus[slot].length;
        }
        return Math.max(count, 0);
    }

    private static int laneSlotForGuidanceIndex(RouteGuidance.State s, int guidanceIndex) {
        if (guidanceIndex < 0 || s.lgIndex == null) return -1;
        for (int i = 0; i < s.lgIndex.length; i++) {
            if (s.lgIndex[i] == guidanceIndex && hasLaneCacheForSlot(s, i)) {
                return i;
            }
        }
        return -1;
    }

    /* Slot ids are shared by the legacy m-cache fallback and the lg-cache
     * arrays.  Keep both caches in the same bounded numeric slot range. */
    private static int[] lanePositionsFor(RouteGuidance.State s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLanePositions != null && slot < s.lgLanePositions.length)
            return s.lgLanePositions[slot];
        if (s.mLanePositions != null && slot >= 0 && slot < s.mLanePositions.length)
            return s.mLanePositions[slot];
        return null;
    }

    private static int[] laneDirectionsFor(RouteGuidance.State s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLaneDirections != null && slot < s.lgLaneDirections.length)
            return s.lgLaneDirections[slot];
        if (s.mLaneDirections != null && slot >= 0 && slot < s.mLaneDirections.length)
            return s.mLaneDirections[slot];
        return null;
    }

    private static int[] laneStatusFor(RouteGuidance.State s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLaneStatus != null && slot < s.lgLaneStatus.length)
            return s.lgLaneStatus[slot];
        if (s.mLaneStatus != null && slot >= 0 && slot < s.mLaneStatus.length)
            return s.mLaneStatus[slot];
        return null;
    }

    private static int[][] laneAnglesFor(RouteGuidance.State s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLaneAngles != null && slot < s.lgLaneAngles.length)
            return s.lgLaneAngles[slot];
        if (s.mLaneAngles != null && slot >= 0 && slot < s.mLaneAngles.length)
            return s.mLaneAngles[slot];
        return null;
    }

    private static int laneCountForManeuver(RouteGuidance.State s, int manIdx) {
        if (manIdx < 0) return 0;
        if (hasLaneCacheForSlot(s, manIdx)) {
            return laneCacheCountForSlot(s, manIdx);
        }
        int count = 0;
        if (s.mLaneCount != null && manIdx < s.mLaneCount.length) {
            count = s.mLaneCount[manIdx];
        }
        if (count <= 0 && s.mLaneDirections != null && manIdx < s.mLaneDirections.length
            && s.mLaneDirections[manIdx] != null) {
            count = s.mLaneDirections[manIdx].length;
        }
        if (count <= 0 && s.mLaneStatus != null && manIdx < s.mLaneStatus.length
            && s.mLaneStatus[manIdx] != null) {
            count = s.mLaneStatus[manIdx].length;
        }
        if (count < 0) count = 0;
        return count;
    }

    private static int resolveLaneGuidanceManeuverIndex(RouteGuidance.State s) {
        /*
         * iOS exposes the currently displayed lane guidance as a top-level
         * currentLaneGuidanceIndex (0x5201 InfoType 16 — written by Maps.app
         * in setCurrentLaneGuidanceIndex: from CarMetadataNavigationListener
         * each location update).  0x5204's TLV1 is iOS's
         * composedGuidanceEventIndex — an EVENT IDENTIFIER, sent for every
         * cached event including future precache.  So the active is purely
         * 0x5201, and we must look up the lg-cache slot whose stored event
         * id matches the active.
         *
         * lg cache slot indices are NOT the same numeric space as maneuver
         * slot indices: in C, rgd_lane_slot_for_iap_index() maintains an
         * independent LRU.  After eviction, lg slot N may hold any cached
         * event id, not necessarily event N — which means resolving by
         * "primary maneuver slot" via lg-cache silently sends data for
         * the wrong event.  Validate s.lgIndex[slot] == active before
         * trusting any lg-cache slot.
         */

        int activeIdx = s.laneGuidanceIndex;
        if (activeIdx < 0) return -1;

        if (s.laneGuidanceSlot >= 0 && hasLaneCacheForSlot(s, s.laneGuidanceSlot)
            && lgSlotMatches(s, s.laneGuidanceSlot, activeIdx)) {
            return s.laneGuidanceSlot;
        }

        int byIndex = laneSlotForGuidanceIndex(s, activeIdx);
        if (byIndex >= 0) return byIndex;

        /*
         * Linked-lane fallback: C publishes mN_linked_lane_guidance_slot
         * pointing at a real lg-cache slot (not a maneuver slot), so it's
         * safe to consult directly.  Only use it when its stored event id
         * still matches the active (the linked field can outlive a cache
         * remap of its target).
         */
        int primary = getFirstManeuverIndex(s);
        int linked = linkedLaneSlotForManeuver(s, primary);
        if (linked >= 0 && hasLaneCacheForSlot(s, linked)
            && lgSlotMatches(s, linked, activeIdx)) {
            return linked;
        }

        /*
         * Legacy m-cache fallback for compatibility with pre-lg-split
         * hooks (which wrote lane data into mN_lane_*).  Skip if the
         * primary slot has any lg-cache data — that lg entry could be
         * unrelated event data (post-eviction) and m-cache lookup with
         * the same numeric slot would alias into it.
         */
        int max = (s.mLaneCount != null) ? s.mLaneCount.length : 0;
        if (activeIdx < max && hasMCacheLaneForSlot(s, activeIdx)
            && !hasLaneCacheForSlot(s, activeIdx)) {
            return activeIdx;
        }
        if (primary >= 0 && hasMCacheLaneForSlot(s, primary)
            && !hasLaneCacheForSlot(s, primary)) {
            return primary;
        }

        return -1;
    }



    private static int linkedLaneSlotForManeuver(RouteGuidance.State s, int manIdx) {
        if (manIdx < 0) return -1;
        int max = (s.mLaneCount != null) ? s.mLaneCount.length : 0;

        if (s.mLinkedLaneGuidanceSlot != null && manIdx < s.mLinkedLaneGuidanceSlot.length) {
            int slot = s.mLinkedLaneGuidanceSlot[manIdx];
            if (slot >= 0 && slot < max) return slot;
        }

        if (s.mLinkedLaneGuidanceIndex != null && manIdx < s.mLinkedLaneGuidanceIndex.length) {
            int linkedIdx = s.mLinkedLaneGuidanceIndex[manIdx];
            if (linkedIdx >= 0 && linkedIdx < max) return linkedIdx;
        }
        return -1;
    }

    private static short mapLanePosition(RouteGuidance.State s, int manIdx, int laneIdx) {
        int[] pos = lanePositionsFor(s, manIdx);
        if (pos == null || laneIdx < 0 || laneIdx >= pos.length) {
            return (short) laneIdx;
        }
        int v = pos[laneIdx];
        if (v < 0 || v > 0x7FFF) return (short) laneIdx;
        return (short) v;
    }

    private static boolean hasLaneAnglesForLane(RouteGuidance.State s, int manIdx, int laneIdx) {
        int[][] lanes = laneAnglesFor(s, manIdx);
        if (lanes == null || lanes.length == 0) return false;
        int sel = (laneIdx >= 0 && laneIdx < lanes.length) ? laneIdx : 0;
        int[] angles = lanes[sel];
        return (angles != null && angles.length > 0);
    }

    private static boolean shouldEmitLane(RouteGuidance.State s, int manIdx, int laneIdx, short laneDir) {
        int[] dirs = laneDirectionsFor(s, manIdx);
        if (dirs == null || laneIdx < 0 || laneIdx >= dirs.length) return true;
        int raw = dirs[laneIdx];

        if (raw == 1000 && !hasLaneAnglesForLane(s, manIdx, laneIdx)) return false;
        return laneDir != (short)0xFF;
    }

    private static final int[] LANE_DIR_NATIVE_ANGLES = {
        -180, -135, -90, -45, 0, 45, 90, 135, 180
    };
    private static final int[] LANE_DIR_NATIVE_CODES = {
        0x72, 0x60, 0x40, 0x20, 0x00, 0xE0, 0xC0, 0xA0, 0x92
    };

    private static int mapRawLaneValueToDirectionCode(int raw) {
        return mapRawLaneValueToDirectionCode(raw, false, 0);
    }

    private static int mapRawLaneValueToDirectionCode(int raw, boolean excludeOneNativeKey, int keyToExclude) {
        if (raw == 1000) return 0xFF;

        int bestCode = 0;
        int bestDiff = 100000;
        for (int i = 0; i < LANE_DIR_NATIVE_ANGLES.length; i++) {
            int key = LANE_DIR_NATIVE_ANGLES[i];
            if (excludeOneNativeKey && key == keyToExclude) continue;
            int d = raw - key;
            if (d < 0) d = -d;
            if (d < bestDiff) {
                bestDiff = d;
                bestCode = LANE_DIR_NATIVE_CODES[i];
            }
        }
        return bestCode;
    }

    private static short mapLaneDirectionFromSentinel(RouteGuidance.State s, int manIdx, int laneIdx) {
        if (manIdx < 0) return (short) 0xFF;

        int[][] laneAngles = laneAnglesFor(s, manIdx);
        if (laneAngles != null && laneAngles.length > 0) {
            int sel = (laneIdx >= 0 && laneIdx < laneAngles.length) ? laneIdx : 0;
            int[] angles = laneAngles[sel];
            if (angles != null && angles.length > 0) {
                return (short)(mapRawLaneValueToDirectionCode(angles[0]) & 0xFF);
            }
        }
        return (short) 0xFF;
    }

    private static short mapLaneDirection(RouteGuidance.State s, int manIdx, int laneIdx) {
        int[] dirs = laneDirectionsFor(s, manIdx);
        if (dirs == null || laneIdx < 0 || laneIdx >= dirs.length) return (short)0xFF;
        int raw = dirs[laneIdx];

        if (raw == 1000) {
            return mapLaneDirectionFromSentinel(s, manIdx, laneIdx);
        }

        return (short)(mapRawLaneValueToDirectionCode(raw) & 0xFF);
    }

    private static byte[] mapLaneSideStreets(RouteGuidance.State s, int manIdx, int laneIdx, short laneDirection) {
        if (manIdx < 0) return new byte[0];
        int[][] lanes = laneAnglesFor(s, manIdx);
        if (lanes == null || lanes.length == 0) return new byte[0];
        int sel = (laneIdx >= 0 && laneIdx < lanes.length) ? laneIdx : 0;
        int[] angles = lanes[sel];
        if (angles == null || angles.length == 0) return new byte[0];

        int start = 0;
        int[] dirs = laneDirectionsFor(s, manIdx);
        if (dirs != null && laneIdx >= 0 && laneIdx < dirs.length && dirs[laneIdx] == 1000) {
            start = 1;
        }
        if (start >= angles.length) return new byte[0];

        int primaryDir = laneDirection & 0xFF;
        int[] codes = new int[angles.length - start];
        int n = 0;
        for (int i = start; i < angles.length; i++) {
            int code = mapRawLaneValueToDirectionCode(angles[i]) & 0xFF;
            if (code == 0xFF) continue;
            /* Skip angles that map to the same BAP direction as the primary --
             * they're not additional directions, just the same lane. */
            if (code == primaryDir) continue;

            boolean dup = false;
            for (int j = 0; j < n; j++) {
                if (codes[j] == code) {
                    dup = true;
                    break;
                }
            }
            if (!dup) codes[n++] = code;
        }
        if (n == 0) return new byte[0];

        for (int i = 1; i < n; i++) {
            int key = codes[i];
            int j = i - 1;
            while (j >= 0 && codes[j] > key) {
                codes[j + 1] = codes[j];
                j--;
            }
            codes[j + 1] = key;
        }

        byte[] out = new byte[n];
        for (int i = 0; i < n; i++) out[i] = (byte)(codes[i] & 0xFF);
        return out;
    }

    private static byte mapGuidanceInfo(RouteGuidance.State s, int manIdx, int laneIdx) {
        int[] status = laneStatusFor(s, manIdx);
        if (status == null || laneIdx < 0 || laneIdx >= status.length) return 0;
        int v = status[laneIdx];
        if (v < 0 || v > 2) return 0;
        return (byte)v;
    }

    /**
     * Create a single CombiBAPNaviManeuverDescriptor.
     */
    private CombiBAPNaviManeuverDescriptor createDescriptor(int main, int dir, int zLevel, byte[] sideStreets) {
        int mappedMain = main;
        if (main == ManeuverMapper.PREPARE_TURN &&
            (dir == ManeuverMapper.DIR_STRAIGHT || dir == ManeuverMapper.DIR_LEFT)) {
            mappedMain = ManeuverMapper.CHANGE_LANE;
        }

        /* Direction is used as-is - ManeuverMapper.applyDsiNavBapDirectionOverride()
         * already handles per-type coarsening.  No second coarsening layer needed. */
        int mappedDir = dir;

        if (sideStreets == null) sideStreets = new byte[0];
        int mappedZ = (zLevel == 1 || zLevel == 2) ? zLevel : 0;
        return new CombiBAPNaviManeuverDescriptor(mappedMain, mappedDir, mappedZ, sideStreets);
    }

    /* ============================================================
     * Utilities
     * ============================================================ */

    private static long getUtcMillis() {
        return System.currentTimeMillis();
    }

    private static int getHuNavigationTimeFormat() {
        try {
            int v = DateMetric.timeFormat;
            int bap = (v == 11) ? 1 : 0;
            Log.d(TAG, "timeFormat DateMetric.timeFormat=" + v + " bap=" + bap);
            return bap;
        } catch (Throwable t) {
            Log.d(TAG, "timeFormat access failed: " + t.getMessage());
        }
        return 0;
    }

    /**
     * Convert UTC epoch millis to local epoch millis using HU's DST-aware offset.
     * Uses IFrameworkAccess.convertUTCTimeToLocalTime() which internally adds
     * utcOffsetMilliseconds (timezone + DST from UTCOffset DSI callback).
     * Always correct regardless of region or DST status.
     */
    private static long convertUtcToLocalMs(long utcMs) {
        return utcMs;
    }

    private static String hexBytes(byte[] b) {
        if (b == null || b.length == 0) return "[]";
        StringBuffer sb = new StringBuffer("[");
        for (int i = 0; i < b.length; i++) {
            if (i > 0) sb.append(",");
            sb.append("0x");
            sb.append(Integer.toHexString(b[i] & 0xFF));
        }
        sb.append("]");
        return sb.toString();
    }

    private static String limitUtf8(String s, int maxBytes) {
        if (s == null) return "";
        if (maxBytes <= 0) return "";
        try {
            byte[] b = s.getBytes("UTF-8");
            if (b.length <= maxBytes) return s;
            int lo = 0;
            int hi = s.length();
            while (lo < hi) {
                int mid = (lo + hi + 1) / 2;
                byte[] bm = s.substring(0, mid).getBytes("UTF-8");
                if (bm.length <= maxBytes) {
                    lo = mid;
                } else {
                    hi = mid - 1;
                }
            }
            return s.substring(0, lo);
        } catch (Exception e) {
            if (s.length() <= maxBytes) return s;
            return s.substring(0, maxBytes);
        }
    }

    /**
     * Pick a directional Unicode arrow matching the maneuver's BAP direction.
     * Uses the same ManeuverMapper output that drives FctID 23 icons.
     */
    private static String directionArrow(RouteGuidance.State s, int idx) {
        if (idx < 0 || s.mType == null || idx >= s.mType.length) {
            return "";
        }
        int type = s.mType[idx];
        int[] mapped = ManeuverMapper.map(
            type, s.mTurnAngle[idx],
            s.mJunctionType[idx], s.mDrivingSide[idx]);
        int main = mapped[0];
        int dir = mapped[1];

        if (main == ManeuverMapper.NO_SYMBOL || main == ManeuverMapper.NO_INFO) {
            return "";
        }
        if (dir == ManeuverMapper.DIR_LEFT)         return "\u2190"; /* left */
        if (dir == ManeuverMapper.DIR_SLIGHT_LEFT)  return "\u2196"; /* upper-left */
        if (dir == ManeuverMapper.DIR_SHARP_LEFT)   return "\u2199"; /* lower-left */
        if (dir == ManeuverMapper.DIR_RIGHT)        return "\u2192"; /* right */
        if (dir == ManeuverMapper.DIR_SLIGHT_RIGHT) return "\u2197"; /* upper-right */
        if (dir == ManeuverMapper.DIR_SHARP_RIGHT)  return "\u2198"; /* lower-right */
        if (dir == ManeuverMapper.DIR_UTURN)        return "\u21B6"; /* uturn */

        /* Only explicit no-turn/follow-road/straight-ahead source types may
         * produce a straight arrow. Other straight direction codes belong to
         * non-straight main elements (arrival/roundabout/etc.) and must not
         * fabricate an up-arrow text prefix. */
        if (type == ManeuverMapper.MT_NO_TURN
                || type == ManeuverMapper.MT_FOLLOW_ROAD
                || type == ManeuverMapper.MT_STRAIGHT_AHEAD) {
            return "\u2191";
        }
        return "";
    }

    private static String keepLastColonPart(String v) {
        if (v == null) return "";
        int pos = v.lastIndexOf(':');
        if (pos >= 0 && pos + 1 < v.length()) {
            String tail = v.substring(pos + 1).trim();
            if (tail.length() > 0) return tail;
        }
        return v;
    }

    private static int getAuthoritativeHeadManeuverIndex(RouteGuidance.State s) {
        int maxIdx = (s != null && s.mType != null) ? s.mType.length : 0;
        if (s == null || s.maneuverOrder == null || s.maneuverOrder.length == 0) return -1;

        for (int i = 0; i < s.maneuverOrder.length; i++) {
            int idx = s.maneuverOrder[i];
            if (idx >= 0 && idx < maxIdx && ManeuverMapper.isValidType(s.mType[idx])) {
                return idx;
            }
        }
        return -1;
    }

    /**
     * Resolve one presentation maneuver and keep its descriptor/distance coherent.
     *
     * iOS can leave START_ROUTE at the head of the authoritative list while a real
     * actionable maneuver is already available.  We intentionally skip START_ROUTE
     * for the descriptor.  During that interval the top-level 0x5201 distance can
     * still describe the head/current iOS presentation state rather than the real
     * maneuver we selected.  If the selected 0x5202 slot has a usable distance and
     * the live value is absent or implausibly larger, use the slot distance until
     * iOS advances the authoritative head and the normal live countdown takes over.
     */
    private static PresentationManeuver resolvePresentationManeuver(RouteGuidance.State s) {
        int idx = getFirstManeuverIndex(s);
        int type = (idx >= 0 && s != null && s.mType != null && idx < s.mType.length)
            ? s.mType[idx] : -1;
        int liveDist = (s != null) ? s.distManeuverM : -1;
        int slotDist = (idx >= 0 && s != null && s.mDistance != null && idx < s.mDistance.length)
            ? s.mDistance[idx] : -1;
        int resolvedDist = liveDist;
        boolean skippedStartRoute = false;
        boolean usedSlotDistance = false;

        int headIdx = getAuthoritativeHeadManeuverIndex(s);
        if (headIdx >= 0 && idx >= 0 && headIdx != idx
                && s.mType[headIdx] == ManeuverMapper.MT_START_ROUTE) {
            skippedStartRoute = true;
            if (slotDist > 0) {
                long maxPlausibleLive = (long) slotDist + PRESENTATION_DISTANCE_TOLERANCE_M;
                if (liveDist <= 0 || (long) liveDist > maxPlausibleLive) {
                    resolvedDist = slotDist;
                    usedSlotDistance = true;
                }
            }
        }

        return new PresentationManeuver(idx, type, resolvedDist, liveDist, slotDist,
            skippedStartRoute, usedSlotDistance);
    }

    private static int getFirstManeuverIndex(RouteGuidance.State s) {
        int maxIdx = (s.mType != null) ? s.mType.length : 0;
        if (s.maneuverOrder == null || s.maneuverOrder.length == 0) return -1;

        int first = -1;
        int firstOrderPos = -1;
        for (int i = 0; i < s.maneuverOrder.length; i++) {
            int idx = s.maneuverOrder[i];
            if (idx >= 0 && idx < maxIdx && ManeuverMapper.isValidType(s.mType[idx])) {
                first = idx;
                firstOrderPos = i;
                break;
            }
        }
        if (first < 0) return -1;

        /* iOS commonly puts START_ROUTE at the head of the authoritative list.
         * Keep it in the cache/order, but present the first real maneuver when one
         * is already available. */
        if (s.mType[first] == ManeuverMapper.MT_START_ROUTE) {
            for (int i = firstOrderPos + 1; i < s.maneuverOrder.length; i++) {
                int idx = s.maneuverOrder[i];
                if (idx >= 0 && idx < maxIdx
                        && ManeuverMapper.isValidType(s.mType[idx])
                        && s.mType[idx] != ManeuverMapper.MT_START_ROUTE) {
                    return idx;
                }
            }
            return -1;
        }
        return first;
    }

    private static int[] getManeuverIndexList(RouteGuidance.State s) {
        if (s.maneuverOrder != null && s.maneuverOrder.length == 0) {
            return null;
        }
        if (s.maneuverOrder != null && s.maneuverOrder.length > 0) {
            return s.maneuverOrder;
        }
        /*
         * Do not synthesize [0..maneuver_count) when iOS has not published
         * an explicit maneuver_list yet.  During reroute, count often arrives
         * before the new slot payloads; falling back to numeric slot order can
         * briefly expose stale pre-reroute slots to the HUD BAP path.
         */
        return null;
    }

    private static int getActionThresholdM(RouteGuidance.State s, int manIdx, int prepareThresholdM) {
        if (manIdx < 0 || s == null || s.mDistance == null || manIdx >= s.mDistance.length) {
            return -1;
        }
        int policyCap = (prepareThresholdM * ACTION_PERCENT_OF_PREPARE) / 100;
        if (policyCap <= 0) return -1;
        int stepDistance = s.mDistance[manIdx];
        if (stepDistance <= 0 || stepDistance > policyCap) return policyCap;
        return stepDistance;
    }
}
