#!/bin/sh
set -u
PATH=/bin:/usr/bin:/sbin:/usr/sbin:$PATH; export PATH
SD="${MIB_SD:-/net/mmx/fs/sda0}"
APP="${MIB_APP:-/mnt/app}"
SYSTEM="${MIB_SYSTEM:-/mnt/system}"
PKGNAME=mib2high-carplay-rgi-v8s-simple
MOD="$SD/mod/$PKGNAME"; PAY="$MOD/payload"; BACKUP="$MOD/backup/pre-v8s"
INSTALLED="$MOD/installed.ok"; ROLLBACK="$MOD/ROLLBACK"
PENDING="$MOD/installing.flag"; COLLECT="$MOD/collected"
# Marker wymuszajacy zbieranie logow, niezaleznie od stanu instalacji. Pozwala
# zebrac sloginfo/pidin/dmdt takze wtedy, gdy brama wersji odmowila instalacji
# albo po rollbacku (rollback usuwa installed.ok, wiec bez tego markera skrypt
# powiedzialby tylko ALREADY_ROLLED_BACK i nie zebral niczego).
# Nazwa pliku COLLECT_LOGS, a nie COLLECT, bo zmienna COLLECT to katalog
# wyjsciowy ($MOD/collected) i lezy w tym samym miejscu - nazwa musi byc
# jednoznaczna takze dla czlowieka patrzacego na karte.
FORCECOLLECT="$MOD/COLLECT_LOGS"
# --- K3346 Etap 4 (RGI w mm-ipod) - osobna transakcja z WLASNA kopia oryginalow.
# Dotyka innych plikow niz v8s, wiec obie czesci cofaja sie niezaleznie.
# Pelny ROLLBACK cofa najpierw Etap 4, potem v8s. ROLLBACK_ETAP4 cofa tylko Etap 4.
E4_BACKUP="$MOD/backup/pre-etap4"; E4_INSTALLED="$MOD/etap4.installed.ok"
E4_PENDING="$MOD/etap4.installing.flag"; E4_ROLLEDBACK="$MOD/etap4.rolledback.ok"
E4_INSTALL_MARK="$MOD/INSTALL_ETAP4"; E4_ROLLBACK_MARK="$MOD/ROLLBACK_ETAP4"
E4_NAMES='libmib2high-carplay-rgi-ipod.so mm-ipod-rgi.sh usblauncher_otg.lua iap2.cfg'
# Stock K3346 (collector v1.2 + backup M.I.B., identyczne): rozmiary plikow, ktore
# Etap 4 zmienia, oraz oczekiwane rozmiary po zmianie (+21 B lua, +139 B iap2.cfg:
# [ident] RGI + Etap 6 0x415a/0x4170/0x4171).
K3346_OTG_LUA_SIZE=19907; K3346_OTG_LUA_NEW_SIZE=19928
K3346_IAP2CFG_SIZE=5852; K3346_IAP2CFG_NEW_SIZE=5991
# Etap 6b: z plikiem BT_MAC na karcie dochodzi sekcja [bluetooth] (+150 B, adres
# ma zawsze 17 znakow).
K3346_IAP2CFG_BT_SIZE=6141; E4_BTMAC_FILE="$MOD/BT_MAC"
K3346_MMIPOD_SIZE=40297
# --- K3346 Etap 5: JAR z klasami o fabrycznych nazwach (lsd/jars/ jest na
# -Xbootclasspath przed lsd.jxe). Osobna transakcja i kopia; ROLLBACK_ETAP5 cofa
# tylko ten JAR, pelny ROLLBACK cofa 5 -> 4 -> v8s. Karta ratunkowa usuwa go tez.
E5_BACKUP="$MOD/backup/pre-etap5"; E5_INSTALLED="$MOD/etap5.installed.ok"
E5_PENDING="$MOD/etap5.installing.flag"; E5_ROLLEDBACK="$MOD/etap5.rolledback.ok"
E5_INSTALL_MARK="$MOD/INSTALL_ETAP5"; E5_ROLLBACK_MARK="$MOD/ROLLBACK_ETAP5"
E5_NAMES='mib2high-k3346-overrides.jar k3346-coverart-fetch'
# --- K3346 Etap 7: warstwa graficzna na VC (strzalka manewru + pasy).
# Nowe pliki (vc_overlay, sonda, konfiguracja, skrypt sondy) ORAZ dwa pliki
# nalezace do wczesniejszych etapow, ktore Etap 7 musi podmienic:
#   mib2high-lvds-control.sh        (v8s)    - startuje nakladke i deklaruje kontekst
#   libmib2high-carplay-rgi-ipod.so (Etap 4) - dosyla pakiety manewru do renderera
# Obie podmiany ida przez wlasna kopie Etapu 7, wiec ROLLBACK_ETAP7 przywraca
# dokladnie te wersje, ktore byly przed nim - bez ruszania transakcji 4 i 5.
# vc_render NIE jest na tej liscie: zostaje nietkniety.
E7_BACKUP="$MOD/backup/pre-etap7"; E7_INSTALLED="$MOD/etap7.installed.ok"
E7_PENDING="$MOD/etap7.installing.flag"; E7_ROLLEDBACK="$MOD/etap7.rolledback.ok"
E7_INSTALL_MARK="$MOD/INSTALL_ETAP7"; E7_ROLLBACK_MARK="$MOD/ROLLBACK_ETAP7"
E7_PROBE_MARK="$MOD/PROBE_ETAP7"
E7_NAMES='vc_overlay vc_overlay_probe mib2high-overlay.conf flag_atlas.rgba k3346-overlay-probe.sh mib2high-lvds-control.sh libmib2high-carplay-rgi-ipod.so'
# --- Wydanie 1.0: wszystkie etapy jednym uruchomieniem (dla uzytkownika bez
# doswiadczenia). Kazdy etap zostaje OSOBNA transakcja z wlasna kopia i
# auto-cofnieciem - INSTALL_ALL tylko wola je po kolei. Pierwszy blad konczy
# przebieg; ponowne uruchomienie wznawia od pierwszego brakujacego etapu.
ALL_INSTALL_MARK="$MOD/INSTALL_ALL"
# --- Wydanie 1.0.1: polityka VC (issue #5). Bundle Java czyta ten plik przy
# starcie HMI: rgi (domyslnie, jak 1.0.0) / map / off. VC H11/0379 robi czarna
# mape przy favoredViewMode=RGI. Plik jest nasz, nie fabryczny: nie ma kopii,
# pelny ROLLBACK go usuwa. Jeden znacznik = jedno uruchomienie (kolejka MAP, OFF, RGI).
VCPOL_FILE="$APP/root/mib2high-vc.conf"
# --- 1.0.1: NavIgnore (M.I.B. "dual navigation"). Bez niego HMI, widzac, ze
# nawigacje przejal iPhone, przelacza zegary z naszej mapy na czarny ekran
# (issue #5). Auto testowe mialo go od 2023, wiec nikt tego nie widzial.
# Tylko SPRAWDZAMY: to cudzy plik, nigdy go nie kopiujemy, nie kasujemy, nie cofamy.
NAVIGNORE_JAR="$APP/eso/hmi/lsd/jars/NavActiveIgnore.jar"
navignore_report(){ if [ -f "$NAVIGNORE_JAR" ]; then say 'NAVIGNORE=PRESENT'; return 0; fi
say 'NAVIGNORE=MISSING - the CarPlay map on the cluster will go black during route guidance.'
say 'Install it in M.I.B.: m.i.b. -> navigation -> NavActiveIgnore -> Enable dual navigation'; }
VCPOL_MARKS='VC_POLICY_MAP VC_POLICY_OFF VC_POLICY_RGI'
say(){ echo "[V8S] $*"; }
fail(){ say "ERROR $*"; exit 1; }
# Same write test / guarded mount -uw logic as the working v8g2 simple installer.
checkrw() {
    MOUNTPOINT="$1"
    TESTFILE="$MOUNTPOINT/.mib2high-rgi-rw-test.$$"
    if ( : > "$TESTFILE" ) 2>/dev/null; then
        rm -f "$TESTFILE" 2>/dev/null
        return 0
    fi
    say "Remounting $MOUNTPOINT read-write..."
    mount -uw "$MOUNTPOINT" >/dev/null 2>&1 || return 1
    if ( : > "$TESTFILE" ) 2>/dev/null; then
        rm -f "$TESTFILE" 2>/dev/null
        return 0
    fi
    return 1
}
# All touched paths are indexed; no hidden earlier-install dependency.
NAMES='libmib2high-carplay-rgi.so mib2high-carplay-rgi.jar vc_render mib2high-lvds-control.sh mib2high-lvds-profile.conf mib2high-lvds-viewareas.conf bundles_dsitracer_enabled.properties traceConfig.properties smartphone_integrator.json carplay_altscreen dsitracer_activated v8l-dsitracer-capture'
pathof(){ case "$1" in
libmib2high-carplay-rgi.so) echo "$APP/root/hooks/$1";;
mib2high-carplay-rgi.jar|mib2high-k3346-overrides.jar) echo "$APP/eso/hmi/lsd/jars/$1";;
vc_render|v8l-dsitracer-capture|mib2high-lvds-control.sh|mib2high-lvds-profile.conf|mib2high-lvds-viewareas.conf|k3346-coverart-fetch) echo "$APP/root/$1";;
vc_overlay|vc_overlay_probe|mib2high-overlay.conf|flag_atlas.rgba|k3346-overlay-probe.sh) echo "$APP/root/$1";;
bundles_dsitracer_enabled.properties|traceConfig.properties|dsitracer_activated) echo "$APP/eso/hmi/lsd/$1";;
smartphone_integrator.json) echo "$SYSTEM/etc/eso/production/smartphone_integrator.json";;
carplay_altscreen) echo "$APP/carplay_altscreen";;
libmib2high-carplay-rgi-ipod.so) echo "$APP/root/hooks/$1";;
mm-ipod-rgi.sh) echo "$APP/root/$1";;
usblauncher_otg.lua) echo "$SYSTEM/etc/usblauncher_otg.lua";;
iap2.cfg) echo "$SYSTEM/etc/mm/iap2.cfg";; esac; }
modeof(){ case "$1" in *.so|vc_render|v8l-dsitracer-capture|k3346-coverart-fetch|*.sh) echo 755;;*)echo 644;;esac; }
# Preserve actual pre-install POSIX permissions (including setuid/setgid/sticky).
# Only uses QNX-standard ls and awk; no stat -c, dirname, cmp or other GNU options.
filemode() {
    ls -ld "$1" | awk '
    NR==1 {
        p=substr($1,2,9)
        if (length(p)!=9) {bad=1;exit}
        special=0; digits=""
        for (i=0;i<3;i++) {
            r=substr(p,1+3*i,1); w=substr(p,2+3*i,1); x=substr(p,3+3*i,1)
            if ((r!="r" && r!="-") || (w!="w" && w!="-") ||
                (x!="x" && x!="-" && x!="s" && x!="S" && x!="t" && x!="T")) {
                bad=1;exit
            }
            v=(r=="r" ? 4:0)+(w=="w" ? 2:0)+(x=="x" || x=="s" || x=="t" ? 1:0)
            digits=digits v
            if (i==0 && (x=="s" || x=="S")) special+=4
            if (i==1 && (x=="s" || x=="S")) special+=2
            if (i==2 && (x=="t" || x=="T")) special+=1
        }
        if (!bad) print special digits
    }
    END { if (NR!=1 || bad) exit 7 }'
}
# Backup pre-existing files OR their absent status; never overwrite first snapshot.
backup_all(){ [ ! -e "$BACKUP" ] || { say 'BACKUP_EXISTS: not overwriting immutable snapshot'; return 1; }; mkdir -p "$BACKUP" || return 1
for n in $NAMES; do
    f=`pathof "$n"`
    if [ -f "$f" ]; then
        MODE=`filemode "$f"` || { say "CANNOT_READ_ORIGINAL_MODE=$f"; return 1; }
        [ -n "$MODE" ] || return 1
        cp "$f" "$BACKUP/$n" || return 1
        echo "$MODE" > "$BACKUP/$n.mode" || return 1
    elif [ -e "$f" ]; then
        say "UNSUPPORTED_EXISTING_FILE_TYPE=$f"
        return 1
    else
        : > "$BACKUP/$n.absent" || return 1
    fi
done
: > "$BACKUP/complete" || return 1; sync || return 1; }
# Exact v8g2 proven deployment sequence: copy temporary, chmod, rename.
install_file() {
    SRC="$1"; DST="$2"; MODE="$3"
    TMP="$DST.v8s.new.$$"
    rm -f "$TMP" 2>/dev/null || return 1
    cp "$SRC" "$TMP" || { say "COPY_FAILED=$DST"; return 1; }
    chmod "$MODE" "$TMP" || { say "CHMOD_FAILED=$DST"; rm -f "$TMP"; return 1; }
    mv -f "$TMP" "$DST" || { say "RENAME_FAILED=$DST"; rm -f "$TMP"; return 1; }
    [ -f "$DST" ] || { say "MISSING_AFTER_RENAME=$DST"; return 1; }
}
restore_all(){ [ -f "$BACKUP/complete" ] || return 1
for n in $NAMES; do
    dest=`pathof "$n"`
    if [ -f "$BACKUP/$n.absent" ]; then
        rm -f "$dest" || return 1
    else
        [ -f "$BACKUP/$n" ] || return 1
        # New backups carry exact mode metadata. Never invent a replacement mode.
        [ -f "$BACKUP/$n.mode" ] || { say "RESTORE_MODE_MISSING=$n"; return 1; }
        MODE=`cat "$BACKUP/$n.mode"` || return 1
        case "$MODE" in
            [0-7][0-7][0-7][0-7]) ;;
            *) say "RESTORE_MODE_INVALID=$n"; return 1;;
        esac
        install_file "$BACKUP/$n" "$dest" "$MODE" || return 1
    fi
