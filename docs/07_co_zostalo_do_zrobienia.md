# 07 – Co zostało do zrobienia (stan na 2026-10-20)

Krótkie podsumowanie dla Ciebie i dla każdego, kto przejmie projekt.
Szczegóły techniczne: `docs\02` (różnice K2161 → K3346), `docs\03` (HMI Java), `docs\06` (wyniki z auta),
`docs\08` (gotowa paczka + instrukcja), `docs\09` (ściągawka do auta),
`docs\10` (**jak odtworzyć build od zera** – komendy, zależności, sumy).
Bieżący stan etapów i lista pułapek: `docs\00_postep.md`.

## Co jest ZROBIONE

| Co | Gdzie |
|---|---|
| Analiza backupu i firmware'u: auto to Audi A4 B9, MHI2_ER_AUG22_K3346 / MU1438, bajt w bajt zgodne z referencją MU1438 (harman-f) | `docs\01`, `docs\02` |
| Mapa funkcji HMI i plan portu | `docs\03`, `docs\04`, `docs\05` |
| Odczyt z auta (tylko odczyt): tryb kombi = **LVDS**, konteksty kombi 72/77 jak u edzenisa, `dmdt gc` działa, pliki w aucie = backup (150/150) | `docs\06` |
| Toolchain: Docker z GCC 4.9.4 dla QNX/ARM, JDK 8, obejście 32-bitowych binutils | `docs\10`, `tools\qnx_build.sh` |
| Port na K3346: Java + 3 binarki natywne, zbudowane i zweryfikowane | `port_k3346\`, `docs\10` |
| `v8l-dsitracer-capture` wyszedł **bit w bit identyczny z wydaniem edzenisa** – dowód, że toolchain to ten sam kompilator | `docs\10` |
| Offsety hooków sprawdzone przeciw driverowi iAP2 **z auta** – 17/17 PASS | `tools\verify_port_offsets.py` |
| Instalator z **blokadą wersji**, rollbackiem i auto-rollbackiem | `port_k3346\altscreen\deployment\custom.sh` |
| `collect()` zbiera też `dmdt-gc.txt`; marker `COLLECT_LOGS` wymusza zbieranie logów nawet po odmowie bramy i po rollbacku | `custom.sh`, `docs\09` |
| Symulacja instalatora bez auta – **62 OK, 0 FAIL** pod `dash` i pod `mksh` (rodzina ksh, jak QNX) | `work\sim_app\sim_result*.txt` |
| Gotowa paczka na kartę SD (16 plików, 469 KB, z gotowymi znacznikami `ZNACZNIKI\`) + instrukcja + ściągawka | `work\sd_etap3\`, `docs\08`, `docs\09` |
| Przegląd poprawności Etapów 2 i 3 – ryzyka niewidoczne dla kompilatora i symulacji sprawdzone na binariach K3346 | `docs\11` |
| Karta ratunkowa na bootloop HMI (działa bez zielonego menu, przez natywny tryb aktualizacji) – symulacja 27 OK, 0 FAIL | `work\sd_ratunek\`, `docs\12` |

## Co jest DO ZROBIENIA

### Teraz: Etap 3 – pierwsza instalacja w aucie

Wymaga auta, prostownika i Twojej zgody. Paczka jest **wydana i czeka**.

- **Do auta: `docs\09_sciagawka_auto.md`** – jedna strona na telefon, z drzewem decyzyjnym
  „kiedy przerwać" i właściwą kolejnością czynności.
- Pełna dokumentacja (co dokładnie zmienia w MMI, trzy poziomy rollbacku): `docs\08`.

Cel: potwierdzić, że mapa CarPlay pojawia się na Virtual Cockpit, że VIEW przełącza układ
i że bieg wsteczny nie zabija sesji CarPlay.

**Najpierw test karty ratunkowej na zdrowym aucie** (`docs\09` punkt A). Instalacja tylko wtedy,
gdy `RATUNEK_LOG.txt` pokaże `WYNIK=OK`. Paczkę instalacyjną kopiuje się **na kartę z M.I.B.**

**Kolejność jest ważna** (dwie pułapki opisane w `docs\00_postep.md`):
testy → zbierz logi (`run-01`) → wyłącz/włącz auto → zbierz logi (`run-02`) → ewentualny rollback.
Logi zawsze **przed** rollbackiem i **przed** wyłączeniem auta.

Po teście: cały katalog `collected\` z karty → `work\auto_test_3\`, potem analiza i decyzja
o dalszych funkcjach. Porównanie `run-01` z `run-02` powie przy okazji, czy `/tmp` w aucie
przeżywa wyłączenie zapłonu – tego dziś nie wiemy.

### Potem: Etap 4 – wskazówki nawigacji (RGI) na VC

Auto nie ma HUD, więc strzałki i odległość muszą iść na VC przez fabryczny kanał BAP. Brakuje:

- **wrappera dla procesu `mm-ipod`** – brakujący element repo edzenisa (patrz `docs\02`);
  `mm-ipod` startuje z `/etc/usblauncher_otg.lua`, komenda potwierdzona w `docs\06`
- aktywacji grupy **0x52** w pamięci (w stocku jest pusta – potwierdzone: `msgtable: group52 stock empty`)
- wpisów w `/etc/mm/iap2.cfg`, z kopią zapasową

Osobna paczka, osobny rollback.

### Dalej: Etap 5 – kolejne funkcje (wzór: luka-dev), po jednej naraz

1. zoom mapy rolką (`changeMapZoomLevel`)
2. okładki albumów na VC
3. touchpad → D-pad
4. telefon CarPlay na VC
5. popup PDC / bieg wsteczny nie zasłaniają CarPlay

Dźwięk 5.1 (B&O) z projektu Luki dotyczy MHI2Q – nie planujemy.

## Czego wciąż NIE WIEMY

- Czy geometria **1440×542** jest właściwa dla panelu VC w B9 (bywa opisywany jako 1440×540).
  Nie da się ustalić offline – `dmdt gs` i `gd` nic nie zwróciły. Oba warianty są w paczce,
  zmiana to edycja pliku tekstowego, bez przebudowy. Rozstrzygnie test nr 8 z `docs\08`.
- Jak zachowa się bieg wsteczny z aktywną mapą na VC (test nr 5 z `docs\08`).
- Sygnał przycisku VIEW i rolki – odczytywany z DSITracera (porty 1798/20991), który ta paczka
  włącza. Do potwierdzenia w aucie.
- Czy CarPlay łączy się kablem, czy adapterem bezprzewodowym (w logu było 8× `connectDevice … CARPLAY`).

## Zasady bezpieczeństwa (stałe)

- Przed każdą instalacją: prostownik, świeży backup M.I.B., gotowy rollback, karta z paczką K3346 pod ręką.
- `ORYGINALNY BACKUP! NIERUSZAĆ` pozostaje nietknięty; praca na kopiach w `work\`.
- Nigdy nie uruchamiamy instalatora K2161 od edzenisa na tym aucie – blokada wersji w naszym
  instalatorze istnieje właśnie po to.
- Zmieniamy kod źródłowy i przebudowujemy; nigdy nie łatamy binarek ani bytecode'u.
- Backup i dane z auta zawierają dane osobowe (VIN, kontakty, współrzędne GPS w `iap2.log`) – nie publikować.
