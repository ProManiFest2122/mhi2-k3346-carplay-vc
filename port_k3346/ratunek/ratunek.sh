#!/bin/ksh
# =============================================================================
# KARTA RATUNKOWA - mib2high-carplay-rgi (port AltScreen v8S) na K3346 / MU1438
#
# Kiedy: HMI nie wstaje po instalacji moda, wiec nie ma GEM ani M.I.B.
#
# Jak sie uruchamia: plik Swdlautorun.txt w katalogu glownym karty ->
# natywny MIBRoot (CASWDLAutoRun) startuje tryb aktualizacji -> M.I.B. 3.6.0
# Launcher/final/finalScript.sh -> Launcher/stuff.sh -> ten plik.
# Wzor: karta ratunkowa luka-dev, potwierdzona w aucie (mib2q-carplay-rgi,
# rescue_MoreIncredibleBash, issue #24).
#
# Co robi - WYLACZNIE to, co HMI czyta przy starcie:
#   1. usuwa /mnt/app/eso/hmi/lsd/dsitracer_activated
#        -> lsd.sh wraca do listy pakietow wbudowanej w lsd.jxe
#           i zdejmuje DSITracer.jar z bootclasspath;
#   2. usuwa /mnt/app/eso/hmi/lsd/jars/mib2high-carplay-rgi.jar
#        oraz (Etap 5) mib2high-k3346-overrides.jar - klasy podmieniajace
#        fabryczne; lsd.sh dokleja KAZDY jar z jars/ do bootclasspath,
#        niezaleznie od DSITracera -> bootclasspath jak przed instalacja;
#   3. przywraca fabryczny traceConfig.properties z tej karty
#        -> HMI czyta go zawsze, niezaleznie od DSITracera.
# Reszte moda cofa potem normalny rollback z karty instalacyjnej przez GEM.
#
# LEKCJA Z TESTU W AUCIE (2026-10-20): pierwsza wersja nie zostawila zadnego
# sladu, choc M.I.B. wykonal sie do konca. Dwie przyczyny, obie usuniete:
#   a) w trybie aktualizacji skrypt dziala na RCC z ubogim PATH - narzedzia
#      (rm, cp, mount...) leza na MMX. M.I.B. zawsze najpierw ustawia PATH
#      z /net/mmx/... (apps/launcher:6) - robimy dokladnie to samo;
#   b) karta jest tylko do odczytu, dopoki M.I.B. nie zrobi 'mount -uw'
#      (apps/mounts:20), a to dzieje sie dopiero po nas. A polecenie
#      z nieudanym przekierowaniem '>> log' w ogole sie nie wykonuje - wiec
#      DZIALANIA RATUNKOWE NIGDY NIE SA ZA PRZEKIEROWANIEM DO LOGU. Log idzie
#      najpierw do RAM, na karte dopiero gdy da sie pisac (tu albo na koncu
#      stuff.sh: ratunek.sh --zapisz-log).
#
# Uzycie:  ratunek.sh <HW> <nosnik>             - ratunek (z poczatku stuff.sh)
#          ratunek.sh --zapisz-log <nosnik>     - tylko dopisz log z RAM na karte
# =============================================================================

# --- (a) srodowisko dokladnie jak M.I.B. na RCC (apps/launcher:6-8) ---
export PATH=:/proc/boot:/sbin:/bin:/usr/bin:/usr/sbin:/net/mmx/bin:/net/mmx/usr/bin:/net/mmx/usr/sbin:/net/mmx/sbin:/net/mmx/mnt/app/armle/bin:/net/mmx/mnt/app/armle/sbin:/net/mmx/mnt/app/armle/usr/bin:/net/mmx/mnt/app/armle/usr/sbin
export LD_LIBRARY_PATH=/net/mmx/mnt/app/root/lib-target:/net/mmx/mnt/eso/lib:/net/mmx/eso/lib:/net/mmx/mnt/app/usr/lib:/net/mmx/mnt/app/armle/lib:/net/mmx/mnt/app/armle/lib/dll:/net/mmx/mnt/app/armle/usr/lib
export IPL_CONFIG_DIR=/etc/eso/production

R_TRYB=ratunek
if [ "$1" = "--zapisz-log" ]; then R_TRYB=log; shift; R_MED="${1:-/net/mmx/fs/sda0}"
else R_MED="${2:-/net/mmx/fs/sda0}"; fi
[ -f "$R_MED/ratunek/traceConfig.properties" ] || R_MED=/net/mmx/fs/sda0
R_SRC="$R_MED/ratunek/traceConfig.properties"
R_LOG="$R_MED/RATUNEK_LOG.txt"
R_LSD=/mnt/app/eso/hmi/lsd
R_NET=/net/mmx$R_LSD
R_JAR1=jars/mib2high-carplay-rgi.jar
R_JAR2=jars/mib2high-k3346-overrides.jar

# Log w RAM (TMP M.I.B. = /net/rcc/dev/shmem). Gdyby i to zawiodlo - /tmp.
R_RAM=/net/rcc/dev/shmem/ratunek.log
( : >> "$R_RAM" ) 2>/dev/null || R_RAM=/tmp/ratunek.log

# Dopisanie do logu nigdy nie przerywa i nigdy niczego nie blokuje.
log() { echo "$*" >> "$R_RAM" 2>/dev/null; }

