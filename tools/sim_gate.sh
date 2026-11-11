#!/usr/bin/env bash
# Symulacja BRAMY kontekstu kombi w mib2high-lvds-control.sh (Etap 7b).
#
# PO CO TO ISTNIEJE. Brama decyduje, czy auto przechodzi z kontekstu 77 (mapa,
# dziala od Etapu 3) na 901 (mapa + nasza nakladka). Zla decyzja = kombi sklada
# displayable, ktorego nikt nie rysuje. Do tej pory ten kod nie mial zadnego
# testu: sim_installer sprawdza tylko, czy plik trafil na miejsce.
#
# Etap 7a pytal o gotowosc `dmdt gd` i dostawal zero bajtow. Teraz brama pyta
# samego renderera, przez /tmp/vc-overlay-ready. Ten plik jest dowodem, wiec
# kazdy sposob, w jaki moglby sklamac, ma tu swoj przypadek - zwlaszcza meldunek
# NIESWIEZY, zostawiony przez renderer ubity na twardo.
#
# Uruchamiac w kontenerze (potrzebuje /mnt i /eso, wiec nie na Windows):
#   docker run --rm -v "$(pwd -W)/port_k3346/altscreen:/src" -v "$(pwd -W)/tools:/tools" \
#     qnx65-armv7-toolchain:4.9 bash -c 'bash /tools/sim_gate.sh'
# Pod mksh (rodzina powloki QNX /bin/sh):
#   ... bash -c 'apt-get update -qq && apt-get install -y -qq mksh && bash /tools/sim_gate.sh'
set -u

CTL_SRC="${CTL_SRC:-/src/deployment/mib2high-lvds-control.sh}"
CONF_SRC="${CONF_SRC:-/src/config/mib2high-overlay.conf}"
PASS=0; FAILED=0
ok(){  PASS=$((PASS+1));     echo "  OK   $*"; }
bad(){ FAILED=$((FAILED+1)); echo "  FAIL $*"; }

[ -r "$CTL_SRC" ]  || { echo "brak $CTL_SRC"; exit 2; }
[ -r "$CONF_SRC" ] || { echo "brak $CONF_SRC"; exit 2; }

CTLLOG=/tmp/mib2high-lvds-control.log
DMLOG=/tmp/sim_gate_dmdt.log

