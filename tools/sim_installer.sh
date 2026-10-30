#!/bin/bash
# Symulacja instalatora K3346 na komputerze - BEZ AUTA.
# Uruchamiac W KONTENERZE Linux, nie na Windows: awk na Windows zjada CR,
# a pliki HMI w aucie maja mieszane konce linii (CRLF + LF), wiec wynik
# na Windows bylby niereprezentatywny.
#
# Buduje atrape drzewa plikow auta z PRAWDZIWYCH plikow zebranych z auta,
# uruchamia paczke z work/sd_etap3 i sprawdza:
#   T1 dobry firmware        -> instaluje, brama przechodzi
#   T2 zmiany                -> nowe pliki sa, 3 pliki zmodyfikowane punktowo
#   T3 ROLLBACK              -> drzewo wraca bit w bit, z trybami plikow
#   T4 zly HMI_Version       -> ODMOWA, zero zapisow
#   T5 zly rozmiar iap2.so   -> ODMOWA, zero zapisow
#   T6 zly rozmiar LSD.JXE   -> ODMOWA, zero zapisow
#   T7 LSD.JXE nieczytelny   -> instaluje (ten sprawdzian jest pomijalny)
#   T8 MIB_FORCE=1 + zly fw  -> swiadome obejscie, instaluje
#   T9 blad w trakcie        -> auto-rollback przywraca stan
#   T13 Etap 4 (RGI w mm-ipod): instalacja, launcher, rollbacki, odmowy, awaria

set -u

CAR=/work/sd_odczyt_1/MIB2_SOFTWARE_COLLECTOR_v1.2_PID_8523907/files
REF=/work/mmx_app
PKG=/work/sd_etap5
SIM=/work/sim_app
PKGNAME=mib2high-carplay-rgi-v8s-simple
LOG=/work/sim_app/sim_result.txt
# Testujemy instalator ZE ZRODEL, nie ten zapieczony w paczce: work/sd_etap5
# powstala 2026-10-21 i jej command.sh jest o kilka etapow starszy. Bez tego
# T1-T15 sprawdzalyby stary kod. Kontener montuje port_k3346/altscreen jako /src.
INSTALLER="${INSTALLER:-/src/deployment/custom.sh}"
use_current_installer(){
    if [ -f "$INSTALLER" ]; then cp "$INSTALLER" "$SIM/sd/mod/command.sh"
    else echo "  UWAGA: brak $INSTALLER - testuje command.sh z paczki (zamontuj /src)"; fi
}

PASS=0; FAILED=0
ok(){   PASS=$((PASS+1));   echo "  OK   $*"; }
bad(){  FAILED=$((FAILED+1)); echo "  FAIL $*"; }

# Migawka plikow: suma kontrolna + tryb. Katalogi osobno, bo restore_all
# swiadomie nie usuwa katalogow (usuniecie eso/hmi/lsd/jars skasowaloby
# 11 fabrycznych jarow - patrz nizej), wiec puste katalogi to residuum
# kosmetyczne, nie regresja.
snapshot_files(){
    for root in "$@"; do
        ( cd "$root" 2>/dev/null || exit 0
          find . -type f | LC_ALL=C sort | while IFS= read -r p; do
              printf '%s %s %s/%s\n' "$(sha256sum < "$p" | cut -d' ' -f1)" "$(stat -c %a "$p")" "$(basename "$root")" "${p#./}"
          done )
    done
}
snapshot_dirs(){
    for root in "$@"; do
        ( cd "$root" 2>/dev/null || exit 0
          find . -type d | LC_ALL=C sort | while IFS= read -r p; do
              printf '%s/%s\n' "$(basename "$root")" "${p#./}"
          done )
    done
}

setup(){
    rm -rf "$SIM/app" "$SIM/system" "$SIM/sd" "$SIM/ifs"
    mkdir -p "$SIM/app/eso/hmi/lsd" "$SIM/app/armle/lib/dll" "$SIM/app/root" \
             "$SIM/system/etc/eso/production" "$SIM/sd" "$SIM/ifs"
    # eso/hmi/lsd/jars JEST katalogiem fabrycznym (11 jarow w K3346) - musi tu byc,
    # inaczej symulacja nieprawdziwie pokazuje go jako "zostawiony po rollbacku".
    mkdir -p "$SIM/app/eso/hmi/lsd/jars"
    cp "$REF"/eso/hmi/lsd/jars/*.jar "$SIM/app/eso/hmi/lsd/jars/" 2>/dev/null || true
    # pliki HMI i konfiguracja - prosto z auta (zachowane mieszane konce linii)
    cp "$CAR/mnt/app/eso/hmi/lsd/bundles_dsitracer_enabled.properties" "$SIM/app/eso/hmi/lsd/"
    cp "$CAR/mnt/app/eso/hmi/lsd/traceConfig.properties"               "$SIM/app/eso/hmi/lsd/"
    cp "$CAR/mnt/app/eso/hmi/lsd/DSITracer.jar"                        "$SIM/app/eso/hmi/lsd/"
    cp "$CAR/mnt/system/etc/eso/production/smartphone_integrator.json" "$SIM/system/etc/eso/production/"
    cp "$CAR/mnt/app/armle/lib/dll/ipod-drvr-iap2.so"                  "$SIM/app/armle/lib/dll/"
    cp "$REF/eso/hmi/version.properties"                               "$SIM/app/eso/hmi/"
    # Etap 4: pliki, ktore zmienia (prawdziwe z auta) + stock mm-ipod
    mkdir -p "$SIM/system/etc/mm" "$SIM/app/armle/usr/sbin"
    cp "$(find "$CAR/mnt/system/etc" -maxdepth 1 -iname usblauncher_otg.lua | head -1)" "$SIM/system/etc/usblauncher_otg.lua"
    cp "$(find "$CAR/mnt/system/etc/mm" -maxdepth 1 -iname iap2.cfg | head -1)"        "$SIM/system/etc/mm/iap2.cfg"
    cp "$REF/armle/usr/sbin/mm-ipod"                                    "$SIM/app/armle/usr/sbin/mm-ipod"
    # rozne tryby, zeby sprawdzic, ze instalator i rollback je zachowuja
    chmod 644 "$SIM/system/etc/usblauncher_otg.lua"; chmod 664 "$SIM/system/etc/mm/iap2.cfg"
    # lsd.jxe: tylko rozmiar ma znaczenie dla bramy, wiec atrapa 58131096 B
    truncate -s 58131096 "$SIM/ifs/lsd.jxe"
    # paczka na "karcie SD" ("setup nocard" = pusta karta, dla T18)
    if [ "${1:-}" != nocard ]; then
        cp -a "$PKG/." "$SIM/sd/"
        use_current_installer
    fi
}

run(){ # run [dodatkowe env...]
    env MIB_SD="$SIM/sd" MIB_APP="$SIM/app" MIB_SYSTEM="$SIM/system" \
        MIB_LSD_JXE="$SIM/ifs/lsd.jxe" MIB_BT_ROOTS="$SIM/btroots" "$@" \
        sh "$SIM/sd/mod/command.sh" 2>&1
}

# --------------------------------------------------------------- T0 (statyczny)
# Polecenia, ktorych NIE MA na MMI (sprawdzone w aucie): sha256sum, cksum,
# md5sum, cmp (collector v1.2) i printf (1.0.1-rc1, 2026-10-30: "printf: cannot
# execute"). W kontenerze sa, wiec symulacja wykonania ich nie zlapie - stad
# osobne sprawdzenie tekstu skryptow, z pominieciem komentarzy.
car_missing(){
    for f in "$@"; do
        [ -f "$f" ] || continue
        grep -nE '(^|[^a-zA-Z_.-])(printf|sha256sum|cksum|md5sum|cmp)([^a-zA-Z_.-]|$)' "$f"             | grep -vE '^[0-9]+:[[:space:]]*#' | sed "s|^|$(basename "$f"):|"
    done
}
echo "=========================================================="
echo " SYMULACJA INSTALATORA K3346 (bez auta)   $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "=========================================================="

echo
echo "--- T0: skrypty dla auta nie uzywaja polecen, ktorych na MMI nie ma"
MISSING=$(car_missing "$INSTALLER" /src/deployment/mib2high-lvds-control.sh /src/deployment/k3346-overlay-probe.sh /src/deployment/etap4/mm-ipod-rgi.sh)
if [ -z "$MISSING" ]; then ok "brak printf/sha256sum/cksum/md5sum/cmp poza komentarzami"
else bad "polecenia nieobecne na MMI:"; printf '%s\n' "$MISSING" | sed 's/^/        /'; fi

# ---------------------------------------------------------------- T1 + T2 + T3
echo
echo "--- T1: instalacja na zgodnym firmware"
setup
BEFORE_F=$(snapshot_files "$SIM/app" "$SIM/system")
BEFORE_D=$(snapshot_dirs  "$SIM/app" "$SIM/system")
OUT=$(run)
echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"VERSION_GATE=PASS"*) ok "brama wersji przeszla";; *) bad "brak VERSION_GATE=PASS";; esac
case "$OUT" in *"INSTALL_RESULT=PASS"*) ok "instalacja PASS";; *) bad "brak INSTALL_RESULT=PASS";; esac

echo
echo "--- T2: czy zmiany sa takie, jak zapowiedziano"
for f in app/root/hooks/libmib2high-carplay-rgi.so app/eso/hmi/lsd/jars/mib2high-carplay-rgi.jar \
         app/root/vc_render app/root/v8l-dsitracer-capture app/root/mib2high-lvds-control.sh \
         app/root/mib2high-lvds-profile.conf app/root/mib2high-lvds-viewareas.conf \
         app/carplay_altscreen app/eso/hmi/lsd/dsitracer_activated; do
    [ -e "$SIM/$f" ] && ok "dodany: $f" || bad "BRAK: $f"
done
grep -q 'MIB2HighCarPlayRGI=org.mib2high.carplay.rgi.Activator' \
     "$SIM/app/eso/hmi/lsd/bundles_dsitracer_enabled.properties" \
  && ok "bundles: aktywator zarejestrowany" || bad "bundles: brak aktywatora"
grep -q 'Autostart.*MIB2HighCarPlayRGI' \
     "$SIM/app/eso/hmi/lsd/bundles_dsitracer_enabled.properties" \
  && ok "bundles: dopisany do autostartu" || bad "bundles: brak w autostarcie"
grep -q 'use.appender=telnet,bluepirat' "$SIM/app/eso/hmi/lsd/traceConfig.properties" \
  && ok "traceConfig: appendery DSITracera wlaczone" || bad "traceConfig: appendery niezmienione"
grep -q 'LD_PRELOAD=/mnt/app/root/hooks/libmib2high-carplay-rgi.so' \
     "$SIM/system/etc/eso/production/smartphone_integrator.json" \
  && ok "smartphone_integrator: LD_PRELOAD dodany" || bad "smartphone_integrator: brak LD_PRELOAD"
# CRLF musi przetrwac transformacje awk
if grep -qU $'\r' "$SIM/app/eso/hmi/lsd/bundles_dsitracer_enabled.properties"; then
    ok "bundles: CRLF zachowane po awk"
else
    bad "bundles: awk zniszczyl CRLF"
fi

echo
echo "--- T3: ROLLBACK przywraca stan poczatkowy"
: > "$SIM/sd/mod/$PKGNAME/ROLLBACK"
OUT=$(run)
echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"ROLLBACK_RESULT=PASS"*) ok "rollback PASS";; *) bad "brak ROLLBACK_RESULT=PASS";; esac
AFTER_F=$(snapshot_files "$SIM/app" "$SIM/system")
AFTER_D=$(snapshot_dirs  "$SIM/app" "$SIM/system")
if [ "$BEFORE_F" = "$AFTER_F" ]; then
    ok "wszystkie pliki identyczne bit w bit, z trybami"
else
    bad "PLIKI roznia sie po rollbacku:"
    diff <(printf '%s\n' "$BEFORE_F") <(printf '%s\n' "$AFTER_F") | head -20 | sed 's/^/        /'
fi
if [ "$BEFORE_D" = "$AFTER_D" ]; then
    ok "brak residuum katalogow"
else
    echo "  INFO zostaly puste katalogi (restore_all swiadomie nie usuwa katalogow):"
    diff <(printf '%s\n' "$BEFORE_D") <(printf '%s\n' "$AFTER_D") | grep '^>' | sed 's/^> /        /'
fi

# ----------------------------------------------------------------------- T4-T6
echo
echo "--- T4: zly HMI_Version -> ODMOWA"
setup
sed -i 's/^HMI_Version:.*/HMI_Version:HMI_AU_MIB2_H_EU_G22_C6_999999/' "$SIM/app/eso/hmi/version.properties"
B4F=$(snapshot_files "$SIM/app" "$SIM/system"); B4D=$(snapshot_dirs "$SIM/app" "$SIM/system")
OUT=$(run)
echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"VERSION_GATE: HMI_Version="*"instalacja PRZERWANA"*) ok "odmowa z powodu HMI_Version";; *) bad "brak odmowy po HMI_Version";; esac
case "$OUT" in *INSTALL_RESULT=PASS*) bad "ZAINSTALOWAL mimo zlej wersji";; *) ok "nie zainstalowal";; esac
[ "$B4F" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ "$B4D" = "$(snapshot_dirs "$SIM/app" "$SIM/system")" ] \
  && ok "zero zapisow do drzewa auta (pliki i katalogi)" || bad "drzewo zmienione mimo odmowy"

echo
echo "--- T5: zly rozmiar ipod-drvr-iap2.so -> ODMOWA"
setup
printf 'x' >> "$SIM/app/armle/lib/dll/ipod-drvr-iap2.so"   # 739926 B
B5F=$(snapshot_files "$SIM/app" "$SIM/system"); B5D=$(snapshot_dirs "$SIM/app" "$SIM/system")
OUT=$(run)
echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"ipod-drvr-iap2.so ma 739926 B"*) ok "odmowa z powodu rozmiaru drivera";; *) bad "brak odmowy po rozmiarze drivera";; esac
case "$OUT" in *INSTALL_RESULT=PASS*) bad "ZAINSTALOWAL mimo niezgodnego drivera";; *) ok "nie zainstalowal";; esac
[ "$B5F" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ "$B5D" = "$(snapshot_dirs "$SIM/app" "$SIM/system")" ] \
  && ok "zero zapisow do drzewa auta (pliki i katalogi)" || bad "drzewo zmienione mimo odmowy"

