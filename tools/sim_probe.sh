#!/usr/bin/env bash
# Symulacja sondy Etapu 7 (k3346-overlay-probe.sh) na komputerze - BEZ AUTA.
#
# PO CO TO ISTNIEJE
# Przy pierwszej wizycie caly glowny blok sondy NIE WYKONAL SIE, a skrypt
# zakonczyl sie komunikatem PROBE_RESULT=DONE, jakby wszystko bylo w porzadku.
# W logu nie bylo ani sladu po dwoch wywolaniach `run_variant`, choc wszystko
# przed i po nich zapisalo sie normalnie.
#
# CZEGO NIE WIEMY
# Przyczyna pozostaje NIEUSTALONA. Sprawdzone i ODRZUCONE:
#   - `$*` po `shift 4` przy `set -u` - dziala poprawnie i w dash, i w mksh;
#   - uszkodzenie pliku na karcie - skrypt jest czysty, LF, skladnia poprawna;
#   - roznica powlok - stara sonda w tej symulacji przechodzi pod OBIEMA.
# Innymi slowy: awarii NIE DA SIE odtworzyc poza autem. Dlatego nowa sonda
# laduje caly stderr do probe.log (`exec 2>>`) - to jedyny sposob, zeby
# nastepnym razem dowiedziec sie, co naprawde powiedziala powloka.
#
# CO WIEC SPRAWDZA TEN TEST
# Nie przyczyne tamtej awarii, bo jej nie umiem odtworzyc. Sprawdza slabsza,
# ale uzyteczna wlasnosc: ze KAZDY blok nowej sondy zostawia slad w logu i w
# plikach wynikowych, oraz ze "pusty wynik" da sie odroznic od "polecenie padlo".
# "Skrypt sie nie wywalil" okazalo sie za slabym sprawdzianem - tamta sonda
# zakonczyla sie kodem 0.
# Uruchamiany pod dash i pod mksh (rodzina powloki QNX /bin/sh).
#
# Uruchamiac w kontenerze:
#   docker run --rm -v "$(pwd -W)/port_k3346/altscreen:/src" -v "$(pwd -W)/tools:/tools" \
#     qnx65-armv7-toolchain:4.9 bash -c 'bash /tools/sim_probe.sh'
set -u

PROBE_SRC="${PROBE_SRC:-/src/deployment/k3346-overlay-probe.sh}"
WORK=/tmp/sim_probe
PASS=0; FAILED=0
ok(){  PASS=$((PASS+1));   echo "  OK   $*"; }
bad(){ FAILED=$((FAILED+1)); echo "  FAIL $*"; }

# --- atrapy narzedzi auta ---------------------------------------------------
# dmdt: gc cos drukuje, gd/gs MILCZA - dokladnie jak na tej jednostce.
# To wazne: test ma potwierdzic, ze sonda radzi sobie ze slepym przyrzadem.
make_fakes(){
    mkdir -p "$WORK/bin"
    cat > "$WORK/bin/dmdt" <<'EOF'
#!/bin/sh
case "${1:-}" in
  gc) echo "displaymanager knows 88 contexts:"; echo "  77 | 4 | NONE"; exit 0 ;;
  gd) exit 0 ;;                 # zero bajtow, jak w aucie
  gs) exit 0 ;;                 # zero bajtow, jak w aucie
  dc) exit 0 ;;
  sc) exit 0 ;;
  ts) printf 'FAKEPNG' > "$3"; exit 0 ;;
  "") echo "[usage]: ./dmdt <command>"; exit 1 ;;
  *)  exit 2 ;;
esac
EOF
    cat > "$WORK/bin/vc_overlay_probe" <<'EOF'
#!/bin/sh
# Atrapa renderera: pisze taki log, jaki pisze prawdziwy kd_surface.c.
{
  echo "0.000 [probe] START pid=$$"
  echo "0.001 kd_surface: kdInitializeNV -> 0"
  echo '0.002 kd_surface: CAPTION="98"'
  echo "0.003 kd_surface: DISPLAY not set (stock recipe, conf display=-1)"
  echo "0.004 kd_surface: SIZE=210x153"
  echo '0.006 kd_surface: window caption="98" 210x153 display=-1 alpha=1 realized native=0x1234'
  echo "0.007 [probe] READY"
} >> /tmp/vc-overlay-probe.log
sleep 30
EOF
    # pidin: udaje liste procesow. Obecnosc /tmp/sim_probe_overlay_live znaczy,
    # ze wlasciwy renderer zyje - sonda musi wtedy ODMOWIC, bo inaczej dwa okna
    # mialyby ten sam caption 98 (DisplayManager nie sprawdza duplikatow).
    cat > "$WORK/bin/pidin" <<'EOF'
