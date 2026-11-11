package de.audi.tghu.connectivity.hmi;

import de.audi.app.terminalmode.k3346.K3346HmiLanguage;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.model.AbstractModelBank;
import de.audi.atip.i18n.ILanguageManager;
import de.audi.atip.i18n.Language;
import de.audi.atip.hmi.model.ChoiceModel;
import de.audi.atip.hmi.model.HMIModel;
import de.audi.atip.hmi.view.HMIView;
import de.audi.atip.log.LogChannel;
import de.audi.atip.log.LogChannelFactory;
import de.audi.atip.model.ConnectivityModelBank;
import de.audi.atip.storage.IStorageAccess;
import de.audi.tghu.connectivity.hmi.evohigh.ConnectivityScreenFactory;
import de.audi.tghu.storage.FlushValuesDict;
import de.esolutions.hmi.widgets.audi.base.AbstractWidget;
import de.esolutions.hmi.widgets.audi.base.eal.IWrappedFont;
import de.esolutions.hmi.widgets.audi.evo.high.widgets.AbstractRendererHigh;
import de.esolutions.hmi.widgets.audi.evo.high.widgets.CompositeRendererHigh;
import de.esolutions.hmi.widgets.audi.evo.widgets.DrawerController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuItemController;
import java.io.File;
import java.io.FileInputStream;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Locale;

/* Wiersz opcji ETA na prawdziwych klasach z lsd.jar, bez auta i bez EAL. */
public final class K3346EtaRowTest {

    private static int ok;
    private static int fail;

    private static void eq(String name, Object want, Object got) {
        if (String.valueOf(want).equals(String.valueOf(got))) {
            ok++;
            System.out.println("  OK   " + name);
        } else {
            fail++;
            System.out.println("  FAIL " + name + " oczekiwano=[" + want + "] jest=[" + got + "]");
        }
    }

    private static String read(File f) throws Exception {
        FileInputStream in = new FileInputStream(f);
        byte[] buf = new byte[16];
        int got = in.read(buf);
        in.close();
        return got <= 0 ? "" : new String(buf, 0, got, "US-ASCII");
    }

    /* Magazyn w pamieci: int[]{namespace, key, value}. */
    private static int[] stored = {0, 0, -1};
    private static boolean storageBroken;
    private static Language language;

    private static IFrameworkAccess framework() {
        final IStorageAccess storage = (IStorageAccess)Proxy.newProxyInstance(
            K3346EtaRowTest.class.getClassLoader(), new Class[] {IStorageAccess.class},
            new InvocationHandler() {
                public Object invoke(Object p, Method m, Object[] a) {
                    if (storageBroken) {
                        throw new IllegalStateException("storage down");
                    }
                    if (m.getName().equals("getInt")) {
                        return Integer.valueOf(stored[2] >= 0
                            && stored[0] == ((Integer)a[0]).intValue()
                            && stored[1] == ((Integer)a[1]).intValue()
                            ? stored[2] : ((Integer)a[2]).intValue());
                    }
                    if (m.getName().equals("setInt")) {
                        stored = new int[] {((Integer)a[0]).intValue(), ((Integer)a[1]).intValue(),
                            ((Integer)a[2]).intValue()};
                    }
                    return null;
                }
            });
        final ILanguageManager languages = (ILanguageManager)Proxy.newProxyInstance(
            K3346EtaRowTest.class.getClassLoader(), new Class[] {ILanguageManager.class},
            new InvocationHandler() {
                public Object invoke(Object p, Method m, Object[] a) {
                    return m.getName().equals("getCurrentLanguage") ? language : null;
                }
            });
        return (IFrameworkAccess)Proxy.newProxyInstance(
            K3346EtaRowTest.class.getClassLoader(), new Class[] {IFrameworkAccess.class},
            new InvocationHandler() {
                public Object invoke(Object p, Method m, Object[] a) {
                    if (m.getName().equals("getStorageMgr")) {
                        return storage;
                    }
                    if (m.getName().equals("getLanguageMgr")) {
                        return languages;
                    }
                    if (m.getName().equals("getClassLoader")) {
                        return K3346EtaRowTest.class.getClassLoader();
                    }
                    if (m.getReturnType() == boolean.class) {
                        return Boolean.FALSE;
                    }
                    return m.getReturnType() == int.class ? (Object)Integer.valueOf(0) : null;
                }
            });
    }

    private static final class QuietChannel extends LogChannel {
        public void log(int a0, java.lang.String a1, java.lang.Object a2, java.lang.Object a3, java.lang.Object a4, java.lang.Object a5, long a6, long a7, long a8, int a9, java.lang.Throwable a10) {
        }

        public void log(int a0, int a1, java.lang.Object a2, java.lang.Object a3, java.lang.Object a4, java.lang.Object a5, long a6, long a7, long a8, int a9, java.lang.Throwable a10) {
        }
    }

    /* Modele prosza o kanal logow juz przy ladowaniu klasy. */
    private static void initLogging() {
        final LogChannel channel = new QuietChannel();
        AbstractModelBank.initLogging((LogChannelFactory)Proxy.newProxyInstance(
            K3346EtaRowTest.class.getClassLoader(), new Class[] {LogChannelFactory.class},
            new InvocationHandler() {
                public Object invoke(Object p, Method m, Object[] a) {
                    return channel;
                }
            }));
    }

    private static DrawerController stockLikeDrawer(MenuController menu, MenuItemController template) {
        DrawerController drawer = new DrawerController(2500001);
        drawer.setViews(new int[] {4188, 2500340},
            new HMIView[][] {{template}, {new MenuItemController()}});
        return drawer;
    }

