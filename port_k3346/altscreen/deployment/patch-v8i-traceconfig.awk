# K2161 v8i SOURCE ONLY: preserve all existing settings, additionally enable
# the exact K2161 map-viewer Control DSI to test gauge viewport correlation.
# Do not run against live MMI without backup/mount/rollback controller.
BEGIN {app=0;key=0;zoom=0;komo=0;map=0}
/^use.appender=/ {app++;$0="use.appender=telnet,bluepirat"}
/^#?org.dsi.ifc.keypanel.DSIKeyPanel=full$/ {key++;$0="org.dsi.ifc.keypanel.DSIKeyPanel=full"}
/^#?org.dsi.ifc.map.DSIMapViewerZoomEngine=full$/ {zoom++;$0="org.dsi.ifc.map.DSIMapViewerZoomEngine=full"}
/^#?org.dsi.ifc.komoview.DSIKOMOView=full$/ {komo++;$0="org.dsi.ifc.komoview.DSIKOMOView=full"}
/^#?org.dsi.ifc.map.DSIMapViewerControl=full$/ {map++;$0="org.dsi.ifc.map.DSIMapViewerControl=full"}
{print}
END {if(app!=1 || key!=1 || zoom!=1 || komo!=1 || map!=1) exit 7}
