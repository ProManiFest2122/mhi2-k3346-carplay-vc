#!/usr/bin/env bash
# Sklada KARTE RATUNKOWA na bootloop HMI: M.I.B. 3.6.0 (ta sama wersja co w aucie)
# + wlaczony Swdlautorun.txt + wywolanie ratunek/ratunek.sh w Launcher/stuff.sh
# + fabryczny traceConfig.properties zebrany z auta.
#
# Czego NIE rusza: metainfo2.txt (podpisany, sekcja [Signature]) i
# Launcher/final/finalScript.sh (jego SHA1 jest w metainfo2.txt). Jedynym
# zmienianym plikiem M.I.B. jest Launcher/stuff.sh - to zaprojektowane miejsce
# na wlasny kod ("no need to touch metainfo2.txt as this is outside of its reach").
#
# Wynik: work/sd_ratunek/  (TO MA BYC OSOBNA KARTA - patrz RATUNEK_README.txt)

set -euo pipefail

PROJ="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
MIB_SRC=$(ls -d "$PROJ"/work/downloads/mib_3.6.0/x/Mr-MIBonk-* 2>/dev/null | head -1)
CAR_TRACE="$PROJ/work/sd_odczyt_1/MIB2_SOFTWARE_COLLECTOR_v1.2_PID_8523907/files/mnt/app/eso/hmi/lsd/traceConfig.properties"
REF_TRACE="$PROJ/work/mmx_app/eso/hmi/lsd/traceConfig.properties"
OUT="$PROJ/work/sd_ratunek"

fail(){ echo "FAIL: $*" >&2; exit 1; }

[ -n "$MIB_SRC" ] && [ -d "$MIB_SRC" ] || fail "brak M.I.B. 3.6.0 w work/downloads/mib_3.6.0/x/"
[ -f "$MIB_SRC/VERSION" ] || fail "brak pliku VERSION w M.I.B."

# --- 1. Integralnosc M.I.B.: finalScript.sh musi miec SHA1 z metainfo2.txt ---
want=$(sed -n 's/^FinalScriptChecksum = "\([0-9a-f]*\)".*/\1/p' "$MIB_SRC/metainfo2.txt" | head -1)
got=$(sha1sum "$MIB_SRC/Launcher/final/finalScript.sh" | cut -d' ' -f1)
[ -n "$want" ] && [ "$want" = "$got" ] || fail "finalScript.sh SHA1 $got != metainfo $want"

# --- 2. Kopia M.I.B. bez zmian ---
rm -rf "$OUT"
mkdir -p "$OUT"
cp -a "$MIB_SRC/." "$OUT/"

# --- 3. Uzbrojenie: _Swdlautorun.txt -> Swdlautorun.txt (pusty) ---
[ -f "$OUT/_Swdlautorun.txt" ] || fail "brak _Swdlautorun.txt w M.I.B."
mv "$OUT/_Swdlautorun.txt" "$OUT/Swdlautorun.txt"
[ ! -s "$OUT/Swdlautorun.txt" ] || fail "Swdlautorun.txt nie jest pusty"

# --- 4. Wywolania ratunku w stuff.sh ---
#  a) zaraz po naglowku: sam ratunek (najpierw - zanim M.I.B. zacznie cokolwiek);
#  b) po 'launcher -all': dopisanie logu na karte. Dopiero wtedy karta jest na
#     pewno zapisywalna - M.I.B. robi 'mount -uw' w apps/mounts, wolanym przez
#     backup/launcher (lekcja z testu w aucie 2026-10-20). Plus jedna linia-slad
#     pisana wprost przez stuff.sh: przy kolejnym tescie ZAWSZE cos sie zapisze,
#     nawet gdyby ratunek.sh nie zostal znaleziony.
ANCHOR='# add echo to output debugging information to RCC log on UART'
ANCHOR2='${2}/apps/launcher -all'
for a in "$ANCHOR" "$ANCHOR2"; do
    n=$(grep -cxF "$a" "$MIB_SRC/Launcher/stuff.sh" || true)
    [ "$n" = 1 ] || fail "kotwica '$a' w stuff.sh wystepuje $n razy (oczekiwano 1)"
