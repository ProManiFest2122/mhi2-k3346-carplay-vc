# 05 – Plan krok po kroku (zatwierdzony 2026-10-20)

## Kontekst

Analiza offline jest zrobiona (`docs\01`–`03`). Port projektu edzenis na Twój firmware jest realny, ale część rzeczy da się
sprawdzić tylko w aucie. Prosisz o konkretne kroki dla osoby, która nie programuje, i pytasz o emulator.
W każdym kroku piszę **kto co robi**: **[Ty]** to czynności w aucie lub kopiowanie plików, **[Claude]** to praca na komputerze.
Nic nie trafia do auta bez Twojej zgody, a przed każdym wgraniem dostajesz pliki do obejrzenia.

Twoje odpowiedzi: brak Maca, masz paczkę aktualizacji K3346, brak HUD, masz prostownik/zasilacz.

## Odpowiedź o emulatorze (iscle/qemu-mhi2)

- Działa tylko na **Macu z procesorem Apple (M1–M4)**. Na Windows nie jest wspierany.
- Wymaga **oficjalnej paczki aktualizacji K3346**. Backup M.I.B. jej nie zastąpi, bo M.I.B. nie zrzuca całego systemu MMX.
- Profil dla A4 K3346 to wersja robocza (draft PR #4, fork `yety-ua`). Dochodził do menu głównego, ale **nie emuluje
  CarPlay ani Virtual Cockpitu**.
- **Wniosek: na razie go pomijamy.** Ewentualnie wrócimy do niego, gdybyś kiedyś miał dostęp do Maca (np. pożyczony
  lub wynajęty w chmurze). Przydałby się tylko do jednego: sprawdzenia, czy zmienione HMI uruchamia się bez zawieszenia.
- Twoja paczka K3346 i tak jest bardzo cenna z dwóch powodów:
  (1) zawiera brakujące w backupie części systemu, które przeanalizuję offline;
  (2) to **ostatnia deska ratunku**: pełne wgranie fabrycznego firmware'u z karty SD, gdyby coś poszło bardzo źle.

---

## ETAP 0: przy komputerze, bez auta (ok. 1–2 dni pracy Claude)

**0.1 [Ty]** Skopiuj paczkę aktualizacji K3346 (cały folder lub plik `.7z`) do nowego folderu
`<projekt>\firmware_K3346\`. Oryginalnej kopii nie ruszaj.

**0.2 [Claude]**
- dopisze `firmware_K3346/` do `.gitignore`;
- potwierdzi wersję paczki (MU1438) i porówna jej hash z referencją harman-f (`d859eca6…`, jeśli to ten sam format archiwum);
- tylko do odczytu wypakuje brakujące elementy do `work\` (`displaymanager`, część stage2 MMX). Narzędzie: `tools\qnx_ifs_extract.py`,
  ewentualnie z dodanym dekoderem formatu LZOZ opisanym przez Lukę w `repos\mib2q-carplay-rgi\docs\reference\MHI2_MU1439_PORT.md`.

**0.3 [Claude]** Dokończy raporty `docs\04_mapa_funkcji.md` (teraz jest niedokończony, zastąpię go krótką wersją)
i `docs\05_plan.md`. Plan 05 będzie wersją tego dokumentu.

**0.4 [Claude]** Przygotuje **zestaw diagnostyczny „tylko odczyt”** na kartę SD:
- collector edzenis v1.2 bez zmian, pobrany z jego GitHub Release, z kontrolą hasha
  (`b2f32076…`; w paczce edzenis plik nazywa się `mod/custom.sh`, u nas `mod/collector_v1.2.sh`, zgodnie z `repos\mib2high-carplay-altscreen-rgi\collector\SHA256SUMS.txt`);
- osobno mały skrypt odczytu, który niczego nie zmienia w MMI i zapisuje tylko na kartę: lista procesów (`pidin ar`),
  log systemowy (`sloginfo`), lista kontekstów wyświetlacza (`dmdt gc/gs`), odczyt trybu kombi (`dumb_persistence_reader`,
  klucz `3221356656`).
- **[Ty]** dostajesz oba skrypty do wglądu z opisem linijka po linijce i decydujesz, czy idą na kartę.

## ETAP 1: w aucie, tylko odczyt (ok. 20 min, bez ryzyka dla MMI)

Potrzebne: karta SD z M.I.B. (ta, której używałeś), auto na zapłonie, podłączony prostownik.

**1.1 [Ty]** Na komputerze skopiuj przygotowany przeze mnie folder na kartę SD (instrukcja z obrazkami ścieżek).
Wcześniej zrób kopię pliku `mod\command.sh`, jeśli już jest na karcie.

**1.2 [Ty]** W aucie:
- włóż kartę do slotu SD1;
- podłącz iPhone'a i włącz CarPlay;
- uruchom nawigację w Apple Maps lub Google Maps do dowolnego celu;
- naciśnij kilka razy VIEW na kierownicy i pokręć lewą rolką.

**1.3 [Ty]** Uruchom M.I.B.: wejdź do zielonego menu (GEM) tak samo jak przy robieniu backupu → m.i.b. → *advanced settings* → *run individual script* →
**Run individual script** (M.I.B. 3.6.0 uruchamia wyłącznie plik `/mod/command.sh`).
Czekaj, aż na ekranie pojawi się **„SAFE TO REMOVE”**. Nie wyjmuj karty wcześniej.

**1.4 [Ty]** Wyjmij kartę i skopiuj z niej nowy folder `MIB2_SOFTWARE_COLLECTOR_v1.2_…` (oraz folder skryptu odczytu)
do `work\auto_odczyt_1\`.

**1.5 [Claude]** Analiza:
- jaki jest tryb kombi (LVDS / KombiType);
- czy `dmdt` odpowiada;
- jak `mm-ipod` jest uruchamiany w praktyce;
- czy pliki z auta są identyczne z backupem;
- uzupełnienie brakujących komponentów.

Wynik: krótki raport „co potwierdzone, co zmienić”.

## ETAP 2: przy komputerze, przygotowanie wersji K3346 (kilka dni pracy Claude)

**2.1 [Ty → zgoda]** Narzędzia do budowania. Każde instaluję dopiero po Twojej zgodzie:
- **JDK 8** do zbudowania części Java;
- **Docker Desktop** z gotowym obrazem kompilatora QNX (`luka-dev/qnx65-armv7-toolchain`) do zbudowania części natywnej.

**2.2 [Claude]** Zmiany wyłącznie w kodzie źródłowym edzenis, nigdy w gotowych binarkach:
- adresy dla K3346 według `work\k3346_iap2_offsets.txt`;
- nazwy pakietów Java (`bap` → `bap.navi`) i 9 różnic interfejsu opisanych w `docs\02` (sekcja E);
- ~~poprawka CRLF w `bundles.awk`~~: niepotrzebna (sprostowanie w `docs\02`); zamiast tego automatyczna kontrola końców linii LF w paczce do auta;
- manewry do kombi przez `updateManeuverDescriptor` + `updateExitView`, bo połączona metoda w K3346 to pusta atrapa (zrobione w Javie portu);
- rozmiar ekranu VC 1440×540 i układy B9;
- **blokada wersji**: instalator odmówi pracy, jeśli wykryje firmware inny niż MU1438 lub niezgodne pliki;
- kopia zapasowa każdego zmienianego pliku i plik `ROLLBACK`, tak jak u edzenis.

**2.3 [Claude]** Testy na komputerze: kompilacja z Twoim `work\lsd.jar` i testy instalatora na kopii plików z `work\`
(symulacja, bez auta).

**2.4 [Ty]** Dostajesz paczkę na kartę SD i opis: co dokładnie zmieni w MMI, jak ją cofnąć i jak wygląda pełny ratunek.

## ETAP 3: w aucie, pierwsza instalacja: tylko mapa CarPlay na VC (ok. 1 h)

Najmniejszy możliwy krok: bez strzałek nawigacji, bez telefonu i bez innych dodatków.

**3.1 [Ty]** Prostownik podłączony. Karta z paczką → M.I.B. → *Run individual script* → „SAFE TO REMOVE” → ręczny restart MMI.
Dokładną kombinację przycisków restartu dla Twojego A4 sprawdzimy i opiszemy w instrukcji do tego etapu.

**3.2 [Ty]** Lista testów (dostaniesz ją do odhaczania):
1. MMI wstaje normalnie, CarPlay się łączy.
2. Mapa CarPlay pojawia się na VC w czasie nawigacji.
3. VIEW przełącza układ VC.
4. **Bieg wsteczny:** kamera na MMI, mapa na VC zostaje, po wyjściu z R CarPlay wraca bez ponownego łączenia.
5. Odłączenie i podłączenie iPhone'a.
6. Wyłączenie i włączenie auta.

**3.3 [Ty]** Ponowne uruchomienie skryptu zbiera logi na kartę. Kopiujesz je do `work\auto_test_3\`.

**3.4 Jeśli coś nie tak:**
- **Poziom 1:** na karcie tworzysz pusty plik `ROLLBACK` → *Run individual script* → restart. Przywraca oryginalne pliki.
- **Poziom 2:** przywrócenie plików z backupu M.I.B.
- **Poziom 3:** pełne wgranie Twojej paczki K3346 z karty SD (aktualizacja przez menu serwisowe).

## ETAP 4: wskazówki nawigacji (strzałki, odległość) na VC

Auto nie ma HUD, więc wskazówki idą tylko na VC przez fabryczny kanał BAP. Wymaga to:
- dodatkowego „wrappera” dla procesu `mm-ipod` (brakujący element repo edzenis, patrz `docs\02`);
- zmiany w `/etc/usblauncher_otg.lua` i `/etc/mm/iap2.cfg`, z kopią zapasową.

Instalacja i testy jak w etapie 3, z osobnym rollbackiem.

## ETAP 5: kolejne funkcje (wzór: Luka), po jednej naraz

Kolejność od najprostszej:
1. zoom mapy rolką;
2. okładki albumów na VC;
3. touchpad → poruszanie po CarPlay;
4. telefon CarPlay na VC;
5. popup PDC nie zasłania CarPlay.

Każda funkcja to osobny cykl: przygotowanie → Twoja zgoda → instalacja → testy → ewentualny rollback.
Dźwięk 5.1 (B&O) z projektu Luki dotyczy innego sprzętu (MHI2Q), więc nie planuję go.

---

## Zasady bezpieczeństwa (stałe)

- Przed każdą instalacją: prostownik, świeży backup M.I.B., gotowy rollback, karta z paczką K3346 pod ręką.
- `ORYGINALNY BACKUP! NIERUSZAĆ` pozostaje nietknięty. `backup_do_edycji\` mogę zmieniać, ale pracuję głównie na kopiach w `work\`.
- Nigdy nie uruchamiamy gotowego instalatora K2161 od edzenis na Twoim aucie.
- Pytam Cię przed: instalacją programów, czymkolwiek, co idzie na kartę SD lub do MMI, oraz przed usuwaniem plików.

## Jak sprawdzimy, że każdy etap się udał

- Etap 0: wersja paczki potwierdzona, `displaymanager` wypakowany, raporty 04/05 gotowe.
- Etap 1: na karcie jest `COLLECTION_COMPLETE.txt`, a w raporcie odpowiedzi na pytania o tryb kombi i `dmdt`.
- Etap 2: kod się kompiluje, testy instalatora na kopiach przechodzą, a instalator odmawia na „złym” firmware w symulacji.
- Etap 3–5: lista testów odhaczona, logi zebrane, rollback sprawdzony przynajmniej raz na początku (instalacja → rollback → instalacja).