    private static void testBank() {
        System.out.println("=== bank modeli");
        ConnectivityModelBank stock = new ConnectivityModelBank();
        K3346EtaRow.Bank bank = new K3346EtaRow.Bank();
        int[] ids = bank.getAllModelIds();
        eq("szesc modeli wiecej", stock.getAllModelIds().length + 6, ids.length);
        eq("nasz id eta", K3346EtaRow.MODEL_ID, ids[ids.length - 6]);
        eq("nasz id karty", K3346EtaRow.CARD_MODEL_ID, ids[ids.length - 5]);
        eq("nasz id strzalki", K3346EtaRow.ARROWS_MODEL_ID, ids[ids.length - 4]);
        eq("nasz id mapy", K3346EtaRow.MAP_MODEL_ID, ids[ids.length - 3]);
        eq("nasz id hud", K3346EtaRow.HUD_MODEL_ID, ids[ids.length - 2]);
        eq("nasz id rozmowy", K3346EtaRow.CALL_MODEL_ID, ids[ids.length - 1]);
        eq("id rozmowy po hud", K3346EtaRow.HUD_MODEL_ID + 1, K3346EtaRow.CALL_MODEL_ID);
        eq("model rozmowy to wybor", "true",
            String.valueOf(bank.getModel(K3346EtaRow.CALL_MODEL_ID) instanceof ChoiceModel));
        eq("model hud to wybor", "true",
            String.valueOf(bank.getModel(K3346EtaRow.HUD_MODEL_ID) instanceof ChoiceModel));
        eq("model strzalki to wybor", "true",
            String.valueOf(bank.getModel(K3346EtaRow.ARROWS_MODEL_ID) instanceof ChoiceModel));
        eq("model mapy to wybor", "true",
            String.valueOf(bank.getModel(K3346EtaRow.MAP_MODEL_ID) instanceof ChoiceModel));
        for (int i = 0; i < stock.getAllModelIds().length; i++) {
            if (stock.getAllModelIds()[i] == K3346EtaRow.MODEL_ID
                    || stock.getAllModelIds()[i] == K3346EtaRow.CARD_MODEL_ID
                    || stock.getAllModelIds()[i] == K3346EtaRow.CALL_MODEL_ID) {
                eq("id nie jest zajety", "wolny", "zajety");
            }
        }
        eq("model karty to wybor", "true",
            String.valueOf(bank.getModel(K3346EtaRow.CARD_MODEL_ID) instanceof ChoiceModel));
        eq("dwa rozne modele", "true", String.valueOf(
            bank.getModel(K3346EtaRow.CARD_MODEL_ID) != bank.getModel(K3346EtaRow.MODEL_ID)));
        eq("karta: ten sam obiekt przy drugim pytaniu", "true", String.valueOf(
            bank.getModel(K3346EtaRow.CARD_MODEL_ID) == bank.getModel(K3346EtaRow.CARD_MODEL_ID)));
        HMIModel model = bank.getModel(K3346EtaRow.MODEL_ID);
        eq("model to wybor", "true", String.valueOf(model instanceof ChoiceModel));
        eq("ten sam obiekt przy drugim pytaniu", "true",
            String.valueOf(model == bank.getModel(K3346EtaRow.MODEL_ID)));
        eq("stockowy model dalej dziala", "true",
            String.valueOf(bank.getModel(2500382) instanceof ChoiceModel));
        eq("wszystkie modele sie tworza", ids.length, bank.getModels().length);
        eq("newBank daje nasz", "true", String.valueOf(K3346EtaRow.newBank() instanceof K3346EtaRow.Bank));
    }

