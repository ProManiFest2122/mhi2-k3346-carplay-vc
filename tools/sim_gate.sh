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
        echo 'case "${1:-}" in'
        echo '  gd|gs) exit 0 ;;'
        echo '  dc) if [ -f /tmp/sim_gate_dc_fails ]; then exit 1; fi; exit 0 ;;'
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

    # pidin milczy, wiec skrypt zawsze probuje startowac procesy
    printf '#!/bin/sh\nexit 0\n' > /mnt/app/armle/bin/pidin
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
}

case_wrong_id(){
    echo "--- $SH: nakladka melduje INNY numer niz conf"
    reset_state; install_overlay wrong
    run_ctl
    if grep -q 'but conf says' "$CTLLOG" 2>/dev/null
    then ok "log wskazuje rozjazd numerow"
    else bad "brak komunikatu o rozjezdzie numerow"; fi
    expect_77 "zly numer"
}

case_junk_report(){
    echo "--- $SH: meldunek ze smieciami w pierwszym polu"
    reset_state; install_overlay junk
    run_ctl
    expect_77 "smieci w meldunku"
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
    # 9, nie 8: miedzy "overlay start" a petla czekania skrypt spi jeszcze raz
    # (deklaracja kontekstu 900), wiec 9. sekunda po starcie nakladki to 8. obrot
    # petli bramy. Sprawdzone, nie zgadniete - dlatego asercja jest na dokladna
    # liczbe, a nie na "cokolwiek wiekszego od 6".
    echo 9 > /tmp/sim_gate_late
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
