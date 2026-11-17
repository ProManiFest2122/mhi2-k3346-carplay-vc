CarPlay on the Audi Virtual Cockpit - MHI2 (Harman) K3346 / MU1438
===================================================================
(Wersja polska nizej.)

ONLY FOR firmware MHI2_ER_AUG22_K3346, MU 1438. The installer checks this
itself and refuses on any other unit without changing anything.

What you need
  - an SD card with M.I.B. (More Incredible Bash) that already works in your
    car: green menu -> m.i.b. opens,
  - a battery charger / power supply connected to the car,
  - about 20 minutes,
  - a fresh M.I.B. backup of your unit (M.I.B. -> backup).

INSTALL
 1. Unzip the -install.zip into the ROOT of your M.I.B. card and allow
    overwriting. It adds its own folder and replaces only mod/command.sh
    and mod/custom.sh (the same installer under both names).
 2. Card into the slot M.I.B. runs from (SD1), ignition on, charger on.
 3. Green menu: m.i.b. -> advanced settings -> run individual script
    (newer M.I.B. versions: Run Custom Script).
 4. Wait for INSTALL_ALL_RESULT=PASS and then SAFE TO REMOVE SD (a few
    minutes). Do not switch the ignition off before that.
 5. Restart the MMI: hold together the big MMI knob + NAV/MAP + RADIO
    for about 10 seconds (A4/A5/Q5 B9; other models: button table in the
    M.I.B. wiki, https://wiki.mibsolution.one/en/MHI2).
 6. When CarPlay is back: engine off, wait until the gauges and the MMI
    go dark (about 1 minute), engine on. An MMI restart does not restart
    the Virtual Cockpit; this does.
    Connect the iPhone and start a route in CarPlay navigation.

KEEP THIS CARD
The installer saves the original car files on this card, in
mod/mib2high-carplay-rgi-v8s-simple/backup/. Uninstalling uses them.
Do not delete or move that folder. When you copy a new zip onto the
card, MERGE the folders, do not replace them (on a Mac "Replace" deletes
everything that was in the old folder). Also copy the whole mod/ folder
to your computer.
Lost it anyway (formatted card, replaced folder, another card)? From
version 2.0.1 the -uninstall.zip and the -update.zip rebuild the backup
from the car's own files and then go on as usual (REPAIR_RESULT=PASS).

UNINSTALL (and before every update)
 1. Unzip the -uninstall.zip into the root of the SAME card.
    It adds the file mod/mib2high-carplay-rgi-v8s-simple/ROLLBACK and
    the installer itself (no mod files).
 2. Run the script as in step 3. Wait for ROLLBACK_RESULT=PASS and
    SAFE TO REMOVE SD, then restart the MMI, then engine off and on as in
    step 6 of the installation.
To update: uninstall the old version, then install the new one.

IF SOMETHING GOES WRONG
 - INSTALL_ALL_RESULT=FAIL: the car is in a safe state - finished parts stay,
   the part that failed was undone. The reason is on the line with ERROR.
   Run the script again to retry, or uninstall.
 - ERROR VERSION_GATE: this is not firmware K3346 / MU1438.
   Nothing was changed. This mod is not for your unit.
 - ERROR this car already has the mod ... no record of it: the mod folder
   on the card was deleted or replaced, or another card installed it.
   Nothing was changed. Use the -update.zip of this version: the first
   run rebuilds the backup and removes the old version, the second one
   installs.
 - ERROR REPAIR refused: one of the car's files has changes that are not
   from this mod, so the backup cannot be rebuilt. Nothing was changed.
   Use the card (folder) that installed the mod, or report it with logs.
 - The arrow on the Virtual Cockpit without its dark background tile:
   first collect the logs (below), then engine off, wait about 1 minute,
   engine on. An MMI restart alone does not help.
 - A line containing "do not reboot": do NOT restart the MMI.
   Uninstall straight away (see above).
 - Logs for a bug report: run the script again (no extra file needed).
   They are saved in mod/mib2high-carplay-rgi-v8s-simple/collected/.
   If the installation failed, first unzip the -collect-logs.zip (adds
   COLLECT_LOGS; delete that file afterwards, it blocks installing).
   Logs contain personal data (VIN, phone name, contacts) - do not post
   them publicly.
 - The MMI does not come back after the restart: see "If the MMI does not
   start" in the README on the project page.

Full guide, known limitations and credits: the README on the project's
GitHub page, where you downloaded this zip.


CarPlay na Virtual Cockpit Audi - MHI2 (Harman) K3346 / MU1438
==============================================================

TYLKO DLA oprogramowania MHI2_ER_AUG22_K3346, MU 1438. Instalator sam to
sprawdza i na innym MMI odmawia, niczego nie zmieniajac.

Co jest potrzebne
  - karta SD z M.I.B. (More Incredible Bash), ktora juz dziala w Twoim aucie:
    zielone menu -> m.i.b. sie otwiera,
  - prostownik / zasilacz podlaczony do auta,
  - okolo 20 minut,
  - swieza kopia MMI zrobiona w M.I.B. (M.I.B. -> backup).

INSTALACJA
 1. Rozpakuj -install.zip do KATALOGU GLOWNEGO karty z M.I.B. i zgodz sie
    na zastapienie plikow. Dodaje wlasny folder i podmienia tylko
    mod/command.sh i mod/custom.sh (ten sam instalator pod dwiema nazwami).
 2. Karta do slotu, z ktorego dziala M.I.B. (SD1), zaplon wlaczony,
    prostownik podlaczony.
 3. Zielone menu: m.i.b. -> advanced settings -> run individual script
    (nowsze wersje M.I.B.: Run Custom Script).
 4. Czekaj na INSTALL_ALL_RESULT=PASS, a potem SAFE TO REMOVE SD (kilka
    minut). Nie wylaczaj zaplonu wczesniej.
 5. Restart MMI: przytrzymaj jednoczesnie duze pokretlo MMI + NAV/MAP +
    RADIO przez okolo 10 sekund (A4/A5/Q5 B9; inne modele: tabela
    przyciskow w wiki M.I.B., https://wiki.mibsolution.one/en/MHI2).
 6. Gdy CarPlay wroci: wylacz silnik, odczekaj, az zgasna zegary i MMI
    (okolo 1 minuty), wlacz silnik. Restart MMI nie restartuje Virtual
    Cockpitu, a to tak.
    Podlacz iPhone'a i wlacz trase w nawigacji CarPlay.

ZACHOWAJ TE KARTE
Instalator zapisuje na niej oryginalne pliki auta, w
mod/mib2high-carplay-rgi-v8s-simple/backup/. Odinstalowanie z nich korzysta.
Nie kasuj i nie przenos tego folderu. Kopiujac nowy zip na karte, SCALAJ
foldery, nie zastepuj ich (na Macu "Zastap" kasuje wszystko, co bylo w
starym folderze). Skopiuj tez caly folder mod/ na komputer.
Folder mimo to przepadl (sformatowana karta, zastapiony folder, inna
karta)? Od wersji 2.0.1 -uninstall.zip i -update.zip same odbudowuja
kopie z plikow auta i dzialaja dalej jak zwykle (REPAIR_RESULT=PASS).

ODINSTALOWANIE (i przed kazda aktualizacja)
 1. Rozpakuj -uninstall.zip do katalogu glownego TEJ SAMEJ karty.
    Dodaje plik mod/mib2high-carplay-rgi-v8s-simple/ROLLBACK i sam
    instalator (bez plikow moda).
 2. Uruchom skrypt jak w kroku 3. Czekaj na ROLLBACK_RESULT=PASS i
    SAFE TO REMOVE SD, potem restart MMI, a potem silnik wylacz/wlacz jak
    w kroku 6 instalacji.
Aktualizacja: odinstaluj stara wersje, potem zainstaluj nowa.

GDY COS POJDZIE NIE TAK
 - INSTALL_ALL_RESULT=FAIL: auto jest w bezpiecznym stanie - ukonczone czesci
   zostaja, czesc, ktora sie nie udala, zostala cofnieta. Powod jest w linii
   z ERROR. Uruchom skrypt jeszcze raz albo odinstaluj.
 - ERROR VERSION_GATE: to nie jest oprogramowanie K3346 / MU1438.
   Nic nie zostalo zmienione. Ten mod nie jest dla Twojego MMI.
 - ERROR this car already has the mod ... no record of it: folder moda na
   karcie zostal skasowany albo zastapiony, albo mod instalowala inna
   karta. Nic nie zostalo zmienione. Uzyj -update.zip tej wersji: pierwsze
   uruchomienie odbudowuje kopie i usuwa stara wersje, drugie instaluje.
 - ERROR REPAIR refused: ktorys plik auta ma zmiany, ktore nie sa od tego
   moda, wiec kopii nie da sie odbudowac. Nic nie zostalo zmienione. Uzyj
   karty (folderu), ktora instalowala mod, albo zglos to z logami.
 - Strzalka na Virtual Cockpit bez ciemnego tla kafelka: najpierw zbierz
   logi (nizej), potem wylacz silnik, odczekaj okolo 1 minuty, wlacz
   silnik. Sam restart MMI nie pomaga.
 - Linia z "do not reboot": NIE restartuj MMI. Od razu odinstaluj (wyzej).
 - Logi do zgloszenia bledu: uruchom skrypt jeszcze raz (bez zadnego pliku).
   Zapisuja sie w mod/mib2high-carplay-rgi-v8s-simple/collected/.
   Jesli instalacja sie nie udala, najpierw rozpakuj -collect-logs.zip
   (dodaje COLLECT_LOGS; potem usun ten plik, blokuje instalacje).
   Logi zawieraja dane osobowe (VIN, nazwa telefonu, kontakty) - nie
   publikuj ich.
 - MMI nie wstaje po restarcie: rozdzial "MMI nie uruchamia sie" w README
   na stronie projektu.

Pelna instrukcja, znane ograniczenia i podziekowania: README na stronie
projektu na GitHubie, skad pobrales ten plik.
