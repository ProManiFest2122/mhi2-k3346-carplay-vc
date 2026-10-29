#!/bin/bash
# Symulacja KARTY RATUNKOWEJ - bez auta. Uruchamiac W JEDNORAZOWYM KONTENERZE
# (--rm) jako root: skrypt tworzy w nim /net/mmx/..., /mnt/app, /sd i podmienia
# 'on', 'mount' i 'sleep' na atrapy w PATH. Nic z tego nie trafia na komputer.
#
# Testowany jest PRAWDZIWY Launcher/stuff.sh i ratunek/ratunek.sh z
# work/sd_ratunek - jedyne atrapy to apps/backup i apps/launcher M.I.B. (zapisuja
# tylko kolejnosc wywolan) oraz polecenia systemowe QNX, ktorych nie ma w Linuksie.
#
#   R1 czyste auto (test przed instalacja) -> drzewo auta bit w bit bez zmian
#   R2 po instalacji -> HMI-boot jak przed instalacja; potem pelny rollback
#      instalatorem -> drzewo identyczne z przed-instalacyjnym
#   R3 pusta/uszkodzona kopia traceConfig na karcie -> traceConfig w aucie
#      NIETKNIETY, reszta ratunku wykonana
#   R4 MMX nieosiagalny -> ponawianie, WYNIK=NIEPEWNY, M.I.B. dziala dalej
#   R5 brak ratunek.sh na karcie -> zmieniony stuff.sh nie psuje M.I.B.,
#      a slad STUFF_KONIEC mowi, ze ratunek.sh nie zostal znaleziony
#
# Warunki z TESTU W AUCIE 2026-10-20 (pierwsza wersja nie zostawila logu):
#   R6 karta TYLKO DO ODCZYTU, odblokowuje ja dopiero M.I.B. (apps/mounts:20)
#      -> dzialania wykonane, log z WYNIK=OK trafia na karte
#   R7 ubogi PATH jak na RCC (narzedzia leza na MMX) -> skrypt sam ustawia PATH
#   R8 karta w ogole sie nie odblokowuje -> dzialania na aucie I TAK wykonane,
#      log zostaje w RAM
#
# RESCUE=<paczka> pozwala uruchomic te same testy na innej wersji karty;
# TYLKO_R678=1 uruchamia tylko R6-R8. Dowod, ze testy lapia blad z auta:
#   RESCUE=/work/test_ratunku_1/sd_ratunek_v1 TYLKO_R678=1  -> musi PASC.

set -u

CARF=/work/sd_odczyt_1/MIB2_SOFTWARE_COLLECTOR_v1.2_PID_8523907/files
REF=/work/mmx_app
RESCUE="${RESCUE:-/work/sd_ratunek}"
INSTPKG=/work/sd_etap5
PKGNAME=mib2high-carplay-rgi-v8s-simple
APP=/net/mmx/mnt/app
SYS=/net/mmx/mnt/system
LSD=$APP/eso/hmi/lsd

PASS=0; FAILED=0
ok(){  PASS=$((PASS+1));     echo "  OK   $*"; }
bad(){ FAILED=$((FAILED+1)); echo "  FAIL $*"; }

# --- atrapy polecen QNX ---
# Uruchamiamy stuff.sh z katalogu /tmp/fakebin: PATH M.I.B. zaczyna sie od ':'
# (= biezacy katalog), wiec atrapy wygrywaja tez wtedy, gdy skrypt ustawia
# wlasny PATH - tak jak na RCC.
mkdir -p /tmp/fakebin
printf '#!/bin/sh\n[ "$1" = "-f" ] && shift 2\nexec "$@"\n' > /tmp/fakebin/on
cat > /tmp/fakebin/mount <<'EOF'
#!/bin/sh
# 'mount -uw <nosnik karty>' = odblokowanie karty (jak apps/mounts:20 w M.I.B.).
# /tmp/mount_blokada = karta sie nie odblokowuje (R8).
[ "$1" = "-uw" ] || exit 0
t=$(readlink -f "$2" 2>/dev/null)
if [ "$t" = "/sd" ]; then
    [ -e /tmp/mount_blokada ] && exit 1
    chmod -R u+w /sd 2>/dev/null