done
sync; }
TX=0; TX4=0; TX5=0; TX7=0
# W trybie INSTALL_ALL etapy NIE moga mowic "SAFE TO REMOVE SD" po drodze: kto
# wyjmie karte po pierwszym etapie, wyjmie ja w trakcie zapisu nastepnego, a jego
# auto-cofniecie potrzebuje kopii z tej karty. Ten komunikat pada raz, na koncu.
ALL=0; RBFAIL=0; QUIET_SAFE=0
stage_done(){ [ "$ALL" -eq 1 ] || say "$@"; }
# Ostatnie slowo przebiegu INSTALL_ALL, ktory sie nie udal. Laik czyta tylko
# koniec ekranu, wiec tam musi stac wynik i jedno polecenie, co dalej.
all_failed(){ [ "$ALL" -eq 1 ] || return 0; say 'INSTALL_ALL_RESULT=FAIL (see the ERROR line above)'
if [ "$RBFAIL" -eq 1 ]; then say 'Do NOT restart the MMI. Uninstall now: put the ROLLBACK file on this card and run the script again.'
else say 'The car is in a safe state: finished parts stay, the failed part was undone.'; say 'Run the script again to retry, or uninstall.'; fi; }
onexit(){ rc="$1"; trap - 0; if [ "$TX" -eq 1 ]; then say 'INSTALL_FAILED: restoring originals'; if restore_all; then rm -f "$PENDING" "$INSTALLED"; say 'AUTO_ROLLBACK_RESULT=PASS'; else RBFAIL=1; say 'AUTO_ROLLBACK_RESULT=FAIL - do not reboot'; fi; fi
if [ "$TX5" -eq 1 ]; then say 'ETAP5_INSTALL_FAILED: restoring originals'; if tx_restore "$E5_BACKUP" "$E5_NAMES"; then rm -f "$E5_PENDING" "$E5_INSTALLED"; : > "$E5_ROLLEDBACK"; sync; say 'ETAP5_AUTO_ROLLBACK_RESULT=PASS'; else RBFAIL=1; say 'ETAP5_AUTO_ROLLBACK_RESULT=FAIL - do not reboot'; fi; fi
if [ "$TX7" -eq 1 ]; then say 'ETAP7_INSTALL_FAILED: restoring originals'; if tx_restore "$E7_BACKUP" "$E7_NAMES"; then rm -f "$E7_PENDING" "$E7_INSTALLED"; : > "$E7_ROLLEDBACK"; sync; say 'ETAP7_AUTO_ROLLBACK_RESULT=PASS'; else RBFAIL=1; say 'ETAP7_AUTO_ROLLBACK_RESULT=FAIL - do not reboot'; fi; fi
if [ "$TX4" -eq 1 ]; then say 'ETAP4_INSTALL_FAILED: restoring originals'; if e4_restore; then rm -f "$E4_PENDING" "$E4_INSTALLED"; : > "$E4_ROLLEDBACK"; sync; say 'ETAP4_AUTO_ROLLBACK_RESULT=PASS'; else RBFAIL=1; say 'ETAP4_AUTO_ROLLBACK_RESULT=FAIL - do not reboot'; fi; fi
[ "$rc" -eq 0 ] || all_failed; exit "$rc"; }
trap 'onexit $?' 0
preflight(){ [ -d "$APP/eso/hmi/lsd" ] || fail 'missing stock LSD directory'; [ -d "$SYSTEM/etc/eso/production" ] || fail 'missing system config directory'; [ -d "$APP/root" ] || fail 'missing stock root directory'; [ -f "$APP/eso/hmi/lsd/DSITracer.jar" ] || fail 'stock DSITracer.jar missing'; [ -f "$APP/eso/hmi/lsd/bundles_dsitracer_enabled.properties" ] || fail 'tracer bundle registry missing'; [ -f "$APP/eso/hmi/lsd/traceConfig.properties" ] || fail 'tracer config missing'; [ -f "$SYSTEM/etc/eso/production/smartphone_integrator.json" ] || fail 'smartphone integrator config missing'; }
# --- Brama wersji K3346 (dodane w porcie na MHI2_ER_AUG22_K3346 / MU1438) ---
# Na tym MMI NIE MA sha256sum, cksum, md5sum ani cmp - potwierdzone przez
# capabilities.txt collectora v1.2 (dostepne: cp awk find ls wc df mount uname).
# Dlatego brama uzywa wylacznie awk i wc -c, i sprawdza trzy rzeczy:
#   1) HMI_Version z version.properties - identyfikuje build HMI;
#   2) dokladny rozmiar ipod-drvr-iap2.so - wzgledem TEGO pliku zweryfikowano
#      offsety w libmib2high-carplay-rgi.so (tools/verify_port_offsets.py: 17/17).
#      Inny plik => hooki pisalyby pod zle adresy w driverze iAP2;
#   3) dokladny rozmiar /ifs/lsd.jxe - wzgledem niego skompilowano JAR.
#      Ten sprawdza sie tylko, jesli plik da sie odczytac: blad tylko przy
#      UDOWODNIONEJ niezgodnosci, nie przy braku mozliwosci sprawdzenia.
#      Nazwa malymi literami (collector v1.2: /net/mmx/ifs/lsd.jxe); QNX
#      rozroznia wielkosc liter - /ifs/LSD.JXE w aucie nie istnieje.
# Awaryjne pominiecie: MIB_FORCE=1 (swiadoma decyzja, na wlasne ryzyko).
K3346_HMI_VERSION='HMI_AU_MIB2_H_EU_G22_C6_190300'
K3346_IAP2_SIZE=739925
K3346_LSD_SIZE=58131096
K3346_LSD_PATH="${MIB_LSD_JXE:-/ifs/lsd.jxe}"
[ -n "${MIB_LSD_JXE:-}" ] || [ -f "$K3346_LSD_PATH" ] || K3346_LSD_PATH=/net/mmx/ifs/lsd.jxe
fsize(){ [ -f "$1" ] || return 1; wc -c < "$1" 2>/dev/null | awk '{print $1; seen=1} END{exit !seen}'; }
version_gate(){
if [ "${MIB_FORCE:-0}" = "1" ]; then say 'VERSION_GATE=BYPASSED (MIB_FORCE=1) - na wlasne ryzyko'; return 0; fi
VP="$APP/eso/hmi/version.properties"
[ -f "$VP" ] || fail "VERSION_GATE: brak $VP - nie moge potwierdzic wersji firmware"
HV=`awk -F: '/^HMI_Version/{gsub(/[ \t\r]/,"",$2); print $2; exit}' "$VP"`
[ -n "$HV" ] || fail "VERSION_GATE: nie odczytano HMI_Version z $VP"
[ "$HV" = "$K3346_HMI_VERSION" ] || fail "VERSION_GATE: HMI_Version='$HV', oczekiwano '$K3346_HMI_VERSION'. To nie jest MHI2_ER_AUG22_K3346/MU1438 - instalacja PRZERWANA"
say "VERSION_GATE: HMI_Version=$HV OK"
DRV="$APP/armle/lib/dll/ipod-drvr-iap2.so"
DSZ=`fsize "$DRV"` || fail "VERSION_GATE: nie moge odczytac $DRV"
[ "$DSZ" = "$K3346_IAP2_SIZE" ] || fail "VERSION_GATE: ipod-drvr-iap2.so ma $DSZ B, oczekiwano $K3346_IAP2_SIZE B. Offsety hookow nie pasuja do tego firmware - instalacja PRZERWANA"
say "VERSION_GATE: ipod-drvr-iap2.so=$DSZ OK"
if LSZ=`fsize "$K3346_LSD_PATH"`; then
    [ "$LSZ" = "$K3346_LSD_SIZE" ] || fail "VERSION_GATE: $K3346_LSD_PATH ma $LSZ B, oczekiwano $K3346_LSD_SIZE B. JAR skompilowano wzgledem innego LSD - instalacja PRZERWANA"
    say "VERSION_GATE: $K3346_LSD_PATH=$LSZ OK"
else
    say "VERSION_GATE: $K3346_LSD_PATH nieczytelny - pomijam ten sprawdzian (pozostale przeszly)"
fi
say 'VERSION_GATE=PASS'
}
install(){ say 'MODE=INSTALL'; checkrw "$SD" || fail 'SD not writable'; preflight; version_gate
[ ! -e "$PENDING" ] || fail 'unfinished prior install: use ROLLBACK'
# Po UDANYM pelnym cofnieciu (rolledback.ok) auto jest znow fabryczne, wiec
# nowa instalacja moze zrobic nowa kopie. Stara nie znika: odsuwamy ja na bok,
# tak samo jak Etapy 4/5/7. Tak wyglada aktualizacja: odinstaluj -> zainstaluj.
if [ -e "$BACKUP" ] && [ -f "$MOD/rolledback.ok" ] && [ ! -e "$INSTALLED" ]; then mv "$BACKUP" "$BACKUP.old.$$" || fail 'cannot rotate old v8s backup'; rm -f "$MOD/rolledback.ok" || fail 'cannot clear rollback flag'; say 'OLD_BACKUP_KEPT=pre-v8s.old'; fi
[ ! -e "$BACKUP" ] || fail 'existing backup retained: use ROLLBACK';
for n in libmib2high-carplay-rgi.so mib2high-carplay-rgi.jar vc_render mib2high-lvds-control.sh mib2high-lvds-profile.conf mib2high-lvds-viewareas.conf v8l-dsitracer-capture; do [ -s "$PAY/$n" ] || fail "missing $n"; done
# Payload files checked for presence above. Wersji firmware pilnuje version_gate
# (awk + wc -c); na tym MMI nie ma narzedzi do sum kontrolnych.
# Keep unrelated configuration unchanged; use existing configuration as input.
awk -f "$MOD/source/bundles.awk" "$APP/eso/hmi/lsd/bundles_dsitracer_enabled.properties" > "$MOD/bundles.$$" || fail 'bundle patch failed'
awk -f "$MOD/source/trace.awk" "$APP/eso/hmi/lsd/traceConfig.properties" > "$MOD/trace.$$" || fail 'trace patch failed'
awk -f "$MOD/source/smartphone.awk" "$SYSTEM/etc/eso/production/smartphone_integrator.json" > "$MOD/smartphone.$$" || fail 'smartphone hook baseline unsupported'
checkrw "$APP" || fail 'app filesystem not writable'; checkrw "$SYSTEM" || fail 'system filesystem not writable'
mkdir -p "$APP/root/hooks" "$APP/eso/hmi/lsd/jars" || fail 'cannot create payload target directories'
backup_all || fail 'original backup failed'; : > "$PENDING" || fail 'cannot mark transaction'; TX=1
for n in libmib2high-carplay-rgi.so mib2high-carplay-rgi.jar vc_render mib2high-lvds-control.sh mib2high-lvds-profile.conf mib2high-lvds-viewareas.conf v8l-dsitracer-capture; do dest=`pathof "$n"`; say "INSTALLING=$n"; install_file "$PAY/$n" "$dest" `modeof "$n"` || fail "install failed $n"; done
install_file "$MOD/bundles.$$" "`pathof bundles_dsitracer_enabled.properties`" 644 || fail 'bundle install failed'
install_file "$MOD/trace.$$" "`pathof traceConfig.properties`" 644 || fail 'trace install failed'
install_file "$MOD/smartphone.$$" "`pathof smartphone_integrator.json`" 644 || fail 'smartphone install failed'
: > "`pathof carplay_altscreen`" || fail 'AltScreen marker failed'
: > "`pathof dsitracer_activated`" || fail 'tracer marker failed'
sync || fail 'pre-success sync failed'; : > "$INSTALLED" || fail 'cannot mark success'; sync || fail 'success sync failed'
rm -f "$PENDING" "$MOD/bundles.$$" "$MOD/trace.$$" "$MOD/smartphone.$$" || fail 'cleanup failed'; sync || fail 'final sync failed'
TX=0; say 'INSTALL_RESULT=PASS (deployment only; car behavior NOT TESTED)'; say 'NAVIGNORE=UNTOUCHED'; stage_done 'Reboot MMI manually, test route/zoom/gauges, run custom script again to collect'; stage_done 'SAFE TO REMOVE SD'; }
rollback(){ say 'MODE=ROLLBACK'; checkrw "$SD" || fail 'SD not writable'
# Karta bez zadnej kopii i bez zadnego znacznika instalacji: ta karta niczego w
# tym aucie nie instalowala, wiec nie ma czego cofac - i NIC nie zostalo ruszone.
# Dawniej konczylo sie to 'restore failed: do not reboot', co laika straszy
# na prozno. Mod zainstalowany z INNEJ karty cofa sie TA karta (ma kopie).
if [ ! -e "$BACKUP" ] && [ ! -e "$INSTALLED" ] && [ ! -e "$PENDING" ] && [ ! -e "$E4_INSTALLED" ] && [ ! -e "$E4_PENDING" ] && [ ! -e "$E5_INSTALLED" ] && [ ! -e "$E5_PENDING" ] && [ ! -e "$E7_INSTALLED" ] && [ ! -e "$E7_PENDING" ]; then
rm -f "$ROLLBACK"; say 'ROLLBACK_RESULT=NOTHING_TO_UNDO (no backup on this card, nothing was changed)'
say 'If the mod is installed in this car, uninstall it with the SD card that installed it.'; say 'SAFE TO REMOVE SD'; return 0; fi
checkrw "$APP" || fail 'app not writable'; checkrw "$SYSTEM" || fail 'system not writable'
# Warstwy od najnowszej: Etap 7, Etap 5, Etap 4, potem v8s - pelny powrot do fabryki.
# Etap 7 MUSI isc pierwszy: jego kopia zawiera biblioteke z czasow Etapu 4, wiec
# odtworzony po Etapie 4 nalozylby ja z powrotem na przywrocony plik fabryczny.
if [ -f "$E7_INSTALLED" ] || [ -f "$E7_PENDING" ]; then tx_restore "$E7_BACKUP" "$E7_NAMES" || fail 'etap7 restore failed: do not reboot'; rm -f "$E7_INSTALLED" "$E7_PENDING" || fail 'cannot clear etap7 flags'; : > "$E7_ROLLEDBACK" || fail 'cannot mark etap7 rollback'; say 'ETAP7_ROLLBACK=PASS'; fi
if [ -f "$E5_INSTALLED" ] || [ -f "$E5_PENDING" ]; then tx_restore "$E5_BACKUP" "$E5_NAMES" || fail 'etap5 restore failed: do not reboot'; rm -f "$E5_INSTALLED" "$E5_PENDING" || fail 'cannot clear etap5 flags'; : > "$E5_ROLLEDBACK" || fail 'cannot mark etap5 rollback'; say 'ETAP5_ROLLBACK=PASS'; fi
if [ -f "$E4_INSTALLED" ] || [ -f "$E4_PENDING" ]; then e4_restore || fail 'etap4 restore failed: do not reboot'; rm -f "$E4_INSTALLED" "$E4_PENDING" || fail 'cannot clear etap4 flags'; : > "$E4_ROLLEDBACK" || fail 'cannot mark etap4 rollback'; say 'ETAP4_ROLLBACK=PASS'; fi
rm -f "$VCPOL_FILE" "$VCPOL_FILE".new.* || fail 'cannot remove VC policy file'
restore_all || fail 'restore failed: do not reboot'; rm -f "$INSTALLED" "$PENDING" "$ROLLBACK" || fail 'cannot clear flags'; : > "$MOD/rolledback.ok" || fail 'cannot mark rollback'; sync || fail 'rollback sync failed'; say 'ROLLBACK_RESULT=PASS'; say 'Reboot manually'; say 'SAFE TO REMOVE SD'; }
# ------------------------------------------------------------------------------
# K3346 Etap 4: RGI w stock mm-ipod.
#  - biblioteka TYLKO z RGI (bez AltScreen) -> /mnt/app/root/hooks/
#  - launcher mm-ipod-rgi.sh (LD_PRELOAD tylko dla mm-ipod, exec stock mm-ipod)
#  - usblauncher_otg.lua: jedna linia startu mm-ipod wola launcher
#  - iap2.cfg: sekcja [ident] z kierunkami 0x5200..0x5204
# /etc to kopia /mnt/system/etc w RAM-dysku (startup.sh: move_etc_to_ramdisk),
# wiec zmiany w $SYSTEM/etc dzialaja po restarcie MMI.
# Wymaga zainstalowanego v8s (bundle Javy odbiera dane RGI na 127.0.0.1:19810).
e4_backup(){ [ ! -e "$E4_BACKUP" ] || { say 'ETAP4_BACKUP_EXISTS: not overwriting immutable snapshot'; return 1; }; mkdir -p "$E4_BACKUP" || return 1
for n in $E4_NAMES; do
    f=`pathof "$n"`
    if [ -f "$f" ]; then
        MODE=`filemode "$f"` || { say "CANNOT_READ_ORIGINAL_MODE=$f"; return 1; }
        [ -n "$MODE" ] || return 1
        cp "$f" "$E4_BACKUP/$n" || return 1
        echo "$MODE" > "$E4_BACKUP/$n.mode" || return 1
    elif [ -e "$f" ]; then
        say "UNSUPPORTED_EXISTING_FILE_TYPE=$f"; return 1
    else
        : > "$E4_BACKUP/$n.absent" || return 1
    fi
done
: > "$E4_BACKUP/complete" || return 1; sync || return 1; }
e4_restore(){ [ -f "$E4_BACKUP/complete" ] || { say 'ETAP4_BACKUP_INCOMPLETE'; return 1; }
for n in $E4_NAMES; do
    dest=`pathof "$n"`
    if [ -f "$E4_BACKUP/$n.absent" ]; then
        rm -f "$dest" || return 1
    else
        [ -f "$E4_BACKUP/$n" ] || return 1
        [ -f "$E4_BACKUP/$n.mode" ] || { say "RESTORE_MODE_MISSING=$n"; return 1; }
        MODE=`cat "$E4_BACKUP/$n.mode"` || return 1
        case "$MODE" in [0-7][0-7][0-7][0-7]) ;; *) say "RESTORE_MODE_INVALID=$n"; return 1;; esac
        install_file "$E4_BACKUP/$n" "$dest" "$MODE" || return 1
    fi
