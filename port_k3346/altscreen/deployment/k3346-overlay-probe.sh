#!/bin/sh
# K3346 Etap 7a-bis - sonda diagnostyczna warstwy graficznej na Virtual Cockpit.
#
# CZEGO SZUKAMY
# Czy DisplayManager MHI2 przyjmie okno utworzone przez nasz proces jako
# displayable, ktory da sie zlozyc nad mapa CarPlay. Luka robi to na MHI2Q
# przez QNX Screen; K3346 nie ma libscreen, wiec idziemy przez rozszerzenie KD.
#
# CZEGO NAUCZYLA POPRZEDNIA WIZYTA (2026-10-26)
#  1. `dmdt gd` i `dmdt gs` zwracaja na tej jednostce ZERO bajtow. `gc` dziala.
#     Sprawdzian "czy nasz numer jest na liscie displayable" byl wiec slepy -
#     odpowiadal "nie" niezaleznie od stanu. Dlatego glownym dowodem jest teraz
#     WLASNY LOG RENDERERA (kd_surface.c loguje kazdy krok z errno), a `dmdt`
#     jest rozpoznawany, nie uzywany jako wyrocznia.
#  2. Caly glowny blok poprzedniej sondy siedzial w funkcji z `shift 4` i `$*`.
#     Przy `set -u` i zerze parametrow pozycyjnych powloki z rodziny ksh zglaszaja
#     "parameter not set" i przerywaja FUNKCJE, wracajac do wywolujacego - skrypt
#     leci dalej, jakby nic. Stad brak jakiegokolwiek sladu w logu.
#     Teraz: zadnego `shift`, zadnego `$*`/`$@`. Pomocnik `dmr` bierze dokladnie
#     dwa argumenty przez $1/$2 - tak samo jak funkcje w custom.sh, ktore na tej
#     jednostce dzialaja od miesiecy.
#  3. Komunikaty bledow szly na ekran MMI, a nie do pliku. Teraz `exec 2>>` na
#     starcie laduje caly stderr w probe.log.
#  4. `rc=$?` nigdy w jednym napisie z podstawieniem polecen - `wc` po drodze
#     nadpisalby status. Kazdy kod wyjscia ladowany do zmiennej osobna linia.
#
# CZEGO NIE ROBI
# Nie instaluje i nie zmienia zadnego pliku w aucie poza logami i katalogiem
# wynikow. Kontekst wyswietlacza przelacza na chwile i sam przywraca 77; gdyby
# zostala ubita w polowie, petla w mib2high-lvds-control.sh i tak wymusza swoj
# kontekst co 3 s, a przy koncu sesji wraca na 72. Poprzednia wizyta potwierdzila
# to w praktyce.
#
# Uzycie: k3346-overlay-probe.sh <katalog-wynikow>
set -u

OUT="${1:?uzycie: k3346-overlay-probe.sh <katalog-wynikow>}"
PROBE="${OVL_PROBE_BIN:-/mnt/app/root/vc_overlay_probe}"
CONF="${OVL_CONF:-/mnt/app/root/mib2high-overlay.conf}"
DMDT="${OVL_DMDT:-/eso/bin/apps/dmdt}"
WAIT_USER="${OVL_WAIT_USER:-20}"
DISPLAY_CLUSTER=4
DISPLAY_MMI=0
LOG="$OUT/probe.log"
LD_FULL=/mnt/app/armle/lib:/mnt/app/armle/usr/lib:/mnt/app/root/lib-target:/eso/lib:/armle/lib:/armle/lib/dll:/armle/usr/lib:/lib:/usr/lib

mkdir -p "$OUT" 2>/dev/null

# Caly stderr do pliku. Poprzednio szedl na ekran MMI i przepadl - to byl powod,
# dla ktorego nie wiedzielismy, czemu sonda milczy.
exec 2>>"$LOG"

say(){ echo "[PROBE] $1"; echo "`date 2>/dev/null` $1" >> "$LOG" 2>/dev/null; }

# dmr NAZWA PODPOLECENIE - uruchom dmdt, zapisz wynik do pliku, zaloguj kod
# wyjscia i rozmiar OSOBNO. "pusty plik" i "polecenie padlo" to dwie rozne
# diagnozy, a poprzednio nie dalo sie ich odroznic.
dmr(){
    DMR_RC=0
    IPL_CONFIG_DIR=/etc/eso/production LD_LIBRARY_PATH=/eso/lib \
        "$DMDT" "$2" > "$OUT/$1" 2>>"$LOG" || DMR_RC=$?
    DMR_SZ=`wc -c < "$OUT/$1" 2>/dev/null || echo "?"`
    say "dmdt $2 -> $1  rc=$DMR_RC  bytes=$DMR_SZ"
}

