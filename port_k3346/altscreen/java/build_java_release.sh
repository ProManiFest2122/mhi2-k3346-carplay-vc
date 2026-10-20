#!/bin/sh
# Build current editable AltScreen v8S Java sources. No historical parity JAR required.
# The OEM K2161 compile dependency is intentionally user-supplied.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$HERE")
: "${JAVA_HOME:?Set JAVA_HOME to a JDK 8 installation}"
: "${K2161_BASE_JAR:?Set K2161_BASE_JAR to your authorized firmware compile dependency}"
[ -x "$JAVA_HOME/bin/javac" ] && [ -x "$JAVA_HOME/bin/jar" ] && [ -s "$K2161_BASE_JAR" ] || { echo "Missing Java 8 toolchain or compile dependency" >&2; exit 2; }
case "$("$JAVA_HOME/bin/javac" -version 2>&1)" in "javac 1.8."*) ;; *) echo "Java 8 required" >&2; exit 2;; esac
SRC="$HERE/build/src"
CLASSES="$HERE/build/release_classes"
mkdir -p "$ROOT/output"
rm -rf "$CLASSES"
mkdir -p "$CLASSES"
"$JAVA_HOME/bin/javac" -source 1.4 -target 1.4 -encoding UTF-8 \
 -cp "$K2161_BASE_JAR" -sourcepath "$SRC" -d "$CLASSES" \
 "$SRC/framework/Log.java" "$SRC/framework/CarplayBus.java" \
 "$SRC/routeguidance/K2161GatedCombiService.java" "$SRC/routeguidance/K2161RouteGuidanceOwnership.java" \
 "$SRC/routeguidance/RouteGuidance.java" "$SRC/routeguidance/BAPBridge.java" \
 "$SRC/routeguidance/ManeuverMapper.java" "$SRC/routeguidance/SideStreets.java" \
 "$SRC/routeguidance/V8KVcTrace.java" "$SRC/routeguidance/V8MFactoryTrace.java" \
 "$SRC/app/CarPlayLifecycle.java" "$SRC/app/Activator.java"
python3 "$HERE/verify_release_classes.py" "$CLASSES"
(cd "$CLASSES" && "$JAVA_HOME/bin/jar" cf "$ROOT/output/mib2high-carplay-rgi.jar" .)
echo "JAVA_SOURCE_BUILD=PASS (historical artifact hash parity NOT guaranteed)"
sha256sum "$ROOT/output/mib2high-carplay-rgi.jar"
