AltScreen v8S - wersja na MHI2_ER_AUG22_K3346 / MU1438 (Audi A4 B9)
===================================================================

Stan: czesc v8s (mapa CarPlay na VC) dziala w aucie od 2026-10-20.
Etap 4 (RGI w mm-ipod, nizej) NIE BYL JESZCZE URUCHOMIONY W AUCIE - zbudowany
i sprawdzony tylko na komputerze (symulacja 115/115). Przed instalacja:
prostownik/zasilacz, swiezy backup M.I.B., oryginalna paczka K3346 pod reka.

Co to jest
----------
Port projektu edzenis/mib2high-carplay-altscreen-rgi (sprawdzonego na Audi Q7 4M,
K2161/MU1421) na firmware K3346/MU1438. Daje mape CarPlay na Virtual Cockpit
(drugi strumien H.264), karte instrukcji i przelaczanie ukladu przyciskiem VIEW.

Co dokladnie zmienia w MMI
--------------------------
Dodawane pliki:
  /mnt/app/root/hooks/libmib2high-carplay-rgi.so   hooki natywne (iAP2)
  /mnt/app/eso/hmi/lsd/jars/mib2high-carplay-rgi.jar   bundle OSGi
  /mnt/app/root/vc_render                          renderer obrazu na kombi
  /mnt/app/root/v8l-dsitracer-capture              odczyt DSITracera (VIEW/rolka)
  /mnt/app/root/mib2high-lvds-control.sh           sterowanie sesja/kontekstami
  /mnt/app/root/mib2high-lvds-profile.conf         geometria obrazu
  /mnt/app/root/mib2high-lvds-viewareas.conf       geometria kart
  /mnt/app/carplay_altscreen                       znacznik
  /mnt/app/eso/hmi/lsd/dsitracer_activated         znacznik

Modyfikowane pliki (kazdy z kopia zapasowa, zmiany minimalne):
  /mnt/app/eso/hmi/lsd/bundles_dsitracer_enabled.properties
      +",MIB2HighCarPlayRGI" w Bundle.Startup.Autostart
      +"MIB2HighCarPlayRGI=org.mib2high.carplay.rgi.Activator"   (2 zmiany)
  /mnt/app/eso/hmi/lsd/traceConfig.properties
      use.appender=client -> telnet,bluepirat; odkomentowane =full dla DSIBAP,
      DSICarKombi, DSIKeyPanel, DSIKombiSync, DSIKOMOView, DSIMapViewerControl
  /mnt/system/etc/eso/production/smartphone_integrator.json
      +"LD_PRELOAD=/mnt/app/root/hooks/libmib2high-carplay-rgi.so"   (1 zmiana)

NavActiveIgnore.jar NIE jest ruszany.

Blokada wersji (dodana w tym porcie)
------------------------------------
Instalator odmawia pracy na innym firmware. Na tym MMI nie ma sha256sum, cksum
ani cmp, wiec brama uzywa awk i wc -c i sprawdza:
  - HMI_Version = HMI_AU_MIB2_H_EU_G22_C6_190300
  - /mnt/app/armle/lib/dll/ipod-drvr-iap2.so ma dokladnie 739925 B
    (wzgledem tego pliku zweryfikowano offsety hookow: 17/17 PASS)
  - /ifs/lsd.jxe ma dokladnie 58131096 B (wzgledem niego skompilowano JAR)
Niezgodnosc => "VERSION_GATE ... instalacja PRZERWANA", nic nie jest zapisywane.
Bramy NIE DA SIE obejsc z M.I.B. - celowo. Zmienna MIB_FORCE=1 dziala tylko przy
uruchomieniu skryptu z powloki (symulacja na komputerze, praca deweloperska);
"run individual script" nie pozwala ustawic zmiennych srodowiskowych.

KOPIA ORYGINALNYCH PLIKOW AUTA JEST NA TEJ KARCIE
-------------------------------------------------
Instalator zapisuje kopie oryginalow w:
  mod/mib2high-carplay-rgi-v8s-simple/backup/pre-v8s/