echo
echo "--- T6: zly rozmiar LSD.JXE -> ODMOWA"
setup
truncate -s 12345 "$SIM/ifs/lsd.jxe"
B6F=$(snapshot_files "$SIM/app" "$SIM/system"); B6D=$(snapshot_dirs "$SIM/app" "$SIM/system")
OUT=$(run)
echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"lsd.jxe ma 12345 B"*) ok "odmowa z powodu rozmiaru LSD.JXE";; *) bad "brak odmowy po LSD.JXE";; esac
[ "$B6F" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ "$B6D" = "$(snapshot_dirs "$SIM/app" "$SIM/system")" ] \
  && ok "zero zapisow do drzewa auta (pliki i katalogi)" || bad "drzewo zmienione mimo odmowy"

# --------------------------------------------------------------------- T7 + T8
echo
echo "--- T7: LSD.JXE nieczytelny -> sprawdzian pomijany, instalacja idzie"
setup
rm -f "$SIM/ifs/lsd.jxe"
OUT=$(run)
case "$OUT" in *"nieczytelny - pomijam"*) ok "sprawdzian LSD pominiety z komunikatem";; *) bad "brak komunikatu o pominieciu";; esac
case "$OUT" in *"INSTALL_RESULT=PASS"*) ok "instalacja PASS (pozostale sprawdziany przeszly)";; *) bad "nie zainstalowal";; esac

echo
echo "--- T8: MIB_FORCE=1 na zlym firmware -> swiadome obejscie"
setup
sed -i 's/^HMI_Version:.*/HMI_Version:COS_INNEGO/' "$SIM/app/eso/hmi/version.properties"
OUT=$(run MIB_FORCE=1)
case "$OUT" in *"VERSION_GATE=BYPASSED"*) ok "obejscie zaraportowane";; *) bad "brak komunikatu o obejsciu";; esac
case "$OUT" in *"INSTALL_RESULT=PASS"*) ok "instalacja PASS przy MIB_FORCE=1";; *) bad "nie zainstalowal przy MIB_FORCE=1";; esac

# ----------------------------------------------------------------------- T9
echo
echo "--- T9: blad w trakcie instalacji -> auto-rollback"
setup
BEFORE_F=$(snapshot_files "$SIM/app" "$SIM/system")
BEFORE_D=$(snapshot_dirs  "$SIM/app" "$SIM/system")
# Wstrzykujemy blad PO petli instalujacej payload (czyli przy TX=1), zeby
# wymusic sciezke auto-rollbacku. To kopia tylko na potrzeby testu - do auta
# idzie nietkniety command.sh.
awk '{print} /INSTALLING=/{print "fail \047TEST_INJECTED_FAILURE\047"}' \
    "$SIM/sd/mod/command.sh" > "$SIM/sd/mod/command.inject.sh"
OUT=$(env MIB_SD="$SIM/sd" MIB_APP="$SIM/app" MIB_SYSTEM="$SIM/system" \
          MIB_LSD_JXE="$SIM/ifs/lsd.jxe" sh "$SIM/sd/mod/command.inject.sh" 2>&1)
echo "$OUT" | tail -8 | sed 's/^/      | /'
case "$OUT" in *"TEST_INJECTED_FAILURE"*) ok "wstrzykniety blad wystapil";; *) bad "blad nie wystapil - test nic nie sprawdzil";; esac
case "$OUT" in *"AUTO_ROLLBACK_RESULT=PASS"*) ok "auto-rollback PASS";; *) bad "auto-rollback nie zadzialal";; esac
AFTER_F=$(snapshot_files "$SIM/app" "$SIM/system")
AFTER_D=$(snapshot_dirs  "$SIM/app" "$SIM/system")
if [ "$BEFORE_F" = "$AFTER_F" ]; then
    ok "wszystkie pliki przywrocone bit w bit po awarii"
else
    bad "PLIKI roznia sie po auto-rollbacku:"
    diff <(printf '%s\n' "$BEFORE_F") <(printf '%s\n' "$AFTER_F") | head -20 | sed 's/^/        /'
fi
if [ "$BEFORE_D" = "$AFTER_D" ]; then
    ok "brak residuum katalogow"
else
    echo "  INFO zostaly puste katalogi:"
    diff <(printf '%s\n' "$BEFORE_D") <(printf '%s\n' "$AFTER_D") | grep '^>' | sed 's/^> /        /'
fi

# ----------------------------------------------------------------------- T10
echo
echo "--- T10: tryb COLLECT (drugie uruchomienie po udanej instalacji)"
setup
OUT=$(run)
case "$OUT" in *"INSTALL_RESULT=PASS"*) ok "instalacja przed zbieraniem";; *) bad "instalacja nie przeszla - T10 nic nie sprawdzi";; esac
# Po instalacji zostaje installed.ok, wiec kolejne uruchomienie wchodzi w collect().
AFTER_INSTALL_F=$(snapshot_files "$SIM/app" "$SIM/system")
OUT=$(run)
echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"MODE=COLLECT"*) ok "wszedl w tryb COLLECT";; *) bad "nie wszedl w COLLECT";; esac
case "$OUT" in *"COLLECT_RESULT="*) ok "zbieranie zakonczone komunikatem";; *) bad "brak COLLECT_RESULT";; esac
# W symulacji nie ma dmdt ani sloginfo - COLLECT ma to znisc jako PARTIAL,
# a NIE wywrocic sie z bledem.
case "$OUT" in *"COLLECT_RESULT=PARTIAL"*) ok "brak dmdt/sloginfo dal PARTIAL, nie blad";; *) echo "  INFO COLLECT_RESULT inny niz PARTIAL (w symulacji oczekiwany PARTIAL)";; esac
[ -d "$SIM/sd/mod/$PKGNAME/collected/run-01" ] && ok "powstal katalog collected/run-01" || bad "brak collected/run-01"
if [ -f "$SIM/sd/mod/$PKGNAME/collected/run-01/trace/collection-errors.txt" ]; then
    grep -q 'DMDT_MISSING' "$SIM/sd/mod/$PKGNAME/collected/run-01/trace/collection-errors.txt" \
      && ok "brak dmdt zaraportowany jako DMDT_MISSING" \
      || echo "  INFO w collection-errors.txt nie ma DMDT_MISSING"
fi
# Najwazniejsze: COLLECT nie moze dotykac drzewa plikow auta.
[ "$AFTER_INSTALL_F" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] \
  && ok "COLLECT nie zmienil ani jednego pliku w drzewie auta" \
  || bad "COLLECT ZMIENIL drzewo auta"
# I jeszcze: dmdt podany przez MIB_DMDT ma byc uzyty i zapisany do pliku.
setup
run >/dev/null
cat > "$SIM/fake-dmdt" <<'EOF'
#!/bin/sh
echo "FAKE_DMDT_CONTEXTS args=$*"
EOF
chmod 755 "$SIM/fake-dmdt"
OUT=$(run MIB_DMDT="$SIM/fake-dmdt")
if [ -f "$SIM/sd/mod/$PKGNAME/collected/run-01/dmdt-gc.txt" ] \
   && grep -q 'FAKE_DMDT_CONTEXTS args=gc' "$SIM/sd/mod/$PKGNAME/collected/run-01/dmdt-gc.txt"; then
    ok "dmdt wywolany z 'gc' i zapisany do dmdt-gc.txt"
else
    bad "dmdt-gc.txt nie powstal albo nie zawiera wywolania 'gc'"
fi

# ----------------------------------------------------------------------- T11
# Wymuszone zbieranie logow markerem COLLECT_LOGS. Sedno: jedna wizyta w aucie
# nie moze skonczyc sie utrata danych diagnostycznych.
mkfakedmdt(){
    cat > "$SIM/fake-dmdt" <<'EOF'
#!/bin/sh
echo "FAKE_DMDT_CONTEXTS args=$*"
EOF
    chmod 755 "$SIM/fake-dmdt"
}

echo
echo "--- T11a: marker COLLECT_LOGS bez zadnej instalacji -> zbiera logi"
setup
mkfakedmdt
BEFORE_F=$(snapshot_files "$SIM/app" "$SIM/system")
: > "$SIM/sd/mod/$PKGNAME/COLLECT_LOGS"
OUT=$(run MIB_DMDT="$SIM/fake-dmdt")
echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"FORCED_COLLECT=1"*) ok "wymuszenie zaraportowane";; *) bad "brak FORCED_COLLECT=1";; esac
case "$OUT" in *"MODE=COLLECT"*) ok "wszedl w COLLECT bez instalacji";; *) bad "nie wszedl w COLLECT";; esac
case "$OUT" in *INSTALL_RESULT=PASS*) bad "ZAINSTALOWAL, a mial tylko zbierac";; *) ok "nie instalowal";; esac
[ -d "$SIM/sd/mod/$PKGNAME/collected/run-01" ] && ok "powstal collected/run-01" || bad "brak collected/run-01"
grep -q 'FAKE_DMDT_CONTEXTS args=gc' "$SIM/sd/mod/$PKGNAME/collected/run-01/dmdt-gc.txt" 2>/dev/null \
  && ok "dmdt gc zebrany tez bez instalacji" || bad "brak dmdt-gc.txt"
[ "$BEFORE_F" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] \
  && ok "drzewo auta nietkniete" || bad "ZMIENIL drzewo auta"

echo
echo "--- T11b: instalacja -> rollback -> COLLECT_LOGS -> nadal zbiera (sedno poprawki)"
setup
mkfakedmdt
run >/dev/null                                    # instalacja
: > "$SIM/sd/mod/$PKGNAME/ROLLBACK"
OUT=$(run); case "$OUT" in *"ROLLBACK_RESULT=PASS"*) ok "rollback wykonany";; *) bad "rollback nie przeszedl";; esac
# Bez markera skrypt powiedzialby tylko ALREADY_ROLLED_BACK i nic nie zebral.
OUT=$(run)
case "$OUT" in *"ALREADY_ROLLED_BACK"*) ok "bez markera faktycznie nie zbiera (potwierdza pulapke)";; *) bad "spodziewany ALREADY_ROLLED_BACK";; esac
: > "$SIM/sd/mod/$PKGNAME/COLLECT_LOGS"
OUT=$(run MIB_DMDT="$SIM/fake-dmdt")
echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"MODE=COLLECT"*) ok "po rollbacku marker WYMUSZA zbieranie";; *) bad "marker nie zadzialal po rollbacku";; esac
[ -d "$SIM/sd/mod/$PKGNAME/collected/run-01" ] && ok "logi zebrane po rollbacku" || bad "brak logow po rollbacku"

echo
echo "--- T11c: ROLLBACK + COLLECT_LOGS naraz -> wygrywa ROLLBACK (bezpieczenstwo)"
setup
mkfakedmdt
run >/dev/null                                    # instalacja
BEFORE_F=$(snapshot_files "$SIM/app" "$SIM/system")
: > "$SIM/sd/mod/$PKGNAME/COLLECT_LOGS"
: > "$SIM/sd/mod/$PKGNAME/ROLLBACK"
OUT=$(run MIB_DMDT="$SIM/fake-dmdt")
echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"ROLLBACK_RESULT=PASS"*) ok "wykonal ROLLBACK, nie zbieranie";; *) bad "ROLLBACK nie wygral - sciezka ratunku zablokowana!";; esac
case "$OUT" in *"MODE=COLLECT"*) bad "wszedl w COLLECT zamiast ratowac";; *) ok "nie wszedl w COLLECT";; esac
[ "$BEFORE_F" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && bad "rollback nic nie cofnal" || ok "rollback faktycznie cofnal zmiany"

# ----------------------------------------------------------------------- T12
# Procedura dokladnie jak w instrukcji: gotowe znaczniki z ZNACZNIKI/ na karcie.
echo
echo "--- T12a: ZNACZNIKI/ w katalogu glownym karty niczego nie wyzwala"
setup
[ -f "$SIM/sd/ZNACZNIKI/ROLLBACK" ] && [ -f "$SIM/sd/ZNACZNIKI/COLLECT_LOGS" ] \
  && ok "paczka zawiera ZNACZNIKI/ROLLBACK i ZNACZNIKI/COLLECT_LOGS" \
  || bad "brak gotowych znacznikow w paczce"
[ ! -s "$SIM/sd/ZNACZNIKI/ROLLBACK" ] && [ ! -s "$SIM/sd/ZNACZNIKI/COLLECT_LOGS" ] \
  && ok "znaczniki sa puste" || bad "znaczniki nie sa puste"
BEFORE_F=$(snapshot_files "$SIM/app" "$SIM/system")
OUT=$(run)
case "$OUT" in *"INSTALL_RESULT=PASS"*) ok "mimo ZNACZNIKI/ na karcie: zwykla instalacja";; *) bad "ZNACZNIKI/ zaklocil instalacje";; esac
case "$OUT" in *"MODE=ROLLBACK"*|*"MODE=COLLECT"*) bad "ZNACZNIKI/ wyzwolil inny tryb";; *) ok "nie wyzwolil rollbacku ani zbierania";; esac

echo
echo "--- T12b: skopiowanie ZNACZNIKI/ROLLBACK do folderu pakietu -> rollback"
cp "$SIM/sd/ZNACZNIKI/ROLLBACK" "$SIM/sd/mod/$PKGNAME/"
OUT=$(run)
echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"ROLLBACK_RESULT=PASS"*) ok "rollback z gotowego znacznika";; *) bad "gotowy znacznik ROLLBACK nie zadzialal";; esac
[ "$BEFORE_F" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] \
  && ok "pliki auta przywrocone bit w bit" || bad "pliki auta niezgodne po rollbacku"
[ ! -e "$SIM/sd/mod/$PKGNAME/ROLLBACK" ] \
  && ok "znacznik ROLLBACK usuniety po uzyciu" || bad "znacznik ROLLBACK zostal na karcie"

echo
echo "--- T12c: skopiowanie ZNACZNIKI/COLLECT_LOGS po rollbacku -> logi"
cp "$SIM/sd/ZNACZNIKI/COLLECT_LOGS" "$SIM/sd/mod/$PKGNAME/"
OUT=$(run)
case "$OUT" in *"FORCED_COLLECT=1"*) ok "zbieranie z gotowego znacznika";; *) bad "gotowy znacznik COLLECT_LOGS nie zadzialal";; esac

