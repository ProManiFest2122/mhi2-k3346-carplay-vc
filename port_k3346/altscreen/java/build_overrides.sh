#!/bin/sh
# K3346 Etap 5: buduje mib2high-k3346-overrides.jar - klasy o FABRYCZNYCH
# nazwach, ktore lsd.sh (lsd/jars/ na -Xbootclasspath przed lsd.jxe) laduje
# zamiast fabrycznych. Kazda klasa jest pisana od nowa ze zdekompilowanego K3346
# (nie lata bytecode'u). Weryfikacja: format Java 1.4 (major 48) i API
# publiczne/chronione identyczne z oryginalem z work/lsd.jar.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$HERE")
: "${JAVA_HOME:?Set JAVA_HOME to a JDK 8 installation}"
: "${K2161_BASE_JAR:?Set K2161_BASE_JAR to the K3346 lsd.jar}"
SRC="$HERE/overrides/src"
CLASSES="$HERE/build/overrides_classes"
rm -rf "$CLASSES"; mkdir -p "$CLASSES" "$ROOT/output"
FILES=$(cd "$SRC" && find . -name '*.java' | sort)
[ -n "$FILES" ] || { echo "brak zrodel w $SRC" >&2; exit 2; }
( cd "$SRC" && "$JAVA_HOME/bin/javac" -source 1.4 -target 1.4 -encoding UTF-8 \
    -cp "$K2161_BASE_JAR" -sourcepath "$SRC" -d "$CLASSES" $FILES )
python3 "$HERE/verify_overrides.py" "$CLASSES" "$K2161_BASE_JAR" "$JAVA_HOME/bin/javap"
(cd "$CLASSES" && "$JAVA_HOME/bin/jar" cf "$ROOT/output/mib2high-k3346-overrides.jar" .)
echo "OVERRIDES_BUILD=PASS"
sha256sum "$ROOT/output/mib2high-k3346-overrides.jar"
