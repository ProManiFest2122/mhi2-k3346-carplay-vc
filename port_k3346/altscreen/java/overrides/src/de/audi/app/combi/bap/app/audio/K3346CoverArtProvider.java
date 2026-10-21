/*
 * K3346 Etap 5.3: dostawca okladki CarPlay dla ponownych prosb VC.
 *
 * Port luka-dev AppConnectorTerminalMode$CoverArtProvider + CoverArtProviderMux:
 * AbstractPictureManager ma jedno miejsce na dostawce okladek (typ 0), zajete
 * przez fabryczny AppConnectorMedia. Nie nadpisujemy go - multiplekser oddaje
 * prosbe CarPlay, gdy mamy okladke i Terminal Mode ma fokus audio, inaczej
 * fabrycznemu dostawcy mediow. Instalowany leniwie przy pierwszej okladce (wtedy
 * Media juz sie zarejestrowalo). Java 1.4.
 */
package de.audi.app.combi.bap.app.audio;

import de.audi.app.combi.bap.app.kombipictures.IPictureManager;
import de.audi.app.combi.bap.app.kombipictures.IPictureProvider;
import de.audi.app.terminalmode.k3346.K3346Diag;
import java.lang.reflect.Field;
import java.util.Map;
import org.dsi.ifc.global.ResourceLocator;

final class K3346CoverArtProvider implements IPictureProvider {
    private static final int PICTURE_TYPE_COVER_ART = 0;

    private final AppConnectorTerminalMode owner;
    private IPictureProvider stock;
    private boolean installed;
    private long entryId = -1;
    private ResourceLocator picture;

    K3346CoverArtProvider(AppConnectorTerminalMode owner) {
        this.owner = owner;
    }

    synchronized void set(long id, ResourceLocator rl) {
        entryId = id;
        picture = rl;
    }

    synchronized void clear() {
        entryId = -1;
        picture = null;
    }

    private synchronized ResourceLocator pictureFor(long id) {
        return picture != null && id == entryId && owner.k3346InFocus() ? picture : null;
    }

    /** Wstawia multiplekser w miejsce dostawcy typu 0 (raz). */
    synchronized void install(IPictureManager manager) {
        if (installed || manager == null) return;
        installed = true;
        IPictureProvider current = findProvider(manager, PICTURE_TYPE_COVER_ART);
        if (current == this) return;
        stock = current;
        manager.registerPictureProvider(PICTURE_TYPE_COVER_ART, this);
        K3346Diag.log("K3346_COVER provider mux installed stock=" + (stock != null));
    }

    private static IPictureProvider findProvider(IPictureManager manager, int type) {
        Class c = manager.getClass();
        while (c != null) {
            try {
                Field f = c.getDeclaredField("pictureProviders");
                f.setAccessible(true);
                Object value = f.get(manager);
                if (!(value instanceof Map)) return null;
                Object p = ((Map)value).get(new Integer(type));
                return p instanceof IPictureProvider ? (IPictureProvider)p : null;
            } catch (NoSuchFieldException e) {
                c = c.getSuperclass();
            } catch (Throwable t) {
                return null;
            }
        }
        return null;
    }

    public void requestPicture(long id) {
        requestPicture(id, 255);
    }

    public void requestPicture(long id, int sourceType) {
        ResourceLocator rl = pictureFor(id);
        if (rl != null) {
            owner.k3346ResponseCoverArt(id, sourceType, rl);
        } else if (stock != null) {
            stock.requestPicture(id, sourceType);
        }
    }
}