shot(){
    SH_RC=0
    IPL_CONFIG_DIR=/etc/eso/production LD_LIBRARY_PATH=/eso/lib \
        "$DMDT" ts "$2" "$OUT/$1" >>"$LOG" 2>&1 || SH_RC=$?
    SH_SZ=`wc -c < "$OUT/$1" 2>/dev/null || echo "?"`
    say "zrzut $1 (display $2)  rc=$SH_RC  bytes=$SH_SZ"
}

say "START pid=$$"
say "probe=$PROBE"

# ODMOWA, jesli wlasciwy renderer zyje.
#
# Od Etapu 7b nakladka naprawde bierze displayable 98 (przez CAPTION okna).
# DisplayManager NIE sprawdza duplikatow - dwa okna z tym samym numerem to
# zachowanie nieokreslone (docs/19). Do tego sonda deklaruje wlasne konteksty,
# a petla w mib2high-lvds-control.sh wymusza swoj co 3 s: byliby dwaj pisarze
# kontekstu naraz. Wczesniej bylo to nieszkodliwe, bo zadne okno nie bylo
# przyjmowane - teraz jest.
#
# Dlatego sonde uruchamia sie PRZED podlaczeniem iPhone'a, kiedy renderer
# jeszcze nie wstal. Wzorzec wyklucza vc_overlay_probe, zeby sonda nie
# odmowila z powodu samej siebie.
if pidin 2>/dev/null | grep vc_overlay 2>/dev/null | grep -v vc_overlay_probe >/dev/null 2>&1; then
    say "renderer vc_overlay DZIALA - sonda nie wystartuje"
    say "odlacz iPhone (albo zakoncz sesje CarPlay) i uruchom skrypt ponownie"
    say "PROBE_RESULT=REFUSED_OVERLAY_RUNNING"
    exit 0
fi
say "renderer vc_overlay nie dziala - mozna probowac"
say "conf=$CONF"
say "dmdt=$DMDT"
if [ -x "$PROBE" ]; then say "binarka sondy: jest"; else say "binarka sondy: BRAK"; fi
if [ -x "$DMDT" ];  then say "dmdt: jest";          else say "dmdt: BRAK"; fi

OVL_ID=`awk '$1=="displayable" { print $2; exit }' "$CONF" 2>/dev/null || true`
case "${OVL_ID:-}" in ''|*[!0-9]*) OVL_ID=98 ;; esac
say "displayable=$OVL_ID"

# ---------------------------------------------------------------------------
# 1. Rozpoznanie dmdt: ktore podpolecenia w ogole cos zwracaja.
# ---------------------------------------------------------------------------
say "--- rozpoznanie dmdt ---"
dmr dm-gc.txt gc      # znany punkt odniesienia: poprzednio 11264 B
dmr dm-gd.txt gd      # poprzednio 0 B
dmr dm-gs.txt gs      # poprzednio 0 B

# Czy to kwestia srodowiska? Dwa warianty wpisane wprost, bez listy argumentow.
RC=0
LD_LIBRARY_PATH=/eso/lib "$DMDT" gd > "$OUT/dm-gd-noipl.txt" 2>>"$LOG" || RC=$?
SZ=`wc -c < "$OUT/dm-gd-noipl.txt" 2>/dev/null || echo "?"`
say "dmdt gd bez IPL_CONFIG_DIR  rc=$RC  bytes=$SZ"

RC=0
IPL_CONFIG_DIR=/etc/eso/production LD_LIBRARY_PATH="$LD_FULL" "$DMDT" gd > "$OUT/dm-gd-fullld.txt" 2>>"$LOG" || RC=$?
SZ=`wc -c < "$OUT/dm-gd-fullld.txt" 2>/dev/null || echo "?"`
say "dmdt gd pelny LD_LIBRARY_PATH  rc=$RC  bytes=$SZ"

# Bez argumentow binarka powinna wypisac usage. Gdyby i to bylo puste, problem
# jest w uruchomieniu, a nie w podpoleceniu.
RC=0
IPL_CONFIG_DIR=/etc/eso/production LD_LIBRARY_PATH=/eso/lib "$DMDT" > "$OUT/dm-usage.txt" 2>>"$LOG" || RC=$?
SZ=`wc -c < "$OUT/dm-usage.txt" 2>/dev/null || echo "?"`
say "dmdt bez argumentow  rc=$RC  bytes=$SZ"