# --- atrapy narzedzi auta ----------------------------------------------------
# Skrypt sam nadpisuje PATH na katalogi QNX, wiec atrapy musza wyladowac tam,
# gdzie on potem patrzy. W jego PATH /bin stoi PRZED /mnt/app/armle/bin, wiec
# `sleep` podmieniamy w /bin - kontener jest --rm, to nic nie kosztuje.
setup_fakes(){
    mkdir -p /mnt/app/root /mnt/app/armle/bin /eso/bin/apps

    {
        echo '#!/bin/sh'
        echo 'echo "dmdt $*" >> /tmp/sim_gate_dmdt.log'
        # Renderer ma swoja sciezke bibliotek, dmdt potrzebuje /eso/lib.
        echo '[ "${LD_LIBRARY_PATH:-}" = /eso/lib ] || echo "badld $*" >> /tmp/sim_gate_dmdt.log'
        echo 'case "${1:-}" in'
        echo '  gd|gs) exit 0 ;;'
        echo '  dc) if [ -f /tmp/sim_gate_dc_fails ]; then exit 1; fi'
        echo '      if [ "$2" = 902 ] && [ -f /tmp/sim_gate_dc902_fails ]; then exit 1; fi; exit 0 ;;'
        echo '  *) exit 0 ;;'
        echo 'esac'
    } > /eso/bin/apps/dmdt

    # `on -p N CMD ARGS` -> odpal CMD ARGS
    {
        echo '#!/bin/sh'
        echo 'while [ $# -gt 0 ]; do'
        echo '  case "$1" in -p|-f) shift 2 ;; *) break ;; esac'
        echo 'done'
        echo 'exec "$@"'
    } > /mnt/app/armle/bin/on

    # pidin domyslnie milczy, wiec skrypt zawsze probuje startowac procesy. Plik
    # /tmp/sim_gate_pidin.N to odpowiedz na N-te wywolanie w sesji.
    {
        echo '#!/bin/sh'
        echo 'N=0'
        echo '[ -f /tmp/sim_gate_pidin_calls ] && N=`cat /tmp/sim_gate_pidin_calls`'
        echo 'N=$((N+1))'
        echo 'echo "$N" > /tmp/sim_gate_pidin_calls'
        echo '[ -f /tmp/sim_gate_pidin.$N ] && cat /tmp/sim_gate_pidin.$N'
        echo 'exit 0'
    } > /mnt/app/armle/bin/pidin
    printf '#!/bin/sh\nexit 0\n' > /mnt/app/armle/bin/slay
    printf '#!/bin/sh\nexit 0\n' > /mnt/app/armle/bin/sloginfo

    # sleep: bez czekania, a po N wywolaniach konczy sesje (kasuje ACTIVE),
    # zeby glowna petla skryptu sie domknela. 24 > 6 sekund bramy, wiec
    # oczekiwanie na meldunek ma szanse wyczerpac sie naturalnie.
    {
        echo '#!/bin/sh'
        echo '# SIM_GATE_SLEEP_STUB'
        echo 'N=0'
        echo '[ -f /tmp/sim_gate_sleeps ] && N=`cat /tmp/sim_gate_sleeps`'
        echo 'N=$((N+1))'
        echo 'echo "$N" > /tmp/sim_gate_sleeps'
        echo '[ -f /tmp/k2161-altscreen-heartbeat ] && : > /tmp/sim_gate_hb_seen'
        # Spozniony meldunek. Liczymy TYLKO sekundy po "overlay start", czyli
        # dokladnie obroty petli czekania bramy - inne sleepy w skrypcie nie
        # moga przesunac tego przypadku.
        echo 'if [ -f /tmp/sim_gate_late ] && grep -q "overlay start" /tmp/mib2high-lvds-control.log 2>/dev/null; then'
        echo '  G=0'
        echo '  [ -f /tmp/sim_gate_gticks ] && G=`cat /tmp/sim_gate_gticks`'
        echo '  G=$((G+1))'
        echo '  echo "$G" > /tmp/sim_gate_gticks'
        echo '  if [ "$G" -ge `cat /tmp/sim_gate_late` ]; then echo "98 210 153" > /tmp/vc-overlay-ready; fi'
        echo 'fi'
        # Flaga dla nakladki znika z koncem sesji, wiec notujemy ja w trakcie.
        echo 'if grep -q "overlay start" /tmp/mib2high-lvds-control.log 2>/dev/null && [ -f /tmp/vc-capsule-off ]; then : > /tmp/sim_gate_saw_off; fi'
        echo 'if grep -q "overlay start" /tmp/mib2high-lvds-control.log 2>/dev/null && grep -q stary /tmp/vc-capsule-text 2>/dev/null; then : > /tmp/sim_gate_stale_seen; fi'
        # Opcje MMI zmieniane w trakcie sesji: "tick plik wartosc" (pusta = usun).
        echo 'if [ -f /tmp/sim_gate_plan ]; then'
        echo '  while read T F V; do'
        echo '    if [ "$N" = "$T" ]; then if [ -n "$V" ]; then echo "$V" > "$F"; else rm -f "$F"; fi; fi'
        echo '  done < /tmp/sim_gate_plan'
        echo 'fi'
        echo '[ "$N" -ge 30 ] && rm -f /tmp/mib2high-lvds-active'
        echo 'exit 0'
    } > /bin/sleep

    chmod +x /eso/bin/apps/dmdt /mnt/app/armle/bin/on /mnt/app/armle/bin/pidin \
             /mnt/app/armle/bin/slay /mnt/app/armle/bin/sloginfo /bin/sleep

    # vc_render: atrapa, zeby skrypt nie odmowil na starcie
    printf '#!/bin/sh\nexit 0\n' > /mnt/app/root/vc_render
    chmod +x /mnt/app/root/vc_render

    cp "$CONF_SRC" /mnt/app/root/mib2high-overlay.conf

    # /bin/sleep to JEDYNA atrapa, ktora podmienia systemowa binarke - i jedyna,
    # ktorej podmiana moze cicho nie wyjsc. Linux odmawia zapisu do pliku, ktory
    # jest wlasnie wykonywany (ETXTBSY, "Text file busy"), wiec wystarczy, ze
    # w tym samym kontenerze dziala jeszcze `sleep` zostawiony przez poprzednia
    # symulacje - np. 30-sekundowa atrapa sondy z sim_probe.sh.
    #
    # Bez atrapy petla czekania bramy spi NAPRAWDE i nic jej nie konczy: licznik,
    # ktory po 30 obrotach kasuje /tmp/mib2high-lvds-active, nigdy nie ruszy.
    # Symulacja nie pada - WISI, w nieskonczonosc, wypisujac `dmdt sc 4 77`.
    # Zdarzylo sie 2026-10-27 i zjadlo 35 minut, zanim ktos spojrzal na stderr.
    # Lepiej odmowic startu z wyjasnieniem niz udawac, ze sie liczy.
    if ! grep -q SIM_GATE_SLEEP_STUB /bin/sleep 2>/dev/null; then
        echo "FAIL: nie udalo sie podmienic /bin/sleep na atrape." >&2
        echo "      Najczestsza przyczyna: w TYM SAMYM kontenerze dziala jeszcze" >&2
        echo "      proces 'sleep' z poprzedniej symulacji, wiec zapis dostaje" >&2
        echo "      ETXTBSY. Kazda symulacje uruchamiaj w OSOBNYM kontenerze." >&2
        exit 2
    fi
}