done
awk -v anchor="$ANCHOR" -v anchor2="$ANCHOR2" '
{ print }
$0 == anchor {
  print ""
  print "# --- KARTA RATUNKOWA K3346 (dodane) - patrz ratunek/ratunek.sh ---------------"
  print "# Uruchamiane jako osobny proces (nie wczytywane), zeby exit w nim nie przerwal"
  print "# reszty tego skryptu. Nosnik z $2, a w razie czego /net/mmx/fs/sda0."
  print "for R_M in \"${2}\" /net/mmx/fs/sda0; do"
  print "    if [ -n \"$R_M\" ] && [ -f \"$R_M/ratunek/ratunek.sh\" ]; then"
  print "        /bin/ksh \"$R_M/ratunek/ratunek.sh\" \"${1}\" \"$R_M\""
  print "        break"
  print "    fi"
  print "done"
  print "# --- koniec bloku ratunkowego ------------------------------------------------"
}
$0 == anchor2 {
  print ""
  print "# --- KARTA RATUNKOWA K3346 (dodane): log na karte, gdy M.I.B. juz ja odblokowal"
  print "R_JEST=brak"
  print "for R_M in \"${2}\" /net/mmx/fs/sda0; do"
  print "    if [ -n \"$R_M\" ] && [ -f \"$R_M/ratunek/ratunek.sh\" ]; then"
  print "        R_JEST=jest"
  print "        /bin/ksh \"$R_M/ratunek/ratunek.sh\" --zapisz-log \"$R_M\""
  print "        break"
  print "    fi"
  print "done"
  print "echo \"STUFF_KONIEC nosnik=${2} ratunek.sh=$R_JEST\" >> \"${2}/RATUNEK_LOG.txt\" 2>/dev/null"
  print "# --- koniec bloku ratunkowego ------------------------------------------------"
}' "$MIB_SRC/Launcher/stuff.sh" > "$OUT/Launcher/stuff.sh"
chmod 755 "$OUT/Launcher/stuff.sh"

# --- 5. Skrypt ratunkowy i fabryczny traceConfig ---
mkdir -p "$OUT/ratunek"
cp "$PROJ/port_k3346/ratunek/ratunek.sh" "$OUT/ratunek/ratunek.sh"
chmod 755 "$OUT/ratunek/ratunek.sh"
[ -s "$CAR_TRACE" ] || fail "brak traceConfig.properties zebranego z auta"
c=$(sha256sum < "$CAR_TRACE" | cut -d' ' -f1); r=$(sha256sum < "$REF_TRACE" | cut -d' ' -f1)
[ "$c" = "$r" ] || fail "traceConfig z auta ($c) != z backupu ($r)"
cp "$CAR_TRACE" "$OUT/ratunek/traceConfig.properties"      # bajt w bajt, z CRLF
[ "$(sha256sum < "$OUT/ratunek/traceConfig.properties" | cut -d' ' -f1)" = "$c" ] \
  || fail "kopia traceConfig rozni sie od oryginalu"

# --- 6. README ---
cp "$PROJ/port_k3346/ratunek/RATUNEK_README.txt" "$OUT/RATUNEK_README.txt"

# --- 7. Skrypty uruchamiane na QNX musza byc LF ---
for f in Launcher/stuff.sh ratunek/ratunek.sh Launcher/final/finalScript.sh; do
    if LC_ALL=C grep -qU $'\r' "$OUT/$f"; then fail "CRLF w $f"; fi
done

# --- 8. Kontrola: zmieniony jest tylko stuff.sh (i dodane/przemianowane pliki) ---
changed=$(cd "$MIB_SRC" && find . -type f | while read -r f; do
    [ -f "$OUT/$f" ] || continue
    cmp -s "$f" "$OUT/$f" || echo "$f"
done)
[ "$changed" = "./Launcher/stuff.sh" ] || fail "zmienione pliki M.I.B.: $changed (oczekiwano tylko stuff.sh)"

( cd "$OUT" && find . -type f ! -name SHA256SUMS.txt | LC_ALL=C sort | xargs -d '\n' sha256sum > SHA256SUMS.txt )

echo "KARTA RATUNKOWA GOTOWA: $OUT"
echo "M.I.B.: $(head -1 "$OUT/VERSION")   finalScript.sh SHA1=$got (zgodny z metainfo2.txt)"
echo "Uzbrojona: Swdlautorun.txt (pusty) w katalogu glownym"
echo "Zmieniony plik M.I.B.: tylko Launcher/stuff.sh"
echo "traceConfig na karcie = z auta: $c"
echo "Plikow: $(find "$OUT" -type f | wc -l)   Razem: $(du -sh "$OUT" | cut -f1)"