# ----------------------------------------------------------------------- T13
# Etap 4: RGI w mm-ipod. Osobna transakcja, osobna kopia oryginalow.
E4M="$SIM/sd/mod/$PKGNAME"
LUA="$SIM/system/etc/usblauncher_otg.lua"; CFG="$SIM/system/etc/mm/iap2.cfg"

echo
echo "--- T13a: INSTALL_ETAP4 bez zainstalowanego v8s -> ODMOWA, zero zapisow"
setup
B=$(snapshot_files "$SIM/app" "$SIM/system"); BD=$(snapshot_dirs "$SIM/app" "$SIM/system")
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"
OUT=$(run); echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"najpierw musi byc zainstalowany v8s"*) ok "odmowa: brak v8s";; *) bad "brak odmowy bez v8s";; esac
case "$OUT" in *"INSTALL_RESULT=PASS"*) bad "marker Etapu 4 wyzwolil instalacje v8s";; *) ok "nie zainstalowal v8s zamiast Etapu 4";; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ "$BD" = "$(snapshot_dirs "$SIM/app" "$SIM/system")" ] \
  && ok "zero zapisow do drzewa auta" || bad "drzewo zmienione mimo odmowy"

echo
echo "--- T13b: v8s -> INSTALL_ETAP4 -> PASS, zmiany dokladnie zapowiedziane"
setup
STOCK=$(snapshot_files "$SIM/app" "$SIM/system"); STOCK_D=$(snapshot_dirs "$SIM/app" "$SIM/system")
OUT=$(run); case "$OUT" in *"INSTALL_RESULT=PASS"*) ok "v8s zainstalowany";; *) bad "v8s nie zainstalowany";; esac
V8S=$(snapshot_files "$SIM/app" "$SIM/system")
cp "$LUA" "$SIM/lua.stock"; cp "$CFG" "$SIM/cfg.stock"
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"
OUT=$(run); echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"ETAP4_INSTALL_RESULT=PASS"*) ok "Etap 4 PASS";; *) bad "brak ETAP4_INSTALL_RESULT=PASS";; esac
[ -f "$E4M/etap4.installed.ok" ] && ok "etap4.installed.ok" || bad "brak etap4.installed.ok"
[ ! -e "$E4M/INSTALL_ETAP4" ] && ok "marker INSTALL_ETAP4 usuniety" || bad "marker INSTALL_ETAP4 zostal"
[ -f "$E4M/installed.ok" ] && ok "v8s nadal zainstalowany" || bad "v8s zniknal"
cmp -s "$SIM/app/root/hooks/libmib2high-carplay-rgi-ipod.so" "$E4M/payload/libmib2high-carplay-rgi-ipod.so" && ok "biblioteka RGI na miejscu" || bad "brak/zla biblioteka RGI"
[ "$(stat -c %a "$SIM/app/root/mm-ipod-rgi.sh")" = 755 ] && ok "launcher 755" || bad "launcher zly tryb"
D=$(diff "$SIM/lua.stock" "$LUA")
[ "$(printf '%s\n' "$D" | grep -c '^[<>]')" = 2 ] && printf '%s\n' "$D" | grep -q '^> .*return "/mnt/app/root/mm-ipod-rgi.sh -d iap2,config=/etc/mm/iap2.cfg,' \
  && ok "lua: zmieniona dokladnie jedna linia (start mm-ipod)" || { bad "lua: zmiany inne niz zapowiedziane"; printf '%s\n' "$D" | head | sed 's/^/        /'; }
[ "$(head -c 5852 "$CFG" | sha256sum)" = "$(sha256sum < "$SIM/cfg.stock")" ] && tail -n 3 "$CFG" | grep -q '^recvmsgs=0x5201,0x5202,0x5204,0x4171$' \
  && tail -n 1 "$CFG" | grep -q '^sendmsgs=0x5200,0x5203,0x415a,0x4170$' && [ "$(wc -c < "$CFG")" = 5991 ] \
  && ok "iap2.cfg: stock bez zmian + [ident] (RGI + Etap 6) dopisane na koncu, 5991 B" || bad "iap2.cfg: zmiany inne niz zapowiedziane"
[ "$(stat -c %a "$LUA")" = 644 ] && [ "$(stat -c %a "$CFG")" = 664 ] && ok "tryby lua/cfg zachowane (644/664)" || bad "tryby lua/cfg zmienione"
if command -v luac5.1 >/dev/null 2>&1; then
    luac5.1 -p "$LUA" && ok "luac -p: nowy usblauncher_otg.lua poprawny skladniowo" || bad "luac -p: BLAD SKLADNI w lua"
else echo "  INFO brak luac5.1 - skladnia lua nie sprawdzona w tym przebiegu"; fi
E4=$(snapshot_files "$SIM/app" "$SIM/system")
OUT=$(run); case "$OUT" in *"MODE=COLLECT"*) ok "kolejne uruchomienie = COLLECT";; *) bad "po Etapie 4 nie wszedl w COLLECT";; esac
grep -q 'ETAP4=INSTALLED' "$E4M/collected/run-01/trace/metadata.txt" 2>/dev/null && ok "metadata: ETAP4=INSTALLED" || bad "metadata bez stanu Etapu 4"
[ -f "$E4M/collected/run-01/live/usblauncher_otg.lua" ] && ok "COLLECT kopiuje aktualny lua" || bad "COLLECT nie skopiowal lua"
[ "$E4" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "COLLECT nie dotknal drzewa auta" || bad "COLLECT zmienil drzewo auta"

echo
echo "--- T13c: launcher: LD_PRELOAD tylko gdy jest biblioteka, argumenty bez zmian"
mkdir -p "$SIM/lt"; printf '#!/bin/sh\necho "PRELOAD=${LD_PRELOAD:-<brak>}"\nfor a in "$@"; do echo "ARG=[$a]"; done\n' > "$SIM/lt/mm-ipod"; chmod 755 "$SIM/lt/mm-ipod"
sed -e "s#/mnt/app/root/hooks/libmib2high-carplay-rgi-ipod.so#$SIM/lt/lib.so#" -e "s#/mnt/app/armle/usr/sbin/mm-ipod#$SIM/lt/mm-ipod#" \
    "$SIM/app/root/mm-ipod-rgi.sh" > "$SIM/lt/w.sh"
: > "$SIM/lt/lib.so"
O=$(env -u LD_PRELOAD sh "$SIM/lt/w.sh" -d iap2,config=/etc/mm/iap2.cfg -l /dev/shmem/iap2.log -vvvvvv)
printf '%s\n' "$O" | grep -qx "PRELOAD=$SIM/lt/lib.so" && ok "LD_PRELOAD = biblioteka RGI" || bad "LD_PRELOAD nieustawiony: $O"
[ "$(printf '%s\n' "$O" | grep -c '^ARG=')" = 5 ] && printf '%s\n' "$O" | grep -qx 'ARG=\[-vvvvvv\]' && ok "argumenty przekazane 1:1" || bad "argumenty zmienione"
O=$(LD_PRELOAD=/x/inny.so sh "$SIM/lt/w.sh"); printf '%s\n' "$O" | grep -qx "PRELOAD=$SIM/lt/lib.so:/x/inny.so" && ok "istniejacy LD_PRELOAD zachowany" || bad "istniejacy LD_PRELOAD zgubiony: $O"
rm -f "$SIM/lt/lib.so"
O=$(env -u LD_PRELOAD sh "$SIM/lt/w.sh"); printf '%s\n' "$O" | grep -qx 'PRELOAD=<brak>' && ok "brak biblioteki -> stock mm-ipod bez preloadu" || bad "bez biblioteki nadal preload: $O"

echo
echo "--- T13d: ponowny INSTALL_ETAP4 gdy juz jest -> ODMOWA, zero zapisow"
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP4: juz zainstalowany"*) ok "odmowa: juz zainstalowany";; *) bad "brak odmowy przy drugiej instalacji";; esac
[ "$E4" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "zero zapisow" || bad "drzewo zmienione"
rm -f "$E4M/INSTALL_ETAP4"

echo
echo "--- T13e: ROLLBACK_ETAP4 -> stan po v8s bit w bit, v8s zostaje"
cp "$SIM/sd/ZNACZNIKI/ROLLBACK_ETAP4" "$E4M/"
OUT=$(run); echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"ETAP4_ROLLBACK_RESULT=PASS"*) ok "rollback Etapu 4 PASS";; *) bad "brak ETAP4_ROLLBACK_RESULT=PASS";; esac
[ "$V8S" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "pliki = stan po v8s, bit w bit z trybami" \
  || { bad "pliki roznia sie od stanu po v8s"; diff <(printf '%s\n' "$V8S") <(snapshot_files "$SIM/app" "$SIM/system") | head | sed 's/^/        /'; }
[ -f "$E4M/installed.ok" ] && [ ! -e "$E4M/etap4.installed.ok" ] && [ ! -e "$E4M/ROLLBACK_ETAP4" ] && ok "flagi: v8s jest, Etap 4 nie, marker usuniety" || bad "flagi po rollbacku Etapu 4 niezgodne"
OUT=$(run); case "$OUT" in *"MODE=COLLECT"*) ok "potem zwykly COLLECT (v8s dziala dalej)";; *) bad "po rollbacku Etapu 4 nie COLLECT";; esac

echo
echo "--- T13f: ponowna instalacja Etapu 4 po jego rollbacku -> PASS (stara kopia odsunieta)"
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP4_INSTALL_RESULT=PASS"*) ok "reinstalacja PASS";; *) bad "reinstalacja zablokowana"; echo "$OUT" | tail -5 | sed 's/^/      | /';; esac
[ "$E4" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "wynik identyczny jak pierwsza instalacja" || bad "reinstalacja dala inny wynik"
ls -d "$E4M"/backup/pre-etap4.old.* >/dev/null 2>&1 && ok "stara kopia zachowana jako pre-etap4.old.*" || bad "stara kopia zniknela"

echo
echo "--- T13g: pelny ROLLBACK z Etapem 4 -> fabryka bit w bit"
cp "$SIM/sd/ZNACZNIKI/ROLLBACK" "$E4M/"
OUT=$(run); echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"ETAP4_ROLLBACK=PASS"*"ROLLBACK_RESULT=PASS"*) ok "Etap 4 cofniety przed v8s, oba PASS";; *) bad "pelny rollback niekompletny";; esac
[ "$STOCK" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "wszystkie pliki = fabryka, bit w bit z trybami" \
  || { bad "pliki roznia sie od fabryki"; diff <(printf '%s\n' "$STOCK") <(snapshot_files "$SIM/app" "$SIM/system") | head | sed 's/^/        /'; }

echo
echo "--- T13h: nie-stockowe pliki -> ODMOWA Etapu 4, zero zapisow"
for wariant in lua cfg mmipod ident; do
    setup; run >/dev/null
    case $wariant in
        lua)    printf -- '-- x\n' >> "$LUA";;
        cfg)    printf '#x\n' >> "$CFG";;
        mmipod) printf 'x' >> "$SIM/app/armle/usr/sbin/mm-ipod";;
        ident)  head -c 5833 "$SIM/cfg.stock" > "$CFG"; printf '[ident]\nrecvmsgs=0\n' >> "$CFG"; [ "$(wc -c < "$CFG")" = 5852 ] || echo "  INFO atrapa [ident] ma inny rozmiar";;
    esac
    B=$(snapshot_files "$SIM/app" "$SIM/system"); BD=$(snapshot_dirs "$SIM/app" "$SIM/system")
    cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"
    OUT=$(run)
    case "$OUT" in *"ETAP4_INSTALL_RESULT=PASS"*) bad "$wariant: ZAINSTALOWAL na nie-stockowym pliku";; *"ERROR"*) ok "$wariant: odmowa ($(printf '%s\n' "$OUT" | grep ERROR | head -1 | cut -c1-90))";; *) bad "$wariant: brak odmowy";; esac
    [ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ "$BD" = "$(snapshot_dirs "$SIM/app" "$SIM/system")" ] && ok "$wariant: zero zapisow" || bad "$wariant: drzewo zmienione"
    [ ! -e "$E4M/backup/pre-etap4" ] && ok "$wariant: bez kopii (nic nie zaczeto)" || bad "$wariant: zostala kopia"
done

echo
echo "--- T13i: awaria w trakcie Etapu 4 -> auto-rollback Etapu 4, v8s nietkniety"
setup; run >/dev/null
V8S=$(snapshot_files "$SIM/app" "$SIM/system")
awk '{print} /INSTALLING=iap2.cfg/{print "fail \047TEST_INJECTED_E4\047"}' "$SIM/sd/mod/command.sh" > "$SIM/sd/mod/command.inject.sh"
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"
OUT=$(env MIB_SD="$SIM/sd" MIB_APP="$SIM/app" MIB_SYSTEM="$SIM/system" MIB_LSD_JXE="$SIM/ifs/lsd.jxe" sh "$SIM/sd/mod/command.inject.sh" 2>&1)
echo "$OUT" | tail -6 | sed 's/^/      | /'
case "$OUT" in *"TEST_INJECTED_E4"*) ok "wstrzykniety blad wystapil (po zapisaniu wszystkich 4 plikow)";; *) bad "blad nie wystapil";; esac
case "$OUT" in *"ETAP4_AUTO_ROLLBACK_RESULT=PASS"*) ok "auto-rollback Etapu 4 PASS";; *) bad "auto-rollback Etapu 4 nie zadzialal";; esac
printf '%s\n' "$OUT" | grep -qE '^\[V8S\] (INSTALL_FAILED|AUTO_ROLLBACK_RESULT)' && bad "ruszyl rollback v8s" || ok "v8s nieruszony przez awarie Etapu 4"
[ "$V8S" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "pliki = stan po v8s, bit w bit" || bad "pliki roznia sie od stanu po v8s"
[ -f "$E4M/installed.ok" ] && [ ! -e "$E4M/etap4.installed.ok" ] && [ ! -e "$E4M/etap4.installing.flag" ] && ok "flagi spojne" || bad "flagi niespojne"
rm -f "$E4M/INSTALL_ETAP4"; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP4_INSTALL_RESULT=PASS"*) ok "po awarii mozna zainstalowac ponownie";; *) bad "po awarii instalacja zablokowana"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac

echo
echo "--- T13j: ROLLBACK_ETAP4 gdy Etapu 4 nie ma -> komunikat, zero zapisow"
setup; run >/dev/null
B=$(snapshot_files "$SIM/app" "$SIM/system")
cp "$SIM/sd/ZNACZNIKI/ROLLBACK_ETAP4" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP4_NOT_INSTALLED"*) ok "komunikat: nic do cofniecia";; *) bad "brak komunikatu";; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ ! -e "$E4M/ROLLBACK_ETAP4" ] && ok "zero zapisow, marker usuniety" || bad "zmiany albo marker zostal"

# ----------------------------------------------------------------------- T14
# Etap 5: JAR z klasami o fabrycznych nazwach. Osobna transakcja.
OVR="$SIM/app/eso/hmi/lsd/jars/mib2high-k3346-overrides.jar"
CAF="$SIM/app/root/k3346-coverart-fetch"

echo
echo "--- T14a: INSTALL_ETAP5 bez v8s -> ODMOWA, zero zapisow"
setup
B=$(snapshot_files "$SIM/app" "$SIM/system"); BD=$(snapshot_dirs "$SIM/app" "$SIM/system")
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"
OUT=$(run)
case "$OUT" in *"ETAP5: najpierw musi byc zainstalowany v8s"*) ok "odmowa: brak v8s";; *) bad "brak odmowy bez v8s";; esac
case "$OUT" in *"INSTALL_RESULT=PASS"*) bad "marker Etapu 5 wyzwolil instalacje v8s";; *) ok "nie zainstalowal v8s zamiast Etapu 5";; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ "$BD" = "$(snapshot_dirs "$SIM/app" "$SIM/system")" ] && ok "zero zapisow" || bad "drzewo zmienione"

