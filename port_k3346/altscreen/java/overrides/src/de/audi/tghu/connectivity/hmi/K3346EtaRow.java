package de.audi.tghu.connectivity.hmi;

import de.audi.app.terminalmode.k3346.K3346Diag;
import de.audi.app.terminalmode.k3346.K3346HmiLanguage;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.model.ChoiceModel;
import de.audi.atip.hmi.model.HMIModel;
import de.audi.atip.hmi.model.listener.DefaultChoiceListener;
import de.audi.atip.hmi.view.HMIView;
import de.audi.atip.hmi.view.IDrawerController;
import de.audi.atip.model.ConnectivityModelBank;
import de.audi.tghu.connectivity.hmi.evohigh.ConnectivityScreenFactory;
import de.audi.tghu.storage.FlushValuesDict;
import de.esolutions.hmi.widgets.audi.base.AbstractWidget;
import de.esolutions.hmi.widgets.audi.evo.high.widgets.AbstractRendererHigh;
import de.esolutions.hmi.widgets.audi.evo.high.widgets.CompositeRendererHigh;
import de.esolutions.hmi.widgets.audi.evo.widgets.DrawerController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuItemController;
import java.io.FileOutputStream;
import java.io.IOException;

/*
 * Our rows in the stock Connectivity options, next to "About Apple CarPlay":
 * CarPlay arrival time and, for the big gauges, the maneuver card.  Pattern:
 * UpmixToneUi by luka-dev, with two differences: the texts come from getText()
 * here, so lang_data.zip stays stock, and the rows go into the option drawer
 * instead of a screen.
 *
 * Each value lives in the stock storage and is mirrored to a file, which
 * dio_manager (native/eta_mode.h) and BAPBridge read.  Every failure leaves the
 * stock bank, the stock factory or the stock drawer: an option is not worth an
 * HMI that does not start.
 */
final class K3346EtaRow {

    static final int MODEL_ID = 2500392;   /* first free index: the stock bank has 0..391 */
    static final int CARD_MODEL_ID = 2500393;
    static final int ARROWS_MODEL_ID = 2500394;
    static final int MAP_MODEL_ID = 2500395;
    static final int HUD_MODEL_ID = 2500396;
    static final int CALL_MODEL_ID = 2500397;
    private static final int TEMPLATE_MODEL = 4188;   /* "Recognition period", a combo box */

    /* Past the stock table (976 entries). */
    static final int TEXT_LABEL = 2509900;
    static final int TEXT_CAPSULE = 2509901;
    static final int TEXT_MAP = 2509902;
    static final int TEXT_BOTH = 2509903;
    static final int TEXT_CARD_LABEL = 2509904;
    static final int TEXT_CARD_FULL = 2509905;
    static final int TEXT_CARD_NONE = 2509908;
    static final int TEXT_ARROWS_LABEL = 2509920;
    static final int TEXT_MAP_LABEL = 2509921;
    static final int TEXT_HUD_LABEL = 2509922;
    static final int TEXT_ON = 2509923;
    static final int TEXT_OFF = 2509924;
    static final int TEXT_CALL_LABEL = 2509925;
    static final int TEXT_CALL_SMOKED = 2509926;
    static final int TEXT_CALL_LIGHT = 2509927;

    static final int NAMESPACE = 1008;   /* IStorageAccess.Namespace.TERMINALMODE */
    static final int KEY = 1000;         /* stock keys here: 1..3 */
    static final int CARD_KEY = 1001;
    static final int ARROWS_KEY = 1002;
    static final int MAP_KEY = 1003;
    static final int HUD_KEY = 1004;
    static final int CALL_KEY = 1005;
    static final int MODE_DEFAULT = 0;