Rollback poziomu 1 dziala TYLKO z ta kopia, czyli z TA SAMA karta.
  - Po instalacji skopiuj CALY katalog mod/ z karty na komputer.
  - Nie kasuj katalogu backup/ i nie nadpisuj folderu mod/ nowa paczka.
  - Nie formatuj tej karty, dopoki mod jest zainstalowany w aucie.
Jesli kopia zginie: zostaje poziom 2 (backup M.I.B.) albo 3 (pelna paczka K3346).

Instalacja
----------
0. NAJPIERW przetestuj karte ratunkowa na zdrowym aucie (osobna karta,
   work\sd_ratunek\, instrukcja RATUNEK_README.txt). Przy bootloopie HMI
   zielone menu nie dziala, wiec zwykly rollback tez nie - ratuje tylko ona.
1. Skopiuj zawartosc tej paczki NA SWOJA KARTE Z M.I.B. (te, ktora dziala
   z GEM), bez formatowania. M.I.B. dziala z karty - na czystej karcie
   "run individual script" nie zadziala.
   W katalogu glownym karty ma byc _Swdlautorun.txt (z podkresleniem).
   Jesli jest Swdlautorun.txt bez podkreslenia - zmien nazwe.
2. Karta do slotu SD1, auto na zaplonie, prostownik podlaczony.
3. M.I.B. -> GEM -> m.i.b. -> advanced settings -> run individual script.
   M.I.B. 3.6.0 uruchamia WYLACZNIE plik /mod/command.sh - dlatego instalator
   jest tu pod ta nazwa.
4. Czekaj na "SAFE TO REMOVE SD". Potem restart MMI recznie.

Rollback
--------
Poziom 1: skopiuj gotowy pusty plik ZNACZNIKI/ROLLBACK do folderu
          mod/mib2high-carplay-rgi-v8s-simple/
          -> run individual script -> restart. Przywraca oryginalne pliki.
          Potrzebny laptop albo telefon z czytnikiem kart.
          Nie tworz pliku recznie w Windows: "Nowy > Dokument tekstowy" daje
          ROLLBACK.txt, ktorego skrypt nie rozpozna.
          Logi zbierz PRZED rollbackiem (po rollbacku zwykle uruchomienie
          nic nie zbiera; wtedy pomaga ZNACZNIKI/COLLECT_LOGS).
          Instalator ma tez auto-rollback: jesli instalacja sie wysypie
          w trakcie, sam cofa zmiany (AUTO_ROLLBACK_RESULT=PASS).
Poziom 2: przywrocenie plikow z backupu M.I.B.
Poziom 3: pelne wgranie fabrycznej paczki K3346 z karty SD.

UWAGA: poziomy 1 i 2 dzialaja tylko z zielonego menu, a ono jest czescia
HMI. Gdy HMI nie wstaje (bootloop): odczekaj do 5 min (GEM bywa wolne po
zimnym starcie), a jesli dalej nic - KARTA RATUNKOWA (uzbrojona), potem
poziom 1.

Geometria obrazu na kombi
-------------------------
payload/mib2high-lvds-profile.conf = "1440 542 30 0 0 0 1440 542" - wartosc
sprawdzona przez edzenisa na MHI2 Harman (ta sama rodzina firmware, ten sam
tryb LVDS i te same konteksty kombi 72/77 co w tym aucie).

Panel VC w A4 B9 bywa opisywany jako 1440x540. Jesli obraz bedzie przesuniety
albo z paskiem na krawedzi, podmien profil na wariant z alt-profiles/:
  alt-profiles/mib2high-lvds-profile.b9-1440x540.conf
   -> payload/mib2high-lvds-profile.conf     (przed instalacja)
albo juz w aucie edytuj /mnt/app/root/mib2high-lvds-profile.conf.
Geometria jest czytana z pliku w czasie dzialania - NIE wymaga przebudowy.
Wszystkie 4 widoki z viewareas.conf mieszcza sie i w 542, i w 540, wiec nie
trzeba nic przeliczac.

Format profile.conf: "szerokosc wysokosc fps features safe_x safe_y safe_w safe_h"
Tylko liczby, bez komentarzy - kazdy nienumeryczny znak powoduje awaryjny
powrot do 800x480.

