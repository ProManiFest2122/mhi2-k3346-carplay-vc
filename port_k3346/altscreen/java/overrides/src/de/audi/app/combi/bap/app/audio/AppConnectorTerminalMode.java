/*
 * K3346 Etap 5.3 - okladka CarPlay na VC.
 *
 * Baza: fabryczna klasa K3346 (CFR 0.152 z work/lsd.jar). Fabryczny konektor
 * CarPlay ignoruje obrazek (AppConnectorMedia wysyla okladki przez
 * PictureManager.responseCoverArt, ten nie). Zmiany (port luka-dev
 * AppConnectorTerminalMode, API publiczne bez zmian):
 *  - konstruktor rejestruje konektor w K3346CoverArtBridge;
 *  - k3346PushCoverArt (pakietowa): przy fokusie audio Terminal Mode wysyla
 *    okladke do VC jak AppConnectorMedia (push), typ zrodla 21 jak u Luki,
 *    wpis 0 (fsgHandle Terminal Mode); dostawca K3346CoverArtProvider oddaje ja
 *    na ponowna prosbe VC.
 * Reszta 1:1 z fabryki.
 */
package de.audi.app.combi.bap.app.audio;

import de.audi.app.bap.fw.functiontypes.BAPFunctionArrayFSG;
import de.audi.app.combi.bap.app.audio.list.SourceListHandler;
import de.audi.app.combi.bap.app.kombipictures.IPictureManager;
import de.audi.app.terminalmode.k3346.K3346Diag;
import de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPAudioSource;
import de.audi.atip.interapp.combi.bap.audio.data.PlayPosition;
import de.vw.mib.bap.generated.audiosd.serializer.PlayPosition_Status;
import org.dsi.ifc.global.ResourceLocator;

public class AppConnectorTerminalMode
extends AbstractAppConnectorAudio
implements CombiBAPServiceTerminalMode {
    /* K3346: zrodlo BAP dla okladki CarPlay (Luka: 21 dziala na VC, 37 nie ma w DSI) */
    private static final int K3346_COVER_SOURCE_TYPE = 21;
    private static final long K3346_COVER_ENTRY = 0L;
    private CombiModuleAudio k3346Module;
    private K3346CoverArtProvider k3346Provider;

    protected AppConnectorTerminalMode(CombiModuleAudio combiModuleAudio) {
        super(combiModuleAudio);
        try {
            this.k3346Module = combiModuleAudio;
            this.k3346Provider = new K3346CoverArtProvider(this);
            K3346CoverArtBridge.setConnector(this);
        } catch (Throwable t) {
            /* K3346: bez okladki, reszta fabrycznie */
        }
    }

    protected int getAudioApplication() {
        return 3;
    }

    protected String getAudioApplicationName() {
        return "TerminalMode";
    }

    protected boolean isAudioApplicationInFocus() {
        return ((CombiModuleAudio)this.moduleFsg).getAudioApplicationFocusHandler().isTerminalModeInFocus();
    }

    public void updateActiveSource(int n, int n2, int n3, boolean bl, boolean bl2, int n4) {
        CombiBAPAudioSource combiBAPAudioSource = new CombiBAPAudioSource();
        combiBAPAudioSource.setSourceType(n);
        combiBAPAudioSource.setSlotNumber(n2);
        combiBAPAudioSource.setPartitionNumber(n3);
        CombiBAPAudioListStates combiBAPAudioListStates = new CombiBAPAudioListStates();
        combiBAPAudioListStates.setMediaBrowserListAvailable(bl);
        combiBAPAudioListStates.setPresetListAvailable(bl2);
        combiBAPAudioListStates.setListState(n4);
        this.updateActiveSource(combiBAPAudioSource, combiBAPAudioListStates);
    }

    public void updateSourceListTerminalMode(int[] nArray, CombiBAPAudioSource[][] combiBAPAudioSourceArray) {
        this.logChannel.log(1000000, "[AppConnectorTerminalMode#updateSourceListTerminalMode] called (specified sourceTypes)");
        BAPFunctionArrayFSG bAPFunctionArrayFSG = this.moduleFsg.getBAPFunctionArrayFSG(32);
        ((SourceListHandler)bAPFunctionArrayFSG.getArrayHandler()).updateSources(3, nArray, combiBAPAudioSourceArray);
    }

    public void updatePlayPosition(PlayPosition playPosition) {
        this.logChannel.log(10000000, "[AppConnectorTerminalMode#updatePlayPosition] %1", (Object)playPosition);
        if (this.isAudioApplicationInFocus()) {
            PlayPosition_Status playPosition_Status = new PlayPosition_Status();
            playPosition_Status.timePosition = playPosition.getTimePosition().getTimeInSeconds();
            playPosition_Status.totalPlayTime = playPosition.getTotalPlayTime().getTimeInSeconds();
            playPosition_Status.attributes.variableBitRateActive = playPosition.isVariableBitRate();
            this.moduleFsg.getBAPFunctionPropertyFSG(52).sendStatusIfChanged(playPosition_Status);
        } else {
            this.logChannel.log(10000000, "[AppConnectorTerminalMode#updatePlayPosition] application not in focus; don't send playPosition");
        }
    }

    /* ---------------- K3346 Etap 5.3 ---------------- */

    boolean k3346InFocus() {
        try {
            return this.isAudioApplicationInFocus();
        } catch (Throwable t) {
            return false;
        }
    }

    boolean k3346PushCoverArt(String path, int id) {
        if (path == null || path.length() == 0 || id == 0 || this.k3346Provider == null) return false;
        if (!this.k3346InFocus()) {
            K3346Diag.log("K3346_COVER push skipped - TerminalMode not in audio focus");
            return false;
        }
        IPictureManager pm = this.k3346Module.getPictureManager();
        this.k3346Provider.install(pm);
        pm.setNotification(IPictureManager.NOTIFICATION_COVER_ART);
        ResourceLocator rl = new ResourceLocator(id, path);
        this.k3346Provider.set(K3346_COVER_ENTRY, rl);
        pm.responseCoverArt(K3346_COVER_ENTRY, K3346_COVER_SOURCE_TYPE, rl, true);
        K3346Diag.log("K3346_COVER pushed id=" + id + " " + path);
        return true;
    }

    void k3346ResponseCoverArt(long entry, int sourceType, ResourceLocator rl) {
        this.k3346Module.getPictureManager().responseCoverArt(entry, sourceType, rl, false);
        K3346Diag.log("K3346_COVER re-request entry=" + entry + " source=" + sourceType);
    }

    void k3346ClearCoverArt() {
        if (this.k3346Provider != null) this.k3346Provider.clear();
    }
}
