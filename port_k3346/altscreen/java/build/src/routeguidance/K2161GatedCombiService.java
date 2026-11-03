package com.luka.carplay.routeguidance;

import com.luka.carplay.framework.Log;

import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationInfo;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationListEntry;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPSemiDynamicRouteInfo;
import de.audi.atip.interapp.combi.bap.navi.data.EtcStatus;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviDestination;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPTMCInfoMessage;

public final class K2161GatedCombiService
    implements CombiBAPServiceNavi {

    private final CombiBAPServiceNavi real;
    private volatile boolean blockRouteGuidance;
    private volatile boolean blockMapPresentation;

    /*
     * VC NavStatus bar over the CarPlay map: altitude, map scale and compass
     * stay hidden for the whole CarPlay session. Street text is not touched.
     * The last stock values are kept and replayed when the bar is released.
     */
    private boolean navStatusHidden;
    private boolean haveAltitude;
    private int altitudeValue;
    private int altitudeUnit;
    private boolean haveCompass;
    private int compassAngle;
    private int compassSymbolic;
    private boolean haveMapScale;
    private int mapScaleAutoZoom;
    private boolean mapScaleAutoZoomActive;
    private int mapScaleValue;
    private int mapScaleUnit;
    private boolean mapScaleIntersection;

    public K2161GatedCombiService(CombiBAPServiceNavi real) {
        if (real == null) {
            throw new IllegalArgumentException("real=null");
        }

        this.real = real;
    }

    private void note(String s) {
        Log.d("RGI-GATE", s);
    }

    private void blocked(String method) {
        note("BLOCK method=" + method);
    }

    public CombiBAPServiceNavi getRealService() {
        return real;
    }

    public void setRouteGuidanceBlocked(boolean blocked) {
        blockRouteGuidance = blocked;

        note(
            "ROUTE_BLOCK=" +
            (blocked ? "1" : "0")
        );
    }

    public boolean isRouteGuidanceBlocked() {
        return blockRouteGuidance;
    }

    public void setMapPresentationBlocked(boolean blocked) {
        blockMapPresentation = false; /* v8j behavior */

        note(
            "MAP_BLOCK=" +
            (false ? "1" : "0") /* v8j MAP_BLOCK=0 */
        );
    }

    public boolean isMapPresentationBlocked() {
        return blockMapPresentation;
    }

    /*
     * Every write is guarded separately: restoreBestEffort() must still hand
     * the raw service back to ClusterService if one of them throws.
     */
    public synchronized void setNavStatusHidden(boolean hidden) {
        if (navStatusHidden == hidden) {
            return;
        }

        navStatusHidden = hidden;

        note(
            "NAV_STATUS_HIDDEN=" +
            (hidden ? "1" : "0")
        );

        if (hidden) {
            try { real.updateAltitude(0x7FFF, 2); } catch (Throwable t) { navStatusFail(t); }
            try { sendHiddenMapScale(); } catch (Throwable t) { navStatusFail(t); }
            try { real.updateCompassInfo(-1, -1); } catch (Throwable t) { navStatusFail(t); }
            return;
        }

        try {
            if (haveAltitude) real.updateAltitude(altitudeValue, altitudeUnit);
        } catch (Throwable t) { navStatusFail(t); }

        try {
            if (haveMapScale) real.updateMapScale(
                mapScaleAutoZoom, mapScaleAutoZoomActive, mapScaleValue,
                mapScaleUnit, mapScaleIntersection);
        } catch (Throwable t) { navStatusFail(t); }

        try {
            if (haveCompass) real.updateCompassInfo(compassAngle, compassSymbolic);
        } catch (Throwable t) { navStatusFail(t); }
    }

    public synchronized boolean isNavStatusHidden() {
        return navStatusHidden;
    }

    private void sendHiddenMapScale() {
        real.updateMapScale(15, false, 0xFFFF, 0xFF, false);
    }

    private void navStatusFail(Throwable t) {
        note("NAV_STATUS_WRITE_FAIL class=" + t.getClass().getName());
    }

    /*
     * ===== NATIVE ROUTE-GUIDANCE WRITES =====
     */

    public void updateRGStatus(int a) {
        if (blockRouteGuidance) {
            blocked("updateRGStatus");
            return;
        }
        real.updateRGStatus(a);
    }

    public void updateRGStatusAndActiveRGType(int a, int b) {
        if (blockRouteGuidance) {
            blocked("updateRGStatusAndActiveRGType");
            return;
        }
        /* K3346 interface has no combined call: stock order is status, then type. */
        real.updateRGStatus(a);
        real.updateActiveRGType(b);
    }

    public void updateActiveRGType(int a) {
        if (blockRouteGuidance) {
            blocked("updateActiveRGType");
            return;
        }
        real.updateActiveRGType(a);
    }

    public void updateActiveRGTypeAndRGStatus(int a, int b) {
        if (blockRouteGuidance) {
            blocked("updateActiveRGTypeAndRGStatus");
            return;
        }
        /* K3346 interface has no combined call. */
        real.updateActiveRGType(a);
        real.updateRGStatus(b);
    }

    public void updateDistanceToNextManeuver(
        int a,
        int b,
        boolean c,
        int d
    ) {
        if (blockRouteGuidance) {
            blocked("updateDistanceToNextManeuver");
            return;
        }
        real.updateDistanceToNextManeuver(a, b, c, d);
    }

    public void updateManeuverDescriptor(
        CombiBAPNaviManeuverDescriptor[] a
    ) {
        if (blockRouteGuidance) {
            blocked("updateManeuverDescriptor");
            return;
        }
        real.updateManeuverDescriptor(a);
    }

    public void updateManeuverDescriptorAndExitView(
        CombiBAPNaviManeuverDescriptor[] a,
        int b,
        int c
    ) {
        if (blockRouteGuidance) {
            blocked("updateManeuverDescriptorAndExitView");
            return;
        }
        real.updateManeuverDescriptorAndExitView(a, b, c);
    }

    public void updateLaneGuidance(
        boolean a,
        CombiBAPNaviLaneGuidanceData[] b
    ) {
        if (blockRouteGuidance) {
            blocked("updateLaneGuidance");
            return;
        }
        real.updateLaneGuidance(a, b);
    }

    public void updateExitView(int a, int b) {
        if (blockRouteGuidance) {
            blocked("updateExitView");
            return;
        }
        real.updateExitView(a, b);
    }

    public void updateManeuverState(int a) {
        if (blockRouteGuidance) {
            blocked("updateManeuverState");
            return;
        }
        real.updateManeuverState(a);
    }

    /*
     * ===== EVERYTHING ELSE PASSES THROUGH =====
     */

    public void showInitializingScreen() {
        real.showInitializingScreen();
    }

    public void hideInitializingScreen() {
        real.hideInitializingScreen();
    }

    public synchronized void updateCompassInfo(int a, int b) {
        compassAngle = a;
        compassSymbolic = b;
        haveCompass = true;
        if (navStatusHidden) {
            return;
        }
        real.updateCompassInfo(a, b);
    }

    /*
     * BAPBridge owns the bar text during a CarPlay route and writes it to the
     * raw service. Leaving the route refreshes the stock text through updateAll().
     */
    public void updateCurrentPositionInfo(String a) {
        if (blockRouteGuidance) {
            blocked("updateCurrentPositionInfo");
            return;
        }
        real.updateCurrentPositionInfo(a);
    }

    public void updateTurnToInfo(String a, String b) {
        if (blockRouteGuidance) {
            blocked("updateTurnToInfo");
            return;
        }
        real.updateTurnToInfo(a, b);
    }

    public void updateDistanceToDestination(
        int a,
        int b,
        boolean c
    ) {
        real.updateDistanceToDestination(a, b, c);
    }

    public void updateTimeToDestination(
        int a,
        int b,
        long c
    ) {
        real.updateTimeToDestination(a, b, c);
    }

    public void updateTMCInfoMessages(
        CombiBAPTMCInfoMessage[] a
    ) {
        real.updateTMCInfoMessages(a);
    }

    public void updateLastDestinationsList(
        CombiBAPDestinationListEntry[] a
    ) {
        real.updateLastDestinationsList(a);
    }

    public void updateFavoriteDestinationsList(
        CombiBAPDestinationListEntry[] a
    ) {
        real.updateFavoriteDestinationsList(a);
    }

    /* K3346 interface addition (not present on K2161): pass through. */
    public void updateHomeAddress(CombiBAPNaviDestination a) {
        real.updateHomeAddress(a);
    }

    /* K3346 interface addition (not present on K2161): pass through. */
    public void updateEtcStatus(EtcStatus a) {
        real.updateEtcStatus(a);
    }

    public void routeGuidanceActDeactResult(int a) {
        real.routeGuidanceActDeactResult(a);
    }

    public void repeatLastNavAnnouncementResult(int a) {
        real.repeatLastNavAnnouncementResult(a);
    }

    public void updateVoiceGuidanceState(int a) {
        real.updateVoiceGuidanceState(a);
    }

    public void updateInfoStates(int a) {
        real.updateInfoStates(a);
    }

    public void updateTrafficBlockIndication(int a) {
        real.updateTrafficBlockIndication(a);
    }

    public void updateMapColor(int a) {
        if (blockMapPresentation) {
            blocked("updateMapColor");
            return;
        }
        real.updateMapColor(a);
    }

    public void updateMapType(int a, int b) {
        if (blockMapPresentation) {
            blocked("updateMapType");
            return;
        }
        real.updateMapType(a, b);
    }

    public void updateSupportedMapTypes(
        boolean a,
        int b
    ) {
        if (blockMapPresentation) {
            blocked("updateSupportedMapTypes");
            return;
        }
        real.updateSupportedMapTypes(a, b);
    }

    public void updateMapView(int a, int b) {
        if (blockMapPresentation) {
            blocked("updateMapView");
            return;
        }
        real.updateMapView(a, b);
    }

    public void updateSupportedMapViews(
        int a,
        int b
    ) {
        if (blockMapPresentation) {
            blocked("updateSupportedMapViews");
            return;
        }
        real.updateSupportedMapViews(a, b);
    }

    public void updateMapVisibility(
        boolean a,
        boolean b
    ) {
        if (blockMapPresentation) {
            blocked("updateMapVisibility");
            return;
        }
        real.updateMapVisibility(a, b);
    }

    public void updateMapOrientation(int a) {
        if (blockMapPresentation) {
            blocked("updateMapOrientation");
            return;
        }
        real.updateMapOrientation(a);
    }

    public synchronized void updateMapScale(
        int a,
        boolean b,
        int c,
        int d,
        boolean e
    ) {
        mapScaleAutoZoom = a;
        mapScaleAutoZoomActive = b;
        mapScaleValue = c;
        mapScaleUnit = d;
        mapScaleIntersection = e;
        haveMapScale = true;
        if (blockMapPresentation) {
            blocked("updateMapScale");
            return;
        }
        /*
         * A steering-wheel MapScale SetGet must always receive a Status, or
         * NAVSD falls into INITIALIZING: resend the hidden value, never drop it.
         */
        if (navStatusHidden) {
            sendHiddenMapScale();
            return;
        }
        real.updateMapScale(a, b, c, d, e);
    }

    public void updateDestinationInfo(
        CombiBAPDestinationInfo a
    ) {
        real.updateDestinationInfo(a);
    }

    public synchronized void updateAltitude(int a, int b) {
        altitudeValue = a;
        altitudeUnit = b;
        haveAltitude = true;
        if (navStatusHidden) {
            return;
        }
        real.updateAltitude(a, b);
    }

    public void updateOnlineNavigationState(
        int a,
        int b,
        int c
    ) {
        real.updateOnlineNavigationState(a, b, c);
    }

    public void updateSemidynamicRouteGuidance(
        CombiBAPSemiDynamicRouteInfo a
    ) {
        real.updateSemidynamicRouteGuidance(a);
    }

    public void poiSearchResult(int a, int b) {
        real.poiSearchResult(a, b);
    }

    public void updatePOIListSize(int a) {
        real.updatePOIListSize(a);
    }

    public void updateFSGSetup(
        int a,
        boolean b
    ) {
        if (blockMapPresentation) {
            blocked("updateFSGSetup");
            return;
        }
        real.updateFSGSetup(a, b);
    }

    public void updateMapPresentation(
        boolean a,
        boolean b,
        boolean c
    ) {
        if (blockMapPresentation) {
            blocked("updateMapPresentation");
            return;
        }
        real.updateMapPresentation(a, b, c);
    }
}