    /* The internal ids are focus-restore keys, unique in the menu. */
    static final Row ETA = new Row(MODEL_ID, 2509910, KEY, TEXT_LABEL,
        new int[] {TEXT_CAPSULE, TEXT_MAP, TEXT_BOTH}, "/tmp/k3346-eta-mode");
    static final Row CARD = new Row(CARD_MODEL_ID, 2509911, CARD_KEY, TEXT_CARD_LABEL,
        new int[] {TEXT_CARD_FULL, TEXT_CARD_NONE},
        "/tmp/k3346-card-mode");
    private static final int[] ON_OFF = {TEXT_ON, TEXT_OFF};
    static final Row ARROWS = new Row(ARROWS_MODEL_ID, 2509912, ARROWS_KEY, TEXT_ARROWS_LABEL,
        ON_OFF, "/tmp/k3346-arrows-mode");
    static final Row MAP = new Row(MAP_MODEL_ID, 2509913, MAP_KEY, TEXT_MAP_LABEL,
        ON_OFF, "/tmp/k3346-map-mode");
    static final Row HUD = new Row(HUD_MODEL_ID, 2509914, HUD_KEY, TEXT_HUD_LABEL,
        ON_OFF, "/tmp/k3346-hud-mode");
    /* The call card on the map (overlay/call_card.h): smoked or light glass, or none.
     * Off also gives the big gauges their ETA box back during a call. */
    static final Row CALL = new Row(CALL_MODEL_ID, 2509915, CALL_KEY, TEXT_CALL_LABEL,
        new int[] {TEXT_CALL_SMOKED, TEXT_CALL_LIGHT, TEXT_OFF}, "/tmp/k3346-callcard-mode");
    private static final Row[] ROWS = {ETA, CARD, ARROWS, MAP, HUD, CALL};

    private static volatile IFrameworkAccess framework;

    private K3346EtaRow() {
    }

    static ConnectivityModelBank newBank() {
        try {
            return new Bank();
        } catch (Throwable t) {
            K3346Diag.log("K3346_ETA_ROW bank_failed " + t);
            return new ConnectivityModelBank();
        }
    }

    static ConnectivityScreenFactory newFactory(IFrameworkAccess fw) {
        try {
            boot(fw);
            return new Factory(fw);
        } catch (Throwable t) {
            K3346Diag.log("K3346_ETA_ROW factory_failed " + t);
            return new ConnectivityScreenFactory(fw);
        }
    }

    static void boot(IFrameworkAccess fw) {
        framework = fw;
        K3346HmiLanguage.setFramework(fw);
        for (int i = 0; i < ROWS.length; i++) {
            ROWS[i].boot();
        }
        K3346Diag.log("K3346_ETA_ROW boot mode=" + ETA.readStored() + " card=" + CARD.readStored()
            + " arrows=" + ARROWS.readStored() + " map=" + MAP.readStored() + " hud=" + HUD.readStored()
            + " call=" + CALL.readStored());
    }

    /* null for an id that is not ours. */
    static String text(int id, boolean polish) {
        return text(id, polish ? "pl" : "en");
    }

    /* null for an id that is not ours; lang as K3346HmiLanguage.lang(). */
    static String text(int id, String lang) {
        String[] row = row(id);
        return row == null ? null : K3346HmiLanguage.pick(row, lang);
    }

    /* Short: the MMI cuts a long label.  The columns of K3346HmiLanguage.LANGS:
     * en, pl, de, ru, cs, fr, it, es. */
    private static String[] row(int id) {
        switch (id) {
            case TEXT_LABEL:
                return new String[] {"CarPlay arrival time", "Czas dojazdu CarPlay", "CarPlay Ankunftszeit",
                    "Время прибытия CarPlay", "Čas příjezdu CarPlay", "Heure d'arrivée CarPlay",
                    "Orario di arrivo CarPlay", "Hora de llegada CarPlay"};
            case TEXT_CAPSULE:
                return new String[] {"Bar", "Kapsuła", "Leiste", "Панель", "Lišta", "Barre", "Barra", "Barra"};
            case TEXT_MAP:
                return new String[] {"Map", "Mapa", "Karte", "Карта", "Mapa", "Carte", "Mappa", "Mapa"};
            case TEXT_BOTH:
                return new String[] {"Both", "Obie", "Beide", "Оба", "Obojí", "Les deux", "Entrambi", "Ambos"};
            case TEXT_CARD_LABEL:
                return new String[] {"CarPlay maneuver card", "Karta manewru CarPlay", "CarPlay Manöverkarte",
                    "Карточка манёвра CarPlay", "Karta manévru CarPlay", "Carte de manœuvre CarPlay",
                    "Scheda manovra CarPlay", "Tarjeta de maniobra CarPlay"};
            case TEXT_CARD_FULL:
                return new String[] {"Full", "Pełna", "Voll", "Полная", "Plná", "Complète", "Completa", "Completa"};
            case TEXT_CARD_NONE:
                return new String[] {"Off", "Brak", "Keine", "Нет", "Žádná", "Aucune", "Nessuna", "Ninguna"};
            case TEXT_ARROWS_LABEL:
                return new String[] {"Arrow on VC", "Strzałka na VC", "Pfeil im VC", "Стрелка на VC",
                    "Šipka na VC", "Flèche sur VC", "Freccia su VC", "Flecha en VC"};
            case TEXT_MAP_LABEL:
                return new String[] {"Map on VC", "Mapa na VC", "Karte im VC", "Карта на VC", "Mapa na VC",
                    "Carte sur VC", "Mappa su VC", "Mapa en VC"};
            case TEXT_HUD_LABEL:
                return new String[] {"CarPlay HUD", "HUD CarPlay", "CarPlay HUD", "HUD CarPlay", "HUD CarPlay",
                    "HUD CarPlay", "HUD CarPlay", "HUD CarPlay"};
            case TEXT_ON:
                return new String[] {"On", "Wł.", "Ein", "Вкл.", "Zap.", "Act.", "Att.", "Act."};
            case TEXT_OFF:
                return new String[] {"Off", "Wył.", "Aus", "Выкл.", "Vyp.", "Désact.", "Disatt.", "Desact."};
            case TEXT_CALL_LABEL:
                return new String[] {"Call card on VC", "Karta rozmowy na VC", "Anrufkarte im VC",
                    "Карточка звонка на VC", "Karta hovoru na VC", "Carte d'appel sur VC",
                    "Scheda chiamata su VC", "Tarjeta de llamada en VC"};
            case TEXT_CALL_SMOKED:
                return new String[] {"Smoked", "Przydymiona", "Getönt", "Тёмная", "Tmavá", "Fumée", "Fumé", "Ahumada"};
            case TEXT_CALL_LIGHT:
                return new String[] {"Light", "Jasna", "Hell", "Светлая", "Světlá", "Claire", "Chiara", "Clara"};
            default:
                return null;
        }
    }

