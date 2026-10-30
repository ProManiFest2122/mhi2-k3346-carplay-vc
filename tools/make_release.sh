#!/usr/bin/env bash
# Buduje wydanie dla uzytkownikow: zipy do rozpakowania na karte z M.I.B.
#
#   tools/make_release.sh 1.0.0              # buduje wszystko ze zrodel i pakuje
#   tools/make_release.sh 1.0.0 --no-build   # pakuje juz zbudowane artefakty
#
# Wynik w work/release/<wersja>/:
#   mhi2-k3346-carplay-vc-<wersja>-install.zip    karta: wszystkie etapy + INSTALL_ALL
#   mhi2-k3346-carplay-vc-<wersja>-uninstall.zip  sam znacznik ROLLBACK
#   mhi2-k3346-carplay-vc-<wersja>-collect-logs.zip  sam znacznik COLLECT_LOGS (logi po nieudanej instalacji)
#   mhi2-k3346-carplay-vc-<wersja>-vc-policy-{rgi,map,off}.zip  znacznik polityki VC (issue #5)
#   SHA256SUMS.txt
#   card/, uninstall-card/   drzewa, z ktorych powstaly zipy (dla symulacji)
#   build-logs/              logi budowania i pakowania
#
# DLACZEGO JEDEN ZIP ZAMIAST PACZEK ETAPOW
# Do tej pory instalacja to: paczka v8s, potem znaczniki INSTALL_ETAP4/5/7 po
# kolei, kazdy osobnym uruchomieniem. Dla kogos, kto nie sledzil projektu, to
# nie do przejscia. Wydanie sklada payload wszystkich etapow w jedno drzewo
# i uzbraja INSTALL_ALL: jedno uruchomienie skryptu w M.I.B., jeden komunikat
# na koncu. Etapy dalej sa osobnymi transakcjami (custom.sh, install_all).
#
# CZEGO W ZIPACH NIE MA
# Zadnego pliku z oprogramowania Audi: pliki auta zmieniaja skrypty awk na
# samym aucie, a work/lsd.jar sluzy wylacznie do kompilacji Javy. Swiadomy
# wyjatek: mib2high-k3346-overrides.jar (Etap 5, klasy HMI przepisane ze zrodel
# Audi) - decyzja wlasciciela projektu przy wydaniu 1.0.
set -euo pipefail

VER="${1:?uzycie: make_release.sh <wersja X.Y.Z[-rcN]> [--no-build]}"
BUILD=1
[ "${2:-}" = "--no-build" ] && BUILD=0
printf '%s' "$VER" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+(-rc[0-9]+)?$' \
    || { echo "wersja musi miec postac X.Y.Z albo X.Y.Z-rcN" >&2; exit 2; }

PROJ="$(cd "$(dirname "$0")/.." && pwd)"
PROJ_WIN="$(cd "$PROJ" && (pwd -W 2>/dev/null || pwd))"
A="$PROJ/port_k3346/altscreen"
PKG=mib2high-carplay-rgi-v8s-simple
NAME="mhi2-k3346-carplay-vc-$VER"
REL="$PROJ/work/release/$VER"
IMAGE="qnx65-armv7-toolchain:4.9"
JDK=zulu8.78.0.19-ca-jdk8.0.412-linux_x64

fail(){ echo "RELEASE_FAIL: $*" >&2; exit 1; }
step(){ echo; echo "### $*"; }

