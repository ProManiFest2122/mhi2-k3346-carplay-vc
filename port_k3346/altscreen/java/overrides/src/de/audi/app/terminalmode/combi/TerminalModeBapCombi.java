/*
 * K3346 Etap 5.3 - okladka CarPlay na VC.
 *
 * Baza: fabryczna klasa K3346 (CFR 0.152 z work/lsd.jar). Zmiany (wzor:
 * luka-dev TerminalModeBapCombi, API publiczne bez zmian):
 *  - urzadzenie CarPlay aktywne -> K3346CoverArt.start() (pobieranie okladki
 *    z demona iAP2), koniec CarPlay -> stop + wyczyszczenie okladki w kombi;
 *  - updateNowPlayingData: listRef 0 i pozycja 0 (jak Luka; domyslne 65535
 *    rozlacza okladke od utworu na VC); jesli jest okladka tego utworu,
 *    najpierw wysylka do VC (K3346CoverArtBridge), potem fabryczne dane utworu;
 *  - nowa okladka po danych utworu -> ponowne wyslanie ostatniego utworu
 *    (tylko przy fokusie audio Terminal Mode, na dyspozytorze Terminal Mode).
 * Kazdy blad dodatkow jest polykany - dane utworu ida jak w fabryce.
 */
package de.audi.app.terminalmode.combi;

import de.audi.app.combi.bap.app.audio.K3346CoverArtBridge;
import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.ITerminalModeComponent;
import de.audi.app.terminalmode.audio.AudioConnectionState;
import de.audi.app.terminalmode.audio.IAudioStateListener;
import de.audi.app.terminalmode.device.IActiveDeviceStateListener;
import de.audi.app.terminalmode.device.TMDevice;
import de.audi.app.terminalmode.events.DefaultEventListener;
import de.audi.app.terminalmode.events.IEventListener;
import de.audi.app.terminalmode.events.TrackDataChangedEvent;
import de.audi.app.terminalmode.events.TrackPlayPositionEvent;
import de.audi.app.terminalmode.k3346.K3346CoverArt;
import de.audi.app.terminalmode.k3346.K3346Diag;
import de.audi.app.terminalmode.osgi.IServiceManager;
import de.audi.app.terminalmode.osgi.IServiceTracker;
import de.audi.atip.interapp.bap.data.TimeStamp;
import de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPCurrentStationInfo;
import de.audi.atip.interapp.combi.bap.audio.data.PlayPosition;
import de.audi.atip.interapp.def.DefaultServiceTrackerCustomizer;
import de.audi.atip.log.LogChannel;
import org.osgi.framework.BundleContext;
import org.osgi.framework.ServiceReference;