# ---------------------------------------------------------------------------
# 2. Stan wyjsciowy. `dmdt ts` to jedyny przyrzad, ktory na pewno dziala -
#    z niego zmierzylismy panel manewru co do piksela
#    (210x153 @ 1091,110 oraz @ 1055,207).
# ---------------------------------------------------------------------------
shot vc-00-before.png "$DISPLAY_CLUSTER"
shot mmi-00-before.png "$DISPLAY_MMI"

if [ ! -x "$PROBE" ]; then
    say 'BRAK binarki sondy - dalej nie ma czego sprawdzac'
    say 'PROBE_RESULT=NO_BINARY'
    exit 0
fi

# ---------------------------------------------------------------------------
# 3. Wariant A: rozmiar zmierzony w aucie (panel manewru).
# ---------------------------------------------------------------------------
say "--- wariant A: 210x153 (zmierzony panel manewru) ---"
slay vc_overlay_probe >/dev/null 2>&1 || true
rm -f /tmp/vc-overlay-probe.log 2>/dev/null || true

LD_LIBRARY_PATH="$LD_FULL" on -p 25 "$PROBE" --conf "$CONF" --w 210 --h 153 --secs 120 \
    >> "$OUT/A-launch.log" 2>&1 &
APID=$!
say "sonda A wystartowana w tle, pid=$APID"
sleep 4

# DOWOD GLOWNY: wlasny log sondy. Mowi, czy kdCreateWindow, CAPTION, SIZE
# i kdRealizeWindow przeszly i z jakim errno - bez posrednictwa dmdt.
# `dmdt gd` tu nie pomoze: numer displayable siedzi na oknie (w jego captionie),
# a nie na zadnej liscie, ktora gd drukuje. Patrz docs/19_displayable_na_mhi2.md.
if [ -f /tmp/vc-overlay-probe.log ]; then
    cp /tmp/vc-overlay-probe.log "$OUT/A-vc-overlay-probe.log" 2>/dev/null
    ASZ=`wc -c < /tmp/vc-overlay-probe.log 2>/dev/null || echo "?"`
    say "log sondy A: $ASZ B"
    grep -E 'kdInitializeNV|CAPTION|DISPLAY|SIZE=|SOURCE_ALPHA|realized|FAIL|REFUSED|READY|layout slot|GL ' \
        /tmp/vc-overlay-probe.log 2>/dev/null | while read -r l; do say "  A| $l"; done
else
    say 'log sondy A: BRAK - binarka nie wystartowala albo padla przed logiem'
    if [ -f "$OUT/A-launch.log" ]; then
        head -20 "$OUT/A-launch.log" 2>/dev/null | while read -r l; do say "  A!| $l"; done
    fi
fi

dmr dm-gd-A.txt gd

# Indeks 0 = przod kompozycji, wiec nasza warstwa idzie PRZED mapa (58).
# Displayable 20 celowo pomijamy - to zrodlo zlej strzalki, ktore zastepujemy.
RC=0
IPL_CONFIG_DIR=/etc/eso/production LD_LIBRARY_PATH=/eso/lib "$DMDT" dc 901 "$OVL_ID" 102 101 58 >>"$LOG" 2>&1 || RC=$?
say "dc 901 {$OVL_ID,102,101,58}  rc=$RC"
RC=0
IPL_CONFIG_DIR=/etc/eso/production LD_LIBRARY_PATH=/eso/lib "$DMDT" sc "$DISPLAY_CLUSTER" 901 >>"$LOG" 2>&1 || RC=$?
say "sc 4 901  rc=$RC"
sleep 2
shot vc-10-A-ctx901.png "$DISPLAY_CLUSTER"

# Przelaczenie VIEW kaze HMI zmienic kontekst po swojemu, a my trzymamy 901.
# Fabryczny widok VC moze na moment wrocic - to obserwacja, nie awaria, i warto
# ja miec w logu, bo wlasciwy renderer bedzie mial ten sam konflikt.
say '>>> TERAZ: nacisnij VIEW (drugi uklad zegarow). Fabryczny widok moze mrugnac <<<'
sleep "$WAIT_USER"
shot vc-11-A-view2.png "$DISPLAY_CLUSTER"

