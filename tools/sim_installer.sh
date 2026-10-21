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
    # paczka na "karcie SD"
    cp -a "$PKG/." "$SIM/sd/"
}

run(){ # run [dodatkowe env...]
    env MIB_SD="$SIM/sd" MIB_APP="$SIM/app" MIB_SYSTEM="$SIM/system" \
        MIB_LSD_JXE="$SIM/ifs/lsd.jxe" "$@" \
        sh "$SIM/sd/mod/command.sh" 2>&1
}

echo "=========================================================="
echo " SYMULACJA INSTALATORA K3346 (bez auta)   $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "=========================================================="

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
echo "--- T14h: COLLECT zbiera obiekty PPS iAP2 (telephony/nowplaying), drzewo auta nietkniete"
setup; run >/dev/null
mkdir -p "$SIM/pps"; printf '@telephony\nCallStateStatus:n:0\n' > "$SIM/pps/telephony"; printf '@nowplaying\nMediaItemTitle::x\n' > "$SIM/pps/nowplaying"
B=$(snapshot_files "$SIM/app" "$SIM/system")
OUT=$(run MIB_PPS_IAP2="$SIM/pps"); R=$(ls -d "$E4M"/collected/run-* | tail -1)
grep -q 'CallStateStatus' "$R/pps/telephony.txt" 2>/dev/null && grep -q 'MediaItemTitle' "$R/pps/nowplaying.txt" 2>/dev/null && ok "pps/telephony.txt i pps/nowplaying.txt zebrane" || bad "brak zrzutu PPS"
[ "$B" = "$(snapshot_files "$SIM/app" "$SIM/system")" ] && ok "drzewo auta nietkniete" || bad "COLLECT zmienil drzewo"

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

echo
echo "=========================================================="
echo " WYNIK: $PASS OK, $FAILED FAIL"
echo "=========================================================="
[ "$FAILED" -eq 0 ] || exit 1