    private static void testTexts() {
        System.out.println("=== teksty");
        eq("etykieta pl", "Czas dojazdu CarPlay", K3346EtaRow.text(K3346EtaRow.TEXT_LABEL, true));
        eq("kapsula pl", "Kapsuła", K3346EtaRow.text(K3346EtaRow.TEXT_CAPSULE, true));
        eq("mapa pl", "Mapa", K3346EtaRow.text(K3346EtaRow.TEXT_MAP, true));
        eq("obie pl", "Obie", K3346EtaRow.text(K3346EtaRow.TEXT_BOTH, true));
        eq("etykieta en", "CarPlay arrival time", K3346EtaRow.text(K3346EtaRow.TEXT_LABEL, false));
        eq("kapsula en", "Bar", K3346EtaRow.text(K3346EtaRow.TEXT_CAPSULE, false));
        eq("mapa en", "Map", K3346EtaRow.text(K3346EtaRow.TEXT_MAP, false));
        eq("obie en", "Both", K3346EtaRow.text(K3346EtaRow.TEXT_BOTH, false));
        eq("obcy tekst nie nasz", null, K3346EtaRow.text(2500902, true));
        eq("sasiad z dolu nie nasz", null, K3346EtaRow.text(K3346EtaRow.TEXT_LABEL - 1, true));
        eq("sasiad z gory nie nasz", null, K3346EtaRow.text(K3346EtaRow.TEXT_CARD_NONE + 1, true));
        eq("karta etykieta pl", "Karta manewru CarPlay", K3346EtaRow.text(K3346EtaRow.TEXT_CARD_LABEL, true));
        eq("karta pelna pl", "Pełna", K3346EtaRow.text(K3346EtaRow.TEXT_CARD_FULL, true));
        eq("po srednia nie nasza", null, K3346EtaRow.text(2509906, true));
        eq("po malej nie nasza", null, K3346EtaRow.text(2509907, true));
        eq("karta brak pl", "Brak", K3346EtaRow.text(K3346EtaRow.TEXT_CARD_NONE, true));
        eq("karta etykieta en", "CarPlay maneuver card", K3346EtaRow.text(K3346EtaRow.TEXT_CARD_LABEL, false));
        eq("karta pelna en", "Full", K3346EtaRow.text(K3346EtaRow.TEXT_CARD_FULL, false));
        eq("karta brak en", "Off", K3346EtaRow.text(K3346EtaRow.TEXT_CARD_NONE, false));
        eq("strzalka pl", "Strzałka na VC", K3346EtaRow.text(K3346EtaRow.TEXT_ARROWS_LABEL, true));
        eq("mapa pl", "Mapa na VC", K3346EtaRow.text(K3346EtaRow.TEXT_MAP_LABEL, true));
        eq("hud pl", "HUD CarPlay", K3346EtaRow.text(K3346EtaRow.TEXT_HUD_LABEL, true));
        eq("wl pl", "Wł.", K3346EtaRow.text(K3346EtaRow.TEXT_ON, true));
        eq("wyl pl", "Wył.", K3346EtaRow.text(K3346EtaRow.TEXT_OFF, true));
        eq("strzalka en", "Arrow on VC", K3346EtaRow.text(K3346EtaRow.TEXT_ARROWS_LABEL, false));
        eq("mapa en", "Map on VC", K3346EtaRow.text(K3346EtaRow.TEXT_MAP_LABEL, false));
        eq("hud en", "CarPlay HUD", K3346EtaRow.text(K3346EtaRow.TEXT_HUD_LABEL, false));
        eq("wl en", "On", K3346EtaRow.text(K3346EtaRow.TEXT_ON, false));
        eq("wyl en", "Off", K3346EtaRow.text(K3346EtaRow.TEXT_OFF, false));
        eq("sasiad wl nie nasz", null, K3346EtaRow.text(K3346EtaRow.TEXT_ARROWS_LABEL - 1, true));
        eq("rozmowa pl", "Karta rozmowy na VC", K3346EtaRow.text(K3346EtaRow.TEXT_CALL_LABEL, true));
        eq("przydymiona pl", "Przydymiona", K3346EtaRow.text(K3346EtaRow.TEXT_CALL_SMOKED, true));
        eq("jasna pl", "Jasna", K3346EtaRow.text(K3346EtaRow.TEXT_CALL_LIGHT, true));
        eq("rozmowa en", "Call card on VC", K3346EtaRow.text(K3346EtaRow.TEXT_CALL_LABEL, false));
        eq("przydymiona en", "Smoked", K3346EtaRow.text(K3346EtaRow.TEXT_CALL_SMOKED, false));
        eq("jasna en", "Light", K3346EtaRow.text(K3346EtaRow.TEXT_CALL_LIGHT, false));
        eq("teksty rozmowy po wyl", K3346EtaRow.TEXT_OFF + 1, K3346EtaRow.TEXT_CALL_LABEL);
        eq("sasiad jasnej nie nasz", null, K3346EtaRow.text(K3346EtaRow.TEXT_CALL_LIGHT + 1, true));

        System.out.println("=== jezyki (2.0)");
        eq("kolejnosc kolumn", "en,pl,de,ru,cs,fr,it,es", join(K3346HmiLanguage.LANGS));
        eq("de_DE", "de", K3346HmiLanguage.lang("de_DE"));
        eq("ru_RU", "ru", K3346HmiLanguage.lang("ru_RU"));
        eq("cs_CZ", "cs", K3346HmiLanguage.lang("cs_CZ"));
        eq("fr_CA", "fr", K3346HmiLanguage.lang("fr_CA"));
        eq("it_IT", "it", K3346HmiLanguage.lang("it_IT"));
        eq("es_MX", "es", K3346HmiLanguage.lang("es_MX"));
        eq("pl_PL", "pl", K3346HmiLanguage.lang("pl_PL"));
        eq("PL", "pl", K3346HmiLanguage.lang("PL"));
        eq("de-AT", "de", K3346HmiLanguage.lang("de-AT"));
        eq(" De_ch ", "de", K3346HmiLanguage.lang(" De_ch "));
        eq("en_GB", "en", K3346HmiLanguage.lang("en_GB"));
        eq("uk_UA: w MMI go nie ma", "en", K3346HmiLanguage.lang("uk_UA"));
        eq("nl_NL: brak tekstow", "en", K3346HmiLanguage.lang("nl_NL"));
        eq("pusty", "en", K3346HmiLanguage.lang(""));
        eq("null jezyk", "en", K3346HmiLanguage.lang(null));
        eq("deu: nie de", "en", K3346HmiLanguage.lang("deu"));
        eq("bez frameworku: en", "en", K3346HmiLanguage.lang());
        eq("indeks ru", 3, K3346HmiLanguage.index("ru"));
        eq("indeks obcy", 0, K3346HmiLanguage.index("xx"));
        eq("pick: brak kolumny = en", "A", K3346HmiLanguage.pick(new String[] {"A", "B"}, "es"));
        eq("pick: pusta kolumna = en", "A", K3346HmiLanguage.pick(new String[] {"A", null, "C"}, "pl"));
        eq("pick: de", "C", K3346HmiLanguage.pick(new String[] {"A", null, "C"}, "de"));

        int[] ids = {K3346EtaRow.TEXT_LABEL, K3346EtaRow.TEXT_CAPSULE, K3346EtaRow.TEXT_MAP, K3346EtaRow.TEXT_BOTH,
            K3346EtaRow.TEXT_CARD_LABEL, K3346EtaRow.TEXT_CARD_FULL, K3346EtaRow.TEXT_CARD_NONE,
            K3346EtaRow.TEXT_ARROWS_LABEL, K3346EtaRow.TEXT_MAP_LABEL, K3346EtaRow.TEXT_HUD_LABEL,
            K3346EtaRow.TEXT_ON, K3346EtaRow.TEXT_OFF, K3346EtaRow.TEXT_CALL_LABEL,
            K3346EtaRow.TEXT_CALL_SMOKED, K3346EtaRow.TEXT_CALL_LIGHT};
        String[] langs = K3346HmiLanguage.LANGS;
        int empty = 0, sameAsEn = 0;
        for (int i = 0; i < ids.length; i++) {
            String en = K3346EtaRow.text(ids[i], "en");
            for (int l = 0; l < langs.length; l++) {
                String t = K3346EtaRow.text(ids[i], langs[l]);
                if (t == null || t.trim().length() == 0) {
                    empty++;
                }
                if (l > 0 && t != null && t.equals(en) && ids[i] != K3346EtaRow.TEXT_HUD_LABEL) {
                    sameAsEn++;
                    System.out.println("  (jak po angielsku: " + ids[i] + " " + langs[l] + ")");
                }
            }
            eq("obcy jezyk = en " + ids[i], en, K3346EtaRow.text(ids[i], "xx"));
            eq("boolean pl = pl " + ids[i], K3346EtaRow.text(ids[i], "pl"), K3346EtaRow.text(ids[i], true));
            eq("boolean en = en " + ids[i], en, K3346EtaRow.text(ids[i], false));
        }
        eq("zaden tekst pusty", 0, empty);
        int sameOnOff = 0, sameCall = 0, sameCard = 0, sameEta = 0;
        for (int l = 0; l < langs.length; l++) {
            if (K3346EtaRow.text(K3346EtaRow.TEXT_ON, langs[l]).equals(K3346EtaRow.text(K3346EtaRow.TEXT_OFF, langs[l]))) {
                sameOnOff++;
            }
            if (K3346EtaRow.text(K3346EtaRow.TEXT_CALL_SMOKED, langs[l]).equals(K3346EtaRow.text(K3346EtaRow.TEXT_CALL_LIGHT, langs[l]))) {
                sameCall++;
            }
            if (K3346EtaRow.text(K3346EtaRow.TEXT_CARD_FULL, langs[l]).equals(K3346EtaRow.text(K3346EtaRow.TEXT_CARD_NONE, langs[l]))) {
                sameCard++;
            }
            String c = K3346EtaRow.text(K3346EtaRow.TEXT_CAPSULE, langs[l]);
            String m = K3346EtaRow.text(K3346EtaRow.TEXT_MAP, langs[l]);
            String b = K3346EtaRow.text(K3346EtaRow.TEXT_BOTH, langs[l]);
            if (c.equals(m) || c.equals(b) || m.equals(b)) {
                sameEta++;
            }
        }
        eq("Wl. rozne od Wyl. w kazdym jezyku", 0, sameOnOff);
        eq("Przydymiona rozna od Jasnej", 0, sameCall);
        eq("Pelna rozna od Brak", 0, sameCard);
        eq("Kapsula/Mapa/Obie rozne", 0, sameEta);
        eq("it Wl.", "Att.", K3346EtaRow.text(K3346EtaRow.TEXT_ON, "it"));
        eq("de Wyl.", "Aus", K3346EtaRow.text(K3346EtaRow.TEXT_OFF, "de"));
        /* the same word happens: "Mapa" (pl, cs, es) is not English; only exact copies of
         * the English text count, and only these two are expected */
        eq("tylko Barra/Mapa itp. bez kopii angielskich", 0, sameAsEn);
        eq("de etykieta", "CarPlay Ankunftszeit", K3346EtaRow.text(K3346EtaRow.TEXT_LABEL, "de"));
        eq("ru etykieta", "Время прибытия CarPlay", K3346EtaRow.text(K3346EtaRow.TEXT_LABEL, "ru"));
        eq("cs wyl", "Vyp.", K3346EtaRow.text(K3346EtaRow.TEXT_OFF, "cs"));
        eq("fr rozmowa", "Carte d'appel sur VC", K3346EtaRow.text(K3346EtaRow.TEXT_CALL_LABEL, "fr"));
        eq("it jasna", "Chiara", K3346EtaRow.text(K3346EtaRow.TEXT_CALL_LIGHT, "it"));
        eq("es brak", "Ninguna", K3346EtaRow.text(K3346EtaRow.TEXT_CARD_NONE, "es"));
        eq("de nie nasz id", null, K3346EtaRow.text(K3346EtaRow.TEXT_CALL_LIGHT + 1, "de"));
        eq("pl bez zmian", "Czas dojazdu CarPlay", K3346EtaRow.text(K3346EtaRow.TEXT_LABEL, "pl"));
    }

