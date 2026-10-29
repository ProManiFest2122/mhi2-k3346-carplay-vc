#!/usr/bin/env bash
# Differential test: the C maneuver mapper must agree with the Java one, always.
#
# WHY
# Etap 7 feeds the cluster overlay straight from the hook in mm-ipod, with no
# Java in the path, so the iAP2 -> BAP mapping exists twice:
#   java/build/src/routeguidance/ManeuverMapper.java   -> VC text and arrow over BAP
#   native/routeguidance/maneuver_map.c                -> the arrow we draw ourselves
# If they ever disagree, the car shows an arrow that contradicts the distance
# and street name printed beside it.  This runs both over an exhaustive grid
# (58 maneuver types x 364 angles x 3 junction types x 2 driving sides,
# 126672 cases) and fails on the first difference.
#
# Needs: Docker (native gcc in the toolchain image) and the portable JDK 8 that
# repos/jxe2jar already ships.  Run from the project root:
#   bash tools/run_overlay_map_diff.sh
set -euo pipefail

PROJ="$(cd "$(dirname "$0")/.." && pwd)"
PROJ_WIN="$(cd "$PROJ" && (pwd -W 2>/dev/null || pwd))"
ALT="$PROJ/port_k3346/altscreen"
WORK="$PROJ/work/overlay_mapdiff"
IMAGE="qnx65-armv7-toolchain:4.9"

JDK_BIN=""
for cand in \
  "$PROJ/repos/jxe2jar/jvms/zulu8.78.0.19-ca-jdk8.0.412-win_x64/bin" \
  "$PROJ/repos/jxe2jar/jvms/zulu8.78.0.19-ca-jdk8.0.412-linux_x64/bin"; do
  if [ -x "$cand/javac" ] || [ -x "$cand/javac.exe" ]; then JDK_BIN="$cand"; break; fi
done
[ -n "$JDK_BIN" ] || { echo "FAIL: portable JDK 8 not found under repos/jxe2jar/jvms" >&2; exit 2; }
JAVAC="$JDK_BIN/javac"; [ -x "$JAVAC" ] || JAVAC="$JDK_BIN/javac.exe"
JAVA="$JDK_BIN/java";   [ -x "$JAVA" ]  || JAVA="$JDK_BIN/java.exe"

rm -rf "$WORK"
PKG="$WORK/java/com/luka/carplay/routeguidance"
mkdir -p "$PKG" "$WORK/classes"

cp "$ALT/java/build/src/routeguidance/ManeuverMapper.java" "$PKG/"
cp "$ALT/overlay/tests/ManeuverMapDump.java" "$PKG/"

echo "== Java side"
"$JAVAC" -encoding UTF-8 -d "$WORK/classes" "$PKG"/*.java
"$JAVA" -cp "$WORK/classes" com.luka.carplay.routeguidance.ManeuverMapDump "$WORK/java.txt"

echo "== C side (container gcc, ASan + UBSan)"
MSYS_NO_PATHCONV=1 docker run --rm --platform=linux/amd64 \
  -v "${PROJ_WIN}/port_k3346/altscreen:/src" \
  -v "${PROJ_WIN}/work/overlay_mapdiff:/out" \
  "$IMAGE" bash -c '
    set -e
    # The image puts the QNX cross binutils first on PATH; host gcc needs host as.
    export PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
    cp -a /src /build && cd /build
    gcc -std=gnu99 -O1 -g -Wall -Wextra -Werror \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -o /tmp/mapdump overlay/tests/maneuver_map_dump.c \
                        native/routeguidance/maneuver_map.c
    /tmp/mapdump > /out/c.txt'

echo "== diff"
if diff -q "$WORK/java.txt" "$WORK/c.txt" >/dev/null; then
  echo "MANEUVER_MAP_DIFF=PASS ($(wc -l < "$WORK/c.txt" | tr -d ' ') cases identical)"
else
  echo "MANEUVER_MAP_DIFF=FAIL"
  echo "--- first differences (< Java, > C) ---"
  diff "$WORK/java.txt" "$WORK/c.txt" | head -40
  exit 1
fi
