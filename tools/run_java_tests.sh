#!/usr/bin/env bash
# Testy Javy na komputerze: bundle (java/build/src + java/test) i klasy Etapu 5
# (java/overrides/src + java/overrides/test). Bez auta.
#
#   tools/run_java_tests.sh                 # wszystkie
#   tools/run_java_tests.sh HudStyle        # tylko klasy z "HudStyle" w nazwie
#   HUD_REPLAY=plik tools/run_java_tests.sh HudStyle   # + przebieg z auta (odleglosci w m, po jednej w linii)
#
# Zrodla kompiluja sie jak w wydaniu (-source 1.4 -target 1.4, wzgledem work/lsd.jar),
# testy zwyklym javac 8. Wszystko buduje sie w /tmp kontenera, nie w drzewie zrodel.
set -euo pipefail

FILTER="${1:-}"
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
PROJ_WIN="$(cd "$PROJ" && (pwd -W 2>/dev/null || pwd))"
IMAGE="qnx65-armv7-toolchain:4.9"
JDK=zulu8.78.0.19-ca-jdk8.0.412-linux_x64

[ -s "$PROJ/work/lsd.jar" ] || { echo "brak work/lsd.jar" >&2; exit 2; }
[ -d "$PROJ/repos/jxe2jar/jvms/$JDK" ] || { echo "brak JDK 8 w repos/jxe2jar/jvms/$JDK" >&2; exit 2; }

REPLAY_MOUNT=()
REPLAY_ARG=""
if [ -n "${HUD_REPLAY:-}" ]; then
    [ -s "$HUD_REPLAY" ] || { echo "brak pliku HUD_REPLAY=$HUD_REPLAY" >&2; exit 2; }
    REPLAY_WIN="$(cd "$(dirname "$HUD_REPLAY")" && (pwd -W 2>/dev/null || pwd))/$(basename "$HUD_REPLAY")"
    REPLAY_MOUNT=(-v "$REPLAY_WIN:/replay.txt:ro")
    REPLAY_ARG=/replay.txt
fi

MSYS_NO_PATHCONV=1 docker run --rm --platform=linux/amd64 \
    -v "$PROJ_WIN/port_k3346/altscreen:/src:ro" -v "$PROJ_WIN/work/lsd.jar:/lsd.jar:ro" \
    -v "$PROJ_WIN/repos/jxe2jar/jvms:/jvms:ro" "${REPLAY_MOUNT[@]}" \
    -e JAVA_HOME="/jvms/$JDK" -e FILTER="$FILTER" -e REPLAY_ARG="$REPLAY_ARG" \
    "$IMAGE" sh -c '
set -eu
J="$JAVA_HOME/bin"
total=0; bad=0
run_set() {  # nazwa, katalog zrodel, katalog testow
    name=$1; src=$2; tst=$3
    out=/tmp/jt-$name
    mkdir -p "$out/main" "$out/test"
    "$J/javac" -nowarn -source 1.4 -target 1.4 -encoding UTF-8 -cp /lsd.jar -d "$out/main" \
        $(find "$src" -name "*.java" | sort) 2> "$out/main.log" \
        || { cat "$out/main.log"; echo "JAVA_TESTS: kompilacja zrodel $name FAIL"; exit 1; }
    "$J/javac" -nowarn -encoding UTF-8 -cp "$out/main:/lsd.jar" -d "$out/test" \
        $(find "$tst" -name "*.java" | sort) 2> "$out/test.log" \
        || { cat "$out/test.log"; echo "JAVA_TESTS: kompilacja testow $name FAIL"; exit 1; }
    for f in $(find "$tst" -name "*Test.java" | sort); do
        cls=$(basename "$f" .java)
        case "$cls" in *"$FILTER"*) ;; *) continue;; esac
        pkg=$(sed -n "s/^package \(.*\);.*/\1/p" "$f" | head -1)
        arg=""
        [ "$cls" = K3346HudStyleTest ] && arg="$REPLAY_ARG"
        total=$((total+1))
        if timeout -k 10 120 "$J/java" -Xverify:none -cp "$out/test:$out/main:/lsd.jar" "$pkg.$cls" $arg \
                > "$out/$cls.log" 2>&1; then
            echo "PASS $cls: $(grep WYNIK "$out/$cls.log" | tail -1)"
        else
            bad=$((bad+1))
            grep -E "FAIL|Exception|Error" "$out/$cls.log" | head -40
            echo "FAIL $cls: $(grep WYNIK "$out/$cls.log" | tail -1)"
        fi
        [ -n "$arg" ] && sed -n "/=== przebieg z auta/,/WYNIK/p" "$out/$cls.log" | grep -v "  OK  " || true
    done
}
run_set bundle /src/java/build/src /src/java/test
run_set overrides /src/java/overrides/src /src/java/overrides/test
echo "JAVA_TESTS: $total uruchomionych, $bad nieudanych"
[ "$total" -gt 0 ] && [ "$bad" -eq 0 ]
'