    /* One option: its model, its stored value and the file that mirrors it. */
    static final class Row {
        final int modelId;
        final int key;
        private final int index;
        private final int internalId;
        private final int labelId;
        private final int[] valueIds;
        volatile String file;
        private volatile ChoiceModel model;

        Row(int modelId, int internalId, int key, int labelId, int[] valueIds, String file) {
            this.modelId = modelId;
            this.index = modelId % 100000;
            this.internalId = internalId;
            this.key = key;
            this.labelId = labelId;
            this.valueIds = valueIds;
            this.file = file;
        }

        void boot() {
            registerFlush();
            int mode = readStored();
            writeFile(mode);
            ChoiceModel m = model;
            if (m != null) {
                m.setValue(mode);
            }
        }

        /* The stock list says which keys reach flash at once; without it a hard
         * restart could lose the choice. */
        private void registerFlush() {
            int[][] old = FlushValuesDict.values;
            for (int i = 0; i < old.length; i++) {
                if (old[i][0] == NAMESPACE && old[i][1] == key) {
                    return;
                }
            }
            int[][] grown = new int[old.length + 1][];
            System.arraycopy(old, 0, grown, 0, old.length);
            grown[old.length] = new int[] {NAMESPACE, key, 0};
            FlushValuesDict.values = grown;
        }

        int clamp(int mode) {
            return mode >= 0 && mode < valueIds.length ? mode : MODE_DEFAULT;
        }

        int readStored() {
            try {
                IFrameworkAccess fw = framework;
                return fw == null ? MODE_DEFAULT
                    : clamp(fw.getStorageMgr().getInt(NAMESPACE, key, MODE_DEFAULT));
            } catch (Throwable t) {
                return MODE_DEFAULT;   /* never written yet, or storage not up */
            }
        }

        boolean writeFile(int mode) {
            FileOutputStream out = null;
            try {
                out = new FileOutputStream(file);
                out.write(new byte[] {(byte)('0' + mode), '\n'});
                return true;
            } catch (IOException e) {
                return false;
            } finally {
                if (out != null) {
                    try {
                        out.close();
                    } catch (IOException e) {
                        /* nothing more to do with this file */
                    }
                }
            }
        }

        void select(int value) {
            int mode = clamp(value);
            try {
                IFrameworkAccess fw = framework;
                if (fw != null) {
                    fw.getStorageMgr().setInt(NAMESPACE, key, mode);
                }
            } catch (Throwable t) {
                K3346Diag.log("K3346_ETA_ROW store_failed key=" + key + " " + t);
            }
            boolean mirrored = writeFile(mode);
            K3346Diag.log("K3346_ETA_ROW selected key=" + key + " mode=" + mode + " mirrored=" + mirrored);
        }

