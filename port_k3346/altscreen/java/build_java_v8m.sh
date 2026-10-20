#!/bin/sh
# Compile V8M Java from editable v8k source files + V8M diagnostic source.
set -eu
HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
PROJECT=$(dirname "$HERE")
: "${JAVA_HOME:?JAVA_HOME required}"
: "${K2161_BASE_JAR:?K2161_BASE_JAR required}"
[ -x "$JAVA_HOME/bin/javac" ] && [ -s "$K2161_BASE_JAR" ] || exit 2
SOURCE="$HERE/build/src"
CLASSES="$HERE/build/v8m_classes"
rm -rf "$CLASSES"
mkdir -p "$CLASSES"
"$JAVA_HOME/bin/javac" -source 1.4 -target 1.4 -encoding UTF-8 -cp "$K2161_BASE_JAR" -sourcepath "$SOURCE" -d "$CLASSES" \
 "$SOURCE/framework/Log.java" "$SOURCE/framework/CarplayBus.java" \
 "$SOURCE/routeguidance/K2161GatedCombiService.java" "$SOURCE/routeguidance/K2161RouteGuidanceOwnership.java" \
 "$SOURCE/routeguidance/RouteGuidance.java" "$SOURCE/routeguidance/BAPBridge.java" \
 "$SOURCE/routeguidance/ManeuverMapper.java" "$SOURCE/routeguidance/SideStreets.java" \
 "$SOURCE/routeguidance/V8KVcTrace.java" "$SOURCE/routeguidance/V8MFactoryTrace.java" \
 "$SOURCE/app/CarPlayLifecycle.java" "$SOURCE/app/Activator.java"
python3 "$HERE/verify_java_v8m.py" "$CLASSES" "$PROJECT/reference/v8l-mib2high-carplay-rgi.jar"
(cd "$CLASSES" && "$JAVA_HOME/bin/jar" cf "$PROJECT/output/mib2high-carplay-rgi.jar" .)
echo V8M_JAVA_SOURCE_BUILD=PASS
sha256sum "$PROJECT/output/mib2high-carplay-rgi.jar"
