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

    public void updateCompassInfo(int a, int b) {
        real.updateCompassInfo(a, b);
    }

    public void updateCurrentPositionInfo(String a) {
        real.updateCurrentPositionInfo(a);
    }

    public void updateTurnToInfo(String a, String b) {
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

    public void updateMapScale(
        int a,
        boolean b,
        int c,
        int d,
        boolean e
    ) {
        if (blockMapPresentation) {
            blocked("updateMapScale");
            return;
        }
        real.updateMapScale(a, b, c, d, e);
    }

    public void updateDestinationInfo(
        CombiBAPDestinationInfo a
    ) {
        real.updateDestinationInfo(a);
    }

    public void updateAltitude(int a, int b) {
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
