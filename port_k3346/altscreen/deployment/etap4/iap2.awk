# K3346 Etap 4: do /mnt/system/etc/mm/iap2.cfg dopisuje na koncu sekcje [ident]
# z kierunkami wiadomosci Route Guidance (edzenis docs/installation.md):
#   MMI -> iPhone: 0x5200, 0x5203     iPhone -> MMI: 0x5201, 0x5202, 0x5204
# Etap 6 (ostatnie polaczenia, dzwonienie z listy): MMI -> iPhone 0x415A
# InitiateCall, 0x4170 StartListUpdates; iPhone -> MMI 0x4171 ListUpdate.
# Reszta pliku bez zmian. Blad (exit 1), jesli plik ma juz sekcje [ident]
# albo wpisy recvmsgs/sendmsgs - wtedy nie jest stockowy i nic nie zmieniamy.
{ print }
/^[ \t]*\[ident\]/ || /^[ \t]*(recvmsgs|sendmsgs)[ \t]*=/ { bad = 1 }
END {
    if (bad) exit 1
    print ""
    print "# K3346 Etap 4 (RGI): Route Guidance message directions"
    print "[ident]"
    print "recvmsgs=0x5201,0x5202,0x5204,0x4171"
    print "sendmsgs=0x5200,0x5203,0x415a,0x4170"
}