echo
echo "--- T14b: v8s + Etap 4 -> INSTALL_ETAP5 -> PASS, tylko JAR dodany"
setup
STOCK=$(snapshot_files "$SIM/app" "$SIM/system")
run >/dev/null; V8S=$(snapshot_files "$SIM/app" "$SIM/system")
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; OUT=$(run); case "$OUT" in *"ETAP4_INSTALL_RESULT=PASS"*) ok "Etap 4 przed Etapem 5";; *) bad "Etap 4 nie przeszedl";; esac
E4S=$(snapshot_files "$SIM/app" "$SIM/system")
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"
OUT=$(run); echo "$OUT" | sed 's/^/      | /'
case "$OUT" in *"ETAP5_INSTALL_RESULT=PASS"*) ok "Etap 5 PASS";; *) bad "brak ETAP5_INSTALL_RESULT=PASS";; esac
cmp -s "$OVR" "$E4M/payload/mib2high-k3346-overrides.jar" && [ "$(stat -c %a "$OVR")" = 644 ] && ok "JAR na miejscu, 644" || bad "JAR brak/zly tryb"
cmp -s "$CAF" "$E4M/payload/k3346-coverart-fetch" && [ "$(stat -c %a "$CAF")" = 755 ] && ok "k3346-coverart-fetch w /mnt/app/root, 755" || bad "narzedzie okladki brak/zly tryb"
D=$(diff <(printf '%s\n' "$E4S") <(snapshot_files "$SIM/app" "$SIM/system") | grep -c '^[<>]')
[ "$D" = 2 ] && ok "jedyne zmiany w drzewie auta = JAR + narzedzie okladki" || bad "zmian w drzewie: $D (oczekiwano 2)"
[ -f "$E4M/etap5.installed.ok" ] && [ ! -e "$E4M/INSTALL_ETAP5" ] && ok "flaga etap5.installed.ok, marker usuniety" || bad "flagi Etapu 5 niezgodne"
E5S=$(snapshot_files "$SIM/app" "$SIM/system")
mkdir -p "$SIM/rcc"; echo "123 K3346_OVERRIDE_PROBE class_loaded TerminalModeSDSHandler" > "$SIM/rcc/k3346-override-probe.log"
OUT=$(run MIB_RCC_SHMEM="$SIM/rcc"); case "$OUT" in *"MODE=COLLECT"*) ok "potem COLLECT";; *) bad "nie COLLECT";; esac
R=$(ls -d "$E4M"/collected/run-* | tail -1)
grep -q 'ETAP5=INSTALLED' "$R/trace/metadata.txt" && ok "metadata: ETAP5=INSTALLED" || bad "metadata bez ETAP5"
grep -q 'class_loaded' "$R/logs/k3346-override-probe.log" 2>/dev/null && ok "COLLECT zebral znacznik sondy" || bad "brak znacznika sondy w logach"
[ "$E5S" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "COLLECT nie dotknal drzewa" || bad "COLLECT zmienil drzewo"

echo
echo "--- T14c: ponowny INSTALL_ETAP5 -> ODMOWA; ROLLBACK_ETAP5 -> stan po Etapie 4"
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP5: juz zainstalowany"*) ok "odmowa: juz zainstalowany";; *) bad "brak odmowy";; esac
rm -f "$E4M/INSTALL_ETAP5"
cp "$SIM/sd/ZNACZNIKI/ROLLBACK_ETAP5" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP5_ROLLBACK_RESULT=PASS"*) ok "rollback Etapu 5 PASS";; *) bad "brak ETAP5_ROLLBACK_RESULT=PASS";; esac
[ "$E4S" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "pliki = stan po Etapie 4 (bit w bit)" || bad "pliki roznia sie od stanu po Etapie 4"
[ -f "$E4M/etap4.installed.ok" ] && [ -f "$E4M/installed.ok" ] && [ ! -e "$E4M/etap5.installed.ok" ] && ok "v8s i Etap 4 zostaja" || bad "flagi niezgodne"

echo
echo "--- T14d: reinstalacja Etapu 5 -> PASS; pelny ROLLBACK -> fabryka (5 -> 4 -> v8s)"
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP5_INSTALL_RESULT=PASS"*) ok "reinstalacja PASS";; *) bad "reinstalacja zablokowana"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
[ "$E5S" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "wynik jak pierwsza instalacja" || bad "inny wynik"
cp "$SIM/sd/ZNACZNIKI/ROLLBACK" "$E4M/"
OUT=$(run); echo "$OUT" | grep -E "ROLLBACK" | sed 's/^/      | /'
case "$OUT" in *"ETAP5_ROLLBACK=PASS"*"ETAP4_ROLLBACK=PASS"*"ROLLBACK_RESULT=PASS"*) ok "kolejnosc 5 -> 4 -> v8s, wszystkie PASS";; *) bad "pelny rollback niekompletny";; esac
[ "$STOCK" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "fabryka bit w bit z trybami" \
  || { bad "rozne od fabryki"; diff <(printf '%s\n' "$STOCK") <(snapshot_files "$SIM/app" "$SIM/system") | head | sed 's/^/        /'; }

echo
echo "--- T14e: awaria w trakcie Etapu 5 -> auto-rollback Etapu 5, reszta nietknieta"
setup; run >/dev/null; V8S=$(snapshot_files "$SIM/app" "$SIM/system")
awk '{print} /INSTALLING=mib2high-k3346-overrides.jar/{print "fail \047TEST_INJECTED_E5\047"}' "$SIM/sd/mod/command.sh" > "$SIM/sd/mod/command.inject.sh"
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"
OUT=$(env MIB_SD="$SIM/sd" MIB_APP="$SIM/app" MIB_SYSTEM="$SIM/system" MIB_LSD_JXE="$SIM/ifs/lsd.jxe" sh "$SIM/sd/mod/command.inject.sh" 2>&1)
case "$OUT" in *"TEST_INJECTED_E5"*) ok "wstrzykniety blad wystapil";; *) bad "blad nie wystapil";; esac
case "$OUT" in *"ETAP5_AUTO_ROLLBACK_RESULT=PASS"*) ok "auto-rollback Etapu 5 PASS";; *) bad "auto-rollback Etapu 5 nie zadzialal";; esac
printf '%s\n' "$OUT" | grep -qE '^\[V8S\] (INSTALL_FAILED|AUTO_ROLLBACK_RESULT|ETAP4_)' && bad "ruszyl inne warstwy" || ok "v8s/Etap 4 nieruszone"
[ "$V8S" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ ! -e "$OVR" ] && ok "pliki = stan przed Etapem 5, JAR-a brak" || bad "pliki rozne"
rm -f "$E4M/INSTALL_ETAP5"; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP5_INSTALL_RESULT=PASS"*) ok "po awarii mozna zainstalowac ponownie";; *) bad "po awarii zablokowane";; esac

echo
echo "--- T14g: aktualizacja JAR-a: oba znaczniki naraz (ROLLBACK_ETAP5 + INSTALL_ETAP5), dwa uruchomienia"
setup; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"; OUT=$(run); case "$OUT" in *"ETAP5_INSTALL_RESULT=PASS"*) ok "stara wersja Etapu 5 zainstalowana";; *) bad "Etap 5 nie przeszedl";; esac
printf 'STARA-WERSJA' > "$OVR"   # udajemy, ze w aucie jest poprzedni JAR
cp "$SIM/sd/ZNACZNIKI/ROLLBACK_ETAP5" "$E4M/"; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"
OUT=$(run); case "$OUT" in *"MODE=ROLLBACK_ETAP5"*"ETAP5_ROLLBACK_RESULT=PASS"*) ok "1. uruchomienie: rollback Etapu 5";; *) bad "1. uruchomienie nie zrobilo rollbacku"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
[ ! -e "$OVR" ] && [ ! -e "$E4M/ROLLBACK_ETAP5" ] && [ -e "$E4M/INSTALL_ETAP5" ] && ok "po 1.: JAR-a brak, ROLLBACK_ETAP5 zuzyty, INSTALL_ETAP5 czeka" || bad "stan po 1. uruchomieniu niezgodny"
OUT=$(run); case "$OUT" in *"ETAP5_INSTALL_RESULT=PASS"*) ok "2. uruchomienie: instalacja nowej wersji";; *) bad "2. uruchomienie nie zainstalowalo"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
cmp -s "$OVR" "$E4M/payload/mib2high-k3346-overrides.jar" && ok "w aucie nowy JAR z paczki" || bad "w aucie nie ma nowego JAR-a"
[ ! -e "$E4M/INSTALL_ETAP5" ] && [ -f "$E4M/etap5.installed.ok" ] && ok "znaczniki zuzyte, etap5.installed.ok" || bad "flagi niezgodne"
OUT=$(run); case "$OUT" in *"MODE=COLLECT"*) ok "3. uruchomienie: zwykly COLLECT";; *) bad "3. uruchomienie nie COLLECT";; esac

echo
echo "--- T13-BT: Etap 6b - BT_MAC na karcie -> sekcja [bluetooth] z adresem; zly BT_MAC -> odmowa bez zmian"
setup; run >/dev/null; V8S=$(snapshot_files "$SIM/app" "$SIM/system")
printf 'zle:00\r\n' > "$E4M/BT_MAC"; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"
OUT=$(run); case "$OUT" in *"BT_MAC ma zly format"*) ok "zly BT_MAC: odmowa";; *) bad "zly BT_MAC przyjety"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
[ "$V8S" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ ! -e "$E4M/etap4.installed.ok" ] && ok "zly BT_MAC: zero zapisow" || bad "zly BT_MAC: drzewo zmienione"
printf '00:00:00:00:00:00\n' > "$E4M/BT_MAC"
OUT=$(run); case "$OUT" in *"BT_MAC to adres pusty"*) ok "pusty adres: odmowa";; *) bad "pusty adres przyjety";; esac
printf '02:00:00:00:00:01\r\n' > "$E4M/BT_MAC"
OUT=$(run); echo "$OUT" | grep -E 'ETAP4_BT|ETAP4_INSTALL' | sed 's/^/      | /'
case "$OUT" in *"ETAP4_INSTALL_RESULT=PASS"*) ok "z BT_MAC: Etap 4 PASS";; *) bad "z BT_MAC: brak PASS"; echo "$OUT" | tail -5 | sed 's/^/      | /';; esac
case "$OUT" in *"ETAP4_BT=ON"*) ok "log ETAP4_BT=ON";; *) bad "brak ETAP4_BT=ON";; esac
case "$OUT" in *"02:00:00:00:00:01"*) bad "adres wypisany w logu";; *) ok "adres nie trafil do logu";; esac
[ "$(head -c 5852 "$CFG" | sha256sum)" = "$(sha256sum < "$SIM/cfg.stock")" ] && ok "iap2.cfg z BT: stockowa czesc bez zmian" || bad "iap2.cfg z BT: stockowa czesc zmieniona"
[ "$(wc -c < "$CFG")" = 6141 ] && ok "iap2.cfg z BT: 6141 B" || bad "iap2.cfg z BT: $(wc -c < "$CFG") B zamiast 6141"
[ "$(tail -n 6 "$CFG" | tr '\n' '|')" = '[bluetooth]|enable=yes|id=1111|name=Bluetooth|connectstatus=enable|mac=02:00:00:00:00:01|' ] \
  && ok "iap2.cfg z BT: sekcja [bluetooth] z mac na koncu" || { bad "iap2.cfg z BT: zla sekcja"; tail -n 7 "$CFG" | od -c | tail -5 | sed 's/^/        /'; }
