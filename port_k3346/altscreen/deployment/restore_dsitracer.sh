#!/bin/sh
set -eu
ROOT="${V8G2_DSI_ROOT:-/mnt/app/eso/hmi/lsd}"
CFG="$ROOT/traceConfig.properties"
MARKER="$ROOT/dsitracer_activated"
BACKUP="$ROOT/traceConfig.properties.pre-v8g2"
STATE="$ROOT/dsitracer_activated.pre-v8g2-state"
[ -f "$BACKUP" ] && [ -f "$STATE" ] || { echo "FAIL: no original tracer backup" >&2; exit 2; }
case "$(cat "$STATE")" in
    present) : > "$MARKER";;
    absent) rm -f "$MARKER";;
    *) echo 'FAIL: invalid saved marker state' >&2; exit 3;;
esac
cp "$BACKUP" "$CFG" || exit 4
rm -f "$BACKUP" "$STATE"
echo 'PASS: original tracer config and marker state restored'
