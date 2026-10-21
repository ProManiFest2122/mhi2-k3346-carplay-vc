/*
 * K3346 Etap 5.2 - rozmowy CarPlay na karcie telefonu VC.
 *
 * Baza: fabryczna klasa K3346 (CFR 0.152 z work/lsd.jar) - identyczna logika
 * jak MU1316 u luka-dev. Jedyna zmiana: usluga CombiBAPServicePhone (HU PHONE
 * 0x28 -> kombi) jest rejestrowana przez K3346GatedPhoneService.wrap(...)
 * zamiast samego AppConnectorPhone. Wrapper przy CarPlay pokazuje rozmowy
 * z iPhone'a, poza CarPlay przepuszcza wszystko 1:1. Blad przy tworzeniu
 * wrappera -> rejestracja fabrycznego obiektu.
 * Etap 5.4: sluchacz akcji VC (CombiBAPServicePhoneListener) jest owijany
 * przez K3346GatedPhoneListener (odbierz/rozlacz przy CarPlay).
 */
package de.audi.app.combi.bap.app.phone;

import de.audi.app.bap.fw.AbstractBAPModuleServiceManager;
import de.audi.app.combi.bap.fw.AbstractCombiModule;
import de.audi.app.terminalmode.k3346.K3346Diag;
import de.audi.atip.activator.AbstractActivator;
import de.audi.atip.interapp.combi.bap.phone.CombiBAPServiceAddressBook;
import de.audi.atip.interapp.combi.bap.phone.CombiBAPServiceAddressBookListener;
import de.audi.atip.interapp.combi.bap.phone.CombiBAPServiceConnectivity;
import de.audi.atip.interapp.combi.bap.phone.CombiBAPServiceMessaging;
import de.audi.atip.interapp.combi.bap.phone.CombiBAPServiceMessagingListener;
import de.audi.atip.interapp.combi.bap.phone.CombiBAPServicePhone;
import de.audi.atip.interapp.combi.bap.phone.CombiBAPServicePhoneListener;
import de.audi.atip.log.LogChannel;
import de.audi.atip.msg.MsgListener;
import de.audi.atip.diag.sw.SwDiagnosisManager;
import org.osgi.framework.BundleContext;
import org.osgi.framework.ServiceReference;
import org.osgi.util.tracker.ServiceTracker;
import org.osgi.util.tracker.ServiceTrackerCustomizer;