grep -q '^sendmsgs=0x5200,0x5203,0x415a,0x4170$' "$CFG" && ok "iap2.cfg z BT: [ident] jest" || bad "iap2.cfg z BT: brak [ident]"
tail -c +5853 "$CFG" | grep -q "$(printf '\r')" && bad "iap2.cfg z BT: CR w dopisku" || ok "iap2.cfg z BT: dopisek bez CR"
cp "$SIM/sd/ZNACZNIKI/ROLLBACK_ETAP4" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP4_ROLLBACK_RESULT=PASS"*) ok "rollback Etapu 4 z BT PASS";; *) bad "rollback z BT nie PASS";; esac
[ "$V8S" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "po rollbacku fabryczny iap2.cfg (stan po v8s bit w bit)" || bad "rollback nie przywrocil stanu"
rm -f "$E4M/BT_MAC"

echo
echo "--- T14h: COLLECT zbiera obiekty PPS iAP2 (telephony/nowplaying), drzewo auta nietkniete"
setup; run >/dev/null
mkdir -p "$SIM/pps"; printf '@telephony\nCallStateStatus:n:0\n' > "$SIM/pps/telephony"; printf '@nowplaying\nMediaItemTitle::x\n' > "$SIM/pps/nowplaying"
B=$(snapshot_files "$SIM/app" "$SIM/system")
OUT=$(run MIB_PPS_IAP2="$SIM/pps"); R=$(ls -d "$E4M"/collected/run-* | tail -1)
grep -q 'CallStateStatus' "$R/pps/telephony.txt" 2>/dev/null && grep -q 'MediaItemTitle' "$R/pps/nowplaying.txt" 2>/dev/null && ok "pps/telephony.txt i pps/nowplaying.txt zebrane" || bad "brak zrzutu PPS"
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "drzewo auta nietkniete" || bad "COLLECT zmienil drzewo"

echo
echo "--- T14k: COLLECT szuka adresu BT (listingi + obiekty z bt/blue w nazwie), tylko odczyt"
setup; run >/dev/null
mkdir -p "$SIM/bt1/services/bluetooth" "$SIM/bt2"
printf '@local\naddress::00:11:22:33:44:55\n' > "$SIM/bt1/services/bluetooth/local"
printf 'btcfg\n' > "$SIM/bt2/bt.cfg"; printf 'inne\n' > "$SIM/bt2/other.txt"
dd if=/dev/zero of="$SIM/bt2/btbig" bs=1024 count=64 2>/dev/null
B=$(snapshot_files "$SIM/app" "$SIM/system"); BB=$(cd "$SIM" && ls -lR bt1 bt2 | md5sum)
OUT=$(run MIB_BT_ROOTS="$SIM/bt1/services $SIM/bt2 $SIM/brak"); R=$(ls -d "$E4M"/collected/run-* | tail -1)
grep -q '00:11:22:33:44:55' "$R/bt-addr/objects.txt" 2>/dev/null && grep -q 'btcfg' "$R/bt-addr/objects.txt" && ok "obiekty z katalogu bluetooth i plik bt.cfg zebrane" || bad "brak obiektow BT"
grep -q 'inne' "$R/bt-addr/objects.txt" 2>/dev/null && bad "skopiowal plik bez bt w nazwie" || ok "pliki bez bt/blue w nazwie pominiete"
grep -q 'other.txt' "$R/bt-addr/listing.txt" 2>/dev/null && ok "listing katalogu zapisany" || bad "brak listingu"
SZ=$(wc -c < "$R/bt-addr/objects.txt"); [ "$SZ" -lt 20000 ] && ok "duzy plik uciety do 8 KB (objects.txt=$SZ B)" || bad "duzy plik nieuciety ($SZ B)"
case "$OUT" in *"COLLECT_RESULT="*) ok "COLLECT dokonczony";; *) bad "COLLECT nie dokonczony";; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ "$BB" = "$(cd "$SIM" && ls -lR bt1 bt2 | md5sum)" ] && ok "drzewo auta i katalogi BT nietkniete" || bad "COLLECT cos zmienil"

echo
echo "--- T14i: sonda okladki w COLLECT: wynik zapisany; zawieszona sonda zabita po 15 s"
setup; run >/dev/null
: > "$SIM/fake-ipod0"
printf '#!/bin/sh\necho "K3346_ARTWORK_PROBE v1 start" > "$1/artwork-probe.txt"\nprintf "\\377\\330\\377" > "$1/artwork.bin"\n' > "$SIM/fake-probe-ok"; chmod 755 "$SIM/fake-probe-ok"
B=$(snapshot_files "$SIM/app" "$SIM/system")
OUT=$(run MIB_ARTWORK_PROBE="$SIM/fake-probe-ok" MIB_IPOD_DEV="$SIM/fake-ipod0"); R=$(ls -d "$E4M"/collected/run-* | tail -1)
grep -q 'K3346_ARTWORK_PROBE' "$R/artwork/artwork-probe.txt" 2>/dev/null && [ -s "$R/artwork/artwork.bin" ] && grep -q 'ARTWORK_PROBE_EXIT=0' "$R/artwork/probe-stdout.txt" && ok "sonda: raport + artwork.bin na karcie, exit 0" || bad "brak wyniku sondy"
ls /tmp/k3346-artwork-probe.* >/dev/null 2>&1 && bad "zostala kopia sondy w /tmp" || ok "kopia sondy w /tmp usunieta"
printf '#!/bin/sh\nsleep 60\n' > "$SIM/fake-probe-hang"; chmod 755 "$SIM/fake-probe-hang"
T0=$(date +%s); OUT=$(run MIB_ARTWORK_PROBE="$SIM/fake-probe-hang" MIB_IPOD_DEV="$SIM/fake-ipod0"); T1=$(date +%s); R=$(ls -d "$E4M"/collected/run-* | tail -1)
grep -q 'ARTWORK_PROBE=TIMEOUT' "$R/artwork/probe-stdout.txt" 2>/dev/null && [ $((T1-T0)) -lt 40 ] && ok "zawieszona sonda zabita (TIMEOUT, $((T1-T0)) s)" || bad "zawieszona sonda nie zabita ($((T1-T0)) s)"
case "$OUT" in *"COLLECT_RESULT="*) ok "COLLECT dokonczony mimo zawieszonej sondy";; *) bad "COLLECT nie dokonczony";; esac
OUT=$(run MIB_ARTWORK_PROBE="$SIM/fake-probe-ok" MIB_IPOD_DEV="$SIM/brak-ipod0"); R=$(ls -d "$E4M"/collected/run-* | tail -1)
[ ! -e "$R/artwork" ] && grep -q 'ARTWORK_PROBE_SKIPPED' "$R/trace/copied-files.txt" && ok "bez iPhone'a (/dev/ipod0) sonda pominieta" || bad "sonda nie pominieta bez /dev/ipod0"
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "drzewo auta nietkniete" || bad "drzewo zmienione"

echo
echo "--- T14j: aktualizacja z Etapu 5 sprzed 5.3 (kopia bez k3346-coverart-fetch) -> rollback PASS, nowa wersja z narzedziem"
setup; run >/dev/null; V8S=$(snapshot_files "$SIM/app" "$SIM/system")
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"; OUT=$(run); case "$OUT" in *"ETAP5_INSTALL_RESULT=PASS"*) ok "Etap 5 zainstalowany";; *) bad "Etap 5 nie przeszedl";; esac
# stan jak w aucie po starej wersji: narzedzia brak, kopia zna tylko JAR
rm -f "$CAF" "$E4M/backup/pre-etap5/k3346-coverart-fetch" "$E4M/backup/pre-etap5/k3346-coverart-fetch.absent" "$E4M/backup/pre-etap5/k3346-coverart-fetch.mode"
cp "$SIM/sd/ZNACZNIKI/ROLLBACK_ETAP5" "$E4M/"; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"
OUT=$(run); case "$OUT" in *"RESTORE_SKIP_NOT_IN_BACKUP=k3346-coverart-fetch"*"ETAP5_ROLLBACK_RESULT=PASS"*) ok "rollback starej wersji PASS (narzedzie pominiete)";; *) bad "rollback starej wersji nie przeszedl"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
[ "$V8S" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "pliki = stan przed Etapem 5" || bad "pliki rozne po rollbacku"
OUT=$(run); case "$OUT" in *"ETAP5_INSTALL_RESULT=PASS"*) ok "instalacja nowej wersji PASS";; *) bad "instalacja nowej wersji nie przeszla"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
[ -f "$CAF" ] && [ -f "$OVR" ] && [ -f "$E4M/backup/pre-etap5/k3346-coverart-fetch.absent" ] && ok "JAR + narzedzie w aucie, nowa kopia zna narzedzie" || bad "stan po aktualizacji niezgodny"

echo
echo "--- T14f: JAR albo narzedzie juz w systemie (stan nieznany) -> ODMOWA, zero zapisow"
setup; run >/dev/null; printf 'obcy' > "$OVR"
B=$(snapshot_files "$SIM/app" "$SIM/system")
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP5: JAR juz jest w systemie"*) ok "odmowa";; *) bad "brak odmowy";; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ ! -e "$E4M/backup/pre-etap5" ] && ok "zero zapisow, bez kopii" || bad "zmiany mimo odmowy"
rm -f "$OVR"; printf 'obcy' > "$CAF"; B=$(snapshot_files "$SIM/app" "$SIM/system")
OUT=$(run); case "$OUT" in *"ETAP5: k3346-coverart-fetch juz jest w systemie"*) ok "odmowa: narzedzie juz jest";; *) bad "brak odmowy dla narzedzia";; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ ! -e "$E4M/backup/pre-etap5" ] && ok "zero zapisow, bez kopii (narzedzie)" || bad "zmiany mimo odmowy (narzedzie)"

# ----------------------------------------------------------------------- T15
echo
echo "--- T15: aktualizacja Etapu 4 i 5 naraz (Etap 6): 4 znaczniki, 4 uruchomienia, kolejnosc 5->4->4->5"
setup; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"; OUT=$(run); case "$OUT" in *"ETAP5_INSTALL_RESULT=PASS"*) ok "stan wyjsciowy: v8s + Etap 4 + Etap 5";; *) bad "stan wyjsciowy nie powstal";; esac
CFGP="$SIM/system/etc/mm/iap2.cfg"; LIBP="$SIM/app/root/hooks/libmib2high-carplay-rgi-ipod.so"
# jak w aucie: stara biblioteka i stary iap2.cfg (bez Etapu 6)
printf 'STARA-LIB' > "$LIBP"; head -c 5852 "$SIM/cfg.stock" > "$CFGP"; printf '\n# K3346 Etap 4 (RGI): Route Guidance message directions\n[ident]\nrecvmsgs=0x5201,0x5202,0x5204\nsendmsgs=0x5200,0x5203\n' >> "$CFGP"
for m in ROLLBACK_ETAP5 ROLLBACK_ETAP4 INSTALL_ETAP4 INSTALL_ETAP5; do cp "$SIM/sd/ZNACZNIKI/$m" "$E4M/"; done
R1=$(run); R2=$(run); R3=$(run); R4=$(run)
case "$R1" in *"MODE=ROLLBACK_ETAP5"*"ETAP5_ROLLBACK_RESULT=PASS"*) ok "1.: rollback Etapu 5";; *) bad "1. nie rollback Etapu 5"; echo "$R1" | tail -3 | sed 's/^/      | /';; esac
case "$R2" in *"MODE=ROLLBACK_ETAP4"*"ETAP4_ROLLBACK_RESULT=PASS"*) ok "2.: rollback Etapu 4";; *) bad "2. nie rollback Etapu 4"; echo "$R2" | tail -3 | sed 's/^/      | /';; esac
case "$R3" in *"ETAP4_INSTALL_RESULT=PASS"*) ok "3.: instalacja nowego Etapu 4";; *) bad "3. nie zainstalowal Etapu 4"; echo "$R3" | tail -3 | sed 's/^/      | /';; esac
case "$R4" in *"ETAP5_INSTALL_RESULT=PASS"*) ok "4.: instalacja nowego Etapu 5";; *) bad "4. nie zainstalowal Etapu 5"; echo "$R4" | tail -3 | sed 's/^/      | /';; esac
cmp -s "$LIBP" "$E4M/payload/libmib2high-carplay-rgi-ipod.so" && tail -n 1 "$CFGP" | grep -q '^sendmsgs=0x5200,0x5203,0x415a,0x4170$' \
  && cmp -s "$OVR" "$E4M/payload/mib2high-k3346-overrides.jar" && [ -f "$CAF" ] && ok "w aucie: nowa biblioteka, iap2.cfg z Etapem 6, nowy JAR, narzedzie okladki" || bad "stan po aktualizacji niezgodny"
ls "$E4M" | grep -q -E '^(INSTALL|ROLLBACK)_' && bad "zostaly znaczniki" || ok "wszystkie znaczniki zuzyte"
OUT=$(run); case "$OUT" in *"MODE=COLLECT"*) ok "5.: zwykly COLLECT";; *) bad "5. nie COLLECT";; esac

# ----------------------------------------------------------------------- T16
# Etap 7: warstwa graficzna na VC. Osobna transakcja, ale dotyka DWOCH plikow
# nalezacych do wczesniejszych etapow (mib2high-lvds-control.sh z v8s i
# biblioteki mm-ipod z Etapu 4), wiec sedno tych testow to: czy cofa sie
# niezaleznie i czy nie rozwala tamtych transakcji.
PKG7=/work/sd_etap7
OVL="$SIM/app/root/vc_overlay"
OVLP="$SIM/app/root/vc_overlay_probe"
OVLC="$SIM/app/root/mib2high-overlay.conf"
OVLS="$SIM/app/root/k3346-overlay-probe.sh"
LCTL="$SIM/app/root/mib2high-lvds-control.sh"
E7LIB="$SIM/app/root/hooks/libmib2high-carplay-rgi-ipod.so"

# Tak, jak robi to uzytkownik: paczka Etapu 7 dokladana NA karte z Etapem 5/6.
setup7(){ setup; cp -a "$PKG7/." "$SIM/sd/"; use_current_installer; }

echo
echo "--- T16a: INSTALL_ETAP7 bez Etapu 4 -> ODMOWA, zero zapisow"
if [ ! -d "$PKG7" ]; then
    bad "brak $PKG7 - najpierw tools/make_sd_etap7.sh"
else
setup7; run >/dev/null
B=$(snapshot_files "$SIM/app" "$SIM/system")
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP7" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP7: wymaga Etapu 4"*) ok "odmowa bez Etapu 4";; *) bad "brak odmowy bez Etapu 4"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ ! -e "$E4M/backup/pre-etap7" ] && ok "zero zapisow, bez kopii" || bad "zmiany mimo odmowy"

echo
echo "--- T16b: v8s + Etap 4 -> INSTALL_ETAP7 -> PASS, 6 plikow, wlasciwa biblioteka"
setup7; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; run >/dev/null
E4STATE=$(snapshot_files "$SIM/app" "$SIM/system")
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP7" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP7_INSTALL_RESULT=PASS"*) ok "instalacja PASS";; *) bad "instalacja nie przeszla"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
MISS=''; for f in "$OVL" "$OVLP" "$OVLC" "$OVLS"; do [ -f "$f" ] || MISS="$MISS $(basename "$f")"; done
[ -z "$MISS" ] && ok "cztery nowe pliki na miejscu" || bad "brakuje:$MISS"
[ "$(stat -c %a "$OVL")" = 755 ] && [ "$(stat -c %a "$OVLS")" = 755 ] && [ "$(stat -c %a "$OVLC")" = 644 ] \
  && ok "tryby 755/755/644" || bad "zle tryby nowych plikow"