# Atrapa nakladki. Tryb decyduje, co melduje:
#   none  - nic (renderer, ktory nie dostal okna)
#   ok    - poprawny numer, ten z conf
#   wrong - inny numer
#   junk  - smieci w pierwszym polu
install_overlay(){
    {
        echo '#!/bin/sh'
        case "$1" in
          ok)    echo 'echo "98 210 153" > /tmp/vc-overlay-ready' ;;
          wrong) echo 'echo "77 210 153" > /tmp/vc-overlay-ready' ;;
          junk)  echo 'echo "nonsense 210 153" > /tmp/vc-overlay-ready' ;;
          none)  echo ': nic nie melduje' ;;
        esac
        echo 'exit 0'
    } > /mnt/app/root/vc_overlay
    chmod +x /mnt/app/root/vc_overlay
}

reset_state(){
    rm -f "$CTLLOG" "$DMLOG" /tmp/sim_gate_sleeps /tmp/sim_gate_dc_fails \
          /tmp/sim_gate_late /tmp/sim_gate_gticks \
          /tmp/sim_gate_saw_off /tmp/sim_gate_stale_seen \
          /tmp/vc-capsule-text /tmp/vc-capsule-slot /tmp/vc-capsule-off /tmp/vc-layout-last \
          /tmp/k3346-no-rgi /tmp/k3346-map-mode /tmp/k3346-arrows-mode \
          /tmp/sim_gate_plan /tmp/sim_gate_dc902_fails /tmp/sim_gate_hb_seen /tmp/vc-skin \
          /tmp/mib2high-lvds-sessions /tmp/vc-view-size \
          /tmp/sim_gate_pidin_calls /tmp/sim_gate_pidin.1 /tmp/sim_gate_pidin.2 \
          /tmp/mib2high-vc-render.profile \
          /tmp/vc-overlay-ready /tmp/vc-overlay-ready.tmp \
          /tmp/mib2high-lvds-control.pid /tmp/k2161-altscreen-heartbeat \
          /tmp/mib2high-lvds-reacquire /mnt/app/root/vc_overlay
    : > /tmp/mib2high-lvds-active
}

run_ctl(){
    "$SH" "$CTL_SRC" >/dev/null 2>&1
}

# Na jakim kontekscie PRACOWALA petla. Nie "ostatni sc": skrypt konczy sesje
# przywracajac fabryczny 72, wiec ostatni wpis zawsze jest 72 i nic nie mowi
# o tym, co kombi pokazywalo w trakcie.
work_ctx(){
    awk '$1=="dmdt" && $2=="sc" && $3=="4" && $4!="72" { c=$4 } END { print c+0 }'         "$DMLOG" 2>/dev/null
}

# Kazdy wariant musi tez skonczyc przywróceniem fabrycznego kontekstu.
expect_restore(){
    if grep -q 'dmdt sc 4 72' "$DMLOG" 2>/dev/null
    then ok "$1: po sesji kontekst wrocil na fabryczny 72"
    else bad "$1: brak przywrocenia kontekstu 72"; fi
}

expect_77(){
    if [ "`work_ctx`" = "77" ]; then ok "$1: kombi pracowalo na 77"
    else bad "$1: kombi pracowalo na `work_ctx`, oczekiwano 77"; fi
    if grep -q 'dmdt dc 901' "$DMLOG" 2>/dev/null
    then bad "$1: zadeklarowal kontekst 901"
    else ok "$1: nie deklarowal kontekstu 901"; fi
    expect_restore "$1"
}