fi
exit 0
EOF
printf '#!/bin/sh\nexit 0\n' > /tmp/fakebin/sleep
chmod 755 /tmp/fakebin /tmp/fakebin/*
export PATH=/tmp/fakebin:$PATH

# Zwykly uzytkownik do R6/R8. W obrazie nie ma useradd, a su odmawia (PAM) -
# wpis do /etc/passwd jednorazowego kontenera + setpriv. Jesli przelaczenie
# nie dziala, test MUSI sie zatrzymac: bez tego R6/R8 "padaja" z powodu samego
# testu (tak bylo w pierwszej wersji tej symulacji), a nie ratunku.
grep -q '^sim:' /etc/passwd || { echo 'sim:x:1000:1000:sim:/tmp:/bin/sh' >> /etc/passwd; echo 'sim:x:1000:' >> /etc/group; }
if [ "$(setpriv --reuid=1000 --regid=1000 --clear-groups id -u 2>/dev/null)" != 1000 ]; then
    echo "BLAD SYMULACJI: nie da sie uruchomic polecenia jako zwykly uzytkownik - R6/R8 bylyby nieprawdziwe"
    exit 2
fi

snap_files(){ ( cd "$1" && find . -type f | LC_ALL=C sort | while IFS= read -r p; do
    printf '%s %s %s\n' "$(sha256sum < "$p" | cut -d' ' -f1)" "$(stat -c %a "$p")" "$p"; done ); }
snap_dirs(){  ( cd "$1" && find . -type d | LC_ALL=C sort ); }
snap(){ snap_files "$APP"; snap_files "$SYS"; snap_dirs "$APP"; snap_dirs "$SYS"; }

setup_car(){
    rm -rf /net/mmx /net/rcc /mnt/app /fs /sd /inst /ifs_fake /tmp/order.txt /tmp/mount_blokada
    mkdir -p "$LSD/jars" "$APP/armle/lib/dll" "$APP/root" "$SYS/etc/eso/production" \
             /net/mmx/fs /fs /ifs_fake /net/rcc/dev/shmem
    chmod 777 /net/rcc/dev/shmem
    : > /tmp/order.txt; chmod 666 /tmp/order.txt
    cp "$CARF/mnt/app/eso/hmi/lsd/bundles_dsitracer_enabled.properties" "$LSD/"
    cp "$CARF/mnt/app/eso/hmi/lsd/traceConfig.properties"               "$LSD/"
    cp "$CARF/mnt/app/eso/hmi/lsd/DSITracer.jar"                        "$LSD/"
    cp "$REF"/eso/hmi/lsd/jars/*.jar "$LSD/jars/" 2>/dev/null || true
    cp "$REF/eso/hmi/version.properties"                                "$APP/eso/hmi/"
    cp "$CARF/mnt/app/armle/lib/dll/ipod-drvr-iap2.so"                  "$APP/armle/lib/dll/"
    cp "$CARF/mnt/system/etc/eso/production/smartphone_integrator.json" "$SYS/etc/eso/production/"
    chmod 644 "$LSD/traceConfig.properties"     # znany tryb - test zachowania trybu
    truncate -s 58131096 /ifs_fake/lsd.jxe
    ln -s "$APP" /mnt/app                        # widok "na MMX" dla on -f mmx
    # karta ratunkowa z atrapami backup/launcher M.I.B.
    cp -a "$RESCUE/." /sd/ 2>/dev/null || { mkdir -p /sd && cp -a "$RESCUE/." /sd/; }
    # Atrapy M.I.B.: najpierw odblokowuja karte (jak backup/launcher przez
    # config/GLOBALS -> apps/mounts -usb), potem notuja kolejnosc.
    for a in backup launcher; do
        printf '#!/bin/sh\n/tmp/fakebin/mount -uw /sd\n[ -f /sd/RATUNEK_LOG.txt ] && w=po_ratunku || w=PRZED_ratunkiem\necho "STUB %s $w" >> /tmp/order.txt\n' "$a" > /sd/apps/$a
        chmod 755 /sd/apps/$a
    done
    ln -s /sd /net/mmx/fs/sda0
    ln -s /sd /fs/sda0
}

run_rescue(){ ( cd /tmp/fakebin && /bin/ksh /sd/Launcher/stuff.sh HW_SIM /sd ) > /tmp/stuff_out.txt 2>&1; }

# Warunki z auta: karta tylko do odczytu, skrypt jako zwykly uzytkownik
# (root ignoruje prawa zapisu, wiec bez tego test niczego by nie dowodzil).
karta_tylko_odczyt(){
    chown -R 1000:1000 /sd /net/mmx/mnt /net/rcc
    chmod -R a-w /sd
    # kontrola: zwykly uzytkownik naprawde nie moze pisac na karte
    if setpriv --reuid=1000 --regid=1000 --clear-groups sh -c ': > /sd/.test_zapisu' 2>/dev/null; then
        echo "BLAD SYMULACJI: karta miala byc tylko do odczytu, a da sie na nia pisac"; exit 2
    fi
}
run_as_sim(){ # $1 = PATH procesu stuff.sh
    setpriv --reuid=1000 --regid=1000 --clear-groups \
        sh -c "cd /tmp/fakebin && PATH='$1' /bin/ksh /sd/Launcher/stuff.sh HW_SIM /sd" > /tmp/stuff_out.txt 2>&1
}

install_mod(){
    mkdir -p /inst && cp -a "$INSTPKG/." /inst/
    env MIB_SD=/inst MIB_APP="$APP" MIB_SYSTEM="$SYS" MIB_LSD_JXE=/ifs_fake/lsd.jxe \
        sh /inst/mod/command.sh 2>&1
    # Etap 5: JAR z klasami podmieniajacymi fabryczne - najgrozniejszy dla HMI,
    # wiec ratunek musi go usuwac. Atrapa (tresc bez znaczenia, liczy sie nazwa).
    if [ -d "$LSD/jars" ]; then
        printf 'PK-atrapa-overrides' > "$LSD/jars/mib2high-k3346-overrides.jar"
        chmod 644 "$LSD/jars/mib2high-k3346-overrides.jar"
    fi
}
rollback_mod(){
    cp /inst/ZNACZNIKI/ROLLBACK "/inst/mod/$PKGNAME/"
    env MIB_SD=/inst MIB_APP="$APP" MIB_SYSTEM="$SYS" MIB_LSD_JXE=/ifs_fake/lsd.jxe \
        sh /inst/mod/command.sh 2>&1
}

STOCK_TRACE=$(sha256sum < "$CARF/mnt/app/eso/hmi/lsd/traceConfig.properties" | cut -d' ' -f1)
hash_of(){ sha256sum < "$1" | cut -d' ' -f1; }

# Bez /bin/ksh ta symulacja nie odmawia - ona CICHO ZDAJE czesc testow.
# Kazdy przypadek postaci "drzewo auta bez zmian" wychodzi OK wlasnie dlatego,
# ze ratunek.sh w ogole sie nie uruchomil. Zdarzylo sie to 2026-10-27 i przez
# chwile wygladalo na regresje w kodzie, ktorego ta symulacja nawet nie czyta.
# Obraz bazowy ma tylko dash, wiec:
#   apt-get install -y mksh && ln -sf /bin/mksh /bin/ksh
if [ ! -x /bin/ksh ]; then
    echo "FAIL: brak /bin/ksh - bez niego ratunek.sh sie nie uruchomi," >&2
    echo "      a symulacja CICHO zdalaby testy 'drzewo bez zmian'." >&2
    echo "      W kontenerze: apt-get install -y mksh && ln -sf /bin/mksh /bin/ksh" >&2
    exit 2
fi

echo "=========================================================="
echo " SYMULACJA KARTY RATUNKOWEJ   powloka: $(readlink -f /bin/ksh)"
echo " paczka: $RESCUE"
echo "=========================================================="

if [ "${TYLKO_R678:-0}" != 1 ]; then

echo; echo "--- R0: paczka karty"
[ -f "$RESCUE/Swdlautorun.txt" ] && [ ! -s "$RESCUE/Swdlautorun.txt" ] && ok "uzbrojona: pusty Swdlautorun.txt" || bad "brak/niepusty Swdlautorun.txt"
[ ! -e "$RESCUE/_Swdlautorun.txt" ] && ok "brak _Swdlautorun.txt" || bad "jest jeszcze _Swdlautorun.txt"
[ ! -e "$RESCUE/mod/command.sh" ] && ok "brak mod/command.sh (launcher niczego nie wczyta)" || bad "na karcie ratunkowej jest mod/command.sh!"
[ "$(hash_of "$RESCUE/ratunek/traceConfig.properties")" = "$STOCK_TRACE" ] && ok "traceConfig na karcie = z auta" || bad "traceConfig na karcie rozny od auta"

echo; echo "--- R1: czyste auto (test przed instalacja)"
setup_car
B=$(snap)
run_rescue
A=$(snap)
if [ "$B" = "$A" ]; then ok "drzewo auta bit w bit bez zmian (tresc, tryby, katalogi)"
else bad "drzewo auta ZMIENIONE na czystym aucie:"; diff <(echo "$B") <(echo "$A") | head -10 | sed 's/^/        /'; fi
grep -q "WYNIK=OK" /sd/RATUNEK_LOG.txt 2>/dev/null && ok "RATUNEK_LOG.txt: WYNIK=OK" || bad "brak WYNIK=OK w logu"
grep -q "STUB backup po_ratunku" /tmp/order.txt && grep -q "STUB launcher po_ratunku" /tmp/order.txt \
  && ok "ratunek wykonal sie PRZED backupem i launcherem M.I.B." || bad "zla kolejnosc: $(tr '\n' ' ' < /tmp/order.txt)"
ls "$LSD"/traceConfig.properties.ratunek.* >/dev/null 2>&1 && bad "zostal plik tymczasowy" || ok "brak plikow tymczasowych"
echo "      RATUNEK_LOG.txt (tak to zobaczy uzytkownik po tescie):"
sed 's/^/      | /' /sd/RATUNEK_LOG.txt

echo; echo "--- R2: po instalacji -> ratunek -> pelny rollback"
setup_car
PRE=$(snap)
OUT=$(install_mod)
case "$OUT" in *INSTALL_RESULT=PASS*) ok "instalacja moda (stan wyjsciowy bootloopu)";; *) bad "instalacja nie przeszla"; echo "$OUT" | tail -5;; esac
[ -e "$LSD/dsitracer_activated" ] && [ -e "$LSD/jars/mib2high-carplay-rgi.jar" ] && [ -e "$LSD/jars/mib2high-k3346-overrides.jar" ] && ok "przed ratunkiem: znacznik, JAR v8s i JAR Etapu 5 sa" || bad "stan po instalacji niezgodny"
MODE_BEFORE=$(stat -c %a "$LSD/traceConfig.properties")
run_rescue
[ ! -e "$LSD/dsitracer_activated" ] && ok "usuniety dsitracer_activated" || bad "dsitracer_activated zostal"
[ ! -e "$LSD/jars/mib2high-carplay-rgi.jar" ] && ok "usuniety nasz JAR" || bad "JAR zostal"
[ ! -e "$LSD/jars/mib2high-k3346-overrides.jar" ] && ok "usuniety JAR Etapu 5 (overrides)" || bad "JAR Etapu 5 zostal"
[ "$(hash_of "$LSD/traceConfig.properties")" = "$STOCK_TRACE" ] && ok "traceConfig = fabryczny" || bad "traceConfig nie jest fabryczny"
[ "$(stat -c %a "$LSD/traceConfig.properties")" = "$MODE_BEFORE" ] && ok "tryb traceConfig zachowany ($MODE_BEFORE)" || bad "tryb traceConfig zmieniony"
JARS_STOCK=$(cd "$REF/eso/hmi/lsd/jars" && ls *.jar | LC_ALL=C sort); JARS_NOW=$(cd "$LSD/jars" && ls *.jar | LC_ALL=C sort)
[ "$JARS_STOCK" = "$JARS_NOW" ] && ok "jars/ = dokladnie fabryczne JAR-y (bootclasspath jak przed instalacja)" || bad "jars/ rozne od fabrycznych"
[ -e "$APP/root/hooks/libmib2high-carplay-rgi.so" ] && ok "reszta moda zostaje (to cofa dopiero rollback) - zgodnie z zalozeniem" || bad "ratunek ruszyl wiecej niz powinien"
OUT=$(rollback_mod)
case "$OUT" in *ROLLBACK_RESULT=PASS*) ok "pelny rollback instalatorem po ratunku";; *) bad "rollback po ratunku nie przeszedl"; echo "$OUT" | tail -5;; esac
POSTF=$(snap_files "$APP"; snap_files "$SYS"); PREF=$(echo "$PRE" | grep -E '^[0-9a-f]{64} ')
[ "$PREF" = "$POSTF" ] && ok "po ratunku + rollbacku pliki = przed instalacja (bit w bit, tryby)" \
  || { bad "pliki po ratunku+rollbacku rozne od przed-instalacyjnych:"; diff <(echo "$PREF") <(echo "$POSTF") | head -10 | sed 's/^/        /'; }

echo; echo "--- R2b: ratunek usuwa pliki Etapu 7, ale nie rusza mapy ani RGI"
# Etap 7 lezy w /mnt/app/root, nie w lsd/. Ratunek ma skasowac TYLKO jego wlasne
# pliki: skrypt sterujacy i biblioteka mm-ipod naleza do v8s / Etapu 4 i ich brak
# zepsulby mape na VC oraz wskazowki nawigacji.
setup_car
install_mod >/dev/null
mkdir -p "$APP/root/hooks"
for f in vc_overlay vc_overlay_probe k3346-overlay-probe.sh mib2high-overlay.conf; do
    printf 'etap7' > "$APP/root/$f"
done
printf 'v8s-control' > "$APP/root/mib2high-lvds-control.sh"
printf 'etap4-lib'   > "$APP/root/hooks/libmib2high-carplay-rgi-ipod.so"
run_rescue
E7_LEFT=''
for f in vc_overlay vc_overlay_probe k3346-overlay-probe.sh mib2high-overlay.conf; do
    [ -e "$APP/root/$f" ] && E7_LEFT="$E7_LEFT $f"
done
[ -z "$E7_LEFT" ] && ok "usuniete wszystkie cztery pliki Etapu 7" || bad "zostaly pliki Etapu 7:$E7_LEFT"
[ -e "$APP/root/mib2high-lvds-control.sh" ] \
  && ok "mib2high-lvds-control.sh zostaje (inaczej mapa na VC by nie wstala)" \
  || bad "ratunek skasowal skrypt sterujacy v8s"
[ -e "$APP/root/hooks/libmib2high-carplay-rgi-ipod.so" ] \
  && ok "biblioteka Etapu 4 zostaje (inaczej RGI by padlo)" \
  || bad "ratunek skasowal biblioteke Etapu 4"

echo; echo "--- R3: pusta kopia traceConfig na karcie -> traceConfig w aucie nietkniety"
setup_car
install_mod >/dev/null
T_BEFORE=$(hash_of "$LSD/traceConfig.properties")
: > /sd/ratunek/traceConfig.properties
run_rescue
[ "$(hash_of "$LSD/traceConfig.properties")" = "$T_BEFORE" ] && [ -s "$LSD/traceConfig.properties" ] \
  && ok "traceConfig w aucie NIE nadpisany pustym plikiem" || bad "traceConfig w aucie uszkodzony!"
grep -q "brak/pusta kopia" /sd/RATUNEK_LOG.txt && ok "w logu ostrzezenie o pustej kopii" || bad "brak ostrzezenia w logu"
[ ! -e "$LSD/dsitracer_activated" ] && [ ! -e "$LSD/jars/mib2high-carplay-rgi.jar" ] \
  && ok "znacznik i JAR mimo to usuniete" || bad "ratunek zatrzymal sie na traceConfig"

echo; echo "--- R4: MMX nieosiagalny -> ponawianie, WYNIK=NIEPEWNY, M.I.B. dziala dalej"
setup_car
mv "$APP" /net/mmx/mnt/app_odlaczony
run_rescue
grep -q "WYNIK=NIEPEWNY" /sd/RATUNEK_LOG.txt && ok "WYNIK=NIEPEWNY (nie udaje sukcesu)" || bad "falszywy sukces przy niedostepnym MMX"
[ "$(grep -c 'ponawiam' /sd/RATUNEK_LOG.txt)" = 6 ] && ok "6 prob ponowienia" || bad "liczba prob: $(grep -c 'ponawiam' /sd/RATUNEK_LOG.txt)"
grep -q "STUB launcher" /tmp/order.txt && ok "M.I.B. (launcher) wykonal sie mimo to" || bad "ratunek zablokowal reszte stuff.sh"
mv /net/mmx/mnt/app_odlaczony "$APP"

echo; echo "--- R5: brak ratunek.sh na karcie -> stuff.sh dziala jak oryginal"
setup_car
rm -f /sd/ratunek/ratunek.sh
B=$(snap); run_rescue; A=$(snap)
[ "$B" = "$A" ] && ok "drzewo auta bez zmian" || bad "drzewo zmienione"
grep -q "WYNIK=" /sd/RATUNEK_LOG.txt 2>/dev/null && bad "WYNIK mimo braku skryptu?" || ok "brak wyniku ratunku (nie wykonal sie)"
grep -q "STUFF_KONIEC nosnik=/sd ratunek.sh=brak" /sd/RATUNEK_LOG.txt 2>/dev/null \
  && ok "slad STUFF_KONIEC mowi wprost: ratunek.sh=brak" || bad "brak sladu STUFF_KONIEC"
grep -q "STUB backup" /tmp/order.txt && grep -q "STUB launcher" /tmp/order.txt && ok "backup i launcher M.I.B. wykonane normalnie" || bad "zmieniony stuff.sh zepsul M.I.B."

fi   # koniec R0-R5

# Wspolne sprawdzenia "po ratunku na zainstalowanym aucie".
sprawdz_dzialania(){
    [ ! -e "$LSD/dsitracer_activated" ] && ok "usuniety dsitracer_activated" || bad "dsitracer_activated zostal"
    [ ! -e "$LSD/jars/mib2high-carplay-rgi.jar" ] && ok "usuniety nasz JAR" || bad "JAR zostal"
    [ ! -e "$LSD/jars/mib2high-k3346-overrides.jar" ] && ok "usuniety JAR Etapu 5 (overrides)" || bad "JAR Etapu 5 zostal"
    [ "$(hash_of "$LSD/traceConfig.properties")" = "$STOCK_TRACE" ] && ok "traceConfig = fabryczny" || bad "traceConfig NIE przywrocony"
}

echo; echo "--- R6: karta tylko do odczytu, odblokowuje ja dopiero M.I.B. (jak w aucie)"
setup_car
install_mod >/dev/null
karta_tylko_odczyt
run_as_sim "/tmp/fakebin:/usr/bin:/bin"
sprawdz_dzialania
grep -q "WYNIK=OK" /sd/RATUNEK_LOG.txt 2>/dev/null && ok "RATUNEK_LOG.txt na karcie z WYNIK=OK" || bad "brak logu z WYNIK=OK na karcie"
grep -q "STUFF_KONIEC nosnik=/sd ratunek.sh=jest" /sd/RATUNEK_LOG.txt 2>/dev/null && ok "slad STUFF_KONIEC na karcie" || bad "brak sladu STUFF_KONIEC"
[ "$(grep -c '=== RATUNEK start' /sd/RATUNEK_LOG.txt 2>/dev/null)" = 1 ] && ok "log dopisany dokladnie raz (bez dubli)" || bad "log zdublowany albo brak"
[ ! -s /net/rcc/dev/shmem/ratunek.log ] && ok "log w RAM wyczyszczony po zapisaniu" || bad "log zostal w RAM"

echo; echo "--- R7: ubogi PATH jak na RCC (narzedzia leza na MMX)"
setup_car
install_mod >/dev/null
( cd /tmp/fakebin && PATH=/nonexistent /bin/ksh /sd/Launcher/stuff.sh HW_SIM /sd ) > /tmp/stuff_out.txt 2>&1
sprawdz_dzialania
grep -q "WYNIK=OK" /sd/RATUNEK_LOG.txt 2>/dev/null && ok "RATUNEK_LOG.txt z WYNIK=OK" || bad "brak logu z WYNIK=OK"

echo; echo "--- R8: karta w ogole sie nie odblokowuje -> dzialania i tak wykonane"
setup_car
install_mod >/dev/null
karta_tylko_odczyt
touch /tmp/mount_blokada
run_as_sim "/tmp/fakebin:/usr/bin:/bin"
sprawdz_dzialania
[ ! -e /sd/RATUNEK_LOG.txt ] && ok "logu na karcie brak (nie da sie pisac) - zgodnie z warunkami" || bad "log na karcie mimo blokady?"
grep -q "WYNIK=OK" /net/rcc/dev/shmem/ratunek.log 2>/dev/null && ok "pelny log z WYNIK=OK zostal w RAM" || bad "brak logu w RAM"
grep -q "STUB launcher" /tmp/order.txt && ok "M.I.B. (launcher) wykonal sie dalej" || bad "ratunek zablokowal M.I.B."

echo
echo "=========================================================="
echo " WYNIK: $PASS OK, $FAILED FAIL"
echo "=========================================================="
[ "$FAILED" -eq 0 ] || exit 1
