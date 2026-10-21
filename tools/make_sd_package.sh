#!/usr/bin/env bash
# Sklada paczke na karte SD dla K3346 z artefaktow zbudowanych w port_k3346/.
# Nie buduje niczego - tylko kopiuje gotowe pliki, sprawdza je i liczy sumy.
# Uruchamiac PO tools/qnx_build.sh i build_java_release.sh.
#
# Uklad wynikowy (= uklad oficjalnej paczki edzenisa, patrz
# port_k3346/altscreen/release_evidence/V8S_PACKAGE_MANIFEST.json), z jedna
# roznica: instalator nazywa sie command.sh, bo M.I.B. 3.6.0 uruchamia
# wylacznie /mod/command.sh.

set -euo pipefail

PROJ="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
A="$PROJ/port_k3346/altscreen"
OUT="${SD_OUT:-$PROJ/work/sd_etap5}"
PKG=mib2high-carplay-rgi-v8s-simple

fail(){ echo "FAIL: $*" >&2; exit 1; }

# --- zrodla artefaktow: payload_name -> sciezka zrodlowa, tryb ---
# Binarki z buildow, konfiguracja i skrypty ze zrodel.
set -- \
  "libmib2high-carplay-rgi.so|$A/output/libmib2high-carplay-rgi.so|755" \
  "mib2high-carplay-rgi.jar|$A/output/mib2high-carplay-rgi.jar|644" \
  "vc_render|$A/renderer/build/vc_render|755" \
  "v8l-dsitracer-capture|$A/diagnostics/v8l-dsitracer-capture|755" \
  "mib2high-lvds-control.sh|$A/deployment/mib2high-lvds-control.sh|755" \
  "mib2high-lvds-profile.conf|$A/config/mib2high-lvds-profile.conf|644" \
  "mib2high-lvds-viewareas.conf|$A/config/mib2high-lvds-viewareas.conf|644" \
  "libmib2high-carplay-rgi-ipod.so|$A/output/libmib2high-carplay-rgi-ipod.so|755" \
  "mm-ipod-rgi.sh|$A/deployment/etap4/mm-ipod-rgi.sh|755" \
  "mib2high-k3346-overrides.jar|$A/output/mib2high-k3346-overrides.jar|644" \
  "k3346-artwork-probe|$A/diagnostics/k3346-artwork-probe|755" \
  "k3346-coverart-fetch|$A/coverart/k3346-coverart-fetch|755"
PAYLOAD_SPECS=("$@")

rm -rf "$OUT"
mkdir -p "$OUT/mod/$PKG/payload" "$OUT/mod/$PKG/source" "$OUT/mod/$PKG/alt-profiles"

# --- instalator jako command.sh (to jego uruchamia M.I.B. 3.6.0) ---
[ -f "$A/deployment/custom.sh" ] || fail "brak instalatora custom.sh"
cp "$A/deployment/custom.sh" "$OUT/mod/command.sh"
chmod 755 "$OUT/mod/command.sh"

# --- payload ---
for spec in "${PAYLOAD_SPECS[@]}"; do
    name="${spec%%|*}"; rest="${spec#*|}"; src="${rest%%|*}"; mode="${rest##*|}"
    [ -s "$src" ] || fail "brak artefaktu: $src (zbuduj go najpierw)"
    cp "$src" "$OUT/mod/$PKG/payload/$name"
    chmod "$mode" "$OUT/mod/$PKG/payload/$name"
done

# --- transformacje awk uruchamiane na aucie ---
for f in bundles.awk smartphone.awk trace.awk; do
    [ -f "$A/deployment/$f" ] || fail "brak $f"
    cp "$A/deployment/$f" "$OUT/mod/$PKG/source/$f"
    chmod 644 "$OUT/mod/$PKG/source/$f"
done

# --- Etap 4: transformacje usblauncher_otg.lua i iap2.cfg ---
for f in usblauncher_otg.awk iap2.awk; do
    [ -f "$A/deployment/etap4/$f" ] || fail "brak etap4/$f"
    cp "$A/deployment/etap4/$f" "$OUT/mod/$PKG/source/$f"
    chmod 644 "$OUT/mod/$PKG/source/$f"
done

# --- alternatywny profil geometrii dla B9 ---
cp "$A/config/mib2high-lvds-profile.b9-1440x540.conf" \
   "$OUT/mod/$PKG/alt-profiles/mib2high-lvds-profile.b9-1440x540.conf"

# --- README ---
cp "$A/deployment/README.k3346.txt" "$OUT/README.txt"

# --- gotowe pliki-znaczniki (puste, BEZ rozszerzenia) ---
# Leza w katalogu glownym karty, poza mod/, wiec skrypt ich nie widzi, dopoki
# uzytkownik nie skopiuje jednego do mod/<pakiet>/. Dlaczego gotowe pliki,
# a nie instrukcja "utworz pusty plik":
#  - w Windows "Nowy > Dokument tekstowy" tworzy ROLLBACK.txt, ktorego
#    skrypt nie rozpozna (zamiast rollbacku zebralby logi);
#  - przy aucie zwykle nie ma czym utworzyc pliku, a skopiowac juz istniejacy
#    da sie z telefonu z czytnikiem kart albo z laptopa.
mkdir -p "$OUT/ZNACZNIKI"
: > "$OUT/ZNACZNIKI/ROLLBACK"
: > "$OUT/ZNACZNIKI/COLLECT_LOGS"
: > "$OUT/ZNACZNIKI/INSTALL_ETAP4"
: > "$OUT/ZNACZNIKI/ROLLBACK_ETAP4"
: > "$OUT/ZNACZNIKI/INSTALL_ETAP5"
: > "$OUT/ZNACZNIKI/ROLLBACK_ETAP5"

# --- kontrola: zadnych CRLF w plikach tekstowych idacych do auta ---
bad=0
while IFS= read -r f; do
    case "$f" in *.so|*.jar|*vc_render|*v8l-dsitracer-capture|*k3346-artwork-probe|*k3346-coverart-fetch) continue;; esac
    if LC_ALL=C grep -qU $'\r' "$f" 2>/dev/null; then echo "CRLF: $f" >&2; bad=1; fi
done < <(find "$OUT" -type f)
[ "$bad" -eq 0 ] || fail "w paczce sa pliki z CRLF - na QNX to psuje skrypty"

# --- kontrola: binarki to naprawde QNX ARM ---
for b in libmib2high-carplay-rgi.so libmib2high-carplay-rgi-ipod.so vc_render v8l-dsitracer-capture k3346-artwork-probe k3346-coverart-fetch; do
    case "$(file -b "$OUT/mod/$PKG/payload/$b")" in
        *ARM*) ;;
        *) fail "$b nie jest binarka ARM";;
    esac
done

# --- sumy kontrolne (dla nas, nie dla auta - MMI ich nie policzy) ---
( cd "$OUT" && find . -type f ! -name SHA256SUMS.txt | LC_ALL=C sort \
    | xargs sha256sum > SHA256SUMS.txt )

echo "PACZKA GOTOWA: $OUT"
echo
( cd "$OUT" && find . -type f | LC_ALL=C sort | while read -r f; do
    printf '%10d  %s\n' "$(wc -c < "$f")" "${f#./}"
  done )
echo
echo "Plikow: $(find "$OUT" -type f | wc -l)   Razem: $(du -sh "$OUT" | cut -f1)"