#!/bin/sh
echo "     1     1 /proc/boot/procnto-instr"
if [ -f /tmp/sim_probe_overlay_live ]; then
  echo "   123     1 /mnt/app/root/vc_overlay"
fi
if [ -f /tmp/sim_probe_probe_live ]; then
  echo "   456     1 /mnt/app/root/vc_overlay_probe"
fi
exit 0
EOF

    cat > "$WORK/bin/slay" <<'EOF'
#!/bin/sh
exit 0
EOF
    # `on -p 25 CMD ARGS` -> odpal CMD ARGS
    cat > "$WORK/bin/on" <<'EOF'
#!/bin/sh
while [ $# -gt 0 ]; do
  case "$1" in -p) shift 2 ;; -f) shift 2 ;; *) break ;; esac
done
exec "$@"
EOF
    chmod 755 "$WORK/bin"/*
}

run_probe(){   # run_probe <powloka>
    SHELL_BIN="$1"
    rm -rf "$WORK/out"; mkdir -p "$WORK/out"
    rm -f /tmp/vc-overlay-probe.log
    PATH="$WORK/bin:$PATH" \
      OVL_PROBE_BIN="$WORK/bin/vc_overlay_probe" \
      OVL_DMDT="$WORK/bin/dmdt" \
      OVL_CONF="$WORK/overlay.conf" \
      OVL_WAIT_USER=0 \
      "$SHELL_BIN" "$PROBE_SRC" "$WORK/out" > "$WORK/stdout.txt" 2>&1
    PROBE_RC=$?
}

check_shell(){ # check_shell <powloka> <etykieta>
    SH="$1"; LABEL="$2"
    echo
    echo "--- sonda pod $LABEL"
    if [ ! -x "$SH" ]; then echo "  (pominieto: brak $SH)"; return 0; fi
    run_probe "$SH"
    L="$WORK/out/probe.log"

    [ "$PROBE_RC" = 0 ] && ok "$LABEL: zakonczyla sie kodem 0" || bad "$LABEL: kod wyjscia $PROBE_RC"
    [ -f "$L" ] && ok "$LABEL: probe.log powstal" || { bad "$LABEL: brak probe.log"; return 0; }
    grep -q 'PROBE_RESULT=DONE' "$L" && ok "$LABEL: doszla do konca" || bad "$LABEL: nie doszla do konca"

    # SEDNO: kazdy blok musi zostawic slad. Brak ktoregokolwiek = cicha porazka,
    # czyli dokladnie to, co zdarzylo sie w aucie.
    for m in 'rozpoznanie dmdt' 'wariant A: 210x153' 'wariant B: 1440x542' \
             'sc 4 77 (przywrocenie)'; do
        grep -q "$m" "$L" && ok "$LABEL: blok '$m'" || bad "$LABEL: BRAK bloku '$m'"
    done

    # Rozpoznanie dmdt ma rozroznic "puste" od "padlo".
    grep -qE 'dmdt gc -> dm-gc.txt +rc=0 +bytes=[1-9]' "$L" \
        && ok "$LABEL: gc rozpoznane jako dzialajace" || bad "$LABEL: brak wyniku gc"
    grep -qE 'dmdt gd -> dm-gd.txt +rc=0 +bytes=0' "$L" \
        && ok "$LABEL: gd rozpoznane jako puste (rc=0, 0 B)" || bad "$LABEL: gd nie rozroznione"
    grep -q 'dmdt bez argumentow' "$L" && ok "$LABEL: proba bez argumentow" || bad "$LABEL: brak proby bez argumentow"

    # Dowod glowny: log renderera przepisany do wynikow i przefiltrowany.
    [ -f "$WORK/out/A-vc-overlay-probe.log" ] && ok "$LABEL: log renderera A zachowany" || bad "$LABEL: brak logu renderera A"
    grep -q 'A| .*kdRealizeWindow\|A| .*realized' "$L" && ok "$LABEL: wynik okna A w logu" || bad "$LABEL: brak wyniku okna A"
    grep -q 'B| ' "$L" && ok "$LABEL: wynik okna B w logu" || bad "$LABEL: brak wyniku okna B"

    # Zrzuty: wszystkie fazy.
    MISS=''
    for p in vc-00-before.png mmi-00-before.png vc-10-A-ctx901.png vc-11-A-view2.png \
             vc-12-A-nobacking.png vc-20-B-full.png vc-21-B-noroute.png vc-30-after.png; do
        [ -f "$WORK/out/$p" ] || MISS="$MISS $p"
    done
    [ -z "$MISS" ] && ok "$LABEL: wszystkie 8 zrzutow" || bad "$LABEL: brak zrzutow:$MISS"

    # Konteksty probowane w obu wariantach.
    for c in 'dc 901' 'sc 4 901' 'dc 902' 'dc 903'; do
        grep -q "$c" "$L" && ok "$LABEL: $c" || bad "$LABEL: brak $c"
    done
}

echo "=========================================================="
echo " SYMULACJA SONDY ETAPU 7 (bez auta)   $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "=========================================================="

rm -rf "$WORK"; mkdir -p "$WORK"
make_fakes
printf 'displayable 98\nwidth 210\nheight 153\n' > "$WORK/overlay.conf"

check_shell /bin/dash dash
check_shell /bin/mksh mksh
if [ ! -x /bin/dash ] && [ ! -x /bin/mksh ]; then check_shell /bin/sh "sh"; fi

# Brak binarki renderera: sonda ma to POWIEDZIEC, a nie udawac sukces.
echo
echo "--- brak binarki renderera -> jawny komunikat"
rm -rf "$WORK/out"; mkdir -p "$WORK/out"
PATH="$WORK/bin:$PATH" OVL_PROBE_BIN=/nie/ma OVL_DMDT="$WORK/bin/dmdt" \
  OVL_CONF="$WORK/overlay.conf" OVL_WAIT_USER=0 \
  sh "$PROBE_SRC" "$WORK/out" >/dev/null 2>&1
grep -q 'PROBE_RESULT=NO_BINARY' "$WORK/out/probe.log" \
  && ok "zglosila NO_BINARY zamiast cichego DONE" || bad "nie zglosila braku binarki"
[ -f "$WORK/out/vc-00-before.png" ] \
  && ok "mimo braku binarki zrzuty stanu wyjsciowego sa" || bad "brak zrzutow przy NO_BINARY"

# Od Etapu 7b nakladka naprawde bierze displayable 98. Sonda bierze ten sam
# numer, wiec nie wolno im dzialac razem.
echo
echo "--- renderer DZIALA -> sonda odmawia (duplikat captionu 98)"
rm -rf "$WORK/out"; mkdir -p "$WORK/out"
: > /tmp/sim_probe_overlay_live
PATH="$WORK/bin:$PATH" OVL_PROBE_BIN="$WORK/bin/vc_overlay_probe" OVL_DMDT="$WORK/bin/dmdt"   OVL_CONF="$WORK/overlay.conf" OVL_WAIT_USER=0   sh "$PROBE_SRC" "$WORK/out" >/dev/null 2>&1
grep -q 'PROBE_RESULT=REFUSED_OVERLAY_RUNNING' "$WORK/out/probe.log"   && ok "odmowila, bo renderer zyje" || bad "nie odmowila przy zywym rendererze"
grep -q 'odlacz iPhone' "$WORK/out/probe.log"   && ok "powiedziala, co zrobic" || bad "brak instrukcji dla kierowcy"
grep -q 'dmdt dc 90' "$WORK/out/probe.log"   && bad "deklarowala konteksty mimo odmowy" || ok "nie deklarowala zadnych kontekstow"
rm -f /tmp/sim_probe_overlay_live

echo
echo "--- sama sonda w pidin NIE blokuje sondy"
rm -rf "$WORK/out"; mkdir -p "$WORK/out"
: > /tmp/sim_probe_probe_live
PATH="$WORK/bin:$PATH" OVL_PROBE_BIN="$WORK/bin/vc_overlay_probe" OVL_DMDT="$WORK/bin/dmdt"   OVL_CONF="$WORK/overlay.conf" OVL_WAIT_USER=0   sh "$PROBE_SRC" "$WORK/out" >/dev/null 2>&1
grep -q 'PROBE_RESULT=REFUSED_OVERLAY_RUNNING' "$WORK/out/probe.log"   && bad "odmowila z powodu samej siebie" || ok "nie pomylila siebie z rendererem"
grep -q 'PROBE_RESULT=DONE' "$WORK/out/probe.log"   && ok "przeszla do konca" || bad "nie dokonczyla"
rm -f /tmp/sim_probe_probe_live

echo
echo "=========================================================="
echo " WYNIK: $PASS OK, $FAILED FAIL"
echo "=========================================================="
[ "$FAILED" -eq 0 ] || exit 1
