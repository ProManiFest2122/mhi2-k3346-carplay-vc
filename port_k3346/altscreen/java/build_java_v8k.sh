#!/bin/sh
# K2161 v8k -- compile VC diagnostic Java from ORIGINAL EDITABLE JAVA SOURCES.
# Source-only rule: this script builds a fresh JAR; it NEVER patches a compiled JAR.
set -eu
SRCROOT=${1:?usage: build_java_v8k.sh PATH_TO_PINNED_GITHUB_REPO_SOURCE}
WORK=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT=$(CDPATH= cd -- "$WORK/.." && pwd)
: "${JAVA_HOME:?JAVA_HOME must point to portable Temurin8}" 
: "${K2161_BASE_JAR:?K2161_BASE_JAR required for compile dependencies}"
[ -s "$K2161_BASE_JAR" ] || { echo 'Audi base JAR missing' >&2; exit 1; }
[ -x "$JAVA_HOME/bin/javac" ] || { echo 'Temurin8 javac missing' >&2; exit 1; }
JAVAC="$JAVA_HOME/bin/javac"
JAR="$JAVA_HOME/bin/jar"
JAVAP="$JAVA_HOME/bin/javap"
case "$("$JAVAC" -version 2>&1)" in "javac 1.8."*) ;; *) echo 'Java 8 REQUIRED' >&2; exit 1;; esac
SRCDIR="$SRCROOT/java/src"
OUT="$PROJECT/java/build"
SOURCE="$OUT/src"
CLASSES="$OUT/classes"
rm -rf "$OUT"
mkdir -p "$SOURCE" "$CLASSES" "$PROJECT/output"
for rel in app/Activator.java app/CarPlayLifecycle.java framework/CarplayBus.java framework/Log.java routeguidance/BAPBridge.java routeguidance/K2161GatedCombiService.java routeguidance/K2161RouteGuidanceOwnership.java routeguidance/ManeuverMapper.java routeguidance/RouteGuidance.java routeguidance/SideStreets.java; do
 [ -s "$SRCDIR/$rel" ] || { echo "Source missing: $rel" >&2; exit 1; }
 mkdir -p "$SOURCE/$(dirname "$rel")"
 cp "$SRCDIR/$rel" "$SOURCE/$rel"
done
# First recreate exact v8j behavior in SOURCE from archived class bytecode comparison.
python3 "$WORK/apply_v8j_behavior_parity_source.py" "$SOURCE"
# Prevent wrong public/HUD-only branch from silently building.
grep -q 'private static boolean VC_RGI_ENABLED = true;' "$SOURCE/routeguidance/BAPBridge.java" || { echo 'VC-enabled source signature missing' >&2; exit 1; }
# Only source-level addition: instrument the VC presentation entry/return points.
python3 "$WORK/apply_java_logging_source.py" "$SOURCE/routeguidance/BAPBridge.java"
cp "$WORK/src/com/luka/carplay/routeguidance/V8KVcTrace.java" "$SOURCE/routeguidance/"
# JDK8 supports -source/-target 1.4 and v8j class format is major 48.
"$JAVAC" -source 1.4 -target 1.4 -encoding UTF-8 -cp "$K2161_BASE_JAR" -sourcepath "$SOURCE" -d "$CLASSES" \
 "$SOURCE/framework/Log.java" \
 "$SOURCE/framework/CarplayBus.java" \
 "$SOURCE/routeguidance/K2161GatedCombiService.java" \
 "$SOURCE/routeguidance/K2161RouteGuidanceOwnership.java" \
 "$SOURCE/routeguidance/RouteGuidance.java" \
 "$SOURCE/routeguidance/BAPBridge.java" \
 "$SOURCE/routeguidance/ManeuverMapper.java" \
 "$SOURCE/routeguidance/SideStreets.java" \
 "$SOURCE/routeguidance/V8KVcTrace.java" \
 "$SOURCE/app/CarPlayLifecycle.java" \
 "$SOURCE/app/Activator.java"
# Enforce class format 48 for ALL output classes.
python3 "$WORK/verify_java_classes.py" "$CLASSES" "$PROJECT/reference/mib2high-carplay-rgi.live.jar"
(cd "$CLASSES" && "$JAR" cf "$PROJECT/output/mib2high-carplay-rgi.jar" .)
[ -s "$PROJECT/output/mib2high-carplay-rgi.jar" ] || { echo 'Java JAR build missing' >&2; exit 1; }
sha256sum "$PROJECT/output/mib2high-carplay-rgi.jar"
echo 'RESULT=JAVA_BUILD_FROM_SOURCE_PASS'