    private static String join(String[] a) {
        StringBuffer b = new StringBuffer();
        for (int i = 0; i < a.length; i++) {
            if (i > 0) {
                b.append(',');
            }
            b.append(a[i]);
        }
        return b.toString();
    }

    private static void testMode(File mode) throws Exception {
        System.out.println("=== tryb");
        K3346EtaRow.ETA.file = mode.getPath();
        eq("0 zostaje", 0, K3346EtaRow.ETA.clamp(0));
        eq("2 zostaje", 2, K3346EtaRow.ETA.clamp(2));
        eq("3 na domyslny", 0, K3346EtaRow.ETA.clamp(3));
        eq("-1 na domyslny", 0, K3346EtaRow.ETA.clamp(-1));

        stored = new int[] {0, 0, -1};
        K3346EtaRow.boot(framework());
        eq("start bez zapisu: domyslny w pliku", "0\n", read(mode));
        boolean flush = false;
        for (int i = 0; i < FlushValuesDict.values.length; i++) {
            flush |= FlushValuesDict.values[i][0] == K3346EtaRow.NAMESPACE
                && FlushValuesDict.values[i][1] == K3346EtaRow.KEY;
        }
        eq("klucz zapisywany od razu na flash", "true", String.valueOf(flush));
        eq("flush dla klucza", "true",
            String.valueOf(FlushValuesDict.isFlushRequired(K3346EtaRow.NAMESPACE, K3346EtaRow.KEY)));
        int rows = FlushValuesDict.values.length;
        K3346EtaRow.boot(framework());
        eq("drugi start nie dokleja klucza", rows, FlushValuesDict.values.length);

        K3346EtaRow.ETA.select(1);
        eq("wybor: plik", "1\n", read(mode));
        eq("wybor: namespace", K3346EtaRow.NAMESPACE, stored[0]);
        eq("wybor: klucz", K3346EtaRow.KEY, stored[1]);
        eq("wybor: wartosc", 1, stored[2]);
        K3346EtaRow.ETA.select(9);
        eq("zla wartosc: plik domyslny", "0\n", read(mode));
        eq("zla wartosc: magazyn domyslny", 0, stored[2]);

        stored = new int[] {K3346EtaRow.NAMESPACE, K3346EtaRow.KEY, 2};
        K3346EtaRow.boot(framework());
        eq("po restarcie wartosc z magazynu", "2\n", read(mode));

        stored = new int[] {K3346EtaRow.NAMESPACE, K3346EtaRow.KEY, 7};
        K3346EtaRow.boot(framework());
        eq("smieci w magazynie: domyslny", "0\n", read(mode));

        K3346EtaRow.Bank bank = new K3346EtaRow.Bank();
        stored = new int[] {K3346EtaRow.NAMESPACE, K3346EtaRow.KEY, 2};
        K3346EtaRow.boot(framework());
        ChoiceModel model = (ChoiceModel)bank.getModel(K3346EtaRow.MODEL_ID);
        eq("model pokazuje zapis", 2, model.getValue());
        stored = new int[] {K3346EtaRow.NAMESPACE, K3346EtaRow.KEY, 1};
        K3346EtaRow.boot(framework());
        eq("fabryka po modelu poprawia wartosc", 1, model.getValue());

        storageBroken = true;
        K3346EtaRow.ETA.select(2);
        eq("magazyn padl: plik i tak", "2\n", read(mode));
        K3346EtaRow.boot(framework());
        eq("magazyn padl przy starcie: domyslny", "0\n", read(mode));
        storageBroken = false;

        System.out.println("=== fabryka");
        K3346EtaRow.ETA.writeFile(0);
        stored = new int[] {K3346EtaRow.NAMESPACE, K3346EtaRow.KEY, 1};
        language = new Language(1, "en_GB", "English", "en", 0, Locale.ENGLISH);
        ConnectivityScreenFactory factory = K3346EtaRow.newFactory(framework());
        eq("fabryka nasza", "true", String.valueOf(factory instanceof K3346EtaRow.Factory));
        eq("fabryka uruchamia start", "1\n", read(mode));
        eq("tekst nasz", "CarPlay arrival time", factory.getText(K3346EtaRow.TEXT_LABEL));
        eq("tekst alternatywny nasz", "Map", factory.getAltText(K3346EtaRow.TEXT_MAP));
        eq("tekst przez widok", "Both", factory.getText(K3346EtaRow.TEXT_BOTH, 1));
        eq("tekst stockowy nie nasz", "", factory.getText(2500902));
        language = new Language(2, "pl_PL", "Polski", "pl", 0, new Locale("pl", "PL"));
        eq("po zmianie jezyka po polsku", "Czas dojazdu CarPlay", factory.getText(K3346EtaRow.TEXT_LABEL));
        eq("kapsula po polsku", "Kapsuła", factory.getAltText(K3346EtaRow.TEXT_CAPSULE));
        language = null;
        eq("bez jezyka po angielsku", "Both", factory.getText(K3346EtaRow.TEXT_BOTH));
        language = new Language(1, "en_GB", "English", "en", 0, Locale.ENGLISH);

        K3346EtaRow.ETA.file = new File(mode.getParentFile(), "brak/plik").getPath();
        eq("zla sciezka", "false", String.valueOf(K3346EtaRow.ETA.writeFile(1)));
        K3346EtaRow.ETA.select(1);
        K3346EtaRow.ETA.file = mode.getPath();
    }