# Brama odmowila: nakladka zyje, ale jej warstwy nie ma na VC, wiec Java nie moze
# liczyc na boki kapsuly (overlay/capsule_text.h).
expect_capsule_off(){
    if [ -f /tmp/sim_gate_saw_off ]
    then ok "$1: nakladka wie, ze jej warstwy nie ma na VC"
    else bad "$1: brak /tmp/vc-capsule-off przy odmowie"; fi
    expect_capsule_clean "$1"
}

expect_capsule_on(){
    if [ -f /tmp/sim_gate_saw_off ]
    then bad "$1: /tmp/vc-capsule-off mimo kontekstu 901"
    else ok "$1: bez /tmp/vc-capsule-off"; fi
    expect_capsule_clean "$1"
}

expect_capsule_clean(){
    if [ -e /tmp/vc-capsule-text ] || [ -e /tmp/vc-capsule-slot ] || [ -e /tmp/vc-capsule-off ]
    then bad "$1: pliki kapsuly zostaly po sesji"
    else ok "$1: po sesji nie ma plikow kapsuly"; fi
}

case_stale_capsule(){
    echo "--- $SH: tekst kapsuly z poprzedniej sesji"
    reset_state; install_overlay ok
    printf 'stary\nstary\nEND\n' > /tmp/vc-capsule-text
    echo '1 1' > /tmp/vc-capsule-slot
    : > /tmp/vc-capsule-off
    : > /tmp/k3346-no-rgi
    # Ostatni uklad zegarow ma przezyc koniec sesji: po ponownym podlaczeniu
    # kabla nikt go nie powtarza az do VIEW (run-55).
    echo 1 > /tmp/vc-layout-last
    run_ctl
    if [ "`cat /tmp/vc-layout-last 2>/dev/null`" = "1" ]
    then ok "zapisany uklad zegarow przezyl sesje"
    else bad "zapisany uklad zegarow zniknal po sesji"; fi
    if [ -e /tmp/k3346-no-rgi ]
    then bad "flaga aplikacji bez wskazowek przezyla sesje"
    else ok "flaga aplikacji bez wskazowek usunieta"; fi
    if [ -f /tmp/sim_gate_stale_seen ]
    then bad "stary tekst przezyl start nakladki"
    else ok "stary tekst usuniety przed startem nakladki"; fi
    expect_capsule_on "stara kapsula"
}

# Kolejne konteksty z logu, bez powtorzen i bez koncowego powrotu na 72.
ctx_seq(){
    awk '$1=="dmdt" && $2=="sc" && $3=="4" { if ($4!=p) { printf "%s ", $4; p=$4 } }' "$DMLOG" 2>/dev/null
}
ctx_count(){
    awk -v c="$1" '$1=="dmdt" && $2=="sc" && $3=="4" && $4==c { n++ } END { print n+0 }' "$DMLOG" 2>/dev/null
}
expect_seq(){
    if [ "`ctx_seq`" = "$2" ]; then ok "$1: konteksty $2"
    else bad "$1: konteksty `ctx_seq`, oczekiwano $2"; fi
}

case_session_counter(){
    echo "--- $SH: licznik sesji od startu (PR #12)"
    reset_state; install_overlay ok
    run_ctl
    if grep -q 'session #1 since boot' "$CTLLOG" 2>/dev/null; then ok "pierwsza sesja = 1"
    else bad "brak session #1"; fi
    : > /tmp/mib2high-lvds-active
    run_ctl
    if grep -q 'session #2 since boot' "$CTLLOG" 2>/dev/null; then ok "ponowne podlaczenie = 2"
    else bad "brak session #2"; fi
    echo 1x > /tmp/mib2high-lvds-sessions
    rm -f "$CTLLOG"
    : > /tmp/mib2high-lvds-active
    run_ctl
    if grep -q 'session #1 since boot' "$CTLLOG" 2>/dev/null; then ok "smieci w pliku = od nowa"
    else bad "smieci w pliku licznika"; fi
    # Bez aktywnej sesji skrypt nic nie zaczal, wiec nie ma co liczyc.
    reset_state; install_overlay ok
    rm -f /tmp/mib2high-lvds-active
    run_ctl
    if [ -f /tmp/mib2high-lvds-sessions ]; then bad "licznik bez sesji"
    else ok "bez sesji licznik stoi"; fi
}