# Kontrola: kontekst bez podkladek KDK - sama nasza warstwa nad mapa.
RC=0
IPL_CONFIG_DIR=/etc/eso/production LD_LIBRARY_PATH=/eso/lib "$DMDT" dc 902 "$OVL_ID" 58 >>"$LOG" 2>&1 || RC=$?
say "dc 902 {$OVL_ID,58}  rc=$RC"
RC=0
IPL_CONFIG_DIR=/etc/eso/production LD_LIBRARY_PATH=/eso/lib "$DMDT" sc "$DISPLAY_CLUSTER" 902 >>"$LOG" 2>&1 || RC=$?
say "sc 4 902  rc=$RC"
sleep 2
shot vc-12-A-nobacking.png "$DISPLAY_CLUSTER"

# ---------------------------------------------------------------------------
# 4. Wariant B: pelne plotno. Od tego zalezy, czy karta rozmowy (Etap 7c) moze
#    trafic nad mape i istniec bez aktywnej trasy.
# ---------------------------------------------------------------------------
say "--- wariant B: 1440x542 (pelne plotno) ---"
slay vc_overlay_probe >/dev/null 2>&1 || true
rm -f /tmp/vc-overlay-probe.log 2>/dev/null || true
sleep 1

LD_LIBRARY_PATH="$LD_FULL" on -p 25 "$PROBE" --conf "$CONF" --w 1440 --h 542 --secs 60 \
    >> "$OUT/B-launch.log" 2>&1 &
BPID=$!
say "sonda B wystartowana w tle, pid=$BPID"
sleep 4

if [ -f /tmp/vc-overlay-probe.log ]; then
    cp /tmp/vc-overlay-probe.log "$OUT/B-vc-overlay-probe.log" 2>/dev/null
    grep -E 'kdInitializeNV|CAPTION|DISPLAY|SIZE=|SOURCE_ALPHA|realized|FAIL|REFUSED|READY|layout slot|GL ' \
        /tmp/vc-overlay-probe.log 2>/dev/null | while read -r l; do say "  B| $l"; done
else
    say 'log sondy B: BRAK'
fi

RC=0
IPL_CONFIG_DIR=/etc/eso/production LD_LIBRARY_PATH=/eso/lib "$DMDT" dc 903 "$OVL_ID" 58 >>"$LOG" 2>&1 || RC=$?
say "dc 903 {$OVL_ID,58}  rc=$RC"
RC=0
IPL_CONFIG_DIR=/etc/eso/production LD_LIBRARY_PATH=/eso/lib "$DMDT" sc "$DISPLAY_CLUSTER" 903 >>"$LOG" 2>&1 || RC=$?
say "sc 4 903  rc=$RC"
sleep 2
shot vc-20-B-full.png "$DISPLAY_CLUSTER"

# Druga przerwa: jedyne pytanie, na ktore ZADEN plik nie odpowie. Zrzuty `dmdt ts`
# nie zawieraja mapy CarPlay (mapa to warstwa sprzetowa, ustalone na wizycie 1),
# a przy odlaczonym iPhonie nie ma zadnej trasy do zakonczenia - stad polecenie
# z poprzedniej procedury zostalo zastapione tym, co da sie tu rozstrzygnac.
say '>>> TERAZ: popatrz na VC. Schodek alfy na dole: czy przez stopnie widac tlo? <<<'
sleep "$WAIT_USER"
shot vc-21-B-noroute.png "$DISPLAY_CLUSTER"

# ---------------------------------------------------------------------------
# 5. Sprzatanie: kontekst wraca na 77, sonda ubita.
# ---------------------------------------------------------------------------
slay vc_overlay_probe >/dev/null 2>&1 || true
sleep 1
RC=0
IPL_CONFIG_DIR=/etc/eso/production LD_LIBRARY_PATH=/eso/lib "$DMDT" sc "$DISPLAY_CLUSTER" 77 >>"$LOG" 2>&1 || RC=$?
say "sc 4 77 (przywrocenie)  rc=$RC"
shot vc-30-after.png "$DISPLAY_CLUSTER"
dmr dm-gd-after.txt gd

for f in /tmp/vc-overlay-probe.log /tmp/vc_overlay.log /tmp/vc_overlay-launch.log \
         /tmp/vc_render.log /tmp/mib2high-lvds-control.log; do
    if [ -f "$f" ]; then cp "$f" "$OUT/" 2>/dev/null; fi
done

sync 2>/dev/null || true
say 'PROBE_RESULT=DONE'
exit 0