# (b) odblokowanie karty - jak apps/mounts:20
r_odblokuj() {
    for m in "$R_MED" /net/mmx/fs/sda0; do
        if mount -uw "$m" 2>/dev/null; then log "mount -uw $m: OK"; else log "mount -uw $m: nie udalo sie"; fi
    done
}

# Przeniesienie logu z RAM na karte. Po udanym dopisaniu RAM jest czyszczony,
# zeby drugie wywolanie (koniec stuff.sh) nie dopisalo tego samego jeszcze raz.
r_zapisz_log() {
    [ -s "$R_RAM" ] || return 0
    mount -uw "$R_MED" 2>/dev/null
    if cat "$R_RAM" >> "$R_LOG" 2>/dev/null; then
        rm -f "$R_RAM" 2>/dev/null
        sync 2>/dev/null
        return 0
    fi
    return 1
}

if [ "$R_TRYB" = log ]; then
    r_zapisz_log
    exit 0
fi

r_stan() {
    for f in "$R_NET/dsitracer_activated" "$R_NET/$R_JAR1" "$R_NET/$R_JAR2" \
             "$R_NET/traceConfig.properties"; do
        if [ -e "$f" ]; then log "$(ls -l "$f" 2>&1)"; else log "brak: $f"; fi
    done
}

# Przywrocenie traceConfig, tylko z niepustej kopii, w dwoch krokach:
#  1. kopia z karty do pliku tymczasowego obok - przerwany odczyt z karty
#     nigdy nie dotknie pliku w aucie;
#  2. przepisanie TRESCI do istniejacego pliku (cat > plik) - ten sam i-wezel,
#     wiec zostaje oryginalny tryb dostepu.
r_trace() {
    R_DST="$R_NET/traceConfig.properties"
    R_TMP="$R_NET/traceConfig.properties.ratunek.$$"
    if [ ! -s "$R_SRC" ]; then log "UWAGA: brak/pusta kopia $R_SRC - traceConfig NIE ruszany"; return 1; fi
    if ! cp "$R_SRC" "$R_TMP" 2>/dev/null || [ ! -s "$R_TMP" ]; then
        rm -f "$R_TMP" 2>/dev/null; log "UWAGA: kopia do $R_TMP nieudana"; return 1
    fi
    if [ -f "$R_DST" ]; then
        if ! cat "$R_TMP" > "$R_DST" 2>/dev/null; then rm -f "$R_TMP" 2>/dev/null; log "UWAGA: zapis $R_DST nieudany"; return 1; fi
        rm -f "$R_TMP" 2>/dev/null
    elif ! mv -f "$R_TMP" "$R_DST" 2>/dev/null; then
        rm -f "$R_TMP" 2>/dev/null; log "UWAGA: mv do $R_DST nieudany"; return 1
    fi
    log "traceConfig przywrocony"
    return 0
}

# Same dzialania - wolane BEZ przekierowan.
r_raz() {
    mount -uw /net/mmx/mnt/app 2>/dev/null
    rm -f "$R_NET/dsitracer_activated" 2>/dev/null
    rm -f "$R_NET/$R_JAR1" 2>/dev/null
    rm -f "$R_NET/$R_JAR2" 2>/dev/null
    r_trace
    sync 2>/dev/null
}

log "=== RATUNEK start ==="
log "$(date 2>&1)"
log "nosnik=$R_MED hw=$1 arg2=$2 log_ram=$R_RAM"
for t in rm cp mv cat mount on ls sync sleep; do log "$(type "$t" 2>&1)"; done
r_odblokuj
log "--- PRZED:"
r_stan

# MMX moze sie wlasnie restartowac (bootloop) - ponawiamy, ale sukces liczymy
# tylko gdy katalog lsd na MMX jest osiagalny; inaczej "pliku nie ma" byloby
# falszywym wynikiem.
R_OK=0
R_I=0
while [ "$R_I" -lt 6 ]; do
    r_raz
    if [ -d "$R_NET" ] && [ ! -e "$R_NET/dsitracer_activated" ] \
       && [ ! -e "$R_NET/$R_JAR1" ] && [ ! -e "$R_NET/$R_JAR2" ]; then
        R_OK=1
        break
    fi
    log "proba $R_I: MMX nieosiagalny albo pliki nadal sa - ponawiam za 10 s"
    sleep 10
    R_I=$((R_I + 1))
done

# Jeszcze raz na samym MMX - na wypadek, gdyby dostep przez /net/mmx zawiodl.
on -f mmx /bin/sh -c "mount -uw /mnt/app; rm -f $R_LSD/dsitracer_activated $R_LSD/$R_JAR1 $R_LSD/$R_JAR2; sync" 2>/dev/null

log "--- PO:"
r_stan
if [ "$R_OK" -eq 1 ]; then
    log "WYNIK=OK (HMI powinno wstac; potem pelny rollback przez GEM karta instalacyjna)"
else
    log "WYNIK=NIEPEWNY (MMX nie byl osiagalny przez /net/mmx; probowano jeszcze przez on -f mmx)"
fi
log "=== RATUNEK koniec ==="

# Na karte teraz, jesli sie da; jesli nie - zrobi to koniec stuff.sh.
r_zapisz_log
exit 0
