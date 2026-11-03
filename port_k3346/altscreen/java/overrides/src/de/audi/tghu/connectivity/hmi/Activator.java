package de.audi.tghu.connectivity.hmi;

import de.audi.atip.activator.AbstractHMIActivator;
import de.audi.atip.hmi.HMIConditionBank;
import de.audi.atip.hmi.view.AbstractScreenFactory;
import de.audi.tghu.connectivity.hmi.evohigh.ConnectivityConditionBank;
import de.audi.tghu.connectivity.hmi.evohigh.ConnectivityScreenFactory;

/* K3346: stock K3346 Activator, except that bank and factory come from
 * K3346EtaRow, which falls back to the stock ones on any failure. */
public class Activator extends AbstractHMIActivator {
    private ConnectivityScreenFactory instance = null;
    private ConnectivityConditionBank conditionBank;

    public Activator() {
        super(25, "Connectivity", System.getProperty("variant.skin", "EvoHigh"), K3346EtaRow.newBank());
    }

    protected synchronized AbstractScreenFactory getScreenFactory() {
        if (this.instance == null) {
            this.instance = K3346EtaRow.newFactory(this.getFramework());
        }
        return this.instance;
    }

    public HMIConditionBank getConditionBank() {
        if (this.conditionBank == null) {
            this.conditionBank = new ConnectivityConditionBank((ConnectivityScreenFactory)this.getScreenFactory());
        }
        return this.conditionBank;
    }
}