        /* Views first: if anything here throws, the menu is untouched. */
        void addTo(DrawerController drawer) {
            HMIView[] refViews = drawer.findViews(TEMPLATE_MODEL);
            MenuItemController ref = (MenuItemController)refViews[0];
            AbstractWidget parent = ref.getParent();
            if (!(parent instanceof MenuController)) {
                throw new IllegalStateException("template row has no menu");
            }

            MenuItemController item = new MenuItemController();
            CompositeRendererHigh renderer = new CompositeRendererHigh(item);
            item.setRenderer(renderer);
            item.setBitmaps(ref.getBitmapIndices());
            renderer.setKzbIDs(new int[] {25, 25, 25, 25, 14});
            renderer.setFonts(((AbstractRendererHigh)ref.getRenderer()).getFonts());
            item.setModelID(modelId);
            item.setTextIds(valueIds);
            item.setLabelId(labelId);
            item.setGlassplateInsetsBottom(6);
            item.setGlassplateInsetsTop(7);
            item.setInternalID(internalId);
            item.setType(MenuItemController.TYPE_COMBO_BOX);

            int[] ids = drawer.getViewIDs();
            int[] newIds = new int[ids.length + 1];
            HMIView[][] newViews = new HMIView[ids.length + 1][];
            int out = 0;
            boolean placed = false;
            for (int i = 0; i < ids.length; i++) {
                /* findViews() searches this list in order of id */
                if (!placed && ids[i] > modelId) {
                    newIds[out] = modelId;
                    newViews[out++] = new HMIView[] {item};
                    placed = true;
                }
                newIds[out] = ids[i];
                newViews[out++] = drawer.findViews(ids[i]);
            }
            if (!placed) {
                newIds[out] = modelId;
                newViews[out] = new HMIView[] {item};
            }
            drawer.setViews(newIds, newViews);
            ((MenuController)parent).add(item);
        }

        ChoiceModel newModel() {
            final ChoiceModel m = new ChoiceModel(modelId);
            m.setChoiceListener(new DefaultChoiceListener() {
                public void itemSelected(int id, int value, int unused, int terminal) {
                    try {
                        m.setValue(value);   /* the combo already shows it */
                        select(value);
                    } catch (Throwable t) {
                        K3346Diag.log("K3346_ETA_ROW select_failed " + t);
                    }
                }
            });
            m.setValue(readStored());
            model = m;
            return m;
        }
    }

    static final class Bank extends ConnectivityModelBank {
        Bank() {
            HMIModel[] grown = new HMIModel[ROWS[ROWS.length - 1].index + 1];
            System.arraycopy(this.models, 0, grown, 0, this.models.length);
            this.models = grown;
            int[] ids = new int[this.modelIDs.length + ROWS.length];
            System.arraycopy(this.modelIDs, 0, ids, 0, this.modelIDs.length);
            for (int i = 0; i < ROWS.length; i++) {
                ids[this.modelIDs.length + i] = ROWS[i].modelId;
            }
            this.modelIDs = ids;
        }

        protected synchronized void createModel(int index) {
            for (int i = 0; i < ROWS.length; i++) {
                if (index == ROWS[i].index) {
                    if (this.models[index] == null) {
                        this.models[index] = ROWS[i].newModel();
                    }
                    return;
                }
            }
            super.createModel(index);
        }
    }

    static final class Factory extends ConnectivityScreenFactory {
        Factory(IFrameworkAccess fw) {
            super(fw);
        }

        public String getText(int id) {
            String ours = id >= TEXT_LABEL ? text(id, K3346HmiLanguage.lang()) : null;
            return ours != null ? ours : super.getText(id);
        }

        public String getAltText(int id) {
            String ours = id >= TEXT_LABEL ? text(id, K3346HmiLanguage.lang()) : null;
            return ours != null ? ours : super.getAltText(id);
        }

        public IDrawerController[] getOptionDrawers(int terminal) {
            IDrawerController[] drawers = super.getOptionDrawers(terminal);
            for (int i = 0; i < ROWS.length; i++) {
                try {
                    ROWS[i].addTo((DrawerController)drawers[0]);
                    K3346Diag.log("K3346_ETA_ROW row_added model=" + ROWS[i].modelId + " terminal=" + terminal);
                } catch (Throwable t) {
                    K3346Diag.log("K3346_ETA_ROW row_failed model=" + ROWS[i].modelId + " " + t);
                }
            }
            return drawers;
        }
    }
}
