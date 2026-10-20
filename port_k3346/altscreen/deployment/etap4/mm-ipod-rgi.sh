#!/bin/sh
# K3346 Etap 4 (RGI): launcher stock mm-ipod z biblioteka RGI w LD_PRELOAD.
# Wolany przez usblauncher_otg.lua zamiast golego "mm-ipod" (pole driver
# usblaunchera to nazwa programu - nie da sie tam wpisac LD_PRELOAD=...).
#  - LD_PRELOAD ustawiany tylko w tym procesie i tylko dla mm-ipod;
#  - istniejacy LD_PRELOAD jest zachowany;
#  - brak biblioteki => stock mm-ipod bez zmian (fail-open do fabryki);
#  - exec: ten sam PID, wiec usblauncher zarzadza procesem jak dotad.
LIB=/mnt/app/root/hooks/libmib2high-carplay-rgi-ipod.so
if [ -f "$LIB" ]; then
    if [ -n "${LD_PRELOAD:-}" ]; then
        LD_PRELOAD="$LIB:$LD_PRELOAD"
    else
        LD_PRELOAD="$LIB"
    fi
    export LD_PRELOAD
fi
exec /mnt/app/armle/usr/sbin/mm-ipod "$@"
