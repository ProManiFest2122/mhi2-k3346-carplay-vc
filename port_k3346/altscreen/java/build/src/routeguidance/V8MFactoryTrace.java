package com.luka.carplay.routeguidance;

import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import de.audi.tghu.navi.app.cluster.ClusterService;
import de.audi.atip.mmicombi.IViewSizeManager;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;
import com.luka.carplay.framework.Log;

/** Passive factory cache and view-size diagnostics. Never writes to factory objects. */
final class V8MFactoryTrace {
    /* Shared mount is accessible from the K2161 Java process AND the SD collector. */
    private static final File LIVE = new File("/net/rcc/dev/shmem/v8m-rgi-factory.log");
    private static final File OLD = new File("/net/rcc/dev/shmem/v8m-rgi-factory.log.1");
    private static final long MAX_BYTES = 262144L;
    private static long lastDistanceSnapshotMs = 0;
    private static boolean firstLine = true;
    private static boolean sinkErrorLogged = false;

    private V8MFactoryTrace() {}

    private static synchronized void line(String detail) {
        /* Proven main Java logger is an independent collection fallback. */
        Log.i("V8M_FACTORY", detail);
        try {
            if (LIVE.length() > MAX_BYTES) {
                if (OLD.exists()) OLD.delete();
                LIVE.renameTo(OLD);
            }
            FileOutputStream stream = new FileOutputStream(LIVE, true);
            try {
                if (firstLine) {
                    stream.write("V8M_FACTORY_SOURCE_LOGGER_INITIALIZED\n".getBytes("UTF-8"));
                    firstLine = false;
                }
                stream.write((System.currentTimeMillis()+" V8M_FACTORY "+detail+"\n").getBytes("UTF-8"));
            } finally { stream.close(); }
        } catch (Throwable ignored) {
            if (!sinkErrorLogged) {
                sinkErrorLogged = true;
                Log.w("V8M_FACTORY", "SEPARATE_SHARED_TRACE_UNAVAILABLE; main Java log is fallback");
            }
        }
    }

    static void descriptor(String step, CombiBAPNaviManeuverDescriptor[] descriptors,
                           int variant, int exitId, Object service, ClusterService cluster) {
        try {
            StringBuffer s = new StringBuffer(160);
            s.append("DESCRIPTOR ").append(step).append(" variant=").append(variant)
             .append(" exit=").append(exitId).append(" service=")
             .append(service == null ? "null" : service.getClass().getName())
             .append(" count=").append(descriptors == null ? -1 : descriptors.length);
            if (descriptors != null) {
                for (int i=0; i<descriptors.length && i<3; i++) {
                    CombiBAPNaviManeuverDescriptor d = descriptors[i];
                    if (d != null) s.append(" slot").append(i).append("=main:").append(d.mainElement)
                        .append(",dir:").append(d.direction).append(",z:").append(d.zLevelGuidance)
                        .append(",side:").append(d.sideStreets == null ? -1 : d.sideStreets.length);
                }
            }
            line(s.toString());
            snapshot("after_" + step, cluster);
        } catch (Throwable ignored) { }
    }

    static void distance(String step, int meters, int encoded, int unit, ClusterService cluster) {
        try {
            line("DISTANCE " + step + " meters=" + meters + " bapValue=" + encoded + " bapUnit=" + unit);
            long now = System.currentTimeMillis();
            synchronized (V8MFactoryTrace.class) {
                if (now - lastDistanceSnapshotMs < 2000) return;
                lastDistanceSnapshotMs = now;
            }
            snapshot("distance_" + step, cluster);
        } catch (Throwable ignored) { }
    }