    private static void testCard(File eta, File card) throws Exception {
        System.out.println("=== karta");
        K3346EtaRow.ETA.file = eta.getPath();
        K3346EtaRow.CARD.file = card.getPath();
        eq("karta: 1 zostaje", 1, K3346EtaRow.CARD.clamp(1));
        eq("karta: stara mala na pelna", 0, K3346EtaRow.CARD.clamp(2));
        eq("karta: stary brak na pelna", 0, K3346EtaRow.CARD.clamp(3));
        eq("eta: 3 dalej na domyslny", 0, K3346EtaRow.ETA.clamp(3));

        stored = new int[] {K3346EtaRow.NAMESPACE, K3346EtaRow.CARD_KEY, 1};
        K3346EtaRow.boot(framework());
        eq("start: karta z magazynu", "1\n", read(card));
        eq("start: eta nie bierze klucza karty", "0\n", read(eta));
        eq("flush dla karty", "true", String.valueOf(
            FlushValuesDict.isFlushRequired(K3346EtaRow.NAMESPACE, K3346EtaRow.CARD_KEY)));
        eq("flush dla eta dalej", "true", String.valueOf(
            FlushValuesDict.isFlushRequired(K3346EtaRow.NAMESPACE, K3346EtaRow.KEY)));

        K3346EtaRow.CARD.select(1);
        eq("wybor karty: plik", "1\n", read(card));
        eq("wybor karty: klucz", K3346EtaRow.CARD_KEY, stored[1]);
        eq("wybor karty: wartosc", 1, stored[2]);
        eq("wybor karty: eta bez zmian", "0\n", read(eta));
        K3346EtaRow.ETA.select(1);
        eq("wybor eta: karta bez zmian", "1\n", read(card));

        K3346EtaRow.Bank bank = new K3346EtaRow.Bank();
        stored = new int[] {K3346EtaRow.NAMESPACE, K3346EtaRow.CARD_KEY, 1};
        ChoiceModel model = (ChoiceModel)bank.getModel(K3346EtaRow.CARD_MODEL_ID);
        eq("model karty pokazuje zapis", 1, model.getValue());
        stored = new int[] {K3346EtaRow.NAMESPACE, K3346EtaRow.CARD_KEY, 0};
        K3346EtaRow.boot(framework());
        eq("start poprawia model karty", 0, model.getValue());

        language = new Language(2, "pl_PL", "Polski", "pl", 0, new Locale("pl", "PL"));
        ConnectivityScreenFactory factory = K3346EtaRow.newFactory(framework());
        eq("fabryka: karta po polsku", "Karta manewru CarPlay", factory.getText(K3346EtaRow.TEXT_CARD_LABEL));
        eq("fabryka: wartosc alternatywna", "Brak", factory.getAltText(K3346EtaRow.TEXT_CARD_NONE));
        language = new Language(1, "en_GB", "English", "en", 0, Locale.ENGLISH);
        K3346EtaRow.CARD.file = new File(card.getParentFile(), "brak/plik").getPath();
        eq("karta: zla sciezka", "false", String.valueOf(K3346EtaRow.CARD.writeFile(1)));
        K3346EtaRow.CARD.file = card.getPath();
    }