case_map_options(){
    echo "--- $SH: opcje Mapa i Strzalka na VC"
    reset_state; install_overlay ok
    echo 1 > /tmp/vc-skin
    echo 1 > /tmp/vc-view-size
    run_ctl
    if grep -q 'dmdt dc 902 98 102 101 33' "$DMLOG" 2>/dev/null
    then ok "kontekst 902 w kolejnosci {98,102,101,33}"
    else bad "zla albo brakujaca deklaracja kontekstu 902"; fi
    expect_seq "bez plikow opcji" "900 901 72 "
    # Pierwszy showUI nastepnej sesji ma znac uklad, zanim bundle go odczyta.
    if [ "`cat /tmp/vc-skin 2>/dev/null`" = "1" ]; then ok "skin przezyl koniec sesji"
    else bad "skin zniknal z koncem sesji"; fi
    if [ "`cat /tmp/vc-view-size 2>/dev/null`" = "1" ]; then ok "rozmiar widoku przezyl koniec sesji"
    else bad "rozmiar widoku zniknal z koncem sesji"; fi
    if grep -q badld "$DMLOG" 2>/dev/null
    then bad "dmdt bez /eso/lib: `grep badld "$DMLOG" | head -1`"
    else ok "dmdt zawsze z /eso/lib"; fi
    if [ -f /tmp/sim_gate_hb_seen ]; then ok "heartbeat w trakcie sesji"
    else bad "brak heartbeatu mimo deklaracji 900"; fi
    # Co sekunde: 3 s migotania fabrycznej mapy z filmow #8-#10 nie moze wrocic.
    if [ "`ctx_count 901`" -ge 8 ]; then ok "901 powtarzane co sekunde"
    else bad "901 wyslane tylko `ctx_count 901` razy"; fi
    if awk '/dmdt sc 4 900/ { s=NR } /dmdt dc 901/ { d=NR } END { exit !(s && d && s < d) }' "$DMLOG"
    then ok "sama mapa przed nakladka"
    else bad "sc 4 900 nie przed deklaracja 901"; fi
    if [ "`ctx_count 901`" -ge 3 ]; then ok "901 dalej powtarzane co kilka sekund"
    else bad "901 wyslane tylko `ctx_count 901` razy"; fi

    reset_state; install_overlay ok
    echo 0 > /tmp/k3346-map-mode; echo 0 > /tmp/k3346-arrows-mode
    run_ctl
    expect_seq "obie Wl" "900 901 72 "

    reset_state; install_overlay ok
    echo 0 > /tmp/k3346-map-mode; echo 1 > /tmp/k3346-arrows-mode
    run_ctl
    expect_seq "mapa Wl, strzalka Wyl" "900 901 72 "

    reset_state; install_overlay ok
    echo 1 > /tmp/k3346-map-mode
    run_ctl
    expect_seq "mapa Wyl, strzalka Wl" "902 72 "
    if [ "`ctx_count 902`" -ge 3 ]; then ok "902 powtarzane jak 901"
    else bad "902 wyslane tylko `ctx_count 902` razy"; fi
    if [ "`cat /tmp/k3346-map-mode 2>/dev/null`" = "1" ]; then ok "wybor mapy przezyl koniec sesji"
    else bad "wybor mapy zniknal z koncem sesji"; fi

    reset_state; install_overlay ok
    echo 1 > /tmp/k3346-map-mode; echo 1 > /tmp/k3346-arrows-mode
    run_ctl
    expect_seq "obie Wyl" "72 "
    # Koniec sesji wysyla 72 dwa razy (petla i trap), wiec w sesji tylko raz.
    if [ "`ctx_count 72`" = "3" ]; then ok "72 tylko raz w sesji"
    else bad "72 wyslane `ctx_count 72` razy, oczekiwano 3"; fi

    reset_state; install_overlay ok
    { echo '10 /tmp/k3346-map-mode 1'; echo '15 /tmp/k3346-arrows-mode 1'
      echo '20 /tmp/k3346-map-mode'; echo '25 /tmp/k3346-arrows-mode'; } > /tmp/sim_gate_plan
    run_ctl
    expect_seq "zmiany w trakcie sesji" "900 901 902 72 901 72 "

    reset_state; install_overlay ok
    : > /tmp/sim_gate_dc902_fails
    echo 1 > /tmp/k3346-map-mode
    run_ctl
    expect_seq "902 odrzucony" "72 "

    reset_state; install_overlay ok
    : > /tmp/sim_gate_dc902_fails
    run_ctl
    expect_seq "902 odrzucony, mapa Wl" "900 901 72 "

    reset_state; install_overlay none
    echo 1 > /tmp/k3346-map-mode
    run_ctl
    expect_seq "nakladka bez meldunku, mapa Wyl" "72 "

    reset_state
    { echo '10 /tmp/k3346-map-mode 1'; echo '20 /tmp/k3346-map-mode'; } > /tmp/sim_gate_plan
    run_ctl
    expect_seq "bez nakladki, mapa w trakcie" "900 77 72 77 72 "

    reset_state; install_overlay ok
    : > /tmp/sim_gate_dc_fails
    run_ctl
    if grep -q 'dmdt sc 4 900' "$DMLOG" 2>/dev/null
    then bad "sc 4 900 mimo odrzuconej deklaracji"
    else ok "bez deklaracji 900 nie ma sc 4 900"; fi
    if [ -f /tmp/sim_gate_hb_seen ]; then bad "heartbeat mimo odrzuconej deklaracji"
    else ok "bez deklaracji 900 nie ma heartbeatu"; fi
    rm -f /tmp/sim_gate_dc_fails
}