done
sync; }
e4_need_size(){ S=`fsize "$1"` || fail "ETAP4_GATE: nie moge odczytac $1"; [ "$S" = "$2" ] || fail "ETAP4_GATE: $1 ma $S B, oczekiwano $2 B - plik nie jest stockowy K3346, nic nie zmieniono"; }
install_etap4(){ say 'MODE=INSTALL_ETAP4'; checkrw "$SD" || fail 'SD not writable'
[ -f "$INSTALLED" ] || fail 'ETAP4: najpierw musi byc zainstalowany v8s (brak installed.ok)'
[ ! -e "$PENDING" ] || fail 'ETAP4: niedokonczona instalacja v8s - use ROLLBACK'
[ ! -e "$E4_PENDING" ] || fail 'ETAP4: niedokonczona instalacja Etapu 4 - use ROLLBACK_ETAP4'
[ ! -e "$E4_INSTALLED" ] || fail 'ETAP4: juz zainstalowany'
preflight; version_gate
e4_need_size "`pathof usblauncher_otg.lua`" "$K3346_OTG_LUA_SIZE"
e4_need_size "`pathof iap2.cfg`" "$K3346_IAP2CFG_SIZE"
e4_need_size "$APP/armle/usr/sbin/mm-ipod" "$K3346_MMIPOD_SIZE"
[ ! -e "`pathof libmib2high-carplay-rgi-ipod.so`" ] || fail 'ETAP4: biblioteka RGI juz jest w systemie - stan nieznany, nic nie zmieniono'
[ ! -e "`pathof mm-ipod-rgi.sh`" ] || fail 'ETAP4: launcher juz jest w systemie - stan nieznany, nic nie zmieniono'
for n in libmib2high-carplay-rgi-ipod.so mm-ipod-rgi.sh; do [ -s "$PAY/$n" ] || fail "missing $n"; done
for n in usblauncher_otg.awk iap2.awk; do [ -s "$MOD/source/$n" ] || fail "missing source/$n"; done
# Etap 6b: opcjonalny adres BT MMI z karty (BT_MAC, jedna linia XX:XX:XX:XX:XX:XX).
# Zly format -> stop przed jakakolwiek zmiana. Adresu nie wypisujemy w logu.
BTMAC=''; E4_IAP2_SIZE=$K3346_IAP2CFG_NEW_SIZE
if [ -f "$E4_BTMAC_FILE" ]; then
    BTMAC=`awk 'NR==1 { gsub(/[\r \t]/, ""); print; exit }' "$E4_BTMAC_FILE"` || fail 'ETAP4: nie moge odczytac BT_MAC - nic nie zmieniono'
    H='[0-9A-Fa-f][0-9A-Fa-f]'
    case "$BTMAC" in $H:$H:$H:$H:$H:$H) ;; *) fail 'ETAP4: BT_MAC ma zly format (oczekiwano XX:XX:XX:XX:XX:XX) - nic nie zmieniono';; esac
    case "$BTMAC" in 00:00:00:00:00:00|[Ff][Ff]:[Ff][Ff]:[Ff][Ff]:[Ff][Ff]:[Ff][Ff]:[Ff][Ff]) fail 'ETAP4: BT_MAC to adres pusty - nic nie zmieniono';; esac
    E4_IAP2_SIZE=$K3346_IAP2CFG_BT_SIZE; say 'ETAP4_BT=ON (BluetoothTransportComponent z BT_MAC)'
