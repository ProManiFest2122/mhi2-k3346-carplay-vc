package de.audi.app.terminalmode.k3346;

import de.audi.atip.base.IFrameworkAccess;

/*
 * Which language the MMI speaks, for the few texts that are ours (the MMI options in
 * the Connection manager and the call card on the cluster). Set once a screen factory
 * has the framework; until then, and on any failure, English.
 *
 * The HMI code is like "de_DE" or "fr_CA"; its language part picks the column. The
 * MMI of K3346 has no Ukrainian (no uk_UA in its HMI), so it is not here.
 */
public final class K3346HmiLanguage {
    /* The order of the columns in every table of ours: English first, the fallback. */
    public static final String[] LANGS = {"en", "pl", "de", "ru", "cs", "fr", "it", "es"};

    private static volatile IFrameworkAccess framework;

    private K3346HmiLanguage() {
    }

    public static void setFramework(IFrameworkAccess fw) {
        framework = fw;
    }

    public static String lang() {
        try {
            return lang(framework.getLanguageMgr()
                .getCurrentLanguage("LANG_COMPONENT_HMI").getHmiCode());
        } catch (Throwable t) {
            return LANGS[0];
        }
    }

    /* "de_DE" -> "de"; a language we have no texts for -> "en". */
    public static String lang(String hmiCode) {
        if (hmiCode == null) {
            return LANGS[0];
        }
        String code = hmiCode.trim().toLowerCase();
        int cut = code.indexOf('_');
        int dash = code.indexOf('-');
        if (cut < 0 || (dash >= 0 && dash < cut)) {
            cut = dash;
        }
        if (cut >= 0) {
            code = code.substring(0, cut);
        }
        for (int i = 0; i < LANGS.length; i++) {
            if (LANGS[i].equals(code)) {
                return LANGS[i];
            }
        }
        return LANGS[0];
    }

    /* The column of a language in LANGS; 0 (English) for anything else. */
    public static int index(String lang) {
        for (int i = 0; i < LANGS.length; i++) {
            if (LANGS[i].equals(lang)) {
                return i;
            }
        }
        return 0;
    }

    /* One text from a row of a table in the order of LANGS; English when the row has
     * nothing for that language. */
    public static String pick(String[] row, String lang) {
        int i = index(lang);
        return i < row.length && row[i] != null ? row[i] : row[0];
    }
}