case_none_installed(){
    echo "--- $SH: nakladki nie ma w ogole"
    reset_state
    run_ctl
    if grep -q 'overlay not installed' "$CTLLOG" 2>/dev/null
    then ok "log mowi, ze nakladki nie ma"
    else bad "brak komunikatu o braku nakladki"; fi
    expect_77 "brak nakladki"
}

case_no_report(){
    echo "--- $SH: nakladka jest, ale nie melduje okna"
    reset_state; install_overlay none
    run_ctl
    if grep -q 'did not report a window' "$CTLLOG" 2>/dev/null
    then ok "log mowi, ze meldunku nie bylo"
    else bad "brak komunikatu o braku meldunku"; fi
    expect_77 "brak meldunku"
    expect_capsule_off "brak meldunku"
}

case_good_report(){
    echo "--- $SH: nakladka melduje wlasciwy numer"
    reset_state; install_overlay ok
    run_ctl
    if grep -q 'overlay ready after' "$CTLLOG" 2>/dev/null
    then ok "log potwierdza gotowosc"
    else bad "brak potwierdzenia gotowosci"; fi
    if grep -q 'dmdt dc 901 98 102 101 58' "$DMLOG" 2>/dev/null
    then ok "kontekst 901 w kolejnosci {98,102,101,58}"
    else bad "zla albo brakujaca deklaracja kontekstu 901"; fi
    if [ "`work_ctx`" = "901" ]; then ok "kombi pracowalo na 901"
    else bad "kombi pracowalo na `work_ctx`, oczekiwano 901"; fi
    expect_restore "meldunek dobry"
    expect_capsule_on "meldunek dobry"
}

case_wrong_id(){
    echo "--- $SH: nakladka melduje INNY numer niz conf"
    reset_state; install_overlay wrong
    run_ctl
    if grep -q 'but conf says' "$CTLLOG" 2>/dev/null
    then ok "log wskazuje rozjazd numerow"
    else bad "brak komunikatu o rozjezdzie numerow"; fi
    expect_77 "zly numer"
    expect_capsule_off "zly numer"
}

case_junk_report(){
    echo "--- $SH: meldunek ze smieciami w pierwszym polu"
    reset_state; install_overlay junk
    run_ctl
    expect_77 "smieci w meldunku"
    expect_capsule_off "smieci w meldunku"
}

case_stale_report(){
    echo "--- $SH: NIESWIEZY meldunek po rendererze ubitym na twardo"
    reset_state; install_overlay none
    # Lezy w /tmp z poprzedniego zycia i wyglada calkiem poprawnie.
    echo "98 210 153" > /tmp/vc-overlay-ready
    run_ctl
    if grep -q 'did not report a window' "$CTLLOG" 2>/dev/null
    then ok "nieswiezy meldunek usuniety przed startem nakladki"
    else bad "zaufal nieswiezemu meldunkowi"; fi
    expect_77 "nieswiezy meldunek"
    expect_capsule_off "nieswiezy meldunek"
}