public class TerminalModeBapCombi
implements ITerminalModeComponent {
    private static final String LOGCLASS = "TerminalModeBapCombi";
    private final IContext context;
    private final BundleContext bundleContext;
    private final LogChannel lc;
    private final IServiceTracker serviceTracker;
    private final IServiceManager serviceManager;
    private final IEventListener eventListener;
    private final CachingEventListener cachingEventListener;
    private volatile CombiBAPServiceTerminalMode bapService;
    private ActiveDeviceStateListener activeDeviceListener;
    private final IAudioStateListener audioStateListener;
    /* K3346 5.3 */
    private volatile boolean k3346Focus;
    private volatile TrackDataChangedEvent k3346LastTrack;

    public TerminalModeBapCombi(IContext iContext) {
        this.context = iContext;
        this.bundleContext = this.context.getFramework().getBundleCxt();
        this.lc = this.context.getLogger().main();
        this.bapService = new NullCombiBAPService(this.lc);
        this.serviceManager = this.context.getServiceManager();
        this.serviceTracker = this.serviceManager.createServiceTracker(CombiBAPServiceTerminalMode.class, new CombiBapServiceTrackerCustomizer());
        this.eventListener = new EventListener();
        this.cachingEventListener = new CachingEventListener();
        this.activeDeviceListener = new ActiveDeviceStateListener();
        this.audioStateListener = new AudioStateListener();
    }

    public void init() {
        this.serviceTracker.open();
        this.context.getEventBus().registerListener(this.cachingEventListener);
        this.context.getDeviceManager().addActiveDeviceListener(this.activeDeviceListener);
        this.context.getAudioManager().addAudioContextListener(this.audioStateListener);
        try {
            K3346CoverArt.get().setListener(new K3346CombiCoverArt(this));
        } catch (Throwable t) {
            /* K3346: bez okladki */
        }
    }

    public void deinit() {
        try {
            K3346CoverArt.get().setListener(null);
            K3346CoverArt.get().stop();
        } catch (Throwable t) {
            /* K3346 */
        }
        this.serviceTracker.close();
        this.context.getEventBus().unregisterListener(this.eventListener);
        this.context.getEventBus().unregisterListener(this.cachingEventListener);
        this.context.getDeviceManager().removeActiveDeviceListener(this.activeDeviceListener);
        this.context.getAudioManager().removeAudioContextListener(this.audioStateListener);
    }

    /* ---------------- K3346 Etap 5.3 ---------------- */

    /* nowa okladka (watek K3346CoverArt) -> ponowne wyslanie utworu na dyspozytorze */
    void k3346OnCoverArt() {
        this.context.getDispatcher().execute(new K3346CombiCoverArtJob(this));
    }

    void k3346ResendTrack() {
        TrackDataChangedEvent last = this.k3346LastTrack;
        if (this.k3346Focus && last != null) {
            this.eventListener.updateNowPlayingData(last);
        }
    }

    /* okladka biezacego utworu -> VC; wynik: czy dolaczyc obrazek do danych utworu */
    private void k3346AttachCoverArt(TrackDataChangedEvent event, CombiBAPCurrentStationInfo info) {
        try {
            this.k3346LastTrack = event;
            K3346CoverArt cover = K3346CoverArt.get();
            String title = event.getTitle();
            String path = cover.pathFor(title);
            int id = cover.idFor(title);
            if (path != null && id != 0 && K3346CoverArtBridge.push(path, id)) {
                info.setPicture(id, path);
            }
        } catch (Throwable t) {
            K3346Diag.log("K3346_COVER attach error " + t);
        }
    }

    private static void k3346DeviceChanged(TMDevice tMDevice) {
        try {
            if (tMDevice.isCarplayDevice()) {
                if (tMDevice.connectionState().is(TMDevice.ConnectionState.ACTIVATING)
                        || tMDevice.connectionState().is(TMDevice.ConnectionState.ACTIVE)) {
                    K3346CoverArt.get().start();
                }
            } else {
                K3346CoverArt.get().stop();
                K3346CoverArtBridge.clear();
            }
        } catch (Throwable t) {
            K3346Diag.log("K3346_COVER device error " + t);
        }
    }

    private class EventListener
    extends DefaultEventListener {
        private EventListener() {
        }

        public void updateNowPlayingData(TrackDataChangedEvent trackDataChangedEvent) {
            int n = trackDataChangedEvent.getTitle().length() > 0 ? 6 : 0;
            int n2 = trackDataChangedEvent.getArtist().length() > 0 ? 73 : 0;
            int n3 = trackDataChangedEvent.getAlbum().length() > 0 ? 74 : 0;
            CombiBAPCurrentStationInfo combiBAPCurrentStationInfo = new CombiBAPCurrentStationInfo();
            combiBAPCurrentStationInfo.setPrimaryInformation(trackDataChangedEvent.getTitle(), n, 0);
            combiBAPCurrentStationInfo.setSecondaryInformation(trackDataChangedEvent.getArtist(), n2);
            combiBAPCurrentStationInfo.setTertiaryInformation(trackDataChangedEvent.getAlbum(), n3);
            /* K3346 5.3: domyslny listRef K3346 = 65535 (brak pozycji) - VC nie laczy wtedy
             * okladki (push dla wpisu 0) z utworem i o nia nie prosi (test 2026-10-21).
             * Jak luka-dev: listRef 0 ("VC rejects non-zero listRef"), pozycja 0. */
            combiBAPCurrentStationInfo.setListRef(0);
            combiBAPCurrentStationInfo.setListAbsolutePosition(0);
            TerminalModeBapCombi.this.k3346AttachCoverArt(trackDataChangedEvent, combiBAPCurrentStationInfo);
            TerminalModeBapCombi.this.bapService.updateCurrentStation(combiBAPCurrentStationInfo);
            TerminalModeBapCombi.this.bapService.updateActiveInfoState(0);
        }

        public void updatePlayPosition(TrackPlayPositionEvent trackPlayPositionEvent) {
            TerminalModeBapCombi.this.bapService.updatePlayPosition(PlayPosition.builder().setTimePosition(TimeStamp.getInstanceFromSeconds(trackPlayPositionEvent.getPlayTime())).setTotalPlayTime(TimeStamp.getInstanceFromSeconds(trackPlayPositionEvent.getTotalTimeOfTrack())).build());
        }
    }

    private final class AudioStateListener
    implements IAudioStateListener {
        private AudioStateListener() {
        }

        public void audioStateChanged(AudioConnectionState audioConnectionState) {
        }

        public void audioFocusChanged(boolean bl) {
            TerminalModeBapCombi.this.lc.log(10000000, "[%1.audioFocusChanged] %2", (Object)TerminalModeBapCombi.LOGCLASS, (Object)new Boolean(bl));
            TerminalModeBapCombi.this.k3346Focus = bl;
            if (bl) {
                TerminalModeBapCombi.this.context.getEventBus().registerListener(TerminalModeBapCombi.this.eventListener);
                TerminalModeBapCombi.this.cachingEventListener.sendCachedEvents();
            } else {
                TerminalModeBapCombi.this.context.getEventBus().unregisterListener(TerminalModeBapCombi.this.eventListener);
                TerminalModeBapCombi.this.cachingEventListener.clearCachedEvents();
            }
        }
    }

    private class CachingEventListener
    extends DefaultEventListener {
        private final TrackPlayPositionEvent INVALID_TIME = new TrackPlayPositionEvent(-1, 0);
        private final TrackDataChangedEvent INVALID_TRACK = new TrackDataChangedEvent.Builder().build();
        private TrackPlayPositionEvent lastPlayPositionEvent = this.INVALID_TIME;
        private TrackDataChangedEvent lastTrackData = this.INVALID_TRACK;

        private CachingEventListener() {
        }

        public void updateNowPlayingData(TrackDataChangedEvent trackDataChangedEvent) {
            this.lastTrackData = trackDataChangedEvent;
        }

        public void updatePlayPosition(TrackPlayPositionEvent trackPlayPositionEvent) {
            this.lastPlayPositionEvent = trackPlayPositionEvent;
        }

        public void sendCachedEvents() {
            if (!this.INVALID_TIME.equals(this.lastPlayPositionEvent)) {
                TerminalModeBapCombi.this.eventListener.updatePlayPosition(this.lastPlayPositionEvent);
            }
            if (!this.INVALID_TRACK.equals(this.lastTrackData)) {
                TerminalModeBapCombi.this.eventListener.updateNowPlayingData(this.lastTrackData);
            }
        }

        public void clearCachedEvents() {
            this.lastPlayPositionEvent = this.INVALID_TIME;
            this.lastTrackData = this.INVALID_TRACK;
        }
    }

    private final class ActiveDeviceStateListener
    implements IActiveDeviceStateListener {
        private ActiveDeviceStateListener() {
        }

        public void updateActiveDeviceState(TMDevice tMDevice) {
            if (tMDevice.connectionState().is(TMDevice.ConnectionState.ACTIVATING)) {
                TerminalModeBapCombi.this.bapService.updateActiveInfoState(9);
                int n = tMDevice.isCarplayDevice() ? 37 : 39;
                TerminalModeBapCombi.this.bapService.updateActiveSource(n, 0, 0, false, false, 0);
                TerminalModeBapCombi.this.bapService.updateCurrentStation(new CombiBAPCurrentStationInfo());
                TerminalModeBapCombi.this.bapService.updateActiveInfoState(9);
            }
            TerminalModeBapCombi.k3346DeviceChanged(tMDevice);
        }
    }

    private final class CombiBapServiceTrackerCustomizer
    extends DefaultServiceTrackerCustomizer {
        private CombiBapServiceTrackerCustomizer() {
        }

        public Object addingService(ServiceReference serviceReference) {
            CombiBAPServiceTerminalMode combiBAPServiceTerminalMode = (CombiBAPServiceTerminalMode)TerminalModeBapCombi.this.context.getServiceManager().getService(serviceReference);
            if (combiBAPServiceTerminalMode != null) {
                TerminalModeBapCombi.this.lc.log(1000000, "[TerminalModeCombi.addingService] %1", (Object)combiBAPServiceTerminalMode);
                TerminalModeBapCombi.this.bapService = combiBAPServiceTerminalMode;
            }
            return combiBAPServiceTerminalMode;
        }

        public void removedService(ServiceReference serviceReference, Object object) {
            TerminalModeBapCombi.this.lc.log(1000000, "[TerminalModeCombi.removedService] %1", object);
            TerminalModeBapCombi.this.bapService = new NullCombiBAPService(TerminalModeBapCombi.this.lc);
            TerminalModeBapCombi.this.bundleContext.ungetService(serviceReference);
        }
    }
}