else
    say 'ETAP4_BT=OFF (brak BT_MAC na karcie)'
fi
awk -f "$MOD/source/usblauncher_otg.awk" "`pathof usblauncher_otg.lua`" > "$MOD/e4lua.$$" || { rm -f "$MOD/e4lua.$$"; fail 'ETAP4: linia startu mm-ipod nie znaleziona dokladnie raz - nic nie zmieniono'; }
awk -v btmac="$BTMAC" -f "$MOD/source/iap2.awk" "`pathof iap2.cfg`" > "$MOD/e4iap2.$$" || { rm -f "$MOD/e4lua.$$" "$MOD/e4iap2.$$"; fail 'ETAP4: iap2.cfg ma juz [ident]/[bluetooth] - nic nie zmieniono'; }
S=`fsize "$MOD/e4lua.$$"`; [ "$S" = "$K3346_OTG_LUA_NEW_SIZE" ] || { rm -f "$MOD/e4lua.$$" "$MOD/e4iap2.$$"; fail "ETAP4: nowy usblauncher_otg.lua ma $S B, oczekiwano $K3346_OTG_LUA_NEW_SIZE B - nic nie zmieniono"; }
S=`fsize "$MOD/e4iap2.$$"`; [ "$S" = "$E4_IAP2_SIZE" ] || { rm -f "$MOD/e4lua.$$" "$MOD/e4iap2.$$"; fail "ETAP4: nowy iap2.cfg ma $S B, oczekiwano $E4_IAP2_SIZE B - nic nie zmieniono"; }
checkrw "$APP" || fail 'app filesystem not writable'; checkrw "$SYSTEM" || fail 'system filesystem not writable'
mkdir -p "$APP/root/hooks" || fail 'cannot create hooks directory'
# Kopia sprzed poprzedniej, cofnietej instalacji Etapu 4 nie blokuje nowej:
# pliki sa wlasnie potwierdzone jako stockowe (rozmiary + brak naszych plikow).
if [ -e "$E4_BACKUP" ] && [ -f "$E4_ROLLEDBACK" ]; then mv "$E4_BACKUP" "$E4_BACKUP.old.$$" || fail 'cannot rotate old etap4 backup'; rm -f "$E4_ROLLEDBACK"; fi
e4_backup || fail 'etap4 original backup failed'; : > "$E4_PENDING" || fail 'cannot mark etap4 transaction'; TX4=1
for n in libmib2high-carplay-rgi-ipod.so mm-ipod-rgi.sh; do say "INSTALLING=$n"; install_file "$PAY/$n" "`pathof $n`" 755 || fail "install failed $n"; done
say 'INSTALLING=usblauncher_otg.lua'; install_file "$MOD/e4lua.$$" "`pathof usblauncher_otg.lua`" "`cat "$E4_BACKUP/usblauncher_otg.lua.mode"`" || fail 'install failed usblauncher_otg.lua'
say 'INSTALLING=iap2.cfg'; install_file "$MOD/e4iap2.$$" "`pathof iap2.cfg`" "`cat "$E4_BACKUP/iap2.cfg.mode"`" || fail 'install failed iap2.cfg'
sync || fail 'pre-success sync failed'; : > "$E4_INSTALLED" || fail 'cannot mark etap4 success'; sync || fail 'success sync failed'
rm -f "$E4_PENDING" "$MOD/e4lua.$$" "$MOD/e4iap2.$$" "$E4_INSTALL_MARK" || fail 'cleanup failed'; sync || fail 'final sync failed'
TX4=0; say 'ETAP4_INSTALL_RESULT=PASS (deployment only; car behavior NOT TESTED)'; stage_done 'Restart MMI (usblauncher czyta lua tylko przy starcie), potem nawigacja CarPlay z trasa'; stage_done 'SAFE TO REMOVE SD'; }
rollback_etap4(){ say 'MODE=ROLLBACK_ETAP4'; checkrw "$SD" || fail 'SD not writable'
if [ ! -f "$E4_INSTALLED" ] && [ ! -f "$E4_PENDING" ]; then rm -f "$E4_ROLLBACK_MARK"; say 'ETAP4_NOT_INSTALLED: nic do cofniecia'; say 'SAFE TO REMOVE SD'; return 0; fi
checkrw "$APP" || fail 'app not writable'; checkrw "$SYSTEM" || fail 'system not writable'
e4_restore || fail 'etap4 restore failed: do not reboot'; rm -f "$E4_INSTALLED" "$E4_PENDING" "$E4_ROLLBACK_MARK" || fail 'cannot clear etap4 flags'
: > "$E4_ROLLEDBACK" || fail 'cannot mark etap4 rollback'; sync || fail 'etap4 rollback sync failed'
say 'ETAP4_ROLLBACK_RESULT=PASS (v8s / mapa na VC zostaje)'; say 'Restart MMI'; say 'SAFE TO REMOVE SD'; }
# ------------------------------------------------------------------------------
# Kopia/przywracanie dla kolejnych etapow: tx_backup KATALOG "NAZWY",
# tx_restore KATALOG "NAZWY". Ta sama logika co e4_backup/e4_restore (te zostaja
# bez zmian - sprawdzone w aucie): kopia z trybem albo znacznik .absent,
# niezmienialna migawka, przywrocenie przez install_file z zapisanym trybem.
tx_backup(){ TB_DIR="$1"; TB_NAMES="$2"
[ ! -e "$TB_DIR" ] || { say "BACKUP_EXISTS=$TB_DIR: not overwriting immutable snapshot"; return 1; }; mkdir -p "$TB_DIR" || return 1
for n in $TB_NAMES; do
    f=`pathof "$n"`
    if [ -f "$f" ]; then
        MODE=`filemode "$f"` || { say "CANNOT_READ_ORIGINAL_MODE=$f"; return 1; }
        [ -n "$MODE" ] || return 1
        cp "$f" "$TB_DIR/$n" || return 1
        echo "$MODE" > "$TB_DIR/$n.mode" || return 1
    elif [ -e "$f" ]; then
        say "UNSUPPORTED_EXISTING_FILE_TYPE=$f"; return 1
    else
        : > "$TB_DIR/$n.absent" || return 1
    fi
done
: > "$TB_DIR/complete" || return 1; sync || return 1; }
tx_restore(){ TR_DIR="$1"; TR_NAMES="$2"
[ -f "$TR_DIR/complete" ] || { say "BACKUP_INCOMPLETE=$TR_DIR"; return 1; }
for n in $TR_NAMES; do
    dest=`pathof "$n"`
    # Kopia sprzed dodania pliku do etapu (np. Etap 5 bez k3346-coverart-fetch):
    # ta transakcja go nie instalowala, wiec nie ma czego przywracac.
    if [ ! -f "$TR_DIR/$n.absent" ] && [ ! -f "$TR_DIR/$n" ] && [ ! -f "$TR_DIR/$n.mode" ]; then
        say "RESTORE_SKIP_NOT_IN_BACKUP=$n"; continue
    fi
    if [ -f "$TR_DIR/$n.absent" ]; then
        rm -f "$dest" || return 1
    else
        [ -f "$TR_DIR/$n" ] || return 1
        [ -f "$TR_DIR/$n.mode" ] || { say "RESTORE_MODE_MISSING=$n"; return 1; }
        MODE=`cat "$TR_DIR/$n.mode"` || return 1
        case "$MODE" in [0-7][0-7][0-7][0-7]) ;; *) say "RESTORE_MODE_INVALID=$n"; return 1;; esac
        install_file "$TR_DIR/$n" "$dest" "$MODE" || return 1
    fi