# Renderer, ktory melduje dopiero w 8. sekundzie.
#
# PO CO. Do 2026-10-27 brama czekala 6 s i to wystarczalo, bo i tak nikt nigdy
# nie zameldowal - meldunek nie mogl powstac (rename() na /tmp zwracal EXDEV).
# Po naprawie meldunek pojawia sie juz przy pierwszej klatce, ale limit podnioslem
# do 10 s i ten przypadek pilnuje, zeby ta rezerwa naprawde dzialala: przy starym
# limicie 6 s brama odmowilaby mimo zdrowej nakladki.
case_late_report(){
    echo "--- $SH: meldunek dopiero w 8. sekundzie czekania"
    reset_state; install_overlay none
    # Miedzy "overlay start" a petla czekania skrypt juz nie spi (rc20), wiec 8. sekunda
    # po starcie nakladki to 8. obrot petli bramy. Asercja jest na dokladna liczbe, a nie
    # na "cokolwiek wiekszego od 6": dodatkowy sleep opoznia mape i strzalke.
    echo 8 > /tmp/sim_gate_late
    run_ctl
    GOT=`grep -o 'overlay ready after [0-9]*s' "$CTLLOG" 2>/dev/null || true`
    if [ "$GOT" = "overlay ready after 8s" ]
    then ok "brama doczekala sie meldunku w 8 s"
    else bad "brama nie doczekala sie spoznionego meldunku (znalazlem: ${GOT:-nic})"; fi
    if grep -q 'did not report a window' "$CTLLOG" 2>/dev/null
    then bad "poddala sie mimo meldunku"
    else ok "nie poddala sie przed czasem"; fi
    if [ "`work_ctx`" = "901" ]; then ok "kombi pracowalo na 901"
    else bad "kombi pracowalo na `work_ctx`, oczekiwano 901"; fi
    expect_restore "spozniony meldunek"
    expect_capsule_on "spozniony meldunek"
}

case_dc_fails(){
    echo "--- $SH: meldunek dobry, ale dmdt dc odmawia"
    reset_state; install_overlay ok
    : > /tmp/sim_gate_dc_fails
    run_ctl
    if grep -q 'overlay context declaration failed' "$CTLLOG" 2>/dev/null
    then ok "log mowi o nieudanej deklaracji"
    else bad "brak komunikatu o nieudanej deklaracji"; fi
    if [ "`work_ctx`" = "77" ]; then ok "kombi pracowalo na 77"
    else bad "kombi pracowalo na `work_ctx`, oczekiwano 77"; fi
    expect_restore "dc odmawia"
    expect_capsule_off "dmdt dc odmawia"
}