case "$REL" in */work/release/*) ;; *) fail "nieoczekiwany katalog wyjsciowy: $REL";; esac
rm -rf "$REL"
mkdir -p "$REL/build-logs"
LOGS="$REL/build-logs"

# --------------------------------------------------------------- 1. budowanie
# Wszystko ze zrodel, w tym samym obrazie, ktorym budowano wersje testowane w
# aucie. Kolejnosc bez znaczenia - artefakty sa niezalezne.
if [ "$BUILD" -eq 1 ]; then
    [ -s "$PROJ/work/lsd.jar" ] || fail "brak work/lsd.jar - zrob go z WLASNEGO lsd.jxe (BUILDING.md)"
    [ -d "$PROJ/repos/jxe2jar/jvms/$JDK" ] || fail "brak JDK 8 w repos/jxe2jar/jvms/$JDK"
    docker image inspect "$IMAGE" >/dev/null 2>&1 || fail "brak obrazu $IMAGE (BUILDING.md)"
    n=0
    while read -r script artifact; do
        [ -n "$script" ] || continue
        n=$((n+1)); log="$LOGS/native-$n-$(basename "$artifact").log"
        step "natywne: $artifact"
        bash "$PROJ/tools/qnx_build.sh" "$script" "$artifact" > "$log" 2>&1 \
            || { tail -20 "$log" >&2; fail "budowanie $artifact (log: $log)"; }
        grep -E "^[0-9a-f]{64} " "$log" | tail -1
    done <<'EOF'
build.sh                               output/libmib2high-carplay-rgi.so
renderer/build_renderer.sh             renderer/build/vc_render
diagnostics/build_dsitracer_qnx_v8l.sh diagnostics/v8l-dsitracer-capture
build_rgi_ipod.sh                      output/libmib2high-carplay-rgi-ipod.so
diagnostics/build_artwork_probe.sh     diagnostics/k3346-artwork-probe
coverart/build_coverart_fetch.sh       coverart/k3346-coverart-fetch
overlay/build_overlay.sh               overlay/output/vc_overlay
overlay/build_probe.sh                 overlay/output/vc_overlay_probe
EOF
    step "Java: bundle OSGi + klasy Etapu 5"
    MSYS_NO_PATHCONV=1 docker run --rm --platform=linux/amd64 \
        -v "$PROJ_WIN/port_k3346/altscreen:/src" -v "$PROJ_WIN/work:/work" \
        -v "$PROJ_WIN/repos/jxe2jar/jvms:/jvms:ro" \
        -e JAVA_HOME="/jvms/$JDK" -e K2161_BASE_JAR=/work/lsd.jar \
        "$IMAGE" sh -c 'sh /src/java/build_java_release.sh && sh /src/java/build_overrides.sh' \
        > "$LOGS/java.log" 2>&1 || { tail -20 "$LOGS/java.log" >&2; fail "budowanie Javy (log: $LOGS/java.log)"; }
    grep -q 'OVERRIDES_VERIFY=PASS' "$LOGS/java.log" || fail "klasy Etapu 5 bez OVERRIDES_VERIFY=PASS"
    grep -E "^[0-9a-f]{64} " "$LOGS/java.log"
else
    step "pomijam budowanie (--no-build): pakuje artefakty, ktore juz sa"
fi

# ------------------------------------------------------- 2. jedno drzewo karty
# Te same skrypty pakujace co przy etapach (ich kontrole LF/ARM/ELF dzialaja
# dalej), kazdy do wlasnego katalogu roboczego, potem zlozenie w jedno.
step "skladanie karty"
STAGE="$REL/stage"; CARD="$REL/card"; UNCARD="$REL/uninstall-card"
SD_OUT="$STAGE/base"  bash "$PROJ/tools/make_sd_package.sh" > "$LOGS/make_sd_package.log" 2>&1 \
    || { tail -5 "$LOGS/make_sd_package.log" >&2; fail "make_sd_package.sh"; }
SD_OUT="$STAGE/etap7" bash "$PROJ/tools/make_sd_etap7.sh"   > "$LOGS/make_sd_etap7.log" 2>&1 \
    || { tail -5 "$LOGS/make_sd_etap7.log" >&2; fail "make_sd_etap7.sh"; }
cmp -s "$STAGE/base/mod/command.sh" "$STAGE/etap7/mod/command.sh" \
    || fail "dwa rozne instalatory w paczkach etapow"

mkdir -p "$CARD/mod/$PKG/payload" "$CARD/mod/$PKG/source"
cp "$STAGE/base/mod/command.sh" "$CARD/mod/command.sh"
cp -a "$STAGE/base/mod/$PKG/payload/." "$CARD/mod/$PKG/payload/"
cp -a "$STAGE/base/mod/$PKG/source/."  "$CARD/mod/$PKG/source/"
for f in "$STAGE/etap7/mod/$PKG/payload/"*; do
    b="$(basename "$f")"
    # Wspolny jest tylko mib2high-lvds-control.sh, a obie paczki biora go z tego
    # samego zrodla. Rozne tresci pod jedna nazwa = blad w skryptach pakujacych.
    if [ -e "$CARD/mod/$PKG/payload/$b" ] && ! cmp -s "$f" "$CARD/mod/$PKG/payload/$b"; then
        fail "kolizja w payloadzie: $b"
    fi
    cp "$f" "$CARD/mod/$PKG/payload/$b"
done
# Bez ZNACZNIKI/, README.txt deweloperskiego i alt-profiles/: to narzedzia
# etapow testowych, laik ich nie potrzebuje, a w katalogu glownym karty z M.I.B.
# moga kolidowac z plikami samego M.I.B.
: > "$CARD/mod/$PKG/INSTALL_ALL"
printf '%s\n' "$VER" > "$CARD/mod/$PKG/RELEASE"
cp "$A/deployment/README_CarPlay_VC.txt" "$CARD/README_CarPlay_VC.txt"

LOGCARD="$REL/collect-logs-card"
mkdir -p "$LOGCARD/mod/$PKG"
: > "$LOGCARD/mod/$PKG/COLLECT_LOGS"

# Polityka VC (issue #5): po jednym zipie na znacznik, kazdy z jednym pustym plikiem.
POLCARDS=""
for pol in rgi map off; do
    PC="$REL/vc-policy-$pol-card"; POLCARDS="$POLCARDS $PC"
    m=`echo "VC_POLICY_$pol" | tr 'a-z' 'A-Z'`
    mkdir -p "$PC/mod/$PKG"
    : > "$PC/mod/$PKG/$m"
done

mkdir -p "$UNCARD/mod/$PKG"
: > "$UNCARD/mod/$PKG/ROLLBACK"
cp "$A/deployment/README_CarPlay_VC.txt" "$UNCARD/README_CarPlay_VC.txt"

# ------------------------------------------------------------- 3. kontrole
step "kontrole karty"
# 3a. instalator ze zrodel, nie stary
cmp -s "$CARD/mod/command.sh" "$A/deployment/custom.sh" || fail "command.sh rozny od deployment/custom.sh"
# 3b. komplet payloadu, ktorego wymagaja etapy
for n in libmib2high-carplay-rgi.so mib2high-carplay-rgi.jar vc_render mib2high-lvds-control.sh \
         mib2high-lvds-profile.conf mib2high-lvds-viewareas.conf v8l-dsitracer-capture \
         libmib2high-carplay-rgi-ipod.so mm-ipod-rgi.sh mib2high-k3346-overrides.jar k3346-coverart-fetch \
         k3346-artwork-probe vc_overlay vc_overlay_probe mib2high-overlay.conf flag_atlas.rgba \
         k3346-overlay-probe.sh libmib2high-carplay-rgi-ipod.etap7.so; do
    [ -s "$CARD/mod/$PKG/payload/$n" ] || fail "brak w payloadzie: $n"
done
cmp -s "$CARD/mod/$PKG/payload/mib2high-lvds-control.sh" "$A/deployment/mib2high-lvds-control.sh" \
    || fail "mib2high-lvds-control.sh na karcie nie jest wersja ze zrodel"
for n in bundles.awk smartphone.awk trace.awk usblauncher_otg.awk iap2.awk; do
    [ -s "$CARD/mod/$PKG/source/$n" ] || fail "brak transformacji: $n"
done
# 3c. teksty idace do auta: LF
bad=""
while IFS= read -r f; do
    case "$(file -b "$f")" in *ELF*|*Zip*|*Java*|data) continue;; esac
    case "$f" in */README_CarPlay_VC.txt) continue;; esac
    if LC_ALL=C grep -q $'\r' "$f"; then bad="$bad $f"; fi