Etap 4: wskazowki nawigacji CarPlay (RGI) - osobny krok, osobny rollback
------------------------------------------------------------------------
Instaluje sie TYLKO na juz zainstalowanym v8s, markerem ZNACZNIKI/INSTALL_ETAP4
skopiowanym do mod/mib2high-carplay-rgi-v8s-simple/ -> run individual script
-> "ETAP4_INSTALL_RESULT=PASS" -> restart MMI.
Dodawane:
  /mnt/app/root/hooks/libmib2high-carplay-rgi-ipod.so   hooki RGI (bez AltScreen)
  /mnt/app/root/mm-ipod-rgi.sh    launcher: LD_PRELOAD tylko dla mm-ipod,
                                  potem exec stock /mnt/app/armle/usr/sbin/mm-ipod
Modyfikowane (kopia w backup/pre-etap4/):
  /mnt/system/etc/usblauncher_otg.lua   1 linia: "mm-ipod -d iap2..." ->
                                        "/mnt/app/root/mm-ipod-rgi.sh -d iap2..."
  /mnt/system/etc/mm/iap2.cfg           dopisana na koncu sekcja [ident]:
                                        recvmsgs=0x5201,0x5202,0x5204
                                        sendmsgs=0x5200,0x5203
Brama: oprocz bramy v8s dokladne rozmiary stock: usblauncher_otg.lua 19907 B,
iap2.cfg 5852 B, mm-ipod 40297 B; po zmianie 19928 B i 5970 B.
Rollback tylko Etapu 4 (mapa na VC zostaje): ZNACZNIKI/ROLLBACK_ETAP4.
Pelny ROLLBACK cofa najpierw Etap 4, potem v8s.
Gdy po Etapie 4 CarPlay sie nie laczy: ROLLBACK_ETAP4 + restart MMI. Karta SD
i zielone menu nie zaleza od zmienionego pliku (to tylko port OTG/iPhone).
Na karte: skopiuj zawartosc paczki Z ZASTAPIENIEM plikow, NICZEGO nie usuwaj
(backup/, installed.ok i collected/ na karcie musza zostac).

Etap 5: klasy HMI podmieniajace fabryczne (Siri, touchpad, telefon, okladka)
---------------------------------------------------------------------------
Instaluje sie TYLKO na zainstalowanym v8s: ZNACZNIKI/INSTALL_ETAP5 do
mod/mib2high-carplay-rgi-v8s-simple/ -> run individual script ->
"ETAP5_INSTALL_RESULT=PASS" -> restart MMI.
Dodawany: /mnt/app/eso/hmi/lsd/jars/mib2high-k3346-overrides.jar
  (lsd.sh dokleja kazdy jar z jars/ do -Xbootclasspath przed lsd.jxe).
Wersja 5.0 = SONDA: jedna klasa (TerminalModeSDSHandler) identyczna z
fabryczna + znacznik /net/rcc/dev/shmem/k3346-override-probe.log.
RYZYKO: blad w podmienionej klasie moze zatrzymac HMI (bootloop). Przed
instalacja zaktualizuj KARTE RATUNKOWA (ratunek.sh usuwa teraz tez ten JAR).
Rollback tylko Etapu 5: ZNACZNIKI/ROLLBACK_ETAP5. Pelny ROLLBACK: 5 -> 4 -> v8s.

Czego ta paczka NIE robi
------------------------
Nie rusza dzwieku, nie zmienia nawigacji fabrycznej, nie zmienia HMI ani
ekranu MMI. Okladki, telefon CarPlay na VC - pozniejsze etapy.

Licencja i zrodla
-----------------
Kod pochodzi z edzenis/mib2high-carplay-altscreen-rgi (commit
bfea86859a5bdaed3c29a16f03fc7ba87ef4a19e). Zmiany: offsety i adaptacje dla
K3346, rozdzielenie updateManeuverDescriptor/updateExitView, brama wersji.
Patrz LICENSE i NOTICE.md w port_k3346/.