    private static void testOnOff(File dir, K3346EtaRow.Row row, int key, String name) throws Exception {
        File f = new File(dir, name);
        String oldFile = row.file;
        row.file = f.getPath();
        eq(name + ": 0 zostaje", 0, row.clamp(0));
        eq(name + ": 1 zostaje", 1, row.clamp(1));
        eq(name + ": 2 na wl", 0, row.clamp(2));
        row.select(1);
        eq(name + ": plik wyl", "1\n", read(f));
        eq(name + ": klucz", key, stored[1]);
        eq(name + ": wartosc", 1, stored[2]);
        row.select(0);
        eq(name + ": plik wl", "0\n", read(f));
        stored = new int[] {K3346EtaRow.NAMESPACE, key, 1};
        K3346EtaRow.boot(framework());
        eq(name + ": start z magazynu", "1\n", read(f));
        eq(name + ": flush", "true", String.valueOf(FlushValuesDict.isFlushRequired(K3346EtaRow.NAMESPACE, key)));
        stored = new int[] {K3346EtaRow.NAMESPACE, key, 5};
        K3346EtaRow.boot(framework());
        eq(name + ": smieci na wl", "0\n", read(f));
        f.delete();
        row.file = oldFile;
    }

    /* Karta rozmowy: trzy wartosci, Przydymiona domyslnie, Wyl. jako ostatnia. */
    private static void testCall(File dir) throws Exception {
        System.out.println("=== karta rozmowy");
        K3346EtaRow.Row row = K3346EtaRow.CALL;
        int key = K3346EtaRow.CALL_KEY;
        File f = new File(dir, "c");
        String oldFile = row.file;
        row.file = f.getPath();
        eq("klucz po hud", K3346EtaRow.HUD_KEY + 1, key);
        eq("0 zostaje", 0, row.clamp(0));
        eq("1 zostaje", 1, row.clamp(1));
        eq("2 zostaje", 2, row.clamp(2));
        eq("3 na przydymiona", 0, row.clamp(3));
        eq("-1 na przydymiona", 0, row.clamp(-1));
        row.select(1);
        eq("plik jasna", "1\n", read(f));
        eq("klucz", key, stored[1]);
        eq("wartosc", 1, stored[2]);
        row.select(2);
        eq("plik wyl", "2\n", read(f));
        row.select(0);
        eq("plik przydymiona", "0\n", read(f));
        stored = new int[] {K3346EtaRow.NAMESPACE, key, 2};
        K3346EtaRow.boot(framework());
        eq("start z magazynu: wyl", "2\n", read(f));
        eq("flush", "true", String.valueOf(FlushValuesDict.isFlushRequired(K3346EtaRow.NAMESPACE, key)));
        stored = new int[] {K3346EtaRow.NAMESPACE, key, 7};
        K3346EtaRow.boot(framework());
        eq("smieci na przydymiona", "0\n", read(f));
        f.delete();
        row.file = oldFile;
    }