public class ServiceManagerPhone
extends AbstractBAPModuleServiceManager {
    private final LogChannel logChannel;

    public ServiceManagerPhone(AbstractCombiModule abstractCombiModule, BundleContext bundleContext) {
        super(abstractCombiModule, bundleContext);
        this.logChannel = abstractCombiModule.getLogChannel();
    }

    public Object addingService(ServiceReference serviceReference) {
        Object object = this.bundleContext.getService(serviceReference);
        if (object instanceof CombiBAPServicePhoneListener) {
            this.logChannel.log(10000000, "[CombiModulePhone.ServiceManagerPhone#addingService] CombiBAPServicePhoneListener found");
            ((AppConnectorPhone)this.module.getAppConnectors().get("Phone")).setAppServiceListener(k3346WrapListener((CombiBAPServicePhoneListener)object));
            return object;
        }
        if (object instanceof CombiBAPServiceAddressBookListener) {
            this.logChannel.log(10000000, "[CombiModulePhone.ServiceManagerPhone#addingService] CombiBAPServiceAddressBookListener found");
            ((AppConnectorAddressBook)this.module.getAppConnectors().get("AddressBook")).setAppServiceListener((CombiBAPServiceAddressBookListener)object);
            return object;
        }
        if (object instanceof CombiBAPServiceMessagingListener) {
            this.logChannel.log(10000000, "[CombiModulePhone.ServiceManagerPhone#addingService] CombiBAPServiceMessagingListener found");
            ((AppConnectorMessaging)this.module.getAppConnectors().get("Messaging")).setAppServiceListener((CombiBAPServiceMessagingListener)object);
            return object;
        }
        this.bundleContext.ungetService(serviceReference);
        return super.addingService(serviceReference);
    }

    public void removedService(ServiceReference serviceReference, Object object) {
        if (object instanceof CombiBAPServicePhoneListener) {
            this.logChannel.log(10000000, "[CombiModulePhone.ServiceManagerPhone#removedService] CombiBAPServicePhoneListener removed");
            ((AppConnectorPhone)this.module.getAppConnectors().get("Phone")).setAppServiceListener(null);
            this.bundleContext.ungetService(serviceReference);
        } else if (object instanceof CombiBAPServiceAddressBookListener) {
            this.logChannel.log(10000000, "[CombiModulePhone.ServiceManagerPhone#removedService] CombiBAPServiceAddressBookListener removed");
            ((AppConnectorAddressBook)this.module.getAppConnectors().get("AddressBook")).setAppServiceListener(null);
            this.bundleContext.ungetService(serviceReference);
        } else if (object instanceof CombiBAPServiceMessagingListener) {
            this.logChannel.log(10000000, "[CombiModulePhone.ServiceManagerPhone#removedService] CombiBAPServiceMessagingListener removed");
            ((AppConnectorMessaging)this.module.getAppConnectors().get("Messaging")).setAppServiceListener(null);
            this.bundleContext.ungetService(serviceReference);
        } else {
            super.removedService(serviceReference, object);
        }
    }

    public void registerServices(AbstractActivator abstractActivator) {
        this.logChannel.log(10000000, "[CombiModulePhone.ServiceManagerPhone#registerServices] Registering AppConnectorPhone as CombiBAPServicePhone");
        abstractActivator.registerService(CombiBAPServicePhone.class.getName(), k3346WrapPhone(this.module.getAppConnectors().get("Phone")), null);
        this.logChannel.log(10000000, "[CombiModulePhone.ServiceManagerPhone#registerServices] Registering AppConnectorAddressBook as CombiBAPServiceAddressBook");
        abstractActivator.registerService(CombiBAPServiceAddressBook.class.getName(), this.module.getAppConnectors().get("AddressBook"), null);
        this.logChannel.log(10000000, "[CombiModulePhone.ServiceManagerPhone#registerServices] Registering AppConnectorMessaging as CombiBAPServiceMessaging");
        abstractActivator.registerService(CombiBAPServiceMessaging.class.getName(), this.module.getAppConnectors().get("Messaging"), null);
        this.logChannel.log(10000000, "[CombiModulePhone.ServiceManagerPhone#registerServices] Registering AppConnectorConnectivity as CombiBAPServiceConnectivity");
        abstractActivator.registerService(CombiBAPServiceConnectivity.class.getName(), this.module.getAppConnectors().get("Connectivity"), null);
        this.logChannel.log(10000000, "[CombiModulePhone.ServiceManagerPhone#registerServices] Registering InitializationManagerPhone as MessageListener");
        abstractActivator.registerService(MsgListener.class.getName(), (Object)this.module.getInitializationManager(), null);
    }

    /* K3346: wrapper rozmow CarPlay; kazdy blad -> obiekt fabryczny bez zmian. */
    private static Object k3346WrapPhone(Object phoneConnector) {
        try {
            Object wrapped = K3346GatedPhoneService.wrap(phoneConnector);
            K3346Diag.log("K3346_PHONE ServiceManagerPhone wrapped=" + (wrapped != phoneConnector));
            return wrapped;
        } catch (Throwable t) {
            K3346Diag.log("K3346_PHONE wrap error " + t + " -> stock");
            return phoneConnector;
        }
    }

    /* K3346 5.4: odbierz/rozlacz z VC przy CarPlay; blad -> sluchacz fabryczny. */
    private static CombiBAPServicePhoneListener k3346WrapListener(CombiBAPServicePhoneListener listener) {
        try {
            CombiBAPServicePhoneListener wrapped = K3346GatedPhoneListener.wrap(listener);
            K3346Diag.log("K3346_PHONE listener wrapped=" + (wrapped != listener));
            return wrapped;
        } catch (Throwable t) {
            K3346Diag.log("K3346_PHONE listener wrap error " + t + " -> stock");
            return listener;
        }
    }

    public void trackServices() {
        String[] stringArray = new String[]{CombiBAPServicePhoneListener.class.getName(), CombiBAPServiceAddressBookListener.class.getName(), SwDiagnosisManager.class.getName()};
        this.serviceTracker = new ServiceTracker(this.bundleContext, stringArray, (ServiceTrackerCustomizer)this);
        this.serviceTracker.open();
    }
}