# Kolejnosc startu (rc20, run-73): pidin raz, nakladka przed mapa, sloginfo po mapie.
line_of(){
    grep -n "$1" "$CTLLOG" 2>/dev/null | head -1 | cut -d: -f1
}
expect_before(){
    A=`line_of "$2"`; B=`line_of "$3"`
    if [ -n "$A" ] && [ -n "$B" ] && [ "$A" -lt "$B" ]; then ok "$1"
    else bad "$1 ('$2' w linii ${A:-brak}, '$3' w linii ${B:-brak})"; fi
}
pidin_calls(){
    cat /tmp/sim_gate_pidin_calls 2>/dev/null || echo 0
}
case_start_order(){
    echo "--- $SH: kolejnosc startu"
    reset_state; install_overlay ok
    run_ctl
    if [ "`pidin_calls`" = "1" ]; then ok "pidin raz na sesje"
    else bad "pidin `pidin_calls` razy, oczekiwano 1"; fi
    expect_before "renderer przed nakladka" "renderer start" "overlay start"
    expect_before "nakladka przed mapa" "overlay start" "cluster gate context=900"
    expect_before "mapa przed sloginfo" "cluster gate context=900" "session active"
    expect_before "sloginfo przed 901" "session active" "overlay ready after"
    # Meldunek od razu: zadnego czekania miedzy startem nakladki a brama.
    GOT=`grep -o 'overlay ready after [0-9]*s' "$CTLLOG" 2>/dev/null || true`
    if [ "$GOT" = "overlay ready after 0s" ]; then ok "bez sleep przed brama"
    else bad "brama: ${GOT:-nic}"; fi

    # Profil zostal z poprzedniej sesji, a renderera nie ma.
    echo "--- $SH: renderera nie ma, profil ten sam"
    reset_state; install_overlay ok
    echo "800 480 30" > /tmp/mib2high-vc-render.profile
    run_ctl
    if grep -q 'renderer start' "$CTLLOG" 2>/dev/null; then ok "renderer startuje"
    else bad "brak startu renderera"; fi

    echo "--- $SH: renderer i nakladka juz dzialaja"
    reset_state; install_overlay none
    echo "98 210 153" > /tmp/vc-overlay-ready
    echo "800 480 30" > /tmp/mib2high-vc-render.profile
    printf '1 /mnt/app/root/vc_render\n2 /mnt/app/root/vc_overlay\n' > /tmp/sim_gate_pidin.1
    printf '2 /mnt/app/root/vc_overlay\n' > /tmp/sim_gate_pidin.2
    run_ctl
    if grep -q 'renderer already running' "$CTLLOG" 2>/dev/null; then ok "renderer zostaje"
    else bad "renderer nie rozpoznany"; fi
    if grep -q 'overlay already running' "$CTLLOG" 2>/dev/null; then ok "nakladka zostaje"
    else bad "nakladka nie rozpoznana"; fi
    if grep -q 'overlay start' "$CTLLOG" 2>/dev/null; then bad "druga nakladka"
    else ok "bez drugiej nakladki"; fi
    if [ "`pidin_calls`" = "2" ]; then ok "nakladka sprawdzona drugi raz"
    else bad "pidin `pidin_calls` razy, oczekiwano 2"; fi

    # Nakladka poprzedniej sesji konczy sie w tej chwili (cleanup): w pierwszym
    # spojrzeniu jest, w drugim juz nie. Bez nowej VC zostalby bez strzalki.
    echo "--- $SH: nakladka poprzedniej sesji w trakcie konczenia"
    reset_state; install_overlay ok
    echo "800 480 30" > /tmp/mib2high-vc-render.profile
    printf '1 /mnt/app/root/vc_render\n2 /mnt/app/root/vc_overlay\n' > /tmp/sim_gate_pidin.1
    run_ctl
    if grep -q 'overlay start' "$CTLLOG" 2>/dev/null; then ok "nowa nakladka"
    else bad "brak nowej nakladki"; fi
    if [ "`work_ctx`" = "901" ]; then ok "kombi na 901"
    else bad "kombi na `work_ctx`"; fi

    echo "--- $SH: zmiana profilu renderera"
    reset_state; install_overlay ok
    echo "1 2 3" > /tmp/mib2high-vc-render.profile
    printf '1 /mnt/app/root/vc_render\n' > /tmp/sim_gate_pidin.1
    run_ctl
    if grep -q 'renderer profile changed' "$CTLLOG" 2>/dev/null; then ok "profil wykryty"
    else bad "profil nie wykryty"; fi
    expect_before "po zmianie profilu nowy renderer" "renderer profile changed" "renderer start"
    if [ "`pidin_calls`" = "1" ]; then ok "bez dodatkowego pidin"
    else bad "pidin `pidin_calls` razy"; fi
}

case_cleanup(){
    echo "--- $SH: po sesji nie zostaje meldunek"
    reset_state; install_overlay ok
    run_ctl
    if [ -f /tmp/vc-overlay-ready ]
    then bad "meldunek zostal po zakonczeniu sesji"
    else ok "meldunek sprzatniety po sesji"; fi
}

run_all(){
    SH="$1"
    case_none_installed
    case_no_report
    case_good_report
    case_wrong_id
    case_junk_report
    case_stale_report
    case_late_report
    case_dc_fails
    case_stale_capsule
    case_map_options
    case_session_counter
    case_start_order
    case_cleanup
}

echo "=========================================================="
echo " SYMULACJA BRAMY KONTEKSTU KOMBI (Etap 7b)"
echo "=========================================================="
setup_fakes

RAN=0
for cand in /bin/dash /bin/mksh; do
    if [ -x "$cand" ]; then run_all "$cand"; RAN=$((RAN+1))
    else echo "--- $cand: pominieto (brak)"; fi
done
if [ "$RAN" -eq 0 ]; then run_all /bin/sh; fi

echo "=========================================================="
echo " WYNIK: $PASS OK, $FAILED FAIL"
echo "=========================================================="
[ "$FAILED" -eq 0 ] || exit 1