    /* Oba wiersze w jednej szufladzie; awaria jednego nie rusza drugiego. */
    private static void testBothRows(IWrappedFont font) {
        System.out.println("=== dwa wiersze");
        MenuController menu = new MenuController();
        MenuItemController template = new MenuItemController();
        CompositeRendererHigh renderer = new CompositeRendererHigh(template);
        template.setRenderer(renderer);
        template.setBitmaps(new int[] {127, 127});
        renderer.setFonts(new IWrappedFont[] {font, font, font});
        menu.add(template);
        DrawerController drawer = stockLikeDrawer(menu, template);
        int before = menu.getChildren().size();

        K3346EtaRow.ETA.addTo(drawer);
        K3346EtaRow.CARD.addTo(drawer);

        int[] ids = drawer.getViewIDs();
        eq("id widokow +2", 4, ids.length);
        eq("kolejnosc: eta przed karta", K3346EtaRow.MODEL_ID, ids[2]);
        eq("kolejnosc: karta", K3346EtaRow.CARD_MODEL_ID, ids[3]);
        eq("oba w menu", before + 2, menu.getChildren().size());
        MenuItemController eta = (MenuItemController)drawer.findViews(K3346EtaRow.MODEL_ID)[0];
        MenuItemController card = (MenuItemController)drawer.findViews(K3346EtaRow.CARD_MODEL_ID)[0];
        eq("karta: model", K3346EtaRow.CARD_MODEL_ID, card.getModelID());
        eq("karta: etykieta", K3346EtaRow.TEXT_CARD_LABEL, card.getLabelId());
        eq("karta: typ", MenuItemController.TYPE_COMBO_BOX, card.getType());
        eq("karta: dwie pozycje", 2, card.getTextIds().length);
        eq("karta: pierwsza pelna", K3346EtaRow.TEXT_CARD_FULL, card.getTextIds()[0]);
        eq("karta: druga brak", K3346EtaRow.TEXT_CARD_NONE, card.getTextIds()[1]);
        eq("eta dalej trzy pozycje", 3, eta.getTextIds().length);
        eq("rozne klucze fokusu", "true",
            String.valueOf(eta.getInternalID() != card.getInternalID()));

        K3346EtaRow.ARROWS.addTo(drawer);
        K3346EtaRow.MAP.addTo(drawer);
        K3346EtaRow.HUD.addTo(drawer);
        eq("wszystkie w menu", before + 5, menu.getChildren().size());
        MenuItemController arrows = (MenuItemController)drawer.findViews(K3346EtaRow.ARROWS_MODEL_ID)[0];
        MenuItemController map = (MenuItemController)drawer.findViews(K3346EtaRow.MAP_MODEL_ID)[0];
        MenuItemController hud = (MenuItemController)drawer.findViews(K3346EtaRow.HUD_MODEL_ID)[0];
        eq("strzalka: etykieta", K3346EtaRow.TEXT_ARROWS_LABEL, arrows.getLabelId());
        eq("mapa: etykieta", K3346EtaRow.TEXT_MAP_LABEL, map.getLabelId());
        eq("hud: etykieta", K3346EtaRow.TEXT_HUD_LABEL, hud.getLabelId());
        eq("wl na pierwszym miejscu", K3346EtaRow.TEXT_ON, hud.getTextIds()[0]);
        eq("wyl na drugim", K3346EtaRow.TEXT_OFF, hud.getTextIds()[1]);
        eq("dwie pozycje", 2, arrows.getTextIds().length);
        eq("fokus strzalka inny", "true", String.valueOf(arrows.getInternalID() != card.getInternalID()));
        eq("fokus mapa inny", "true", String.valueOf(map.getInternalID() != arrows.getInternalID()));
        eq("fokus hud inny", "true", String.valueOf(hud.getInternalID() != map.getInternalID()));

        K3346EtaRow.CALL.addTo(drawer);
        eq("szesc w menu", before + 6, menu.getChildren().size());
        MenuItemController call = (MenuItemController)drawer.findViews(K3346EtaRow.CALL_MODEL_ID)[0];
        eq("rozmowa: model", K3346EtaRow.CALL_MODEL_ID, call.getModelID());
        eq("rozmowa: etykieta", K3346EtaRow.TEXT_CALL_LABEL, call.getLabelId());
        eq("rozmowa: typ", MenuItemController.TYPE_COMBO_BOX, call.getType());
        eq("rozmowa: trzy pozycje", 3, call.getTextIds().length);
        eq("rozmowa: pierwsza przydymiona", K3346EtaRow.TEXT_CALL_SMOKED, call.getTextIds()[0]);
        eq("rozmowa: druga jasna", K3346EtaRow.TEXT_CALL_LIGHT, call.getTextIds()[1]);
        eq("rozmowa: trzecia wyl", K3346EtaRow.TEXT_OFF, call.getTextIds()[2]);
        eq("rozmowa: ostatnia w szufladzie", K3346EtaRow.CALL_MODEL_ID,
            drawer.getViewIDs()[drawer.getViewIDs().length - 1]);
        int[] focus = {eta.getInternalID(), card.getInternalID(), arrows.getInternalID(),
            map.getInternalID(), hud.getInternalID()};
        for (int i = 0; i < focus.length; i++) {
            eq("fokus rozmowy inny " + i, "true", String.valueOf(call.getInternalID() != focus[i]));
        }
    }