    static void snapshot(String point, ClusterService cluster) {
        try {
            if (cluster == null) { line("SNAPSHOT " + point + " cluster=null"); return; }
            Object listener = field(cluster, "combiBAPListener");
            Object viewMode = field(cluster, "clusterViewMode");
            Object cached = field(listener, "maneuverDescriptor");
            StringBuffer s = new StringBuffer(350);
            s.append("SNAPSHOT ").append(point)
             .append(" vcValid=").append(val(cluster,"rgiDataValid"))
             .append(" favored=").append(val(viewMode,"favoredViewMode"))
             .append(" favoredReceived=").append(val(viewMode,"favoredViewModeReceived"))
             .append(" factoryRgType=").append(val(listener,"rgType"))
             .append(" factoryRgStatus=").append(val(listener,"rgStatus"))
             .append(" factoryManeuverState=").append(val(listener,"maneuverState"))
             .append(" factoryManeuverDistance=").append(val(listener,"maneuverDistance"))
             .append(" factoryExitVariant=").append(val(listener,"exitViewVariant"))
             .append(" factoryExitID=").append(val(listener,"exitviewID"))
             .append(" validRgTypeReceived=").append(val(listener,"validRGTypeReceived"))
             .append(" factorySyncStatus=").append(val(listener,"rgStuffSyncStatus"))
             .append(" factoryLargeMapView=").append(val(listener,"largeMapView"))
             .append(" factoryMapView=").append(val(listener,"mapView"))
             .append(" factoryLeftMenu=").append(val(listener,"leftSideMenuOpen"))
             .append(" factoryRightMenu=").append(val(listener,"rightSideMenuOpen"))
             .append(" factoryLVDSVisible=").append(val(listener,"lvdsMapVisible"));
            if (cached != null && cached.getClass().isArray()) {
                int count = Array.getLength(cached);
                s.append(" factoryDescriptorCount=").append(count);
                for (int i=0; i<count && i<3; i++) {
                    Object d = Array.get(cached,i);
                    s.append(" factorySlot").append(i).append("Main=").append(val(d,"mainElement"))
                     .append(" factorySlot").append(i).append("Dir=").append(val(d,"direction"))
                     .append(" factorySlot").append(i).append("Z=").append(val(d,"zLevelGuidance"));
                }
            } else s.append(" factoryDescriptor=null");
            Object next = field(cluster,"nextManeuverElement");
            s.append(" komoRouteType=").append(val(next,"routeInfoElementType"))
             .append(" komoExitIcon=").append(val(next,"exitIconId"))
             .append(" komoStreetIcon=").append(val(next,"streetIconID"))
             .append(" komoManeuverCount=").append(arrayCount(field(next,"maneuverDescriptor")));
            /* Firmware's existing IViewSizeManager getters, not VIEW-button guesses. */
            Object manager = field(listener,"viewSizeManagerFPK");
            s.append(" actualViewSize=").append(get(manager,"getCurrentViewSize"))
             .append(" preferredViewSize=").append(get(manager,"getPreferredViewSize"))
             .append(" requestedViewSize=").append(get(manager,"getRequestedViewSize"))
             .append(" largeViewRequest=").append(get(manager,"isLargeViewSizeRequestActive"));
            line(s.toString());
        } catch (Throwable t) { line("SNAPSHOT_FAILED " + point + " " + t.getClass().getName()); }
    }

    private static int arrayCount(Object o) {
        if (o == null || !o.getClass().isArray()) return -1;
        return Array.getLength(o);
    }
    private static String get(Object o,String name) {
        if (!(o instanceof IViewSizeManager)) return "?";
        try {
            IViewSizeManager m = (IViewSizeManager)o;
            if ("getCurrentViewSize".equals(name)) return String.valueOf(m.getCurrentViewSize());
            if ("getPreferredViewSize".equals(name)) return String.valueOf(m.getPreferredViewSize());
            if ("getRequestedViewSize".equals(name)) return String.valueOf(m.getRequestedViewSize());
            if ("isLargeViewSizeRequestActive".equals(name)) return String.valueOf(m.isLargeViewSizeRequestActive());
            return "?";
        } catch (Throwable ignored) { return "?"; }
    }
    private static String val(Object o,String key) {
        Object x=field(o,key);
        if (x == null) return "?";
        if (x instanceof Number || x instanceof Boolean) return x.toString();
        return x.getClass().getName();
    }
    private static Object field(Object o,String name) {
        if (o == null) return null;
        try {
            Class c=o.getClass();
            while (c != null) {
                try {
                    Field f=c.getDeclaredField(name);
                    f.setAccessible(true);
                    return f.get(o);
                } catch (NoSuchFieldException nf) { c=c.getSuperclass(); }
            }
        } catch (Throwable ignored) { }
        return null;
    }
}
