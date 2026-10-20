# Retain all unrelated K2161 DSITracer configuration verbatim.
# Normalize only previously approved diagnostic settings, tolerating missing,
# commented, alternate-value or duplicate entries without blocking install.
BEGIN { app=0; key=0; zoom=0; komo=0; map=0; kombisync=0; bap=0; carkombi=0 }
/^[[:space:]]*#?[[:space:]]*use[.]appender[[:space:]]*=/ {
    if (++app==1) print "use.appender=telnet,bluepirat"
    next
}
/^[[:space:]]*#?[[:space:]]*org[.]dsi[.]ifc[.]bap[.]DSIBAP[[:space:]]*=/ {
    if (++bap==1) print "org.dsi.ifc.bap.DSIBAP=full"
    next
}
/^[[:space:]]*#?[[:space:]]*org[.]dsi[.]ifc[.]carkombi[.]DSICarKombi[[:space:]]*=/ {
    if (++carkombi==1) print "org.dsi.ifc.carkombi.DSICarKombi=full"
    next
}
/^[[:space:]]*#?[[:space:]]*org[.]dsi[.]ifc[.]keypanel[.]DSIKeyPanel[[:space:]]*=/ {
    if (++key==1) print "org.dsi.ifc.keypanel.DSIKeyPanel=full"
    next
}
/^[[:space:]]*#?[[:space:]]*org[.]dsi[.]ifc[.]map[.]DSIMapViewerZoomEngine[[:space:]]*=/ {
    if (++zoom==1) print "org.dsi.ifc.map.DSIMapViewerZoomEngine=full"
    next
}
/^[[:space:]]*#?[[:space:]]*org[.]dsi[.]ifc[.]komoview[.]DSIKOMOView[[:space:]]*=/ {
    if (++komo==1) print "org.dsi.ifc.komoview.DSIKOMOView=full"
    next
}
/^[[:space:]]*#?[[:space:]]*org[.]dsi[.]ifc[.]map[.]DSIMapViewerControl[[:space:]]*=/ {
    if (++map==1) print "org.dsi.ifc.map.DSIMapViewerControl=full"
    next
}
/^[[:space:]]*#?[[:space:]]*org[.]dsi[.]ifc[.]kombisync[.]DSIKombiSync[[:space:]]*=/ {
    if (++kombisync==1) print "org.dsi.ifc.kombisync.DSIKombiSync=full"
    next
}
{ print }
END {
    if (!app) print "use.appender=telnet,bluepirat"
    if (!key) print "org.dsi.ifc.keypanel.DSIKeyPanel=full"
    if (!zoom) print "org.dsi.ifc.map.DSIMapViewerZoomEngine=full"
    if (!komo) print "org.dsi.ifc.komoview.DSIKOMOView=full"
    if (!map) print "org.dsi.ifc.map.DSIMapViewerControl=full"
    if (!kombisync) print "org.dsi.ifc.kombisync.DSIKombiSync=full"
    if (!bap) print "org.dsi.ifc.bap.DSIBAP=full"
    if (!carkombi) print "org.dsi.ifc.carkombi.DSICarKombi=full"
}