    private static void testRow() {
        System.out.println("=== wiersz");
        AbstractWidget.framework = framework();   /* czytany przy ladowaniu klas menu */
        MenuController menu = new MenuController();
        MenuItemController template = new MenuItemController();
        CompositeRendererHigh renderer = new CompositeRendererHigh(template);
        template.setRenderer(renderer);
        template.setBitmaps(new int[] {127, 127});
        IWrappedFont font = (IWrappedFont)Proxy.newProxyInstance(
            K3346EtaRowTest.class.getClassLoader(), new Class[] {IWrappedFont.class},
            new InvocationHandler() {
                public Object invoke(Object p, Method m, Object[] a) {
                    return null;
                }
            });
        IWrappedFont[] fonts = new IWrappedFont[] {font, font, font};
        renderer.setFonts(fonts);
        menu.add(template);
        DrawerController drawer = stockLikeDrawer(menu, template);
        int before = menu.getChildren().size();

        K3346EtaRow.ETA.addTo(drawer);

        int[] ids = drawer.getViewIDs();
        eq("id widokow +1", 3, ids.length);
        eq("kolejnosc: szablon", 4188, ids[0]);
        eq("kolejnosc: stock", 2500340, ids[1]);
        eq("kolejnosc: nasz", K3346EtaRow.MODEL_ID, ids[2]);
        HMIView[] ours = drawer.findViews(K3346EtaRow.MODEL_ID);
        eq("widok znaleziony", 1, ours == null ? 0 : ours.length);
        MenuItemController item = (MenuItemController)ours[0];
        eq("model", K3346EtaRow.MODEL_ID, item.getModelID());
        eq("etykieta", K3346EtaRow.TEXT_LABEL, item.getLabelId());
        eq("typ lista wyboru", MenuItemController.TYPE_COMBO_BOX, item.getType());
        eq("trzy pozycje", 3, item.getTextIds().length);
        eq("pierwsza kapsula", K3346EtaRow.TEXT_CAPSULE, item.getTextIds()[0]);
        eq("druga mapa", K3346EtaRow.TEXT_MAP, item.getTextIds()[1]);
        eq("trzecia obie", K3346EtaRow.TEXT_BOTH, item.getTextIds()[2]);
        eq("grafika ze wzoru", "true",
            String.valueOf(item.getBitmapIndices() == template.getBitmapIndices()
                || java.util.Arrays.equals(item.getBitmapIndices(), template.getBitmapIndices())));
        eq("czcionki ze wzoru", "true", String.valueOf(
            ((AbstractRendererHigh)item.getRenderer()).getFonts() != null
            && ((AbstractRendererHigh)item.getRenderer()).getFonts()[0] == font));
        eq("wiersz w menu", before + 1, menu.getChildren().size());
        eq("stockowe widoki bez zmian", template, drawer.findViews(4188)[0]);

        System.out.println("=== wiersz: bez wzoru menu zostaje");
        MenuController menu2 = new MenuController();
        MenuItemController orphan = new MenuItemController();
        CompositeRendererHigh orphanRenderer = new CompositeRendererHigh(orphan);
        orphan.setRenderer(orphanRenderer);
        orphanRenderer.setFonts(fonts);
        DrawerController noParent = stockLikeDrawer(menu2, orphan);
        int[] untouched = noParent.getViewIDs();
        boolean threw = false;
        try {
            K3346EtaRow.ETA.addTo(noParent);
        } catch (Throwable t) {
            threw = true;
        }
        eq("blad zgloszony", "true", String.valueOf(threw));
        eq("widoki nietkniete", "true", String.valueOf(untouched == noParent.getViewIDs()));

        DrawerController empty = new DrawerController(2500001);
        empty.setViews(new int[] {2500340}, new HMIView[][] {{new MenuItemController()}});
        threw = false;
        try {
            K3346EtaRow.ETA.addTo(empty);
        } catch (Throwable t) {
            threw = true;
        }
        eq("brak wzoru: blad", "true", String.valueOf(threw));
        eq("brak wzoru: widoki nietkniete", 1, empty.getViewIDs().length);

        testBothRows(font);
    }

    public static void main(String[] argv) throws Exception {
        initLogging();
        File dir = File.createTempFile("etarow", "");
        dir.delete();
        dir.mkdir();
        File mode = new File(dir, "mode");
        File card = new File(dir, "card");

        eq("plik eta dla kodu natywnego", "/tmp/k3346-eta-mode", K3346EtaRow.ETA.file);
        eq("plik karty dla kodu natywnego", "/tmp/k3346-card-mode", K3346EtaRow.CARD.file);
        eq("plik strzalki dla nakladki", "/tmp/k3346-arrows-mode", K3346EtaRow.ARROWS.file);
        eq("plik mapy dla skryptu", "/tmp/k3346-map-mode", K3346EtaRow.MAP.file);
        eq("plik hud dla bundla", "/tmp/k3346-hud-mode", K3346EtaRow.HUD.file);
        eq("plik karty rozmowy dla nakladki", "/tmp/k3346-callcard-mode", K3346EtaRow.CALL.file);
        /* boot() w testach pisze pliki kazdego wiersza: nie do /tmp prawdziwego auta */
        K3346EtaRow.ARROWS.file = new File(dir, "a").getPath();
        K3346EtaRow.MAP.file = new File(dir, "m").getPath();
        K3346EtaRow.HUD.file = new File(dir, "h").getPath();
        K3346EtaRow.CALL.file = new File(dir, "r").getPath();
        testTexts();
        testBank();
        testMode(mode);
        testCard(mode, card);
        System.out.println("=== wlacz/wylacz");
        testOnOff(dir, K3346EtaRow.ARROWS, K3346EtaRow.ARROWS_KEY, "strzalka");
        testOnOff(dir, K3346EtaRow.MAP, K3346EtaRow.MAP_KEY, "mapa");
        testOnOff(dir, K3346EtaRow.HUD, K3346EtaRow.HUD_KEY, "hud");
        testCall(dir);
        testRow();

        mode.delete();
        card.delete();
        new File(dir, "a").delete();
        new File(dir, "m").delete();
        new File(dir, "h").delete();
        new File(dir, "r").delete();
        dir.delete();
        System.out.println("WYNIK_ETA_ROW: " + ok + " OK, " + fail + " FAIL");
        System.exit(fail == 0 ? 0 : 1);
    }
}