done < <(find "$CARD" "$UNCARD" "$LOGCARD" $POLCARDS -type f)
[ -z "$bad" ] || fail "CRLF w plikach dla auta:$bad"
# 3d. binarki ARM
for b in libmib2high-carplay-rgi.so libmib2high-carplay-rgi-ipod.so libmib2high-carplay-rgi-ipod.etap7.so \
         vc_render v8l-dsitracer-capture k3346-artwork-probe k3346-coverart-fetch vc_overlay vc_overlay_probe; do
    case "$(file -b "$CARD/mod/$PKG/payload/$b")" in *ARM*) ;; *) fail "$b nie jest binarka ARM";; esac
done
# 3e. zadnych danych z auta i plikow Audi poza swiadomym wyjatkiem
if find "$CARD" "$UNCARD" -type f \( -iname '*.jxe' -o -iname 'lsd.jar' -o -iname '*.log' -o -iname 'BT_MAC' \
        -o -iname '*.properties' -o -iname '*.lua' -o -iname 'iap2.cfg' -o -iname '*.json' \) | grep -q .; then
    find "$CARD" "$UNCARD" -type f \( -iname '*.jxe' -o -iname 'lsd.jar' -o -iname '*.log' -o -iname 'BT_MAC' \
        -o -iname '*.properties' -o -iname '*.lua' -o -iname 'iap2.cfg' -o -iname '*.json' \) >&2
    fail "w drzewie karty sa pliki, ktore nie powinny wyjsc z komputera"
