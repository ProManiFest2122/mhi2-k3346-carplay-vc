# K3346 Etap 4: w /mnt/system/etc/usblauncher_otg.lua podmienia TYLKO nazwe
# programu w aktywnej linii startu mm-ipod:
#   return "mm-ipod -d iap2,config=/etc/mm/iap2.cfg,...
#   return "/mnt/app/root/mm-ipod-rgi.sh -d iap2,config=/etc/mm/iap2.cfg,...
# Reszta pliku bajt w bajt bez zmian (linia komentarza "-- driver"mm-ipod..." tez).
# Blad (exit 1), jesli linia nie wystepuje dokladnie raz - wtedy plik jest
# nieznany i instalator nic nie zmienia.
{
    if (index($0, "return \"mm-ipod -d iap2,config=/etc/mm/iap2.cfg,") > 0 && $0 !~ /^[ \t]*--/) {
        n++
        sub(/return "mm-ipod -d /, "return \"/mnt/app/root/mm-ipod-rgi.sh -d ")
    }
    print
}
END { if (n != 1) exit 1 }