# Sedno poprawki kolizji nazw: w aucie ma wyladowac biblioteka Z PAYLOADU ETAPU 7.
cmp -s "$E7LIB" "$E4M/payload/libmib2high-carplay-rgi-ipod.etap7.so" \
  && ok "w aucie biblioteka Etapu 7" || bad "w aucie nie ta biblioteka"
cmp -s "$LCTL" "$E4M/payload/mib2high-lvds-control.sh" \
  && ok "skrypt sterujacy podmieniony" || bad "skrypt sterujacy nie podmieniony"
ls "$E4M" | grep -q '^INSTALL_ETAP7$' && bad "znacznik nie zuzyty" || ok "znacznik zuzyty"

echo
echo "--- T16c: ponowny INSTALL_ETAP7 -> ODMOWA; ROLLBACK_ETAP7 -> stan po Etapie 4 bit w bit"
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP7" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP7: juz zainstalowany"*) ok "odmowa ponownej instalacji";; *) bad "brak odmowy ponownej";; esac
rm -f "$E4M/INSTALL_ETAP7"
cp "$SIM/sd/ZNACZNIKI/ROLLBACK_ETAP7" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP7_ROLLBACK_RESULT=PASS"*) ok "rollback PASS";; *) bad "rollback nie przeszedl"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
[ "$E4STATE" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] \
  && ok "drzewo = stan po Etapie 4, bit w bit" || bad "drzewo rozne od stanu po Etapie 4"
[ ! -e "$OVL" ] && [ ! -e "$OVLP" ] && [ ! -e "$OVLC" ] && [ ! -e "$OVLS" ] \
  && ok "pliki Etapu 7 usuniete" || bad "zostaly pliki Etapu 7"

echo
echo "--- T16d: brak pliku wspoldzielonego w aucie -> ODMOWA, zero zapisow"
# Gdyby kopia zapisala znacznik .absent, rollback by tego pliku NIE przywrocil.
setup7; run >/dev/null; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; run >/dev/null
rm -f "$LCTL"; B=$(snapshot_files "$SIM/app" "$SIM/system")
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP7" "$E4M/"
OUT=$(run); case "$OUT" in *"brak mib2high-lvds-control.sh w aucie"*) ok "odmowa przy braku skryptu";; *) bad "brak odmowy przy braku skryptu";; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ ! -e "$E4M/backup/pre-etap7" ] && ok "zero zapisow" || bad "zmiany mimo odmowy"

echo
echo "--- T16e: awaria w trakcie Etapu 7 -> auto-rollback, v8s i Etap 4 nietkniete"
setup7; run >/dev/null; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; run >/dev/null
E4STATE=$(snapshot_files "$SIM/app" "$SIM/system")
# Zaczep musi byc LITERALNY w zrodle: pliki z petli id? przez say "INSTALLING=$n",
# wiec ich nazwy w tekscie skryptu nie wystepuja.
awk '{print} /INSTALLING=libmib2high-carplay-rgi-ipod.so/{print "fail \047TEST_INJECTED_E7\047"}' "$SIM/sd/mod/command.sh" > "$SIM/sd/mod/command.inject7.sh"
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP7" "$E4M/"
OUT=$(env MIB_SD="$SIM/sd" MIB_APP="$SIM/app" MIB_SYSTEM="$SIM/system" MIB_LSD_JXE="$SIM/ifs/lsd.jxe" sh "$SIM/sd/mod/command.inject7.sh" 2>&1)
case "$OUT" in *"TEST_INJECTED_E7"*) ok "wstrzykniety blad wystapil";; *) bad "blad nie wystapil";; esac
case "$OUT" in *"ETAP7_AUTO_ROLLBACK_RESULT=PASS"*) ok "auto-rollback Etapu 7 PASS";; *) bad "auto-rollback Etapu 7 nie zadzialal";; esac
printf '%s\n' "$OUT" | grep -qE '^\[V8S\] (INSTALL_FAILED|ETAP4_INSTALL_FAILED|ETAP5_INSTALL_FAILED)' && bad "ruszyl inne warstwy" || ok "v8s/Etap 4/5 nieruszone"
[ "$E4STATE" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "drzewo = stan po Etapie 4" || bad "drzewo rozne"
rm -f "$E4M/INSTALL_ETAP7"; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP7" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP7_INSTALL_RESULT=PASS"*) ok "po awarii mozna zainstalowac ponownie";; *) bad "po awarii zablokowane";; esac

echo
echo "--- T16f: pierwszenstwo znacznikow - ratunek zawsze wygrywa"
cp "$SIM/sd/ZNACZNIKI/PROBE_ETAP7" "$E4M/"; cp "$SIM/sd/ZNACZNIKI/ROLLBACK" "$E4M/"
OUT=$(run); case "$OUT" in *"MODE=ROLLBACK"*) ok "ROLLBACK bije PROBE_ETAP7";; *) bad "PROBE_ETAP7 zablokowal ratunek";; esac
rm -f "$E4M/PROBE_ETAP7" "$E4M/ROLLBACK"
setup7; run >/dev/null; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/PROBE_ETAP7" "$E4M/"; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP7" "$E4M/"
OUT=$(run); case "$OUT" in *"ETAP7_INSTALL_RESULT=PASS"*) ok "INSTALL_ETAP7 idzie przed sonda";; *) bad "sonda wyprzedzila instalacje";; esac

echo
echo "--- T16g: PROBE_ETAP7 - odmowa bez instalacji, uruchomienie po instalacji"
# Wlasny stan wyjsciowy, nie resztki po poprzednim tescie.
setup7; run >/dev/null; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP7" "$E4M/"; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/PROBE_ETAP7" "$E4M/"
OUT=$(env OVL_WAIT_USER=0 OVL_PROBE_BIN=/nie/ma OVL_DMDT=/nie/ma MIB_SD="$SIM/sd" MIB_APP="$SIM/app" \
          MIB_SYSTEM="$SIM/system" MIB_LSD_JXE="$SIM/ifs/lsd.jxe" sh "$SIM/sd/mod/command.sh" 2>&1)
case "$OUT" in *"PROBE_ETAP7_RESULT=DONE"*) ok "sonda uruchomiona po instalacji";; *) bad "sonda nie ruszyla"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
ls -d "$E4M"/collected/probe-* >/dev/null 2>&1 && ok "katalog wynikow sondy powstal" || bad "brak katalogu wynikow sondy"
ls "$E4M" | grep -q '^PROBE_ETAP7$' && bad "znacznik sondy nie zuzyty" || ok "znacznik sondy zuzyty"
setup7; run >/dev/null; cp "$SIM/sd/ZNACZNIKI/PROBE_ETAP7" "$E4M/"
OUT=$(run); case "$OUT" in *"brak k3346-overlay-probe.sh w aucie"*) ok "odmowa sondy bez instalacji";; *) bad "sonda bez instalacji nie odmowila";; esac

echo
echo "--- T16h: pelny ROLLBACK po Etapie 7 -> fabryka bit w bit"
setup7; FAB=$(snapshot_files "$SIM/app" "$SIM/system")
run >/dev/null; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP7" "$E4M/"; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/ROLLBACK" "$E4M/"; OUT=$(run)
case "$OUT" in *"ROLLBACK_RESULT=PASS"*) ok "pelny ROLLBACK PASS";; *) bad "pelny ROLLBACK nie przeszedl"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
[ "$FAB" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "drzewo = fabryka, bit w bit" || bad "drzewo rozne od fabryki"

echo
echo "--- T16i: INSTALL_ETAP4 po dolozeniu paczki Etapu 7 bierze SWOJA biblioteke"
# To jest regresja na kolizje nazw w payloadzie: gdyby Etap 7 wlozyl swoja
# biblioteke pod nazwa Etapu 4, to sprzatanie po Etapie 6 (ROLLBACK_ETAP4 +
# INSTALL_ETAP4, uzbrojone na karcie uzytkownika) zainstalowaloby wersje
# Etapu 7, a ROLLBACK_ETAP7 nie mialby czego cofac.
setup7; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; run >/dev/null
cmp -s "$E7LIB" "$E4M/payload/libmib2high-carplay-rgi-ipod.so" \
  && ok "Etap 4 zainstalowal swoja biblioteke" || bad "Etap 4 wzial biblioteke Etapu 7"
cmp -s "$E7LIB" "$E4M/payload/libmib2high-carplay-rgi-ipod.etap7.so" \
  && bad "w aucie biblioteka Etapu 7 po samym Etapie 4" || ok "biblioteki sa rozne, transakcje rozdzielone"

echo
echo "--- T16j: COLLECT zbiera dmdt gd i gs"
mkfakedmdt
OUT=$(run MIB_DMDT="$SIM/fake-dmdt"); R=$(ls -d "$E4M"/collected/run-* | tail -1)
grep -q 'args=gd' "$R/dmdt-gd.txt" 2>/dev/null && ok "dmdt-gd.txt z wywolaniem 'gd'" || bad "brak dmdt-gd.txt"
grep -q 'args=gs' "$R/dmdt-gs.txt" 2>/dev/null && ok "dmdt-gs.txt z wywolaniem 'gs'" || bad "brak dmdt-gs.txt"
grep -q 'ETAP7=' "$R/trace/metadata.txt" 2>/dev/null && ok "metadata ma stan Etapu 7" || bad "metadata bez stanu Etapu 7"

echo
echo "--- T16k: pelna procedura z karty, w kolejnosci (sprzatanie 6 -> Etap 7)"
setup7; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; run >/dev/null
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"; run >/dev/null
# stan jak na karcie uzytkownika: oba znaczniki Etapu 4 czekaja uzbrojone
for m in ROLLBACK_ETAP4 INSTALL_ETAP4; do cp "$SIM/sd/ZNACZNIKI/$m" "$E4M/"; done
R1=$(run); R2=$(run)
case "$R1" in *"ETAP4_ROLLBACK_RESULT=PASS"*) ok "1.: rollback Etapu 4";; *) bad "1. nie rollback Etapu 4";; esac
case "$R2" in *"ETAP4_INSTALL_RESULT=PASS"*) ok "2.: instalacja Etapu 4";; *) bad "2. nie instalacja Etapu 4";; esac
case "$R2" in *"ETAP4_BT=OFF"*) ok "2.: iap2.cfg bez sekcji [bluetooth]";; *) bad "2. BT nie wylaczone";; esac
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP7" "$E4M/"; R3=$(run)
case "$R3" in *"ETAP7_INSTALL_RESULT=PASS"*) ok "3.: instalacja Etapu 7";; *) bad "3. nie instalacja Etapu 7";; esac
ls "$E4M" | grep -q -E '^(INSTALL|ROLLBACK|PROBE)_' && bad "zostaly znaczniki" || ok "wszystkie znaczniki zuzyte"
R4=$(run); case "$R4" in *"MODE=COLLECT"*) ok "4.: zwykly COLLECT";; *) bad "4. nie COLLECT";; esac

echo
echo "--- T16l: WSZYSTKIE cztery znaczniki naraz -> wlasciwa kolejnosc bez wracania po karte"
# Praktyczne sedno: uzytkownik nie ma przy aucie czytnika kart, wiec musi moc
# wlozyc komplet znacznikow w domu i w aucie tylko uruchamiac skrypt.
setup7; run >/dev/null; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP5" "$E4M/"
cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; run >/dev/null; run >/dev/null
for m in ROLLBACK_ETAP4 INSTALL_ETAP4 INSTALL_ETAP7 PROBE_ETAP7; do cp "$SIM/sd/ZNACZNIKI/$m" "$E4M/"; done
S1=$(run); S2=$(run)
S3=$(run)
S4=$(env OVL_WAIT_USER=0 OVL_PROBE_BIN=/nie/ma OVL_DMDT=/nie/ma MIB_SD="$SIM/sd" MIB_APP="$SIM/app" \
         MIB_SYSTEM="$SIM/system" MIB_LSD_JXE="$SIM/ifs/lsd.jxe" sh "$SIM/sd/mod/command.sh" 2>&1)
S5=$(run)
case "$S1" in *"ETAP4_ROLLBACK_RESULT=PASS"*) ok "1.: ROLLBACK_ETAP4";; *) bad "1. nie rollback Etapu 4"; echo "$S1" | tail -2 | sed 's/^/      | /';; esac
case "$S2" in *"ETAP4_INSTALL_RESULT=PASS"*) ok "2.: INSTALL_ETAP4";; *) bad "2. nie instalacja Etapu 4"; echo "$S2" | tail -2 | sed 's/^/      | /';; esac
case "$S3" in *"ETAP7_INSTALL_RESULT=PASS"*) ok "3.: INSTALL_ETAP7";; *) bad "3. nie instalacja Etapu 7"; echo "$S3" | tail -2 | sed 's/^/      | /';; esac
case "$S4" in *"PROBE_ETAP7_RESULT=DONE"*) ok "4.: PROBE_ETAP7";; *) bad "4. nie sonda"; echo "$S4" | tail -2 | sed 's/^/      | /';; esac
case "$S5" in *"MODE=COLLECT"*) ok "5.: COLLECT";; *) bad "5. nie COLLECT";; esac
ls "$E4M" | grep -q -E '^(INSTALL|ROLLBACK|PROBE)_' && bad "zostaly znaczniki" || ok "wszystkie zuzyte, po jednym na uruchomienie"
# Samoblokada: gdyby krok 2 padl, Etap 7 NIE moze wejsc na polamany stan.
setup7; run >/dev/null
for m in INSTALL_ETAP7 PROBE_ETAP7; do cp "$SIM/sd/ZNACZNIKI/$m" "$E4M/"; done
OUT=$(run); case "$OUT" in *"ETAP7: wymaga Etapu 4"*) ok "bez Etapu 4 caly lancuch sie zatrzymuje";; *) bad "Etap 7 wszedl bez Etapu 4";; esac
fi

