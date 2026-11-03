package de.audi.app.terminalmode.k3346;

import de.audi.atip.base.IFrameworkAccess;

/*
 * Which language the MMI speaks, for the few texts that are ours. Set once a
 * screen factory has the framework; until then, and on any failure, English.
 */
public final class K3346HmiLanguage {
    private static volatile IFrameworkAccess framework;

    private K3346HmiLanguage() {
    }

    public static void setFramework(IFrameworkAccess fw) {
        framework = fw;
    }

    public static boolean isPolish() {
        try {
            return isPolish(framework.getLanguageMgr()
                .getCurrentLanguage("LANG_COMPONENT_HMI").getHmiCode());
        } catch (Throwable t) {
            return false;
        }
    }

    public static boolean isPolish(String hmiCode) {
        return hmiCode != null && hmiCode.toLowerCase().startsWith("pl");
    }
}
