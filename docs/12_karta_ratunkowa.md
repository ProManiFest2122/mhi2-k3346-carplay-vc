# 12 – Karta ratunkowa na bootloop HMI (2026-10-20)

Pisane dla: Ciebie oraz kogoś, kto przejmie projekt i musi wiedzieć, **dlaczego** ta karta ma
zadziałać i czego nie wiadomo.

Paczka: `work\sd_ratunek\` (M.I.B. 3.6.0 + ratunek, 240 plików, 18 MB).
Instrukcja na karcie: `RATUNEK_README.txt`. Symulacja: `work\sim_app\sim_rescue_result.txt`.

## Problem, który rozwiązuje

Zwykły rollback (plik `ROLLBACK` → *run individual script*) i backup M.I.B. wymagają M.I.B.,
a M.I.B. działa z zielonego menu (GEM). **GEM jest częścią Javy HMI** — `lsd.sh:218-225` ustawia go
jako opcje tej samej maszyny wirtualnej (`-Dde.audi.gem.*`, `-DenableGEM=true`). Jeśli HMI nie
wstaje, nie ma GEM, M.I.B. ani rollbacku. Wcześniejsza wersja `docs\09` tego nie mówiła.

Ryzyko bootloopu oceniam jako **niskie, nie zerowe** (`docs\11`): JAR nie przesłania żadnej klasy
HMI, lista pakietów po przełączeniu jest kompletna (160/160), `Activator.start()` praktycznie nie
ma czym rzucić. Nieznane: reakcja frameworka LSD na błąd w `start()`.

## Jak działa — i na czym to opieram

```
Swdlautorun.txt w katalogu głównym karty
  → MIBRoot (natywny) — moduł CASWDLAutoRun — sam startuje tryb aktualizacji
    → M.I.B. 3.6.0 metainfo2.txt (podpisany) → Launcher/final/finalScript.sh (SHA1 w metainfo)
      → /bin/ksh ${2}/Launcher/stuff.sh ${1} ${2}
        → [dodane] /bin/ksh ratunek/ratunek.sh      ← nasz ratunek, najpierw
        → apps/backup -b, apps/launcher -all         ← oryginał M.I.B., bez zmian