fi
for d in backup collected; do
    [ ! -e "$CARD/mod/$PKG/$d" ] || fail "w drzewie karty jest $d/"
done
# 3f. tylko znane katalogi glowne - nic, co nadpisaloby pliki M.I.B.
top="$(cd "$CARD" && ls -A | LC_ALL=C sort | tr '\n' ' ')"
[ "$top" = "README_CarPlay_VC.txt mod " ] || fail "nieoczekiwana zawartosc katalogu glownego: $top"
echo "kontrole: OK"

# ------------------------------------------------------------------ 4. zipy
# Python, bo w Git Bash nie ma zip. Stala data wpisow i sortowanie: ten sam
# stan zrodel daje ten sam zip.
step "zipy"
python3 - "$CARD" "$REL/$NAME-install.zip" "$UNCARD" "$REL/$NAME-uninstall.zip" "$LOGCARD" "$REL/$NAME-collect-logs.zip"     "$REL/vc-policy-rgi-card" "$REL/$NAME-vc-policy-rgi.zip" "$REL/vc-policy-map-card" "$REL/$NAME-vc-policy-map.zip"     "$REL/vc-policy-off-card" "$REL/$NAME-vc-policy-off.zip" <<'PY'
import os, sys, zipfile
def pack(root, out):
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for d, dirs, files in os.walk(root):
            dirs.sort()
            rel_d = os.path.relpath(d, root).replace(os.sep, "/")
            if rel_d != ".":
                zi = zipfile.ZipInfo(rel_d + "/", (2026, 1, 1, 0, 0, 0))
                zi.external_attr = 0o40755 << 16
                z.writestr(zi, b"")
            for f in sorted(files):
                p = os.path.join(d, f)
                arc = os.path.relpath(p, root).replace(os.sep, "/")
                zi = zipfile.ZipInfo(arc, (2026, 1, 1, 0, 0, 0))
                zi.external_attr = 0o100644 << 16
                zi.compress_type = zipfile.ZIP_DEFLATED
                with open(p, "rb") as fh:
                    z.writestr(zi, fh.read())
    print("%-60s %10d B" % (os.path.basename(out), os.path.getsize(out)))
pack(sys.argv[1], sys.argv[2])
pack(sys.argv[3], sys.argv[4])
pack(sys.argv[5], sys.argv[6])
for k in range(7, len(sys.argv), 2):
    pack(sys.argv[k], sys.argv[k + 1])
PY
( cd "$REL" && sha256sum "$NAME-install.zip" "$NAME-uninstall.zip" "$NAME-collect-logs.zip" "$NAME-vc-policy-rgi.zip" "$NAME-vc-policy-map.zip" "$NAME-vc-policy-off.zip" > SHA256SUMS.txt && cat SHA256SUMS.txt )
( cd "$CARD" && find . -type f | LC_ALL=C sort | xargs sha256sum ) > "$LOGS/card-SHA256SUMS.txt"

# Zip sprawdzony od strony odbiorcy: rozpakowany ma dac dokladnie drzewo karty.
python3 - "$REL/$NAME-install.zip" "$CARD" <<'PY'
import hashlib, os, sys, zipfile
z = zipfile.ZipFile(sys.argv[1]); root = sys.argv[2]
names = sorted(n for n in z.namelist() if not n.endswith("/"))
disk = sorted(os.path.relpath(os.path.join(d, f), root).replace(os.sep, "/")
              for d, _, fs in os.walk(root) for f in fs)
assert names == disk, "zip != karta"
for n in names:
    a = hashlib.sha256(z.read(n)).hexdigest()
    b = hashlib.sha256(open(os.path.join(root, n), "rb").read()).hexdigest()
    assert a == b, n
print("install.zip == card/: %d plikow" % len(names))
PY

rm -rf "$STAGE"
echo
echo "RELEASE=$VER"
echo "KATALOG=$REL"
echo "Dalej: symulacja na tych zipach (BUILDING.md, 'Release'), potem test w aucie."
echo "MAKE_RELEASE=PASS"