# ----------------------------------------------------------------------- T17
# Wydanie 1.0: karta z WSZYSTKIMI etapami i uzbrojonym INSTALL_ALL - tak, jak
# laik dostaje ja z install.zip. Jedno uruchomienie ma zrobic cala robote,
# a kazdy blad ma zostawic auto w spojnym stanie, z ktorego da sie wznowic.
setupall(){ setup7; : > "$E4M/INSTALL_ALL"; }
lines_safe(){ printf '%s\n' "$1" | grep -c 'SAFE TO REMOVE SD'; }

echo
echo "--- T17a: czyste auto + INSTALL_ALL -> cztery etapy w jednym uruchomieniu"
if [ ! -d "$PKG7" ]; then
    bad "brak $PKG7 - najpierw tools/make_sd_etap7.sh"
else
setupall; FAB=$(snapshot_files "$SIM/app" "$SIM/system")
OUT=$(run)
for m in 'INSTALL_RESULT=PASS' 'ETAP4_INSTALL_RESULT=PASS' 'ETAP5_INSTALL_RESULT=PASS' 'ETAP7_INSTALL_RESULT=PASS' 'INSTALL_ALL_RESULT=PASS'; do
    case "$OUT" in *"$m"*) ok "$m";; *) bad "brak $m"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
done
[ "$(lines_safe "$OUT")" = 1 ] && ok "SAFE TO REMOVE SD dokladnie raz" || bad "SAFE TO REMOVE SD $(lines_safe "$OUT") razy"
[ "$(printf '%s\n' "$OUT" | tail -1)" = '[V8S] SAFE TO REMOVE SD' ] && ok "...i to na samym koncu" || bad "ostatnia linia: $(printf '%s\n' "$OUT" | tail -1)"
case "$OUT" in *"Restart MMI (usblauncher"*|*"Reboot MMI manually"*) bad "posrednie polecenia restartu";; *) ok "bez posrednich polecen restartu";; esac
[ -f "$E4M/installed.ok" ] && [ -f "$E4M/etap4.installed.ok" ] && [ -f "$E4M/etap5.installed.ok" ] && [ -f "$E4M/etap7.installed.ok" ] \
  && ok "cztery znaczniki instalacji" || bad "brakuje znacznikow instalacji"
[ ! -e "$E4M/INSTALL_ALL" ] && ok "INSTALL_ALL zuzyty" || bad "INSTALL_ALL zostal"
cmp -s "$E7LIB" "$E4M/payload/libmib2high-carplay-rgi-ipod.etap7.so" && ok "w aucie biblioteka Etapu 7" || bad "w aucie nie ta biblioteka"
[ -f "$SIM/app/eso/hmi/lsd/jars/mib2high-k3346-overrides.jar" ] && [ -f "$OVL" ] && ok "JAR Etapu 5 i nakladka na miejscu" || bad "brak plikow Etapu 5/7"
OUT=$(run); case "$OUT" in *"MODE=COLLECT"*) ok "kolejne uruchomienie = zbieranie logow";; *) bad "kolejne uruchomienie nie zbiera logow";; esac
ALLSTATE=$(snapshot_files "$SIM/app" "$SIM/system")

echo
echo "--- T17b: INSTALL_ALL drugi raz -> nic nie zmienia, mowi jak aktualizowac"
: > "$E4M/INSTALL_ALL"; OUT=$(run)
case "$OUT" in *"INSTALL_ALL_RESULT=ALREADY_INSTALLED"*) ok "rozpoznal pelna instalacje";; *) bad "nie rozpoznal"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
case "$OUT" in *"uninstall first"*) ok "podpowiedz aktualizacji";; *) bad "brak podpowiedzi aktualizacji";; esac
[ "$ALLSTATE" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "zero zapisow" || bad "drzewo zmienione"
[ ! -e "$E4M/INSTALL_ALL" ] && ok "znacznik zuzyty" || bad "znacznik zostal"

echo
echo "--- T17c: pelny ROLLBACK po INSTALL_ALL -> fabryka bit w bit"
: > "$E4M/ROLLBACK"; OUT=$(run)
case "$OUT" in *"ROLLBACK_RESULT=PASS"*) ok "ROLLBACK PASS";; *) bad "ROLLBACK nie przeszedl"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
for m in ETAP7_ROLLBACK=PASS ETAP5_ROLLBACK=PASS ETAP4_ROLLBACK=PASS; do
    case "$OUT" in *"$m"*) ok "$m";; *) bad "brak $m";; esac
done
[ "$FAB" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "drzewo = fabryka, bit w bit" || bad "drzewo rozne od fabryki"
[ ! -e "$E4M/ROLLBACK" ] && ok "znacznik ROLLBACK zuzyty" || bad "ROLLBACK zostal"

echo
echo "--- T17d: aktualizacja = ponowna instalacja po odinstalowaniu"
: > "$E4M/INSTALL_ALL"; OUT=$(run)
case "$OUT" in *"INSTALL_ALL_RESULT=PASS"*) ok "ponowna instalacja PASS";; *) bad "ponowna instalacja zablokowana"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
case "$OUT" in *"OLD_BACKUP_KEPT"*) ok "stara kopia v8s odsunieta, nie skasowana";; *) bad "brak OLD_BACKUP_KEPT";; esac
ls -d "$E4M"/backup/pre-v8s.old.* >/dev/null 2>&1 && ok "pre-v8s.old.* jest na karcie" || bad "stara kopia zniknela"
[ "$ALLSTATE" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "stan = jak po pierwszej instalacji" || bad "stan inny niz po pierwszej instalacji"
# Najwazniejsze: NOWA kopia musi byc fabryczna, czyli ROLLBACK po aktualizacji
# dalej prowadzi do fabryki, a nie do poprzedniej wersji moda.
: > "$E4M/ROLLBACK"; OUT=$(run)
[ "$FAB" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "ROLLBACK po aktualizacji -> fabryka" || bad "po aktualizacji ROLLBACK nie daje fabryki"

echo
echo "--- T17e: awaria w Etapie 5 -> 5 cofniety, v8s+4 zostaja, bez SAFE TO REMOVE SD"
# Stan wzorcowy najpierw: v8s + Etap 4 z pojedynczych znacznikow.
setup7; run >/dev/null; cp "$SIM/sd/ZNACZNIKI/INSTALL_ETAP4" "$E4M/"; run >/dev/null
REF4=$(snapshot_files "$SIM/app" "$SIM/system")
setupall
awk '{print} /INSTALLING=k3346-coverart-fetch/{print "fail \047TEST_INJECTED_E5\047"}' "$SIM/sd/mod/command.sh" > "$SIM/sd/mod/command.inject5.sh"
OUT=$(env MIB_SD="$SIM/sd" MIB_APP="$SIM/app" MIB_SYSTEM="$SIM/system" MIB_LSD_JXE="$SIM/ifs/lsd.jxe" MIB_BT_ROOTS="$SIM/btroots" sh "$SIM/sd/mod/command.inject5.sh" 2>&1)
case "$OUT" in *"TEST_INJECTED_E5"*) ok "wstrzykniety blad wystapil";; *) bad "blad nie wystapil - test nic nie sprawdzil";; esac
case "$OUT" in *"ETAP5_AUTO_ROLLBACK_RESULT=PASS"*) ok "auto-cofniecie Etapu 5";; *) bad "Etap 5 nie cofniety";; esac
case "$OUT" in *"INSTALL_ALL_RESULT=PASS"*) bad "zglosil sukces mimo bledu";; *) ok "brak falszywego sukcesu";; esac
case "$OUT" in *"MODE=INSTALL_ETAP7"*) bad "Etap 7 ruszyl po bledzie Etapu 5";; *) ok "Etap 7 nie ruszyl";; esac
[ "$(lines_safe "$OUT")" = 0 ] && ok "ani razu SAFE TO REMOVE SD" || bad "SAFE TO REMOVE SD mimo bledu"
case "$OUT" in *"INSTALL_ALL_RESULT=FAIL"*) ok "wynik FAIL wprost";; *) bad "brak INSTALL_ALL_RESULT=FAIL";; esac
[ "$(printf '%s\n' "$OUT" | tail -1)" = '[V8S] Run the script again to retry, or uninstall.' ] && ok "ostatnia linia mowi, co dalej" || bad "ostatnia linia: $(printf '%s\n' "$OUT" | tail -1)"
[ -f "$E4M/installed.ok" ] && [ -f "$E4M/etap4.installed.ok" ] && [ ! -e "$E4M/etap5.installed.ok" ] && [ ! -e "$E4M/etap5.installing.flag" ] \
  && ok "v8s i Etap 4 zainstalowane, Etap 5 czysty" || bad "zle znaczniki po bledzie"
[ -e "$E4M/INSTALL_ALL" ] && ok "INSTALL_ALL zostal (do wznowienia)" || bad "INSTALL_ALL zuzyty mimo bledu"
[ "$REF4" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "drzewo = stan po v8s + Etap 4, bit w bit" || bad "drzewo rozne od stanu po Etapie 4"

echo
echo "--- T17f: wznowienie - ten sam znacznik, zwykly instalator"
OUT=$(run)
case "$OUT" in *"STAGE_MAP=already installed"*"STAGE_ROUTE=already installed"*) ok "v8s i Etap 4 pominiete";; *) bad "nie pominal zainstalowanych"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
case "$OUT" in *"ETAP5_INSTALL_RESULT=PASS"*"ETAP7_INSTALL_RESULT=PASS"*"INSTALL_ALL_RESULT=PASS"*) ok "dokonczyl 5 i 7";; *) bad "nie dokonczyl"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
[ "$ALLSTATE" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "stan = jak po instalacji bez bledu" || bad "stan inny niz po czystej instalacji"
: > "$E4M/ROLLBACK"; run >/dev/null
[ "$FAB" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "ROLLBACK po wznowieniu -> fabryka" || bad "ROLLBACK po wznowieniu nie daje fabryki"

echo
echo "--- T17g: zly firmware + INSTALL_ALL -> ODMOWA, zero zapisow, bez 'wyjmij karte'"
setupall
sed -i 's/^HMI_Version:.*/HMI_Version:HMI_AU_MIB2_H_EU_G22_C6_999999/' "$SIM/app/eso/hmi/version.properties"
B=$(snapshot_files "$SIM/app" "$SIM/system"); OUT=$(run)
case "$OUT" in *"instalacja PRZERWANA"*) ok "odmowa bramy wersji";; *) bad "brak odmowy";; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && [ ! -e "$E4M/backup/pre-v8s" ] && ok "zero zapisow, bez kopii" || bad "zapisy mimo odmowy"
[ "$(lines_safe "$OUT")" = 0 ] && ok "bez SAFE TO REMOVE SD" || bad "SAFE TO REMOVE SD przy odmowie"
case "$OUT" in *"INSTALL_ALL_RESULT=FAIL"*"safe state"*) ok "FAIL + auto bezpieczne";; *) bad "brak czytelnego wyniku odmowy";; esac

echo
echo "--- T17h: ROLLBACK z karty, ktora niczego nie instalowala -> nic do cofniecia"
setup7; : > "$E4M/ROLLBACK"
B=$(snapshot_files "$SIM/app" "$SIM/system"); OUT=$(run)
case "$OUT" in *"ROLLBACK_RESULT=NOTHING_TO_UNDO"*) ok "NOTHING_TO_UNDO";; *) bad "brak NOTHING_TO_UNDO"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
case "$OUT" in *"do not reboot"*) bad "straszy 'do not reboot' bez powodu";; *) ok "nie straszy";; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "zero zapisow" || bad "zapisy przy pustym rollbacku"
[ ! -e "$E4M/ROLLBACK" ] && ok "znacznik zuzyty" || bad "znacznik zostal"

echo
echo "--- T17i: ROLLBACK i INSTALL_ALL naraz -> najpierw ROLLBACK"
setupall; run >/dev/null; : > "$E4M/INSTALL_ALL"; : > "$E4M/ROLLBACK"
OUT=$(run); case "$OUT" in *"MODE=ROLLBACK"*) ok "ROLLBACK wygrywa";; *) bad "INSTALL_ALL zablokowal ratunek";; esac
fi

# ----------------------------------------------------------------------- T19
# Polityka VC (1.0.1, issue #5): znaczniki VC_POLICY_* zapisuja nasz plik
# konfiguracyjny, pelny ROLLBACK go usuwa, bez v8s nic sie nie dzieje.
VCPOL="$SIM/app/root/mib2high-vc.conf"
echo
echo "--- T19a: VC_POLICY_MAP bez zainstalowanego moda -> ODMOWA, zero zapisow"
setup; B=$(snapshot_files "$SIM/app" "$SIM/system")
: > "$SIM/sd/mod/$PKGNAME/VC_POLICY_MAP"
# bez installed.ok dyspozytor najpierw zainstalowalby v8s - znacznik polityki ma pierwszenstwo
OUT=$(run)
case "$OUT" in *"VC_POLICY: the mod is not installed"*) ok "odmowa bez instalacji";; *) bad "brak odmowy"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "zero zapisow" || bad "zapisy mimo odmowy"
[ -e "$SIM/sd/mod/$PKGNAME/VC_POLICY_MAP" ] && ok "znacznik zostal (nic nie zrobiono)" || bad "znacznik zuzyty mimo odmowy"

echo
echo "--- T19b: po instalacji: MAP -> plik, OFF -> nadpisany, RGI -> nadpisany; ROLLBACK -> fabryka"
setup7; FAB=$(snapshot_files "$SIM/app" "$SIM/system"); : > "$E4M/INSTALL_ALL"; run >/dev/null
for pol in map off rgi; do
    m=$(echo "VC_POLICY_$pol" | tr 'a-z' 'A-Z')
    : > "$E4M/$m"; OUT=$(run)
    case "$OUT" in *"VC_POLICY_RESULT=PASS policy=$pol"*) ok "$m: PASS";; *) bad "$m: brak PASS"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
    [ "$(cat "$VCPOL" 2>/dev/null)" = "vc_policy=$pol" ] && ok "$m: plik = vc_policy=$pol" || bad "$m: zla tresc pliku: $(cat "$VCPOL" 2>/dev/null)"
    [ "$(stat -c %a "$VCPOL")" = 644 ] && ok "$m: tryb 644" || bad "$m: tryb $(stat -c %a "$VCPOL")"
    [ ! -e "$E4M/$m" ] && ok "$m: znacznik zuzyty" || bad "$m: znacznik zostal"
    ls "$SIM/app/root" | grep -q 'mib2high-vc.conf.new' && bad "$m: zostal plik tymczasowy" || ok "$m: bez pliku tymczasowego"