```

| Fakt | Dowód |
|---|---|
| Autorun obsługuje **natywny** `MIBRoot`, nie Java HMI | ciągi `CASWDLAutoRun.cpp`, `/SwdlAutoRun.txt`, `SWDL AutoRun is started` w `MIBRoot` z IFS K3346; 0 trafień w `lsd.jar` |
| Silnik aktualizacji to osobny proces | `pidin` z auta: `./bin/apps/swdlclient -sm-name swdlclient` |
| Autorun **już zadziałał w tym aucie** | instalacja M.I.B. 3.6.0 przez samo włożenie karty |
| Autorun odpali ponownie przy zainstalowanym M.I.B. | launcher przy każdym przebiegu kasuje zapis wersji: `rm .../SWDL/FileCopyInfo/Launcher.info` (`apps/launcher:60`) |
| `stuff.sh` to zaprojektowane miejsce na własny kod | komentarz w nim: „no need to touch metainfo2.txt as this is outside of its reach" |
| Podpis i suma kontrolna nienaruszone | `finalScript.sh` SHA1 `ba391875…` = `FinalScriptChecksum` w `metainfo2.txt`; `make_rescue_card.sh` sprawdza, że zmieniony jest **tylko** `stuff.sh` |
| Skrypt działa po stronie RCC | `GLOBALS`: `TMP=/net/rcc/dev/shmem`; komentarz „debug do logu RCC na UART" — dlatego ścieżki `/net/mmx/...` i `on -f mmx` |
| Wzór potwierdzony w aucie | luka-dev `rescue_MoreIncredibleBash/` (issue #24, 2026-09-26) — identyczny objaw, MHI2Q, M.I.B. 3.7.1 |

Wersja M.I.B. na karcie to **3.6.0, ta sama co w aucie** — autorun przeinstalowuje M.I.B., więc
nie chcemy przy okazji podnieść wersji (luka używał 3.7.1; nowsze M.I.B. uruchamiają `custom.sh`
zamiast `command.sh`).

## Test 1 w aucie (2026-10-20) — mechanizm działa, pierwsza wersja skryptu nie

**Co zadziałało:** autorun uruchomił tryb aktualizacji, M.I.B. wykonał `stuff.sh` do końca —
`backup -b` zapisał backup (`MU1438-version.txt`, `storage1/2.raw`, 11:22–11:24), launcher przeszedł
wszystkie kroki (log M.I.B.: `RCC-launcher` → `Swdlautorun.txt disabled` → … → `GEM activated`),
karta się rozbroiła. Auto w stanie jak po instalacji M.I.B.

**Co nie zadziałało:** `RATUNEK_LOG.txt` w ogóle nie powstał. Dwie przyczyny, obie potwierdzone:

1. **Karta jest tylko do odczytu, dopóki M.I.B. jej nie odblokuje.** `apps/mounts:20` robi
   `mount -uw $VOLUME`, ale wołają go dopiero `backup`/`launcher` (przez `config/GLOBALS`) — po nas.
   A w powłoce **polecenie z nieudanym przekierowaniem nie wykonuje się wcale**. Ślad `ksh -x` w
   symulacji: `+ >>/sd/RATUNEK_LOG.txt` → `Permission denied` → `r_once` (z `rm`/`cp`) pominięty.
2. **Ubogi `PATH` na RCC.** Każdy skrypt M.I.B. zaczyna od `export PATH=…:/net/mmx/bin:…`
   (`apps/launcher:6`), bo narzędzia leżą na MMX. Pierwsza wersja `PATH` nie ustawiała.

Skutek w prawdziwym bootloopie dla wersji 1 (symulacja w warunkach z auta): przy karcie RO usunięte
byłyby tylko znacznik i JAR (jedyna linia bez przekierowania — `on -f mmx`), **bez** `traceConfig`
i bez logu; przy ubogim `PATH` — **nic**. Symulacja wcześniej tego nie łapała, bo atrapa karty była
zawsze zapisywalna, a kontener miał pełny `PATH`.

**Poprawka (wersja 2):** `PATH`/`LD_LIBRARY_PATH` dokładnie jak `apps/launcher:6-7`; `mount -uw`
nośnika przed zapisem; działania nigdy za przekierowaniem; log najpierw w RAM
(`/net/rcc/dev/shmem/ratunek.log`, czyli `$TMP` M.I.B.), na kartę na końcu; drugie wywołanie na
końcu `stuff.sh` (po `launcher -all`, gdy karta jest na pewno odblokowana) dopisuje log, plus
linia-ślad `STUFF_KONIEC nosnik=… ratunek.sh=jest|brak` pisana wprost przez `stuff.sh` — przy
następnym teście zawsze coś się zapisze. Log zawiera diagnostykę: argumenty, wynik `mount -uw`,
`type rm cp mount on …`.

## Test 2 w aucie (2026-10-20, 11:51) — WYNIK=OK, mechanizm potwierdzony w całości

Log (`work\test_ratunku_2\RATUNEK_LOG.txt`): `nosnik=/net/mmx.mibhigh.net/fs/sda0` (pełna nazwa
qnet), `mount -uw` obu ścieżek OK, PRZED/PO bez znacznika i JAR-a (czyste auto), `traceConfig
przywrocony`, `WYNIK=OK`, `STUFF_KONIEC … ratunek.sh=jest`, log dopisany dokładnie raz. M.I.B. jak
w teście 1. Cały łańcuch sprawdzony w aucie: autorun → tryb aktualizacji → M.I.B. → `ratunek.sh`
na RCC → odblokowanie karty → MMX przez `/net/mmx` → działania → log.

Korekty względem analizy po teście 1 (z diagnostyki w logu):
- **`PATH` nie był główną przyczyną.** `rm`, `cp`, `mv`, `cat`, `mount`, `on`, `ls`, `sleep` są
  w `/bin` na RCC; tylko `sync` pochodzi z `/net/mmx/bin`. Przyczyną braku logu w teście 1 była
  **karta tylko do odczytu** + przekierowanie blokujące polecenie. Ustawienie `PATH` jak w M.I.B.
  zostaje (bez niego nie ma `sync`, a to tani bezpiecznik).
- **Na czystym aucie zmienia się czas modyfikacji `traceConfig.properties`** (2019 → 01.01.1970,
  bo w trybie aktualizacji zegar MMX nie jest ustawiony). Treść jest identyczna (kopia z auta,
  SHA-256 sprawdzone przy budowie karty), tryb zachowany (`rwxrwxrwx`, czyli 777 – listing
  firmware'u podawał prawdę). Bez wpływu na działanie; wcześniejsze „bit w bit bez zmian" dotyczyło
  treści i trybów, nie czasu.

**Warunek przed instalacją moda spełniony** – po ponownym uzbrojeniu karty.

Nadal niesprawdzone: zachowanie w trakcie prawdziwego bootloopu HMI (potwierdzone tylko u luka-dev,
MHI2Q). Argument za: autorun (`MIBRoot`) i `swdlclient` są natywne, mod ich nie dotyka.

## Co robi `ratunek.sh`

Tylko to, co HMI czyta przy starcie:

1. usuwa `lsd/dsitracer_activated` → `lsd.sh:265` wraca do listy pakietów z `lsd.jxe`,
   `lsd.sh:387` zdejmuje `DSITracer.jar` z bootclasspath;
2. usuwa `lsd/jars/mib2high-carplay-rgi.jar` → bootclasspath jak przed instalacją;
3. przywraca fabryczny `traceConfig.properties` z karty (HMI czyta go zawsze).

Szczegóły wykonania:
- **ponawianie** do 6 razy co 10 s — w bootloopie MMX może się akurat restartować; sukces liczy
  się tylko, gdy katalog `lsd` na MMX jest osiągalny (inaczej „pliku nie ma" byłoby fałszem);
- na koniec jeszcze raz przez `on -f mmx` na samym MMX;
- `traceConfig`: najpierw kopia z karty do pliku tymczasowego (przerwany odczyt nie dotknie auta),
  potem przepisanie treści do istniejącego pliku — **tryb dostępu zostaje**; pusta kopia na karcie
  nigdy nie nadpisze pliku w aucie;
- log `RATUNEK_LOG.txt` na karcie: stan przed, po i `WYNIK=OK` albo `WYNIK=NIEPEWNY`.

Celowo **nie** rusza `smartphone_integrator.json` ani binarek w `/mnt/app/root/` — nie blokują
startu HMI. Resztę cofa normalny rollback przez GEM, który jest z tym zgodny (symulacja R2).

## Trzy rzeczy, które łatwo przeoczyć

1. **To musi być osobna karta.** `Swdlautorun.txt` uruchamia tryb aktualizacji przy każdym włożeniu.
2. **Po każdym użyciu karta sama się rozbraja.** Launcher zmienia `Swdlautorun.txt` na
   `_Swdlautorun.txt` (`apps/launcher:29-36`, „żeby uniknąć pętli"). **Po teście trzeba ją ponownie
   uzbroić**, inaczej przy prawdziwym bootloopie nic nie zrobi.
3. **Launcher wczytuje `mod/command.sh` z karty** (`apps/launcher:117-118`, przez `.`). Na karcie
   ratunkowej go nie ma. Ale **karta instalacyjna nie może być uzbrojona** (`Swdlautorun.txt`):
   wtedy tryb aktualizacji wczytałby nasz instalator w niesprawdzonym kontekście. Po instalacji
   M.I.B. Twoja karta jest rozbrojona (launcher to zrobił) — wystarczy tego nie zmieniać.

## Weryfikacja na komputerze — 45 OK, 0 FAIL (pod `mksh`, rodzina ksh jak QNX)

Wersja 2. Doszły scenariusze z warunkami z auta (uruchamiane jako zwykły użytkownik przez
`setpriv` — root ignoruje prawa zapisu; symulacja zatrzymuje się, jeśli przełączenie nie działa):
**R6** karta RO odblokowywana dopiero przez M.I.B., **R7** ubogi `PATH` jak na RCC, **R8** karta
w ogóle się nie odblokowuje (działania i tak wykonane, log zostaje w RAM). Dowód, że testy łapią
błąd z auta: te same R6–R8 na wersji 1 (`work\test_ratunku_1\sd_ratunek_v1`) → 10 FAIL,
w tym dokładnie objaw z auta (brak logu, `traceConfig` nieprzywrócony).

Tabela z wersji 1 (R1–R5) obowiązuje nadal:

Testowany jest prawdziwy `stuff.sh` i `ratunek.sh` z karty; atrapy to tylko `apps/backup`,
`apps/launcher` M.I.B. oraz `on`/`mount`/`sleep` (nie istnieją w Linuksie).

| Test | Wynik |
|---|---|
| R1 czyste auto (test przed instalacją) | drzewo plików **bit w bit bez zmian** (treść, tryby, katalogi); ratunek przed backupem i launcherem; brak plików tymczasowych |
| R2 po instalacji | znacznik i JAR usunięte, `traceConfig` fabryczny z zachowanym trybem, `jars/` = dokładnie fabryczne; potem pełny rollback → **pliki identyczne z przed-instalacyjnymi** |
| R3 pusta kopia na karcie | `traceConfig` w aucie nietknięty, ostrzeżenie w logu, reszta ratunku wykonana |
| R4 MMX nieosiągalny | 6 prób, `WYNIK=NIEPEWNY` (nie udaje sukcesu), M.I.B. działa dalej |
| R5 brak `ratunek.sh` | zmieniony `stuff.sh` zachowuje się jak oryginał |

## Czego to nie gwarantuje

- Test na zdrowym aucie dowodzi, że autorun, tryb aktualizacji i skrypt działają w **tym** aucie
  z **tą** wersją M.I.B. **Nie** dowodzi, że zadziałają w trakcie bootloopu HMI — to potwierdzono
  tylko u luki (MHI2Q). Argument za: autorun i `swdlclient` są natywne, mod nie dotyka `MIBRoot`.
- Dokładny przebieg ekranów w bootloopie (u luki: logo ↔ czarny, twardy restart, czerwone menu,
  Continue → Cancel) może się różnić.
- **Twardy restart MMI dla A4 B9 z MIB2 High nie jest potwierdzony.** Pewne: bezpiecznik albo
  akumulator (wiki M.I.B.). Do sprawdzenia podczas testu: przytrzymanie On/Off ≥ 10 s — tak opisuje
  biuletyn Audi, ale dla nowszych MMI (MIB2+).

## Druga droga: telnet (opcjonalna, wymaga przejściówki)

Firmware tego auta ma w pełni skonfigurowany dostęp przez przejściówkę D-Link DUB-E100:
`inetd.conf` — `telnet ... root /usr/sbin/telnetd`; `usblauncher_otg.lua` — `devnp-asix.so` dla
VID `0x2001`, PID `0x3c05` / `0x1a02`; `extnet.sh` — MMI `172.16.250.248/24`, oczekiwany host
`172.16.250.250`.

**Hasło roota jest znane.** Lista haseł dołączona do M.I.B. (`2022_06_MHI2_Password_List_V4.0.pdf`,
na obu kartach) ma wiersz dla `MHI2_ER_AUG22_K3346` / MU1438. Hash z tej listy jest **identyczny**
z hashem roota w `work\system\shadow` z Twojego auta (sprawdzone porównaniem napisów, nic nie
łamane). Hasła celowo nie przepisuję do dokumentacji — jest w PDF-ie, wiersz MU 1438 / K3346, nie
„Emergency RCC".

Jak użyć przy bootloopie (zamiast albo po karcie ratunkowej):
1. Na laptopie skopiuj `ZNACZNIKI\ROLLBACK` do `mod\mib2high-carplay-rgi-v8s-simple\` na
   **karcie instalacyjnej**, włóż ją do SD1.
2. Przejściówka D-Link do portu USB w aucie, kabel do laptopa; laptop: adres `172.16.250.250`,
   maska `255.255.255.0`.
3. Telnet (np. PuTTY) na `172.16.250.248`, port **23** (MMX), login `root`, hasło z PDF-u.
4. `mount -uw /net/mmx/fs/sda0` i `sh /net/mmx/fs/sda0/mod/command.sh` — to ten sam rollback co
   z GEM (`ROLLBACK_RESULT=PASS`), tylko bez HMI.

**Nieprzetestowane w tym aucie.** Jeśli kupisz przejściówkę, sprawdź połączenie i logowanie na
zdrowym aucie — tak samo jak kartę ratunkową. Bez tego to tylko wiedza z firmware'u i z dokumentacji.

## Sprawdzenie przygotowanej karty (2026-10-20)

`tools\sprawdz_karte.ps1`: FAT32, 238/238 sum zgodnych, uzbrojona, nieużyta. Dodatkowo porównanie
z Twoją kartą M.I.B., którą autorun już raz zadziałał w aucie (`ORYGINALNY BACKUP! NIERUSZAĆ\mib\`):
**232 z 235 plików M.I.B. bajt w bajt identyczne**, w tym podpisany `metainfo2.txt`, `finalScript.sh`,
`hashes.txt`, `Launcher/version/MIB`, `apps/launcher`, `apps/backup`. Różnice dokładnie zamierzone:
`Launcher/stuff.sh` (wywołanie ratunku), `_Swdlautorun.txt` → `Swdlautorun.txt`, brak `WMPInfo.xml`
(artefakt Windows Media Playera). Tryb aktualizacji dostanie więc ten sam pakiet, który już zadziałał.