done
sync; }
# ------------------------------------------------------------------------------
# K3346 Etap 5: mib2high-k3346-overrides.jar -> /mnt/app/eso/hmi/lsd/jars/,
# k3346-coverart-fetch (5.3, uruchamiany przez HMI) -> /mnt/app/root/.
# Dziala po restarcie MMI (lsd.sh buduje bootclasspath przy starcie HMI).
# Ryzyko: blad w podmienionej klasie moze zatrzymac HMI (bootloop) -> karta
# ratunkowa usuwa ten JAR (ratunek.sh, R_JAR2), potem ROLLBACK_ETAP5 przez GEM.
install_etap5(){ say 'MODE=INSTALL_ETAP5'; checkrw "$SD" || fail 'SD not writable'
[ -f "$INSTALLED" ] || fail 'ETAP5: najpierw musi byc zainstalowany v8s (brak installed.ok)'
[ ! -e "$PENDING" ] || fail 'ETAP5: niedokonczona instalacja v8s - use ROLLBACK'
[ ! -e "$E5_PENDING" ] || fail 'ETAP5: niedokonczona instalacja Etapu 5 - use ROLLBACK_ETAP5'
[ ! -e "$E5_INSTALLED" ] || fail 'ETAP5: juz zainstalowany'
preflight; version_gate
[ -d "$APP/eso/hmi/lsd/jars" ] || fail 'ETAP5: brak katalogu lsd/jars'
[ ! -e "`pathof mib2high-k3346-overrides.jar`" ] || fail 'ETAP5: JAR juz jest w systemie - stan nieznany, nic nie zmieniono'
[ ! -e "`pathof k3346-coverart-fetch`" ] || fail 'ETAP5: k3346-coverart-fetch juz jest w systemie - stan nieznany, nic nie zmieniono'
[ -s "$PAY/mib2high-k3346-overrides.jar" ] || fail 'missing mib2high-k3346-overrides.jar'
[ -s "$PAY/k3346-coverart-fetch" ] || fail 'missing k3346-coverart-fetch'
checkrw "$APP" || fail 'app filesystem not writable'
if [ -e "$E5_BACKUP" ] && [ -f "$E5_ROLLEDBACK" ]; then mv "$E5_BACKUP" "$E5_BACKUP.old.$$" || fail 'cannot rotate old etap5 backup'; rm -f "$E5_ROLLEDBACK"; fi
tx_backup "$E5_BACKUP" "$E5_NAMES" || fail 'etap5 original backup failed'; : > "$E5_PENDING" || fail 'cannot mark etap5 transaction'; TX5=1
say 'INSTALLING=mib2high-k3346-overrides.jar'; install_file "$PAY/mib2high-k3346-overrides.jar" "`pathof mib2high-k3346-overrides.jar`" 644 || fail 'install failed mib2high-k3346-overrides.jar'
say 'INSTALLING=k3346-coverart-fetch'; install_file "$PAY/k3346-coverart-fetch" "`pathof k3346-coverart-fetch`" 755 || fail 'install failed k3346-coverart-fetch'
sync || fail 'pre-success sync failed'; : > "$E5_INSTALLED" || fail 'cannot mark etap5 success'; sync || fail 'success sync failed'
rm -f "$E5_PENDING" "$E5_INSTALL_MARK" || fail 'cleanup failed'; sync || fail 'final sync failed'
TX5=0; say 'ETAP5_INSTALL_RESULT=PASS (deployment only; car behavior NOT TESTED)'; stage_done 'Restart MMI; gdyby HMI nie wstalo - karta ratunkowa'; stage_done 'SAFE TO REMOVE SD'; }
rollback_etap5(){ say 'MODE=ROLLBACK_ETAP5'; checkrw "$SD" || fail 'SD not writable'
if [ ! -f "$E5_INSTALLED" ] && [ ! -f "$E5_PENDING" ]; then rm -f "$E5_ROLLBACK_MARK"; say 'ETAP5_NOT_INSTALLED: nic do cofniecia'; say 'SAFE TO REMOVE SD'; return 0; fi
checkrw "$APP" || fail 'app not writable'
tx_restore "$E5_BACKUP" "$E5_NAMES" || fail 'etap5 restore failed: do not reboot'; rm -f "$E5_INSTALLED" "$E5_PENDING" "$E5_ROLLBACK_MARK" || fail 'cannot clear etap5 flags'
: > "$E5_ROLLEDBACK" || fail 'cannot mark etap5 rollback'; sync || fail 'etap5 rollback sync failed'
say 'ETAP5_ROLLBACK_RESULT=PASS (v8s i Etap 4 zostaja)'; say 'Restart MMI'; say 'SAFE TO REMOVE SD'; }
# --- Etap 7 ---------------------------------------------------------------
# Instalacja wymaga Etapu 4 (hook w mm-ipod dostarcza dane manewru) i v8s
# (mapa na VC, nad ktora rysujemy). Bez Etapu 4 nakladka nie mialaby czego
# pokazac, wiec odmawiamy zamiast instalowac polowe funkcji.
install_etap7(){ say 'MODE=INSTALL_ETAP7'; checkrw "$SD" || fail 'SD not writable'
[ -f "$INSTALLED" ] || fail 'ETAP7: najpierw musi byc zainstalowany v8s (brak installed.ok)'
[ ! -e "$PENDING" ] || fail 'ETAP7: niedokonczona instalacja v8s - use ROLLBACK'
[ -f "$E4_INSTALLED" ] || fail 'ETAP7: wymaga Etapu 4 (dane manewru z mm-ipod)'
[ ! -e "$E4_PENDING" ] || fail 'ETAP7: niedokonczona instalacja Etapu 4 - use ROLLBACK_ETAP4'
[ ! -e "$E7_PENDING" ] || fail 'ETAP7: niedokonczona instalacja Etapu 7 - use ROLLBACK_ETAP7'
[ ! -e "$E7_INSTALLED" ] || fail 'ETAP7: juz zainstalowany'
preflight; version_gate
# Biblioteka Etapu 7 lezy w payloadzie pod WLASNA nazwa (.etap7.so). Gdyby
# dzielila nazwe z payloadem Etapu 4, INSTALL_ETAP4 instalowalby ja zamiast
# swojej, a ROLLBACK_ETAP7 przywracalby ja "sprzed Etapu 7" - czyli nic by nie
# cofal. Cel instalacji jest ten sam plik co u Etapu 4, zrodlo inne.
E7_LIB_SRC=libmib2high-carplay-rgi-ipod.etap7.so
for n in vc_overlay vc_overlay_probe mib2high-overlay.conf flag_atlas.rgba k3346-overlay-probe.sh mib2high-lvds-control.sh "$E7_LIB_SRC"; do
    [ -s "$PAY/$n" ] || fail "ETAP7: missing $n w payload"
done
# Pliki wspoldzielone musza istniec w aucie - inaczej kopia zapisalaby znacznik
# .absent i rollback by ich nie przywrocil.
[ -f "`pathof mib2high-lvds-control.sh`" ] || fail 'ETAP7: brak mib2high-lvds-control.sh w aucie - stan nieznany'
[ -f "`pathof libmib2high-carplay-rgi-ipod.so`" ] || fail 'ETAP7: brak biblioteki Etapu 4 w aucie - stan nieznany'
checkrw "$APP" || fail 'app filesystem not writable'
if [ -e "$E7_BACKUP" ] && [ -f "$E7_ROLLEDBACK" ]; then mv "$E7_BACKUP" "$E7_BACKUP.old.$$" || fail 'cannot rotate old etap7 backup'; rm -f "$E7_ROLLEDBACK"; fi
tx_backup "$E7_BACKUP" "$E7_NAMES" || fail 'etap7 original backup failed'; : > "$E7_PENDING" || fail 'cannot mark etap7 transaction'; TX7=1
for n in vc_overlay vc_overlay_probe k3346-overlay-probe.sh mib2high-lvds-control.sh; do
    say "INSTALLING=$n"; install_file "$PAY/$n" "`pathof $n`" 755 || fail "install failed $n"
done
# Konfiguracja i tekstura flagi celu - oba 644, oba czytane w locie.
for n in mib2high-overlay.conf flag_atlas.rgba; do
    say "INSTALLING=$n"; install_file "$PAY/$n" "`pathof $n`" 644 || fail "install failed $n"
done
say 'INSTALLING=libmib2high-carplay-rgi-ipod.so'
install_file "$PAY/$E7_LIB_SRC" "`pathof libmib2high-carplay-rgi-ipod.so`" 755 || fail 'install failed libmib2high-carplay-rgi-ipod.so'
sync || fail 'pre-success sync failed'; : > "$E7_INSTALLED" || fail 'cannot mark etap7 success'; sync || fail 'success sync failed'
rm -f "$E7_PENDING" "$E7_INSTALL_MARK" || fail 'cleanup failed'; sync || fail 'final sync failed'
TX7=0; say 'ETAP7_INSTALL_RESULT=PASS (deployment only; car behavior NOT TESTED)'
stage_done 'Restart MMI, potem CarPlay z trasa. Jesli DisplayManager nie przyjmie naszego'
stage_done 'displayable, skrypt zostaje na kontekscie 77 i wszystko dziala jak dotad.'
stage_done 'SAFE TO REMOVE SD'; }
rollback_etap7(){ say 'MODE=ROLLBACK_ETAP7'; checkrw "$SD" || fail 'SD not writable'
if [ ! -f "$E7_INSTALLED" ] && [ ! -f "$E7_PENDING" ]; then rm -f "$E7_ROLLBACK_MARK"; say 'ETAP7_NOT_INSTALLED: nic do cofniecia'; say 'SAFE TO REMOVE SD'; return 0; fi
checkrw "$APP" || fail 'app not writable'
tx_restore "$E7_BACKUP" "$E7_NAMES" || fail 'etap7 restore failed: do not reboot'; rm -f "$E7_INSTALLED" "$E7_PENDING" "$E7_ROLLBACK_MARK" || fail 'cannot clear etap7 flags'
: > "$E7_ROLLEDBACK" || fail 'cannot mark etap7 rollback'; sync || fail 'etap7 rollback sync failed'
say 'ETAP7_ROLLBACK_RESULT=PASS (v8s, Etap 4 i 5 zostaja)'; say 'Restart MMI'; say 'SAFE TO REMOVE SD'; }
# Sonda Etapu 7a: tylko odczyt + jeden odwracalny eksperyment z kontekstem.
# Nie wymaga zainstalowanego Etapu 7 jako transakcji, ale potrzebuje binarek,
# wiec dziala po INSTALL_ETAP7 (albo gdy ktos wgral je recznie).
probe_etap7(){ say 'MODE=PROBE_ETAP7'; checkrw "$SD" || fail 'SD not writable'
# pathof, nie zakodowana sciezka: inaczej sonda jest niesprawdzalna w symulacji
# (tam drzewo auta stoi pod MIB_APP) i cicho zawsze odmawia.
P=`pathof k3346-overlay-probe.sh`
[ -x "$P" ] || fail 'ETAP7: brak k3346-overlay-probe.sh w aucie - najpierw INSTALL_ETAP7'
d="$COLLECT/probe-`date +%Y%m%d-%H%M%S 2>/dev/null || echo run`"
mkdir -p "$d" || fail 'cannot create probe output dir'
say "PROBE_OUT=$d"
say 'Sonda trwa ok. 1,5 minuty. Dwa razy zrobi 20-sekundowa przerwe:'
say '  1) nacisnij VIEW - przelacz na drugi uklad zegarow'
say '  2) popatrz na VC i ocen schodek alfy na dole siatki'
say 'iPhone ma byc ODLACZONY - inaczej sonda odmowi.'
say 'UWAGA: po wizycie 2026-10-27 sonda nie jest juz potrzebna do niczego'
say 'nowego - odpowiedziala na swoje pytania (okno przyjete, rozmiar co do'
say 'piksela, pozycja zawsze (0,0), alfa mieszana per-piksel). Zostaje jako'
say 'narzedzie regresji, gdyby nakladka przestala sie pokazywac.'
sh "$P" "$d" 2>&1 | while read -r l; do say "$l"; done
sync || fail 'probe sync failed'
rm -f "$E7_PROBE_MARK"
say 'PROBE_ETAP7_RESULT=DONE'; say 'SAFE TO REMOVE SD'; }
# Collect files emitted by the already running car services. No additional services
# or network listeners are started, and no unverified QNX utilities are needed.
copyif(){
    [ -f "$1" ] || return 0
    cp "$1" "$2/" || return 1
}
collect(){
    say 'MODE=COLLECT'
    checkrw "$SD" || fail 'SD not writable'
    i=1
    while [ "$i" -le 99 ]; do
        if [ "$i" -lt 10 ]; then d="$COLLECT/run-0$i"; else d="$COLLECT/run-$i"; fi
        [ -e "$d" ] || break
        i=$((i+1))
    done
    [ "$i" -le 99 ] || fail 'collection folders full'
    mkdir -p "$d/live" "$d/logs" "$d/trace" || fail 'cannot make collection folder'
    RESULT=PASS
    COPIED=0
    for n in $NAMES; do
        f=`pathof "$n"`
        if [ -f "$f" ]; then
            if cp "$f" "$d/live/"; then :; else RESULT=PARTIAL; echo "LIVE_COPY_FAILED=$f" >> "$d/trace/collection-errors.txt"; fi
        fi
    done
    # Etap 4: aktualne usblauncher_otg.lua / iap2.cfg / launcher / biblioteka
    # (stock albo zmienione - wynik porownania mowi, w jakim stanie jest auto).
    for n in $E4_NAMES $E5_NAMES; do
        f=`pathof "$n"`
        if [ -f "$f" ]; then
            if cp "$f" "$d/live/"; then :; else RESULT=PARTIAL; echo "LIVE_COPY_FAILED=$f" >> "$d/trace/collection-errors.txt"; fi
        fi
    done
    # Explicit glob set from the known working v8g2 collector and v8i logs.
    for f in /tmp/carplay_hook.log /tmp/carplay_hook.log.1 /tmp/carplay_hook.log.2 /tmp/carplay_hook.log.3 \
             /tmp/mib2high-carplay-rgi-native.log /tmp/k2161-rgi-rx.log /tmp/k2161-rgi-launcher.log \
             /tmp/mib2high-lvds-control.log /tmp/vc_render.log /tmp/vc_render-launch.log \
             /tmp/vc_overlay.log /tmp/vc_overlay-launch.log /tmp/vc-overlay-probe.log \
             /tmp/mib2high-vc-render.profile /tmp/*rgi*.log /tmp/*rgd*.log /tmp/*bap*.log \
             /tmp/*DSI*.log /tmp/*dsi*.log /tmp/*tracer*.log /tmp/*zoom*.log \
             /tmp/*gauge*.log /tmp/v8l-dsitracer-*.log /tmp/v8l-dsitracer-status.log /tmp/v8l-dsitracer-launch.log /tmp/v8k-rgi-vc-java.log /tmp/v8k-rgi-vc-java.log.1 \
             /tmp/v8l-rgi-factory.log /tmp/v8l-rgi-factory.log.1 \
             /tmp/v8l-dsitracer-1798-*.log /tmp/v8l-dsitracer-20991-*.raw \
             /tmp/v8l-dsitracer-*-status.log /tmp/v8l-dsitracer-launch-*.log \
             /tmp/iap2.log /tmp/k3346-rgi-ipod-rx.bin /tmp/k3346-rgi-ipod-hook.log.1 \
             /tmp/k3346-rgi-ipod-hook.log.2 /tmp/k3346-rgi-ipod-hook.log.3; do
        [ -f "$f" ] || continue
        if cp "$f" "$d/logs/"; then
            echo "$f" >> "$d/trace/copied-files.txt"
            COPIED=$((COPIED+1))
        else
            RESULT=PARTIAL
            echo "LOG_COPY_FAILED=$f" >> "$d/trace/collection-errors.txt"
        fi
    done
    # The established Java logger uses the RCC shared-memory path. Preserve
    # its raw output even if /tmp happens to be a different JVM namespace.
    if [ -f /net/rcc/dev/shmem/mib2high-carplay-rgi.log ]; then
        if cp /net/rcc/dev/shmem/mib2high-carplay-rgi.log "$d/logs/mib2high-carplay-rgi-java-shmem.log"; then
            echo '/net/rcc/dev/shmem/mib2high-carplay-rgi.log' >> "$d/trace/copied-files.txt"
            COPIED=$((COPIED+1))
        else
            RESULT=PARTIAL
            echo 'JAVA_SHMEM_LOG_COPY_FAILED' >> "$d/trace/collection-errors.txt"
        fi
    fi
    # v8m source-built factory logger writes to the same proven Java shared mount.
    # This resolves v8l's inaccessible /tmp Java-process namespace.
    RCC_SHMEM="${MIB_RCC_SHMEM:-/net/rcc/dev/shmem}"
    FACTORY_FILES=0
    for f in "$RCC_SHMEM/v8m-rgi-factory.log" "$RCC_SHMEM/v8m-rgi-factory.log.1"; do
        if [ -s "$f" ]; then
            if cp "$f" "$d/logs/"; then
                echo "$f" >> "$d/trace/copied-files.txt"
                COPIED=$((COPIED+1))
                FACTORY_FILES=$((FACTORY_FILES+1))
            else
                RESULT=PARTIAL
                echo "FACTORY_LOG_COPY_FAILED=$f" >> "$d/trace/collection-errors.txt"
            fi
        fi
    done
    # Etap 5: znacznik sondy przeslaniania klas (pisze go podmieniona klasa HMI).
    for f in "$RCC_SHMEM/k3346-override-probe.log"; do
        if [ -f "$f" ]; then
            if cp "$f" "$d/logs/"; then echo "$f" >> "$d/trace/copied-files.txt"; COPIED=$((COPIED+1))
            else RESULT=PARTIAL; echo "PROBE_LOG_COPY_FAILED=$f" >> "$d/trace/collection-errors.txt"; fi
        fi
    done
    # The main Java shmem log contains V8M_FACTORY records as fallback.
    if [ "$FACTORY_FILES" -eq 0 ]; then
        if [ -f "$d/logs/mib2high-carplay-rgi-java-shmem.log" ] && \
            grep 'V8M_FACTORY' "$d/logs/mib2high-carplay-rgi-java-shmem.log" >/dev/null 2>&1; then
            echo 'FACTORY_TRACE=MAIN_JAVA_LOG_FALLBACK' >> "$d/trace/copied-files.txt"
        else
            RESULT=PARTIAL
            echo 'FACTORY_TRACE_MISSING (verify guidance ran; inspect main Java log)' >> "$d/trace/collection-errors.txt"
        fi
    fi
    TRACE_FILES=0
    for f in "$d/logs"/v8l-dsitracer-1798-??.log "$d/logs"/v8l-dsitracer-20991-??.raw; do
        if [ -s "$f" ]; then TRACE_FILES=$((TRACE_FILES+1)); fi
    done
    if [ "$TRACE_FILES" -eq 0 ]; then
        RESULT=PARTIAL
        echo 'NO_BUFFERED_DSITRACER_DATA (capture may not have connected or no events emitted)' >> "$d/trace/collection-errors.txt"
    fi
    if [ "$COPIED" -eq 0 ]; then
        RESULT=PARTIAL
        echo 'NO_NATIVE_LOG_FILES_COPIED' >> "$d/trace/collection-errors.txt"
    fi
    if sloginfo > "$d/trace/sloginfo-raw.txt" 2>&1; then
        cp "$d/trace/sloginfo-raw.txt" "$d/sloginfo.txt" || RESULT=PARTIAL
    else
        RESULT=PARTIAL
        echo 'SLOGINFO_FAILED' >> "$d/trace/collection-errors.txt"
    fi
    pidin ar > "$d/pidin-ar.txt" 2>&1 || { RESULT=PARTIAL; echo 'PIDIN_FAILED' >> "$d/trace/collection-errors.txt"; }
    netstat -an > "$d/netstat-an.txt" 2>&1 || { RESULT=PARTIAL; echo 'NETSTAT_FAILED' >> "$d/trace/collection-errors.txt"; }
    # Etap 5.3: sonda okladki (tylko odczyt, jedno polaczenie klienta iAP2).
    # Uruchamiana z kopii w /tmp, z limitem 15 s; zawieszona -> zabita (TIMEOUT).
    # Sensowna tylko przy podlaczonym iPhonie i grajacej muzyce w CarPlay.
    ART_PROBE="${MIB_ARTWORK_PROBE:-$PAY/k3346-artwork-probe}"
    ART_DEV="${MIB_IPOD_DEV:-/dev/ipod0}"
    if [ -f "$ART_PROBE" ] && [ -e "$ART_DEV" ]; then
        mkdir -p "$d/artwork" || RESULT=PARTIAL
        ART_TMP="/tmp/k3346-artwork-probe.$$"
        if cp "$ART_PROBE" "$ART_TMP" 2>/dev/null && chmod 755 "$ART_TMP" 2>/dev/null; then
            LD_LIBRARY_PATH=/mnt/app/armle/usr/lib:/mnt/app/armle/lib:/eso/lib:/lib:/usr/lib "$ART_TMP" "$d/artwork" > "$d/artwork/probe-stdout.txt" 2>&1 &
            ART_PID=$!
            i=0
            while kill -0 "$ART_PID" 2>/dev/null && [ "$i" -lt 15 ]; do sleep 1; i=$((i+1)); done
            if kill -0 "$ART_PID" 2>/dev/null; then
                kill -9 "$ART_PID" 2>/dev/null
                echo "ARTWORK_PROBE=TIMEOUT" >> "$d/artwork/probe-stdout.txt"
            else
                wait "$ART_PID" 2>/dev/null; echo "ARTWORK_PROBE_EXIT=$?" >> "$d/artwork/probe-stdout.txt"
            fi
            rm -f "$ART_TMP" 2>/dev/null
        else
            RESULT=PARTIAL; echo 'ARTWORK_PROBE_COPY_FAILED' >> "$d/trace/collection-errors.txt"
        fi
    else
        echo "ARTWORK_PROBE_SKIPPED probe=$ART_PROBE dev=$ART_DEV" >> "$d/trace/copied-files.txt"
    fi
    # Etap 5: zrzut obiektow PPS sterownika iAP2 (tylko odczyt). telephony = zrodlo
    # rozmow na VC (5.2), nowplaying = metadane/okladka (5.3). Dane osobowe (numery).
    PPS_IAP2="${MIB_PPS_IAP2:-/ramdisk/pps/iap2}"
    if [ -d "$PPS_IAP2" ]; then
        mkdir -p "$d/pps" || RESULT=PARTIAL
        for n in telephony nowplaying device; do
            [ -f "$PPS_IAP2/$n" ] || continue
            cp "$PPS_IAP2/$n" "$d/pps/$n.txt" 2>/dev/null || { RESULT=PARTIAL; echo "PPS_COPY_FAILED=$n" >> "$d/trace/collection-errors.txt"; }
        done
    fi
    # Etap 6b: gdzie MMI trzyma wlasny adres Bluetooth (do BluetoothTransportComponent
    # w identyfikacji iAP2). TYLKO ODCZYT: listingi katalogow i kopie (max 8 KB) malych
    # obiektow z blue/bt w nazwie. Informacyjne - nie zmienia RESULT. Dane osobowe
    # (adresy sparowanych telefonow) - zostaja na karcie.
    BT_ROOTS="${MIB_BT_ROOTS:-/ramdisk/pps /pps /pps/services /pps/system /var /var/app /mnt /net /dev /net/rcc/pps /net/rcc/ramdisk/pps /net/rcc/var}"
    mkdir -p "$d/bt-addr" 2>/dev/null && {
        for r in $BT_ROOTS; do
            [ -d "$r" ] || continue
            echo "== $r" >> "$d/bt-addr/listing.txt"
            ls -la "$r" >> "$d/bt-addr/listing.txt" 2>&1
            for f in "$r"/*[Bb][Tt]* "$r"/*[Bb]lue* "$r"/*BLUE*; do
                [ -e "$f" ] || continue
                case "$f" in /dev/*) continue;; esac
                if [ -d "$f" ]; then
                    echo "== $f" >> "$d/bt-addr/listing.txt"
                    ls -la "$f" >> "$d/bt-addr/listing.txt" 2>&1
                    for g in "$f"/*; do
                        [ -f "$g" ] || continue
                        echo "== $g" >> "$d/bt-addr/objects.txt"
                        dd if="$g" bs=4096 count=2 2>/dev/null >> "$d/bt-addr/objects.txt"
                        echo >> "$d/bt-addr/objects.txt"
                    done
                elif [ -f "$f" ]; then
                    echo "== $f" >> "$d/bt-addr/objects.txt"
                    dd if="$f" bs=4096 count=2 2>/dev/null >> "$d/bt-addr/objects.txt"
                    echo >> "$d/bt-addr/objects.txt"
                fi
            done
        done
    }
    # Stan kontekstow wyswietlacza - TYLKO ODCZYT: 'gc' = get contexts, zadnego
    # 'sc'/'dc', zeby nie zmieniac stanu ekranu w trakcie zbierania logow.
    # Punkt odniesienia z etapu 1 (przed instalacja): 87 kontekstow,
    # 77={20,102,101,58}, brak 900. Po instalacji oczekujemy kontekstu 900
    # z displayable 58 - to dowod, ze obraz jest podpiety tam, gdzie ma byc.
    # LD_LIBRARY_PATH podajemy tylko dla tego wywolania (nie export), zeby nie
    # zmieniac srodowiska pozostalych polecen zbierania.
    DMDT_BIN="${MIB_DMDT:-/eso/bin/apps/dmdt}"
    if [ -x "$DMDT_BIN" ]; then
        if LD_LIBRARY_PATH=/eso/lib IPL_CONFIG_DIR=/etc/eso/production \
           "$DMDT_BIN" gc > "$d/dmdt-gc.txt" 2>&1; then :
        else RESULT=PARTIAL; echo 'DMDT_GC_FAILED' >> "$d/trace/collection-errors.txt"; fi
        # Etap 7: gd to jedyne miejsce, gdzie widac displayable z ID, TYPEM,
        # ROZMIAREM i liczba buforow - czyli czy nasz numer jest wolny i jaka
        # geometrie dostaje panel manewru. gs daje liste wyswietlaczy.
        # Oba sa krotkie, w przeciwienstwie do gc, ktore DisplayManager i tak
        # ucina na 11264 B (wszystkie dotychczasowe zrzuty koncza sie na
        # kontekscie 78 z 88 - to limit po stronie dmdt, nie tego skryptu).
        if LD_LIBRARY_PATH=/eso/lib IPL_CONFIG_DIR=/etc/eso/production \
           "$DMDT_BIN" gd > "$d/dmdt-gd.txt" 2>&1; then :
        else RESULT=PARTIAL; echo 'DMDT_GD_FAILED' >> "$d/trace/collection-errors.txt"; fi
        if LD_LIBRARY_PATH=/eso/lib IPL_CONFIG_DIR=/etc/eso/production \
           "$DMDT_BIN" gs > "$d/dmdt-gs.txt" 2>&1; then :
        else RESULT=PARTIAL; echo 'DMDT_GS_FAILED' >> "$d/trace/collection-errors.txt"; fi
    else
        RESULT=PARTIAL; echo "DMDT_MISSING=$DMDT_BIN" >> "$d/trace/collection-errors.txt"
    fi
    # Focused index is optional; the original raw logs above are retained.
    grep -Ei 'V8I_|zoom|ZoomArea|ViewScreenViewPort|DSIMapViewer|DSIKOMO|DSIKombiSync|screenFormat|VC_TRACE|DSITracer|RGI|HUD|19810|gauge|dial' \
        "$d/trace/sloginfo-raw.txt" > "$d/trace/events.txt" 2>/dev/null || true
    {
        echo 'COLLECTION_VERSION=v8s+k3346-etap4'
        if [ -f "$E4_INSTALLED" ]; then echo 'ETAP4=INSTALLED'; elif [ -f "$E4_PENDING" ]; then echo 'ETAP4=PENDING'; elif [ -f "$E4_ROLLEDBACK" ]; then echo 'ETAP4=ROLLED_BACK'; else echo 'ETAP4=NOT_INSTALLED'; fi
        if [ -f "$E5_INSTALLED" ]; then echo 'ETAP5=INSTALLED'; elif [ -f "$E5_PENDING" ]; then echo 'ETAP5=PENDING'; elif [ -f "$E5_ROLLEDBACK" ]; then echo 'ETAP5=ROLLED_BACK'; else echo 'ETAP5=NOT_INSTALLED'; fi
        echo 'ETAP5 sonda: logs/k3346-override-probe.log (K3346_OVERRIDE_PROBE class_loaded = podmiana klas dziala)'
        echo 'ETAP4 logi: k3346-rgi-ipod-rx.log (oczekiwane driver_base_ready, callsite_ready, msg=5201/5202), k3346-rgi-ipod-native.log, iap2.log (IdentificationAccepted)'
        echo "LOG_FILES_COPIED=$COPIED"
        echo 'DSITRACER_STATUS=ports 1798+20991 recorder requested when AltScreen starts; check status and launch logs for execution'
        echo 'Gauge layout: LIVE DSIBAP service=50 FctID=54 bit0: 0=BIG, 1=SMALL; check V8N_GAUGE_BAP54 and VIEW_REACQUIRE logs'
        echo "FACTORY_LOG_FILES_COPIED=$FACTORY_FILES"
        echo 'DSIBAP=full DSICarKombi=full expected after install'
        echo 'dmdt-gc.txt: oczekiwany kontekst 900 z displayable 58 oraz 77={20,102,101,58}; etap 1 (przed instalacja) mial 87 kontekstow i brak 900'
        if [ -f "$NAVIGNORE_JAR" ]; then echo 'NAVIGNORE=PRESENT'; else echo 'NAVIGNORE=MISSING'; fi
        if [ -f "$VCPOL_FILE" ]; then echo "VC_POLICY_FILE=`cat "$VCPOL_FILE"`"; else echo 'VC_POLICY_FILE=none (rgi)'; fi
        if [ -f "$E7_INSTALLED" ]; then echo 'ETAP7=INSTALLED'; elif [ -f "$E7_PENDING" ]; then echo 'ETAP7=PENDING'; elif [ -f "$E7_ROLLEDBACK" ]; then echo 'ETAP7=ROLLED_BACK'; else echo 'ETAP7=NOT_INSTALLED'; fi
        if [ -f "$MOD/RELEASE" ]; then echo "RELEASE=`cat "$MOD/RELEASE"`"; else echo 'RELEASE=dev (paczka etapowa)'; fi
        echo 'ETAP7 logi: /tmp/vc_overlay.log (EVT_READY, pierwsza klatka, fps, brak progress watchdog TIMEOUT), vc_overlay-launch.log'
        echo 'ETAP7 dmdt-gd.txt: czy nasz displayable (domyslnie 98) jest na liscie = czy DisplayManager przyjal okno'
        echo 'ETAP7 lvds-control.log: overlay start / overlay context=901 declared / staying on 77'
        echo 'ETAP7 k3346-rgi-ipod-native.log: overlay_build i overlay_send = hook wysyla manewry do renderera'
    } > "$d/trace/metadata.txt" || RESULT=PARTIAL
    sync || fail 'collection sync failed'
    say "COLLECT_RESULT=$RESULT saved=$d"
    [ "$QUIET_SAFE" -eq 1 ] || say 'SAFE TO REMOVE SD'
}
# Wszystkie etapy, po kolei, kazdy tylko jesli go jeszcze nie ma. Funkcje etapow
# sa te same co przy znacznikach pojedynczych - ich odmowy, kopie i auto-cofniecia
# dzialaja bez zmian; fail() w etapie konczy caly przebieg.
install_all(){ say 'MODE=INSTALL_ALL'; ALL=1
[ ! -f "$MOD/RELEASE" ] || say "RELEASE=`cat "$MOD/RELEASE"`"
# Wszystko juz jest: niczego nie nadpisujemy. Nowa wersja na karcie NIE
# podmienia starej po cichu - aktualizacja to odinstaluj, potem zainstaluj,
# zeby kazdy etap dostal swieza kopie oryginalow.
if [ -f "$INSTALLED" ] && [ -f "$E4_INSTALLED" ] && [ -f "$E5_INSTALLED" ] && [ -f "$E7_INSTALLED" ]; then
rm -f "$ALL_INSTALL_MARK" || fail 'cannot clear INSTALL_ALL'; sync
say 'INSTALL_ALL_RESULT=ALREADY_INSTALLED (nothing changed)'
say 'To update to a new version: uninstall first, then install again.'; say 'SAFE TO REMOVE SD'; return 0; fi
if [ -f "$INSTALLED" ]; then say 'STAGE_MAP=already installed'; else install; fi
if [ -f "$E4_INSTALLED" ]; then say 'STAGE_ROUTE=already installed'; else install_etap4; fi
if [ -f "$E5_INSTALLED" ]; then say 'STAGE_PHONE=already installed'; else install_etap5; fi
if [ -f "$E7_INSTALLED" ]; then say 'STAGE_ARROW=already installed'; else install_etap7; fi
rm -f "$ALL_INSTALL_MARK" || fail 'cannot clear INSTALL_ALL'; sync || fail 'final sync failed'
say 'INSTALL_ALL_RESULT=PASS'
navignore_report
say 'Restart the MMI now: hold the big MMI knob + NAV/MAP + RADIO together for ~10 s'
say '(A4/A5/Q5 B9; other models: button table in the M.I.B. wiki).'
say 'Then connect the iPhone and start CarPlay navigation.'
say 'SAFE TO REMOVE SD'; }
# Zapis polityki VC. Atomowo (plik tymczasowy + mv), zeby HMI nigdy nie
# przeczytalo polowy pliku. Nie wymaga kopii: przed nami pliku nie bylo.
# Najpierw COLLECT: kto zmienia polityke, robi to w trakcie trasy, a logi tej
# trasy (pod poprzednia polityka) sa wlasnie tym, czego potrzebujemy. Kilka
# znacznikow naraz = kolejka: kazde uruchomienie zuzywa jeden (MAP, OFF, RGI),
# wiec caly test da sie przygotowac na karcie za jednym razem.
set_vc_policy(){ pol="$1"; mark="$2"; say "MODE=VC_POLICY policy=$pol"; checkrw "$SD" || fail 'SD not writable'
[ -f "$INSTALLED" ] || fail 'VC_POLICY: the mod is not installed (no installed.ok) - nothing changed'
[ ! -e "$PENDING" ] || fail 'VC_POLICY: unfinished install - use ROLLBACK'
[ -d "$APP/root" ] || fail 'VC_POLICY: missing app root directory'
checkrw "$APP" || fail 'app filesystem not writable'
# resztki po przerwanym zapisie (tez po bledzie printf w 1.0.1-rc1) - nic nie znacza, ale nie zostawiamy smieci
rm -f "$VCPOL_FILE".new.* 2>/dev/null
QUIET_SAFE=1; collect; QUIET_SAFE=0
# echo, nie printf: na MMI nie ma printf (test 2026-10-30: "printf: cannot execute").
echo "vc_policy=$pol" > "$VCPOL_FILE.new.$$" || fail 'cannot write VC policy'
chmod 644 "$VCPOL_FILE.new.$$" || { rm -f "$VCPOL_FILE.new.$$"; fail 'cannot chmod VC policy'; }
mv -f "$VCPOL_FILE.new.$$" "$VCPOL_FILE" || { rm -f "$VCPOL_FILE.new.$$"; fail 'cannot install VC policy'; }
rm -f "$MOD/$mark" || fail 'cannot clear VC policy marker'; sync || fail 'sync failed'
say "VC_POLICY_RESULT=PASS policy=$pol (logs of the previous route saved above)"
for m in $VCPOL_MARKS; do [ ! -f "$MOD/$m" ] || say "NEXT_QUEUED=$m (run the script again during the next route)"; done
say 'Restart the MMI now: hold the big MMI knob + NAV/MAP + RADIO together for ~10 s'
say '(A4/A5/Q5 B9; other models: button table in the M.I.B. wiki).'
say 'SAFE TO REMOVE SD'; }
[ -d "$SD" ] || fail 'SD not mounted'; [ -d "$MOD" ] || fail 'package missing'
# ROLLBACK ma pierwszenstwo NAD wymuszonym zbieraniem: zapomniany na karcie plik
# COLLECT_LOGS nie moze nigdy zablokowac sciezki ratunku. Bezpieczenstwo przed wygoda.
if [ -f "$ROLLBACK" ]; then rollback
elif [ -f "$E7_ROLLBACK_MARK" ]; then rollback_etap7
elif [ -f "$E5_ROLLBACK_MARK" ]; then rollback_etap5
elif [ -f "$E4_ROLLBACK_MARK" ]; then rollback_etap4
elif [ -f "$FORCECOLLECT" ]; then say 'FORCED_COLLECT=1 (plik COLLECT_LOGS)'; collect
elif [ -f "$ALL_INSTALL_MARK" ]; then install_all
elif [ -f "$E4_INSTALL_MARK" ]; then install_etap4
elif [ -f "$E5_INSTALL_MARK" ]; then install_etap5
elif [ -f "$E7_INSTALL_MARK" ]; then install_etap7
elif [ -f "$E7_PROBE_MARK" ]; then probe_etap7
elif [ -f "$MOD/VC_POLICY_MAP" ]; then set_vc_policy map VC_POLICY_MAP
elif [ -f "$MOD/VC_POLICY_OFF" ]; then set_vc_policy off VC_POLICY_OFF
elif [ -f "$MOD/VC_POLICY_RGI" ]; then set_vc_policy rgi VC_POLICY_RGI
elif [ -f "$INSTALLED" ]; then collect
elif [ -f "$MOD/rolledback.ok" ]; then say 'ALREADY_ROLLED_BACK; no automatic reinstallation'
else install; fi
exit 0