done
OUT=$(run); R=$(ls -d "$E4M"/collected/run-* | tail -1)
grep -q 'VC_POLICY_FILE=vc_policy=rgi' "$R/trace/metadata.txt" 2>/dev/null && ok "COLLECT zapisuje polityke" || bad "metadata bez polityki"
: > "$E4M/ROLLBACK"; OUT=$(run)
case "$OUT" in *"ROLLBACK_RESULT=PASS"*) ok "ROLLBACK PASS";; *) bad "ROLLBACK nie przeszedl"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
[ ! -e "$VCPOL" ] && ok "plik polityki usuniety" || bad "plik polityki zostal"
[ "$FAB" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "drzewo = fabryka, bit w bit" || bad "drzewo rozne od fabryki"

echo
echo "--- T19e: resztka pliku tymczasowego (blad printf z rc1) -> sprzatnieta"
setup7; : > "$E4M/INSTALL_ALL"; run >/dev/null
: > "$VCPOL.new.4242"; : > "$E4M/VC_POLICY_MAP"; run >/dev/null
ls "$SIM/app/root" | grep -q 'mib2high-vc.conf.new' && bad "resztka zostala po zmianie polityki" || ok "zmiana polityki sprzata resztke"
: > "$VCPOL.new.4343"; : > "$E4M/ROLLBACK"; run >/dev/null
ls "$SIM/app/root" 2>/dev/null | grep -q 'mib2high-vc.conf' && bad "ROLLBACK zostawil plik polityki/resztke" || ok "ROLLBACK usuwa plik i resztki"

echo
echo "--- T19c: ROLLBACK ma pierwszenstwo przed znacznikiem polityki"
setup7; : > "$E4M/INSTALL_ALL"; run >/dev/null
: > "$E4M/VC_POLICY_MAP"; : > "$E4M/ROLLBACK"; OUT=$(run)
case "$OUT" in *"MODE=ROLLBACK"*) ok "ROLLBACK pierwszy";; *) bad "polityka wyprzedzila ROLLBACK";; esac

echo
echo "--- T19d: jedna karta na caly test: ROLLBACK + INSTALL_ALL + MAP + OFF + RGI naraz"
# Tak jak u uzytkownika: auto z zainstalowana poprzednia wersja, na karte trafia
# wszystko za jednym razem, w aucie tylko kolejne uruchomienia skryptu.
setup7; FAB=$(snapshot_files "$SIM/app" "$SIM/system"); : > "$E4M/INSTALL_ALL"; run >/dev/null
for m in ROLLBACK INSTALL_ALL VC_POLICY_MAP VC_POLICY_OFF VC_POLICY_RGI; do : > "$E4M/$m"; done
nruns(){ ls -d "$E4M"/collected/run-* 2>/dev/null | wc -l; }
O1=$(run); case "$O1" in *"ROLLBACK_RESULT=PASS"*) ok "1.: odinstalowanie";; *) bad "1. nie ROLLBACK"; echo "$O1" | tail -3 | sed 's/^/      | /';; esac
[ "$FAB" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "1.: fabryka bit w bit" || bad "1.: nie fabryka"
O2=$(run); case "$O2" in *"INSTALL_ALL_RESULT=PASS"*) ok "2.: instalacja";; *) bad "2. nie instalacja"; echo "$O2" | tail -3 | sed 's/^/      | /';; esac
[ ! -e "$VCPOL" ] && ok "2.: polityka domyslna (brak pliku)" || bad "2.: plik polityki juz jest"
N0=$(nruns)
O3=$(run); case "$O3" in *"COLLECT_RESULT="*"VC_POLICY_RESULT=PASS policy=map"*) ok "3.: logi + map";; *) bad "3. nie logi+map"; echo "$O3" | tail -4 | sed 's/^/      | /';; esac
case "$O3" in *"NEXT_QUEUED=VC_POLICY_OFF"*"NEXT_QUEUED=VC_POLICY_RGI"*) ok "3.: zapowiada OFF i RGI";; *) bad "3.: brak zapowiedzi kolejki";; esac
[ "$(lines_safe "$O3")" = 1 ] && [ "$(printf '%s\n' "$O3" | tail -1)" = '[V8S] SAFE TO REMOVE SD' ] && ok "3.: SAFE TO REMOVE SD raz, na koncu" || bad "3.: SAFE TO REMOVE SD nie raz/nie na koncu"
R3=$(ls -d "$E4M"/collected/run-* | sort -V | tail -1)
grep -q 'VC_POLICY_FILE=none (rgi)' "$R3/trace/metadata.txt" 2>/dev/null && ok "3.: logi z trasy pod rgi" || bad "3.: metadata nie pokazuje poprzedniej polityki"
O4=$(run); case "$O4" in *"VC_POLICY_RESULT=PASS policy=off"*) ok "4.: logi + off";; *) bad "4. nie off";; esac
R4=$(ls -d "$E4M"/collected/run-* | sort -V | tail -1)
grep -q 'VC_POLICY_FILE=vc_policy=map' "$R4/trace/metadata.txt" 2>/dev/null && ok "4.: logi z trasy pod map" || bad "4.: zla polityka w logach"
O5=$(run); case "$O5" in *"VC_POLICY_RESULT=PASS policy=rgi"*) ok "5.: logi + rgi";; *) bad "5. nie rgi";; esac
case "$O5" in *"NEXT_QUEUED"*) bad "5.: kolejka nie pusta";; *) ok "5.: kolejka pusta";; esac
R5=$(ls -d "$E4M"/collected/run-* | sort -V | tail -1)
grep -q 'VC_POLICY_FILE=vc_policy=off' "$R5/trace/metadata.txt" 2>/dev/null && ok "5.: logi z trasy pod off" || bad "5.: zla polityka w logach"
[ "$(( $(nruns) - N0 ))" = 3 ] && ok "trzy nowe katalogi logow" || bad "katalogow logow: $(( $(nruns) - N0 ))"
O6=$(run); case "$O6" in *"MODE=COLLECT"*) ok "6.: zwykle zbieranie logow";; *) bad "6. nie COLLECT";; esac
[ "$(cat "$VCPOL" 2>/dev/null)" = "vc_policy=rgi" ] && ok "na koniec polityka rgi" || bad "na koniec: $(cat "$VCPOL" 2>/dev/null)"

# ----------------------------------------------------------------------- T18
# Zipy wydania dokladnie tak, jak dostaje je uzytkownik: rozpakowane na karte,
# bez naszych paczek etapowych i bez ZNACZNIKI/. Wlaczane zmienna RELEASE_DIR
# (katalog z work/release/<wersja>, tools/make_release.sh).
if [ -n "${RELEASE_DIR:-}" ]; then
unz(){ python3 -c 'import sys,zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' "$1" "$2"; }
ZIP_I=$(ls "$RELEASE_DIR"/*-install.zip 2>/dev/null | head -1)
ZIP_U=$(ls "$RELEASE_DIR"/*-uninstall.zip 2>/dev/null | head -1)
echo
echo "--- T18a: install.zip na karte -> jedno uruchomienie"
if [ -z "$ZIP_I" ] || [ -z "$ZIP_U" ]; then
    bad "brak zipow w $RELEASE_DIR"
else
RM=/sd/mod/$PKGNAME
setup nocard; FAB=$(snapshot_files "$SIM/app" "$SIM/system")
unz "$ZIP_I" "$SIM/sd"
TOP=$(cd "$SIM/sd" && ls -A | LC_ALL=C sort | tr '\n' ' ')
[ "$TOP" = "README_CarPlay_VC.txt mod " ] && ok "w katalogu glownym karty tylko mod/ i README" || bad "katalog glowny: $TOP"
[ -e "$SIM$RM/INSTALL_ALL" ] && ok "INSTALL_ALL uzbrojony w zipie" || bad "zip bez INSTALL_ALL"
if [ -f "$INSTALLER" ]; then cmp -s "$SIM/sd/mod/command.sh" "$INSTALLER" && ok "w zipie instalator ze zrodel" || bad "w zipie inny instalator niz w zrodlach"; fi
VER=$(cat "$SIM$RM/RELEASE" 2>/dev/null)
OUT=$(run)
case "$OUT" in *"INSTALL_ALL_RESULT=PASS"*) ok "INSTALL_ALL_RESULT=PASS";; *) bad "instalacja z zipa nie przeszla"; echo "$OUT" | tail -6 | sed 's/^/      | /';; esac
case "$OUT" in *"RELEASE=$VER"*) ok "wersja $VER w wyniku";; *) bad "brak RELEASE w wyniku";; esac
[ "$(lines_safe "$OUT")" = 1 ] && [ "$(printf '%s\n' "$OUT" | tail -1)" = '[V8S] SAFE TO REMOVE SD' ] \
  && ok "SAFE TO REMOVE SD raz, na koncu" || bad "SAFE TO REMOVE SD nie raz albo nie na koncu"
[ -f "$SIM$RM/installed.ok" ] && [ -f "$SIM$RM/etap4.installed.ok" ] && [ -f "$SIM$RM/etap5.installed.ok" ] && [ -f "$SIM$RM/etap7.installed.ok" ] \
  && ok "cztery etapy zainstalowane" || bad "nie wszystkie etapy"
INST=$(snapshot_files "$SIM/app" "$SIM/system")
OUT=$(run); R=$(ls -d "$SIM$RM"/collected/run-* 2>/dev/null | tail -1)
grep -q "RELEASE=$VER" "$R/trace/metadata.txt" 2>/dev/null && ok "zebrane logi znaja wersje" || bad "metadata bez wersji"

echo
echo "--- T18b: uninstall.zip na te sama karte -> fabryka bit w bit"
unz "$ZIP_U" "$SIM/sd"
OUT=$(run)
case "$OUT" in *"ROLLBACK_RESULT=PASS"*) ok "ROLLBACK_RESULT=PASS";; *) bad "odinstalowanie nie przeszlo"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
[ "$FAB" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "drzewo = fabryka, bit w bit" || bad "drzewo rozne od fabryki"

echo
echo "--- T18c: aktualizacja - install.zip jeszcze raz na te sama karte"
unz "$ZIP_I" "$SIM/sd"
OUT=$(run)
case "$OUT" in *"INSTALL_ALL_RESULT=PASS"*) ok "ponowna instalacja z zipa";; *) bad "ponowna instalacja z zipa nie przeszla"; echo "$OUT" | tail -4 | sed 's/^/      | /';; esac
[ "$INST" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "stan = jak po pierwszej instalacji" || bad "stan inny niz po pierwszej instalacji"
unz "$ZIP_U" "$SIM/sd"; OUT=$(run)
[ "$FAB" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "i znowu fabryka po odinstalowaniu" || bad "po aktualizacji odinstalowanie nie daje fabryki"

echo
echo "--- T18d: oba zipy na karcie, ktora niczego nie instalowala"
# Np. rozpakowane, ale instalacja nigdy nie uruchomiona, albo inna karta.
setup nocard; B=$(snapshot_files "$SIM/app" "$SIM/system")
unz "$ZIP_I" "$SIM/sd"; unz "$ZIP_U" "$SIM/sd"
OUT=$(run)
case "$OUT" in *"ROLLBACK_RESULT=NOTHING_TO_UNDO"*) ok "nic do cofniecia, bez strachu";; *) bad "zly wynik"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "zero zapisow" || bad "zapisy"

echo
echo "--- T18f: vc-policy-map.zip na zainstalowanej karcie -> polityka map, uninstall -> fabryka"
ZIP_M=$(ls "$RELEASE_DIR"/*-vc-policy-map.zip 2>/dev/null | head -1)
if [ -z "$ZIP_M" ]; then bad "brak vc-policy-map.zip"; else
setup nocard; FAB=$(snapshot_files "$SIM/app" "$SIM/system")
unz "$ZIP_I" "$SIM/sd"; run >/dev/null
unz "$ZIP_M" "$SIM/sd"; OUT=$(run)
case "$OUT" in *"VC_POLICY_RESULT=PASS policy=map"*) ok "polityka map z zipa";; *) bad "polityka z zipa nie przeszla"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
[ "$(cat "$SIM/app/root/mib2high-vc.conf" 2>/dev/null)" = "vc_policy=map" ] && ok "plik w aucie" || bad "brak pliku w aucie"
unz "$ZIP_U" "$SIM/sd"; run >/dev/null
[ "$FAB" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "uninstall -> fabryka" || bad "uninstall nie daje fabryki"
fi

echo
echo "--- T18e: collect-logs.zip po nieudanej instalacji -> zbiera logi, nie ponawia"
ZIP_L=$(ls "$RELEASE_DIR"/*-collect-logs.zip 2>/dev/null | head -1)
if [ -z "$ZIP_L" ]; then bad "brak collect-logs.zip"; else
setup nocard; B=$(snapshot_files "$SIM/app" "$SIM/system")
unz "$ZIP_I" "$SIM/sd"; sed -i 's/^HMI_Version:.*/HMI_Version:HMI_AU_MIB2_H_EU_G22_C6_999999/' "$SIM/app/eso/hmi/version.properties"
OUT=$(run); case "$OUT" in *"INSTALL_ALL_RESULT=FAIL"*) ok "instalacja odmowila (stan wyjsciowy)";; *) bad "brak odmowy";; esac
unz "$ZIP_L" "$SIM/sd"; OUT=$(run)
case "$OUT" in *"FORCED_COLLECT=1"*"COLLECT_RESULT="*) ok "zebral logi";; *) bad "nie zebral logow"; echo "$OUT" | tail -3 | sed 's/^/      | /';; esac
case "$OUT" in *"MODE=INSTALL_ALL"*) bad "ponowil instalacje";; *) ok "nie ponowil instalacji";; esac
ls -d "$SIM$RM"/collected/run-* >/dev/null 2>&1 && ok "katalog run-NN powstal" || bad "brak run-NN"
fi
fi
fi

echo
echo "=========================================================="
echo " WYNIK: $PASS OK, $FAILED FAIL"
echo "=========================================================="
[ "$FAILED" -eq 0 ] || exit 1
