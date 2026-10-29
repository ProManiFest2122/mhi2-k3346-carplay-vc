#!/usr/bin/env bash
# Sklada paczke Etapu 7 (warstwa graficzna na VC) do work/sd_etap7/.
#
# DLACZEGO OSOBNY SKRYPT, A NIE make_sd_package.sh
# make_sd_package.sh robi `rm -rf` na work/sd_etap5 i odtwarza CALA paczke ze
# swiezych buildow. To by podmienilo rowniez pliki, ktore sa juz w aucie
# i zostaly tam przetestowane - a swiezy build biblioteki v8s NIE wychodzi
# dzis bit w bit taki, jak ten wgrany w aucie (058413dd... vs b8da2da0...).
# Etap 7 jest warstwa DOKLADANA na istniejaca karte, dokladnie tak jak Etapy 4
# i 5, wiec niesie tylko swoje wlasne pliki i nie dotyka reszty.
#
# Zawartosc paczki:
#   mod/command.sh                      - instalator (z obsluga Etapu 7)
#   mod/<PKG>/payload/vc_overlay        - renderer nakladki
#   mod/<PKG>/payload/vc_overlay_probe  - sonda Etapu 7a
#   mod/<PKG>/payload/mib2high-overlay.conf
#   mod/<PKG>/payload/flag_atlas.rgba    - tekstura flagi celu (896 kB)
#   mod/<PKG>/payload/k3346-overlay-probe.sh
#   mod/<PKG>/payload/mib2high-lvds-control.sh            (aktualizacja v8s)
#   mod/<PKG>/payload/libmib2high-carplay-rgi-ipod.etap7.so (aktualizacja Etapu 4)
#   ZNACZNIKI/{PROBE_ETAP7,INSTALL_ETAP7,ROLLBACK_ETAP7}
#
# DLACZEGO BIBLIOTEKA MA SUFIKS .etap7
# Etap 4 instaluje z payloadu plik o nazwie libmib2high-carplay-rgi-ipod.so.
# Gdyby Etap 7 wlozyl tam swoja wersje pod ta sama nazwa, INSTALL_ETAP4 (ktory
# na karcie uzytkownika czeka uzbrojony) zainstalowalby biblioteke Etapu 7
# zamiast swojej, a ROLLBACK_ETAP7 odtwarzalby stan "sprzed Etapu 7", ktorym
# bylaby ta sama biblioteka - czyli nie cofalby niczego. Osobna nazwa w
# payloadzie trzyma obie transakcje rozdzielnie.
set -euo pipefail

PROJ="$(cd "$(dirname "$0")/.." && pwd)"
A="$PROJ/port_k3346/altscreen"
OUT="${SD_OUT:-$PROJ/work/sd_etap7}"
PKG=mib2high-carplay-rgi-v8s-simple

fail(){ echo "FAIL: $*" >&2; exit 1; }

set -- \
  "vc_overlay|$A/overlay/output/vc_overlay|755" \
  "vc_overlay_probe|$A/overlay/output/vc_overlay_probe|755" \
  "mib2high-overlay.conf|$A/config/mib2high-overlay.conf|644" \
  "flag_atlas.rgba|$A/overlay/resources/flag_atlas.rgba|644" \
  "k3346-overlay-probe.sh|$A/deployment/k3346-overlay-probe.sh|755" \
  "mib2high-lvds-control.sh|$A/deployment/mib2high-lvds-control.sh|755" \
  "libmib2high-carplay-rgi-ipod.etap7.so|$A/output/libmib2high-carplay-rgi-ipod.so|755"
SPECS=("$@")

rm -rf "$OUT"
mkdir -p "$OUT/mod/$PKG/payload" "$OUT/ZNACZNIKI"

[ -f "$A/deployment/custom.sh" ] || fail "brak instalatora custom.sh"
cp "$A/deployment/custom.sh" "$OUT/mod/command.sh"
chmod 755 "$OUT/mod/command.sh"

for spec in "${SPECS[@]}"; do
    name="${spec%%|*}"; rest="${spec#*|}"; src="${rest%%|*}"; mode="${rest##*|}"
    [ -s "$src" ] || fail "brak artefaktu: $src (zbuduj go najpierw)"
    cp "$src" "$OUT/mod/$PKG/payload/$name"
    chmod "$mode" "$OUT/mod/$PKG/payload/$name"
done

# Znaczniki jako gotowe puste pliki: w Eksploratorze Windows "Nowy -> Dokument
# tekstowy" daje PROBE_ETAP7.txt, ktorego dyspozytor nie rozpozna.
for m in PROBE_ETAP7 INSTALL_ETAP7 ROLLBACK_ETAP7; do : > "$OUT/ZNACZNIKI/$m"; done

# --- kontrole, ktore maja zlapac bledy TU, a nie w aucie -------------------

# 1. Konce linii: wszystko, co uruchamia QNX, musi byc LF.
bad_crlf=""
for f in "$OUT/mod/command.sh" \
         "$OUT/mod/$PKG/payload/k3346-overlay-probe.sh" \
         "$OUT/mod/$PKG/payload/mib2high-lvds-control.sh" \
         "$OUT/mod/$PKG/payload/mib2high-overlay.conf"; do
    if LC_ALL=C grep -q $'\r' "$f"; then bad_crlf="$bad_crlf $f"; fi
done
[ -z "$bad_crlf" ] || fail "CRLF w plikach dla auta:$bad_crlf"

# 2. Architektura: binarki musza byc ARM, nie x86 z pomylki.
for b in vc_overlay vc_overlay_probe libmib2high-carplay-rgi-ipod.etap7.so; do
    case "$(file -b "$OUT/mod/$PKG/payload/$b")" in
        *ARM*) : ;;
        *) fail "$b nie jest binarka ARM" ;;
    esac
done

# 3. Zaleznosci bibliotek nakladki - libiow/libscreen nie istnieja w aucie.
python3 "$PROJ/tools/verify_overlay_elf.py" \
    "$OUT/mod/$PKG/payload/vc_overlay" \
    "$OUT/mod/$PKG/payload/vc_overlay_probe" || fail "weryfikacja ELF nakladki"

# 4. Sumy kontrolne calej paczki.
( cd "$OUT" && find . -type f ! -name SHA256SUMS.txt -print0 \
    | sort -z | xargs -0 sha256sum > SHA256SUMS.txt )

echo
echo "PAKIET: $OUT"
find "$OUT" -type f | sort | sed "s|$OUT/|  |"
echo
echo "Plikow: $(find "$OUT" -type f | wc -l | tr -d ' ')   Rozmiar: $(du -sh "$OUT" | cut -f1)"
echo "SD_ETAP7_PACKAGE=OK"
