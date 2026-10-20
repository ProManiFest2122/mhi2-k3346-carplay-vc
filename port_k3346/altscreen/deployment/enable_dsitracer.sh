#!/bin/sh
# Editable deployment source; changes *text* stock DSITracer configuration only.
# A full M.I.B installer must mount /mnt/app read-write and do its own backup/rollback.
set -eu
ROOT="${V8G2_DSI_ROOT:-/mnt/app/eso/hmi/lsd}"
CFG="$ROOT/traceConfig.properties"
MARKER="$ROOT/dsitracer_activated"
BACKUP="$ROOT/traceConfig.properties.pre-v8g2"
MARKER_STATE="$ROOT/dsitracer_activated.pre-v8g2-state"
[ -f "$CFG" ] || { echo "FAIL: missing tracer configuration" >&2; exit 2; }
[ -f "$ROOT/DSITracer.jar" ] || { echo "FAIL: missing stock DSITracer.jar" >&2; exit 3; }
[ -f "$ROOT/bundles_dsitracer_enabled.properties" ] || { echo "FAIL: missing stock DSITracer bundles" >&2; exit 4; }
[ ! -e "$BACKUP" ] || { echo "FAIL: backup already exists; refuse ambiguous repeated install" >&2; exit 5; }
[ ! -e "$MARKER_STATE" ] || { echo "FAIL: marker-state file already exists" >&2; exit 6; }
TMP="$CFG.v8g2.tmp.$$"
trap 'rm -f "$TMP"' EXIT HUP INT TERM
# Fail closed unless exact known full-scope entries are present, once each.
awk '
BEGIN { app=0; k=0; z=0; c=0; fail=0 }
/^use.appender=/ { app++; $0="use.appender=telnet,bluepirat" }
/^#?org.dsi.ifc.keypanel.DSIKeyPanel=full$/ { k++; $0="org.dsi.ifc.keypanel.DSIKeyPanel=full" }
/^#?org.dsi.ifc.map.DSIMapViewerZoomEngine=full$/ { z++; $0="org.dsi.ifc.map.DSIMapViewerZoomEngine=full" }
/^#?org.dsi.ifc.komoview.DSIKOMOView=full$/ { c++; $0="org.dsi.ifc.komoview.DSIKOMOView=full" }
{ print }
END { if (app!=1||k!=1||z!=1||c!=1) exit 7 }
' "$CFG" > "$TMP" || { echo "FAIL: unexpected DSITracer scope layout" >&2; exit 7; }
cp "$CFG" "$BACKUP" || exit 8
if [ -e "$MARKER" ]; then echo present > "$MARKER_STATE"; else echo absent > "$MARKER_STATE"; fi
chmod 644 "$TMP"
if ! mv "$TMP" "$CFG"; then
  cp "$BACKUP" "$CFG" || true
  rm -f "$BACKUP" "$MARKER_STATE"
  exit 9
fi
if ! : > "$MARKER"; then
  cp "$BACKUP" "$CFG" || true
  rm -f "$BACKUP" "$MARKER_STATE"
  exit 10
fi
printf 'PASS: DSITracer text config enabled; 3 scopes full, ports 1798/20991; reboot needed\n'
