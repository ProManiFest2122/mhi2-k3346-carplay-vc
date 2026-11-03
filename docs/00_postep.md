# Postęp

## Etap analizy (bez zmian w aucie)

- [x] 1–8. Analiza offline → docs\01–05

## Plan (docs\05_plan.md)

- [x] Etap 0: paczka K3346 w `firmware_K3346\` (zweryfikowana), raporty 04/05, zestaw diagnostyczny
- [x] Etap 1: odczyt z auta (tylko odczyt) → docs\06_odczyt_z_auta_1.md
- [x] **Etap 2: przygotowanie wersji K3346 – ZAKOŃCZONY** → docs\08_paczka_etap3_instrukcja.md
  - [x] Narzędzia: JDK 8 (przenośny, `repos\jxe2jar\jvms\`), Docker + obraz kompilatora `qnx65-armv7-toolchain:4.9`
  - [x] Katalog roboczy `port_k3346\` ze źródłami edzenis (końce linii LF)
  - [x] Moduł Java dostosowany do K3346, skompilowany i zweryfikowany (nietestowany w aucie)
  - [x] Adresy K3346 wpisane w źródła natywne, sprawdzone `tools\verify_port_offsets.py` – **17/17 przeciw driverowi z auta**
  - [x] Skrypty instalacyjne edzenis sprawdzone na plikach K3346 (działają)
  - [x] Oficjalna paczka AltScreen v1.0 pobrana i zweryfikowana (`work\downloads\altscreen_v1.0\`)
  - [x] **Kompilacja części natywnej** – 3 binarki QNX ARM: `libmib2high-carplay-rgi.so`,
        `vc_render`, `v8l-dsitracer-capture`. Ta ostatnia wyszła **bit w bit identyczna
        z wydaniem edzenisa** → dowód, że toolchain to ten sam kompilator (GCC 4.9.4).
  - [x] **Geometria ekranu VC** – ustalone, że geometria jest czytana z plików konfiguracyjnych
        w czasie działania, więc **nie wymaga rekompilacji**. Domyślnie sprawdzone 1440×542,
        wariant B9 1440×540 dołączony w `alt-profiles\`. Wszystkie 4 widoki mieszczą się w obu.
  - [x] **Instalator K3346 z blokadą wersji i rollbackiem** – brama na `HMI_Version` +
        rozmiary `ipod-drvr-iap2.so` i `lsd.jxe` (na MMI nie ma narzędzi do sum kontrolnych);
        obejście `MIB_FORCE=1` działa **tylko z powłoki** (symulacja) – z M.I.B. nie da się ustawić
        zmiennej, więc w aucie bramki nie da się obejść
  - [x] **Testy instalatora na komputerze (symulacja)** – **62 OK, 0 FAIL**, zarówno pod `dash`,
        jak i pod `mksh` (powłoka z rodziny ksh, jak QNX `/bin/sh`)
        (`work\sim_app\sim_result.txt`, `sim_result_mksh.txt`): instalacja, 3 scenariusze odmowy
        z zerem zapisów, rollback i auto-rollback bit w bit, zachowanie CRLF, tryb COLLECT,
        wymuszone zbieranie logów, procedura z gotowymi znacznikami `ZNACZNIKI\`
  - [x] **Przegląd poprawności Etapów 2 i 3** → `docs\11_przeglad_etapu_2_i_3.md`
  - [x] **Paczka na kartę SD + instrukcja** – `work\sd_etap3\` (16 plików, 469 KB),
        sumy w `SHA256SUMS.txt` zweryfikowane
- [ ] **Etap 3: pierwsza instalacja – tylko mapa CarPlay na VC (+ test biegu wstecznego)**
      ← **W TOKU: paczka wydana, czeka na test w aucie.**
      Dokumentacja: docs\08. **Do auta: docs\09_sciagawka_auto.md** (jedna strona na telefon).
  - [x] `collect()` rozszerzony o `dmdt-gc.txt` (stan kontekstów wyświetlacza, tylko odczyt)
  - [x] Ściągawka do auta z drzewem decyzyjnym „kiedy przerwać”
  - [x] **Marker `COLLECT_LOGS`** – wymusza zbieranie logów niezależnie od stanu instalacji
        (także po odmowie bramy i po rollbacku). `ROLLBACK` ma nad nim pierwszeństwo, żeby
        zapomniany marker nie zablokował ścieżki ratunku
  - [x] **Poprawiona kolejność w ściągawce**: logi PRZED rollbackiem i PRZED wyłączeniem auta
        (patrz „Pułapki” niżej)
  - [x] **Karta ratunkowa na bootloop HMI** → `docs\12`, `work\sd_ratunek\`. Rollback przez GEM
        nie działa, gdy HMI nie wstaje (GEM to część Javy HMI). Karta: M.I.B. 3.6.0 + `Swdlautorun.txt`
        + `ratunek.sh` (usuwa `dsitracer_activated` i JAR, przywraca `traceConfig`), uruchamiana
        przez natywny tryb aktualizacji. Symulacja **27 OK, 0 FAIL** pod `mksh`
  - [x] Instrukcja przygotowania kart (`docs\13`) + skrypt sprawdzający `tools\sprawdz_karte.ps1`
        (tylko odczyt; 10 przypadków testowych – poprawne karty GOTOWA, każdy błąd wykryty)
  - [x] **Karta instalacyjna** przygotowana i sprawdzona (2026-10-20): `sprawdz_karte.ps1` → GOTOWA
        (FAT32, 15/15 sum, M.I.B. obecny, autorun rozbrojony). M.I.B. na karcie = V3.6.0, a
        `individual.sh`, `apps/launcher`, `Launcher/stuff.sh` bajt w bajt jak analizowane wydanie.
        Karta jest w całości zabezpieczona: `ORYGINALNY BACKUP! NIERUSZAĆ\mib\` = pełna kopia karty
        sprzed Etapu 1 (2106 plików, wszystkie na karcie bez zmian ścieżki i rozmiaru), a wszystko
        dodane później jest w `work\sd_odczyt_1\` (Etap 1) i `work\sd_etap3\` (paczka).
        **Po instalacji `mod\` z karty skopiować na dysk** – wtedy pojawi się tam kopia oryginałów auta.
  - [x] **Karta ratunkowa** przygotowana i sprawdzona (2026-10-20): GOTOWA (FAT32, 238/238 sum,
        uzbrojona, nieużyta). 232/235 plików M.I.B. bajt w bajt jak Twoja karta M.I.B., którą autorun
        już raz zadziałał (w tym podpisany `metainfo2.txt`) – różnice tylko zamierzone.
  - [x] **Telnet jako druga droga ratunku** – hash roota z auta zgodny z listą haseł M.I.B. dla
        K3346 (hasło w PDF na karcie, nie przepisane do dokumentacji). Wymaga przejściówki D-Link,
        nieprzetestowane w aucie – `docs\12`
  - [x] **Test 1 karty ratunkowej w aucie** (2026-10-20): **mechanizm działa** (autorun → tryb
        aktualizacji → M.I.B. `stuff.sh` → backup + launcher, karta rozbrojona), **ale wersja 1
        skryptu nie zostawiła logu**. Przyczyny (`docs\12`): karta RO do czasu `mount -uw` przez
        M.I.B. + nieudane przekierowanie blokuje polecenie; brak `PATH` na RCC. W bootloopie wersja 1
        ratowałaby połowicznie albo wcale. Dowody: `work\test_ratunku_1\`.
  - [x] **Karta ratunkowa wersja 2** – poprawka, symulacja **45 OK, 0 FAIL** (nowe R6–R8 w warunkach
        z auta; na wersji 1 te same testy dają 10 FAIL); `sprawdz_karte.ps1` wykrywa „użyta bez logu"
  - [x] **Karta ratunkowa wersja 2** przygotowana i sprawdzona (2026-10-20): GOTOWA (FAT32, 238/238
        sum zgodnych z wersją 2, uzbrojona, czysta – bez śladów testu 1)
  - [x] **Test 2 karty ratunkowej w aucie** (2026-10-20 11:51): **WYNIK=OK** – cały łańcuch ratunku
        sprawdzony w aucie. Korekta: przyczyną z testu 1 była karta RO, nie `PATH` (narzędzia są w
        `/bin` RCC, z MMX tylko `sync`). Log: `work\test_ratunku_2\`, `docs\12`.
  - [x] **Karta ratunkowa uzbrojona ponownie** i sprawdzona: GOTOWA (FAT32, 238/238, `Swdlautorun.txt`,
        w logu historia testu 2 z `WYNIK=OK`)
  - [x] **Instalacja moda w aucie** (2026-10-20, ~20 s): `VERSION_GATE=PASS` (HMI_Version OK,
        `ipod-drvr-iap2.so=739925` OK), 7× `INSTALLING`, `INSTALL_RESULT=PASS`, `NAVIGNORE=UNTOUCHED`.
        **Odstępstwo:** sprawdzian `LSD.JXE` pominięty („nieczytelny") – **mój błąd ścieżki**: w aucie
        plik to `/ifs/lsd.jxe` (małe litery: collector `/net/mmx/ifs/lsd.jxe`, HMI `-jxe:/ifs/lsd.jxe`),
        w bramce wpisane `/ifs/LSD.JXE` (tak pokazywał Windows), a IFS QNX rozróżnia wielkość liter.
        Bez wpływu na bezpieczeństwo: dwa krytyczne sprawdziany przeszły, a zgodność `lsd.jxe` z JAR-em
        potwierdzona w Etapie 1 (SHA-256 `e43d80e7…`). Lekcja: sprawdzian „pomiń, gdy nieczytelny"
        po cichu staje się pusty przy złej ścieżce. **Poprawione** w paczce (`/ifs/lsd.jxe`, rezerwowo
        `/net/mmx/ifs/lsd.jxe`), symulacja 62/62 dash+mksh. Na karcie do podmiany `mod\command.sh`
        (sha256 `f3e9b690…`); na karcie wciąż stary (`52b3ff84…`) – podmiana tylko za zgodą użytkownika.
  - [x] **Pierwsze testy + logi** (2026-10-20, `run-01`/`run-02` → `work\auto_test_3\karta_mod\`) –
        szczegóły **`docs\14_wyniki_etap3.md`**: mapa na VC działa w obu układach, VIEW przełącza,
        renderer 15–23 kl/s bez błędów; **przy wstecznym mapa na VC gra dalej**, CarPlay na MMI wraca
        w chwili, gdy auto wyłącza kamerę (o czasie decyduje fabryczny system parkowania, nie mod).
        Zerwania CarPlay (`treat as cable removed`) były **już przed modem** (log Etapu 1) – kabel/USB.
  - [x] **Testy użytkownika** (2026-10-20): wsteczny = fabryczne zachowanie (przycisk PDC przywraca
        CarPlay; po kilkunastu metrach wraca sam – tak było przed modem); **rolka zoomuje mapę na VC
        w obu układach**; CarPlay i mapa wracają po odłączeniu iPhone'a i po wyłączeniu/włączeniu.
        **Etap 3 zaliczony.**
  - [x] Zdjęcia przeanalizowane (`docs\14`, sekcja „Zdjęcia"): geometria OK; karta instrukcji
        zasłonięta tylko przez fabryczne ostrzeżenie „otwarte drzwi/maska" → geometria bez zmian.
        Kabel USB uszkodzony, zerwania znane sprzed modu – nie zajmujemy się tym.
  - [ ] Kiedyś **[Ty]**: zimny start (auto zamknięte, MMI uśpione) + logi `run-03`
- [ ] Etap 4: strzałki / odległość na VC (RGI) – **`docs\15_etap4_rgi.md`**
  - [x] Paczka `work\sd_etap4\` (2026-10-20): osobna biblioteka RGI bez AltScreen
        (`build_rgi_ipod.sh`; konstruktor AltScreen kasowałby znacznik sesji VC w `mm-ipod`),
        launcher `mm-ipod-rgi.sh` (LD_PRELOAD + exec), 1 linia w `usblauncher_otg.lua`,
        `[ident]` w `iap2.cfg`; zmiany w `/mnt/system/etc` (bo `/etc` to kopia w RAM-dysku,
        działa po restarcie). Osobna transakcja `INSTALL_ETAP4` / `ROLLBACK_ETAP4` z kopią
        `backup\pre-etap4\`; pełny ROLLBACK cofa oba. Biblioteka v8s bit w bit bez zmian.
        Symulacja 115/115 (dash, mksh, `luac -p`), offsety ALL PASS.
  - [x] Wgrane w aucie 2026-10-20 13:35, PASS; logi `work\auto_test_4\karta_mod\` (run-03)
  - [x] Analiza: RGI działa end-to-end (345× 0x5201, 38× 0x5202, BAP LSG 50 FctID 23 `13:64` /
        `13:-64` na szynie), odległość/ulica/flaga na VC OK. **Strzałka w KDK zawsze prosto:**
        kontekst 77 zawiera displayable 20 = obraz manewru z fabrycznej nawigacji MMI (nie z BAP).
        Szczegóły `docs\15`, sekcja „Wynik w aucie”.
  - [x] Decyzja użytkownika (2026-10-20): **strzałkę KDK odkładamy** – podpowiedzi pokazuje karta
        Google Maps w mapie CarPlay. Panel zostaje (dobra odległość, zła strzałka). **Etap 4 zaliczony.**
- [x] **Etap 7 (dawniej 4b): własna warstwa graficzna na VC – strzałka manewru + pasy — STRZAŁKA DZIAŁA
  (2026-10-28, wizyta 9).** Pasy ruchu: ścieżka podpięta, ale **niesprawdzone w aucie** – nie było jazdy
  trasą z wieloma pasami (0 linii `engine: lanes` we wszystkich przebiegach) → **`docs\18_etap7_nakladka.md`**
      ← **Zbudowane i sprawdzone na komputerze (2026-10-26), w aucie NIETESTOWANE.** Paczka `work\sd_etap7\`.
  - [x] Rozpoznanie: `vc_render` nie ma dostępu do pikseli (`NvSSVideoDecode` dekoduje i wyświetla naraz),
        więc nakładka musi być osobnym displayable. Firmware to potrafi – kontekst 77 to już 4 warstwy.
  - [x] **Droga znaleziona:** Luka używa QNX Screen, na K3346 nie ma `libscreen`, ale rozszerzenie
        **KD QNX** ma komplet odpowiedników (`ID_STRING` 1002, `MANAGED` 1013, `SOURCE_ALPHA` 1011).
        Fabryczne `loadandshowimage` i `libdisplayinit.so` robią dokładnie tę sekwencję (GLES2!).
  - [x] **Port Luki: 9345 linii bez zmian**, przepisany tylko `platform_harman.c` + `kd_surface.c`
        (zamiast `platform_qnx.c` + `cluster_surface.c`). Spis: `overlay\PROVENANCE.md`
  - [x] Zbudowane: `vc_overlay` (602 KB), `vc_overlay_probe` (52 KB), biblioteka hooka z zasilaniem
  - [x] **Zasilanie bez zmian w Javie HMI** – hook w `mm-ipod` → TCP :19800, pakiety 48 B Luki;
        renderer jest serwerem (bo `mm-ipod` restartuje się przy każdym kablu), własny wątek
        z zablokowanym SIGPIPE (SIGPIPE w `mm-ipod` zabiłby CarPlay)
  - [x] **Mapper C ↔ Java: 126 672 przypadki identyczne** (`tools\run_overlay_map_diff.sh`)
  - [x] **Format transmisji sprawdzony dekoderem Luki** (`cr_decode_maneuver`): 54 OK, 0 FAIL (ASan+UBSan)
  - [x] Parser konfiguracji: 69 OK, 0 FAIL (po Etapie 7b; wcześniej 42 – doszły zakresy numeru
        displayable i offsety panelu). Zależności ELF: PASS (`libiow`/`libscreen` zablokowane)
  - [x] **Biblioteka v8s bit w bit bez zmian** – sprawdzone buildem z wyciętymi blokami Etapu 7
  - [x] Brama bezpieczeństwa: kontekst 901 tylko gdy `dmdt gd` potwierdzi nasz displayable,
        inaczej zostaje 77 (= dzisiejszy stan, mapa działa)
  - [x] Transakcja `INSTALL_ETAP7`/`ROLLBACK_ETAP7` + sonda `PROBE_ETAP7`; COLLECT zbiera teraz
        `dmdt gd` i `gs` (gc jest ucinane na 11 264 B – limit `dmdt`, nie nasz)
  - [x] **Zabezpieczenia dopisane (2026-10-26)**: blok T16 w `sim_installer.sh` (**236 OK, 0 FAIL**,
        dash i mksh), `sprawdz_karte.ps1 -Typ etap7`, karta ratunkowa usuwa pliki Etapu 7
        (`sim_rescue.sh` **52 OK, 0 FAIL** pod mksh; ta symulacja wymaga `/bin/ksh`)
  - [x] **Karta przygotowana i uzbrojona (2026-10-26)**: `sprawdz_karte -Typ etap7` → GOTOWA,
        0 błędów, 0 uwag. Komplet 4 znaczników w folderze pakietu — dyspozytor zużywa po jednym
        na uruchomienie, więc **przy aucie nie trzeba czytnika kart** (test T16l). Z Etapów 4–6
        zmieniony tylko `mib2high-lvds-control.sh` (celowo); `backup\pre-v8s`, znaczniki stanu
        i 20 katalogów `collected\` nietknięte. Stary `SHA256SUMS.txt` z karty zachowany jako
        `SHA256SUMS.sprzed-etap7.txt`
  - [x] **Te zabezpieczenia złapały trzy błędy**, każdy wyszedłby dopiero w aucie:
        (1) kolizja nazw w payloadzie – `INSTALL_ETAP4` wgrywałby bibliotekę Etapu 7, a
        `ROLLBACK_ETAP7` nie cofałby niczego → osobna nazwa `…ipod.etap7.so`;
        (2) pełny `ROLLBACK` nie cofał Etapu 7 → łańcuch 7→5→4→v8s, Etap 7 musi być pierwszy;
        (3) sonda miała zakodowaną ścieżkę zamiast `pathof` → zawsze odmawiała w symulacji
  - [x] **Stan karty `I:\` sprawdzony (2026-10-26)**: `ROLLBACK_ETAP4` i `INSTALL_ETAP4` leżą
        uzbrojone z datą 2026-10-22 i są nietknięte → **sprzątanie po Etapie 6 nie było wykonane**
        (auto kasuje znacznik po użyciu; pliki z auta mają datę `1980-01-01`). `command.sh` na karcie
        = `34252c4e…` identyczny z `work\sd_etap5`, `BT_MAC` brak
  - [x] **Wizyta 1 w aucie (2026-10-26)** – `probe-run` + `run-21` na karcie. Instalacja PASS,
        sekwencja w zaplanowanej kolejności, **nic się nie zepsuło** (mapa 15–21 kl/s, `rc=0x0`),
        brama zadziałała (`staying on 77`). Szczegóły: `docs\18`, sekcja „Wynik wizyty 1”
  - [x] **ZMIERZONA geometria panelu manewru**: **210×153** w obu kotwiczeniach,
        pozycje **(1091, 110)** i **(1055, 207)**; płótno 1440×542. Z zrzutów `dmdt ts 4`.
        Koryguje `docs\02:133` („large … 328×180” było szacunkiem z innego auta, nieprawdziwym dla K3346).
        Wartość wpisana w `config\mib2high-overlay.conf`
  - [x] **`dmdt ts` to działający przyrząd pomiarowy** – zrzuca kompozyt DisplayManagera
        (widać displayable 20, nie widać mapy – mapa to warstwa sprzętowa)
  - [x] **PRZYCZYNA ZNALEZIONA offline (2026-10-27)** → **`docs\19_displayable_na_mhi2.md`**.
        W binarce `displaymanager` **z naszego auta**: napisy `ID_STRING` i `MANAGED` **nie występują
        w ogóle**, jest za to `KD_WINDOWPROPERTY_CAPTION (%p): %d` oraz
        `CAPTION evaluated to 0. Most likely CAPTION string was invalid`.
        **Okno GL deklaruje numer displayable przez CAPTION (69), nie przez `ID_STRING_QNX` (1002)** —
        a my ustawialiśmy to drugie. Strumienie wideo idą osobnym kanałem
        (`KD_STREAMPROPERTY_DISPLAYABLE_ID_NV`), co tłumaczy, czemu `vc_render --layer 58` działa od Etapu 3.
        Poprawka: jedna właściwość w `overlay\kd_surface.c`. **Etap 7 zostaje bez zmian w Javie HMI.**
  - [x] **Niezależne potwierdzenie (2026-10-27)**: `repos\mib2q-carplay-rgi\docs\reference\MHI2_MU1439_PORT.md`
        – Luka rozebrał w IDA **ten sam build** `displaymanager` (583 837 B, bajt w bajt jak nasz,
        framework `5.41.10.SR5 MIB2CLU6`), tylko z firmware K3347/MU1439. Podaje fabryczny przepis
        z `libdisplayinit.so display_create_window`: caption → `atoi()` → numer, **bez żadnej
        walidacji**; `"0"`/nie-liczba daje id 0, `"-1"` jest ignorowane, >64 znaki odrzucane.
        Stąd zakres 1..109 w `overlay_cfg_sanitize()`. Dwie metody (napisy vs dekompilacja),
        jedna odpowiedź – ale jego sekcja ma `[decompile]` i **nie była testowana na aucie**
  - [x] Trop pochodzi z projektu **chefranov/mhi2-au37x-carplay** (MHI2 AU37x) — jako wskazówka;
        opis w `publish_src\README.md` i `LICENSING.md`. Żaden jego plik nie trafia do repo
  - [x] `dmdt gd`/`gs` zwracają **zero bajtów** na tej jednostce (`gc` działa) – przestaje mieć
        znaczenie, bo dowodem przyjęcia okna jest log renderera, a nie lista z `dmdt`
  - [ ] **Nadal nieustalone:** czemu główny blok starej sondy w ogóle się nie wykonał.
        Odrzucone: `$*` po `shift 4` przy `set -u` (działa w dash i mksh), uszkodzenie pliku,
        różnica powłok – **stara sonda w symulacji przechodzi pod obiema**. Awarii nie da się
        odtworzyć poza autem; nowa sonda łapie stderr, więc następnym razem sama to powie
  - [x] **Poprawki na wizytę 2 (2026-10-26)**: sonda przepisana – cały stderr do `probe.log`
        (`exec 2>>`), każdy blok loguje kod wyjścia i rozmiar osobno, rozpoznanie `dmdt`
        w 5 wariantach; dowodem przyjęcia okna jest **log renderera**, nie lista z `dmdt`;
        `collect()` zbiera `/tmp/vc_overlay*.log`. Nowa symulacja `tools\sim_probe.sh`
        (**43 OK**, dash + mksh) wymusza, żeby **każdy blok zostawił ślad** – „kod wyjścia 0”
        okazał się za słabym sprawdzianem
  - [x] **Etap 7b zrobiony (2026-10-27)** – poprawka wdrożona, nie tylko opisana:
        1. `overlay\kd_surface.c` ustawia **CAPTION (69)**; `ID_STRING_QNX` i `MANAGED_QNX`
           usunięte, `DISPLAY_QNX` domyślnie nie ustawiane (`display -1`, jak fabryczny przepis).
           Sprawdzone w **zbudowanej binarce**: `vc_overlay` i `vc_overlay_probe` nie zawierają
           napisów `ID_STRING`/`MANAGED`, zawierają `CAPTION` (`strings` nie ma w Git Bash –
           liczone po bajtach w Pythonie)
        2. **brama przestała pytać `dmdt gd`**: renderer po pierwszej wystawionej klatce pisze
           `/tmp/vc-overlay-ready` („id w h", atomowo przez `rename`), a `mib2high-lvds-control.sh`
           czeka do 6 s, sprawdza numer i tylko wtedy bierze kontekst 901. Nieświeży meldunek
           (renderer ubity na twardo) jest **usuwany przed startem** nakładki
        3. slot układu VC czytany z `V8H_LAYOUT slot=` w `/tmp/carplay_hook.log` i **logowany**,
           jeszcze nie stosowany – biblioteka Etapu 3 nietknięta
        4. sonda ma na **każdym** zrzucie schodek alfy (6 stopni nad czystym buforem)
        5. geometria potwierdzona z naszego firmware: `komoviewstyle.conf` zna `2; KVS_FPK;210x153`
           i `3; KVS_FPK;328x181` – pomiar 210×153 to styl 2 co do piksela
  - [x] **Nowa symulacja `tools\sim_gate.sh`** – pierwszy test bramy w ogóle (8 przypadków,
        **54 OK** pod dash i mksh): brak nakładki, brak meldunku, meldunek dobry, zły numer, śmieci,
        **nieświeży meldunek**, `dmdt dc` odmawia, sprzątanie po sesji. Miara to kontekst, na którym
        **pracowała pętla**, nie ostatni `sc` – skrypt kończy przywróceniem 72
  - [x] **Karta odświeżona (2026-10-27)**: `sprawdz_karte -Typ etap7` → **GOTOWA, 0 uwag**.
        Wymienione 5 plików + manifest; biblioteka Etapu 7 **bit-identyczna** (`4d81839730c0…`),
        czyli hook natywny nietknięty. Sekwencja: `ROLLBACK_ETAP7` → `INSTALL_ETAP7` → restart MMI
        → `PROBE_ETAP7` → sesja CarPlay z trasą → COLLECT
  - [x] **Ściągawka do auta: `docs\20_sciagawka_etap7b.md`** (2026-10-27) – oś czasu sondy co do
        sekundy, lista zdjęć, drzewo „kiedy przerwać". Przy jej pisaniu wyszło, że **dwa komunikaty
        na ekranie MMI były nieaktualne** po przejściu na procedurę z odłączonym iPhonem:
        `custom.sh` zapowiadał „ok. 3 minut" (realnie 56 s przerw + 8 zrzutów, czyli ~1,5 min)
        i prosił o „zakonczenie trasy", a sonda wypisywała to samo – przy odłączonym telefonie nie
        ma czego kończyć. Druga przerwa dostała zadanie wykonalne: **ocenę schodka alfy**, bo to
        jedyne pytanie, na które zrzut nie odpowie (mapy w kompozycie nie ma)
  - [x] **Wizyta 2 wykonana (2026-10-27)** – sonda przeszła, dane w `collected\probe-run\`
        i `collected\run-22\`. Zdjęć MMI nie ma (zapomniane) i nie były potrzebne: nakładka jest
        na zrzutach `dmdt ts`, bo idzie przez kompozytor DisplayManagera.
        **Poprawka CAPTION działa** – `kd_surface: CAPTION="98"` →
        `window caption="98" 210x153 display=-1 alpha=1 realized native=beeb0006`.
        Zmierzone w pikselach ze zrzutów (dekoder PNG w czystym Pythonie, bez Pillow):

        | Pytanie | Odpowiedź |
        |---|---|
        | DM przyjmuje nasze okno? | **tak** – zrealizowane i skomponowane |
        | rozmiar | **dokładnie** 210×153, bbox `(0,0)-(209,152)` |
        | pozycja | **(0,0)** – DM **nie nadaje** pozycji displayable'owi bez wpisu |
        | pełne płótno (wariant B) | działa, pełne 1440×542, bez obcięcia |
        | alfa | schodek **0, 51, 102, 153, 204, 255** = `255·a` co do jedynki |
        | VIEW | okno nie zniknęło ani nie drgnęło (`vc-11` = `vc-10`) |

  - [x] **Dwa własne błędy znalezione z logów wizyty 2** – z telefonem brama odmówiła
        (`overlay did not report a window within 6s; staying on 77`), mapa działała normalnie:
        1. **`rename()` nie działa na `/tmp`** – `cannot write /tmp/vc-overlay-ready errno=18`
           (EXDEV). Sam `/tmp` jest zapisywalny (leżą tam wszystkie nasze logi), więc padało
           wyłącznie `rename()`. To **rozstrzyga starą niewiadomą: `/tmp` to `/dev/shmem`,
           czyli RAM** – a ten system plików nie ma `rename()`.
        2. **health-check był nieprawdziwy** – `kdGetWindowPropertyiv(VISIBILITY)` na tym DM
           **nigdy** się nie udaje. Dowód: drukowane `errno` to 18, 89, 236, 236, 236… – śmieć po
           poprzednim wywołaniu, nie błąd tego odczytu. Sonda loguje to samo i rysuje 282 klatki.
           Co 5 s renderer niszczył zdrowe okno i kasował meldunek: 15 cykli na sesję, 2 sesje.
        3. pochodna: meldunek powstawał tylko przy swapie, a renderer swapuje tylko mając scenę –
           po pierwszej nieudanej próbie nie było już kolejnej
  - [x] **Etap 7c wdrożony (2026-10-27)** – naprawa + pełne płótno:
        1. `overlay_ready.h` pisze meldunek jednym `open`+`write`, **bez `rename()`** i bez `.tmp`
        2. `kd_surface_lost()`: nieudany odczyt `VISIBILITY` to **nie** utrata okna (ostrzeżenie raz
           na proces); utratę zgłasza dopiero nieudany `eglSwapBuffers`
        3. meldunek wychodzi po **wyrenderowaniu pustej przezroczystej klatki** zaraz po powstaniu
           okna – nie czeka na pierwszy manewr
        4. **okno = całe płótno 1440×542**, panel rysowany na offsecie w środku.
           Kod Luki **nietknięty**: `render_init()` adoptuje framebuffer, który jest związany w
           chwili startu (`glGetIntegerv(GL_FRAMEBUFFER_BINDING)`), więc platforma podstawia mu
           własne FBO w rozmiarze panelu, a `platform_swap()` przenosi je na płótno
        5. slot układu VC **stosowany**, nie tylko logowany – po VIEW strzałka jedzie z zegarami
        6. brama czeka **10 s** zamiast 6
        7. nowe klucze `canvas_width`/`canvas_height` w `mib2high-overlay.conf` + **furtka awaryjna**
           opisana w pliku: wpisanie `210`/`153` wraca do małego okna w rogu, bez przebudowy
        8. `composite_panel()` wyłącza wszystkie tablice atrybutów przed swoim rysowaniem –
           `vb_flush()` Luki zostawia je włączone (patrz pułapki na końcu tego pliku)
  - [x] **Wizyta 3 wykonana (2026-10-27)** – Etap 7c zadziałał w całości. Z `collected/run-23/`:
        `overlay ready after 0s; context=901 {98,102,101,58} declared`,
        `READY /tmp/vc-overlay-ready -> 98 1440x542`, `render: init multi-layer 210x153
        (default fbo=1)` i **jedna** linia o nieczytelnym `VISIBILITY` zamiast 15 cykli recreate.
        Zdjęcia dokładają odpowiedź na jedyne otwarte pytanie: **mapa CarPlay jest widoczna dookoła
        naszego okna**, czyli przezroczystość pełnego płótna dociera do warstwy sprzętowej z mapą.
  - [x] **Dlaczego nie było strzałki – oba końca łącza były klientami**
        (rozstrzygnięte z `run-23/netstat-an.txt`: **portu 19800 nie ma tam w ogóle**):
        `overlay/upstream/server.c` to u Luki TCP **klient**, a nasz `overlay_feed.c` też robił
        `connect()`. Nikt nie wołał `bind()`/`listen()`. Stąd `not yet listening, will retry`
        i `commands=0`. Co gorsza komentarz w naszym `overlay_feed.h` **opisywał topologię
        odwrotnie** niż kod – twierdził, że serwerem jest renderer.
  - [x] **Etap 7d wdrożony (2026-10-27)**:
        1. **feed nasłuchuje** (`bind`/`listen`/`accept` na 127.0.0.1:19800), renderer zostaje
           klientem – czyli dokładnie topologia Luki (producent nasłuchuje), więc jego `server.c`
           z pętlą ponawiania co 200 ms **zostaje nietknięty**
        2. **odtwarzanie stanu po połączeniu** – inaczej panel czeka na kolejny `RGI update`
        3. **czytanie z gniazda** – renderer pisze w tę stronę (`cr_server_mark_frame_ready`);
           nieczytany bufor prędzej czy później zablokowałby mu wysyłkę, a bez czytania nie
           wiadomo, że się rozłączył, dopóki nie padnie zapis
        4. **panel nie jest malowany, gdy nie ma czego** (`platform_set_content_present()`
           przez istniejący szew `platform.h`) – to też rozstrzygnie, czyj jest czarny prostokąt
        5. **recepta kontekstu w pliku na karcie** (`context_recipe`), żeby ewentualna poprawka
           na podkładki 101/102 nie wymagała przebudowy
        6. doinstalowany **`flag_atlas.rgba`** (896 kB, mieliśmy go w repo, nigdy nie trafił do auta)
           i renderer startuje z `/mnt/app/root` jako katalogiem bieżącym
  - [x] **Wizyta 4 wykonana (2026-10-27)** – Etap 7d zadziałał. Z `collected/run-24/`:
        `server: connected to 127.0.0.1:19800`, `engine: reveal from clear icon=2`,
        `scene: kind=0 fallback=0 commands=34`, `platform_harman: panel drawn`,
        `engine: first frame ready`, `render: flag atlas loaded 1792x128 (14 frames)`.
        Bramka „nie maluj pustego panelu” też działa – **bez nawigacji czarny prostokąt zniknął**.
        Strzałki nadal nie ma, a z nawigacją prostokąt wraca.
  - [x] **Dlaczego nie ma strzałki – renderer dostawał płótno, na którym nie umie rysować.**
        `maneuver_render` Luki **nie skaluje się**: `CR_DEFAULT_WIDTH/HEIGHT` (328×181) to jego
        układ współrzędnych, nie rozmiar okna. Z tych stałych liczy się środkowanie manewru
        (`render.c:1219`), przesunięcie MVP (`render.c:1280`), nożyce (`render.c:1407`) i **każdy**
        element 2D (`render.c:1425`), a solver kadrowania pasów ma aspekt `328.f/181` wpisany
        na sztywno (`scene/lane_panel.cpp:286`). My podawaliśmy **210×153**, bo po pomiarze
        fabrycznego kafelka (2026-10-26) wpisaliśmy pomiar w `width/height`, czyli w rozmiar
        renderu zamiast w rozmiar wycinka. Scena renderowała się poprawnie, w złym kadrze –
        34 komendy rysowania i zero strzałki na kombi.
        **Minę mieliśmy opisaną własną ręką**: `overlay_cfg.h:14` mówił `width 328
        # render source size`, a trzy linijki niżej ostrzegał, że zmiana tego psuje aspekt i FOV.
        Sonda z Etapu 7a ma nawet stałe `REF_CROP_X/Y/W/H` = 59/27/210/153 z komentarzem
        „Luka's popup crop”. Wniosek na przyszłość: **gdy komentarz nazywa pole `render source
        size`, pomiar ekranu do niego nie należy.**
  - [x] **Etap 7e wdrożony (2026-10-27)** – trzy prostokąty rozdzielone w konfiguracji:
        1. `render_width/render_height` (328×181) – płótno renderera, **nie dotykać**;
           `platform_get_framebuffer_size()` zwraca właśnie to, więc `render_init` dostaje
           swój układ z powrotem
        2. `crop_x/crop_y` (59/27) + `width/height` (210×153) – wycinek, który trafia na kombi;
           to `CR_POPUP_*` Luki, czyli dokładnie prostokąt, w który renderer wkadrowuje manewr
        3. `x/y`, `x_alt/y_alt` – gdzie ten wycinek ląduje na płótnie 1440×542 (bez zmian)
        Matematyka blitu wyjechała do `overlay/overlay_blit.h` (dwa odwrócenia osi i mapowanie
        1:1 w jednym miejscu) i ma własny test hosta – **221 asercji**, sprawdzających nie osiem
        liczb, tylko własność, dla której istnieją: *teksel `(crop_x+i, crop_y+j)` pokazuje się
        w pikselu kombi `(dst_x+i, dst_y+j)`*. Stara karta (sam `width/height`) czyta się
        poprawnie – brakujące klucze biorą wartości Luki.
  - [x] **Renderer czyta swój własny panel i wpisuje wynik do logu** (`panel probe` w
        `platform_harman.c`): pokrycie alfy na całym płótnie i osobno w wycinku, zakres alfy,
        najjaśniejszy piksel i mapka 16×8. Powód: zawartości panelu nie dało się dotąd ustalić
        **żadną** drogą poza zdjęciem – mapa CarPlay jedzie warstwą sprzętową i nie ma jej na
        `dmdt ts`, a ciemny prostokąt na nocnym zdjęciu wygląda tak samo, czy to nasz panel
        malowany nieprzezroczyście, czy fabryczna podkładka 101/102 prześwitująca tam, gdzie my
        jesteśmy przezroczyści. Dwie wizyty poszły na tę niejednoznaczność.
  - [x] **`context_recipe` zostaje `102 101 58`** – zdjęcie bez nawigacji pokazuje, że na tych
        podkładkach siedzi fabryczna chrome mapy („85m”, „<ulica>”, „200m”, „offroad”), więc
        wpisanie samego `58` kosztowałoby te elementy. **Korekta 2026-11-02:** to był błędny wniosek
        ze zdjęcia. `1.0.3-rc3` usunął wysokość i skalę samym BAP, więc ten pasek rysuje VC, nie
        warstwy 101/102. Zielona karta („towards <ulica>”) jest
        **po angielsku przy polskim UI auta** → rysuje ją iPhone w strumieniu CarPlay (58),
        nie kombi; jej żadna zmiana recepty nie dotyczy.
  - [x] **Wizyta 5 wykonana (2026-10-28)** – Etap 7e zadziałał: `framebuffer 328x181`,
        `SSAA 525x290 -> 328x181`, `crop=210x153@(59,27)`, `server: connected`, `commands=34`,
        ~25 cykli silnika z treścią. Strzałki nadal nie ma, czarny prostokąt **w obu** układach.
  - [x] **Sonda rozstrzygnęła: prostokąt jest NASZ, a nasz FBO nie ma alfy.**
        `panel probe 1/8 canvas=100.0% crop=100.0% a=[255..255] brightest=(0,0,0)`, mapka
        `################` w całości. Odczyt padł zaraz po tym, jak `render_end_frame` wyczyścił
        bufor do `(0,0,0,0)` – gdyby kanał alfa istniał, tło musiałoby wyjść jako 0. Wyszło 255,
        jednolicie na całym 328×181. A blit kopiuje panel z **wyłączonym** mieszaniem, więc
        wpisywał alfę 255 na cały kafelek: nieprzezroczysta czarna płyta niezależnie od tego, co
        renderer narysował. Okno (FBO 0) alfę ma – EGL prosi o 8 bitów, schodek z Etapu 7a wrócił
        jako `255·a`. Problem dotyczy **tekstury podpiętej jako FBO**: GLES2 gwarantuje alfę
        tylko dla `RGBA4` i `RGB5_A1`, `RGBA8` w FBO to rozszerzenie (`GL_OES_rgb8_rgba8`).
  - [x] **Trzy wady mojej sondy**, przez które ta wizyta dała mniej, niż mogła:
        (1) zmierzyła **pierwszą klatkę treści**, czyli moment, w którym `main.c` ustawia
        `g_fade_alpha = 0` – `brightest=(0,0,0)` niczego tam nie dowodzi; (2) drugi odczyt
        wyłączył ją **na całą sesję** (`glReadPixels refused`), bo `glGetError()` złapał błąd
        zostawiony w kolejce przez kod Luki, a nie przez odczyt; (3) mapka liczyła **tylko
        wycinek**, więc „rysuje obok" i „nie rysuje wcale" dawały ten sam log.
  - [x] **Etap 7f wdrożony (2026-10-28)**:
        1. **format FBO mierzony, nie zakładany** – kandydaci `RGBA8`, `RGBA4`, `RGB5A1`,
           wygrywa pierwszy, który przejdzie test. Test pyta o alfę **przez RGB**, nie przez
           kanał alfa (sterownik, który gubi alfę w FBO, może też kłamać w `glReadPixels`):
           czyść do `(0,0,0,A)`, rysuj biały kwadrat z `glBlendFunc(GL_DST_ALPHA, GL_ZERO)`,
           czytaj RGB. Przy `A=0` ma wyjść 0; 255 znaczy brak kanału, bo GLES2 definiuje
           `GL_DST_ALPHA` jako 1, gdy alfy nie ma. Decyzja w `overlay/overlay_fbo.h`, test
           hosta przechodzi **wszystkie 64 kombinacje** werdyktów
        2. **brak alfy = brak blitu.** Jeśli żaden format nie przejdzie, `composite_panel()`
           nie kładzie panelu w ogóle – kombi zostaje przy obrazie fabrycznym. Nakładka,
           która nie umie się wtopić, ma zniknąć, a nie zasłonić mapę
        3. **bufory Luki biorą ten sam typ** (`platform_fbo_type()` przez szew `platform.h`,
           pięć alokacji w `render.c`). Przy sprawnym `RGBA8` zwraca `GL_UNSIGNED_BYTE`, czyli
           dokładnie to, co było
        4. **sonda naprawiona** – pierwszy odczyt po 1 s, kolejka błędów GL czyszczona
           i logowana przed odczytem, nieudany odczyt nie wyłącza sondy, mapka i pokrycie RGB
           liczone na **całym** płótnie, z zaznaczonym wycinkiem
        5. `PROVENANCE.md` wymienia teraz **wszystkie** zmiany w kodzie Luki – twierdził, że
           jest jedna, a od Etapu 7d były już trzy
  - [x] **Wizyta 6 wykonana (2026-10-28)** – bez poprawy na kombi: bez trasy czysto, z trasą
        czarny prostokąt bez strzałki, w obu układach. Ale log rozstrzygnął sprawę:
        `fbo alpha test RGBA8: alpha OK (dst.a=0 -> rgb 0, dst.a=1 -> rgb 255)`, a mimo to
        trzy odczyty w ciągu 10 s: `alpha canvas=100.0% | rgb canvas=0.0%`, czyli panel to
        **dokładnie `(0,0,0,255)` w każdym pikselu całego płótna**. Do tego `GL error 0x502`.
  - [x] **SPROSTOWANIE: diagnoza z Etapu 7f była błędna.** Bufor RGBA8 kanał alfa **ma** – test
        przez RGB to dowodzi. Alfa 255 przy zerowym RGB to nie „brak kanału", tylko wartość,
        którą GLES2 **z definicji zwraca przy próbkowaniu niekompletnej tekstury**: `(0,0,0,1)`
        (spec. GLES 2.0, §3.8.2). Siatka „bez alfy nie kładź panelu" z 7f zostaje – jest poprawna
        i tani bezpiecznik – ale nie trafiła w przyczynę.
  - [x] **Przyczyna czarnej płyty (cztery wizyty): bufory Luki NPOT bez `CLAMP_TO_EDGE`.**
        Tekstura NPOT z trybem zawijania innym niż `CLAMP_TO_EDGE` jest w czystym GLES2
        niekompletna. Luka ustawia `CLAMP_TO_EDGE` na każdej teksturze (maski, atlas flagi)
        **poza dwiema, które niosą gotowy obraz**: `g_ss_tex` i `g_fxaa_tex`, 525×290, domyślne
        `GL_REPEAT`. FXAA czyta więc `(0,0,0,1)` z SSAA, ostatni przebieg czyta `(0,0,0,1)` z FXAA,
        panel wychodzi nieprzezroczysty i czarny. Scena (34 komendy) rysuje się poprawnie –
        tylko nikt nie umie jej odczytać. Na Adreno w MHI2Q (`GL_OES_texture_npot`) `REPEAT` na
        NPOT jest legalne, dlatego u Luki działa, a jego aktualny `render.c` ma nadal ten sam kod.
        Na wizytach 3–5 bufor SSAA też był NPOT (336×245, 525×290) – płyta była zawsze ta sama,
        zmieniały się tylko moje hipotezy.
  - [x] **Etap 7g wdrożony (2026-10-28)**:
        1. `CLAMP_TO_EDGE` na `g_ss_tex` i `g_fxaa_tex` (`overlay/upstream/render.c`, piąta
           zmiana w kodzie Luki, opisana w `PROVENANCE.md`). `render_set_viewport` tylko
           re-specyfikuje te same obiekty, parametry przeżywają – tam zmiana niepotrzebna.
           W naszej binarce: **3 → 5** miejsc `WRAP_S`, **6 → 10** `CLAMP_TO_EDGE` (zliczone
           w kodzie ARM) – dowód, że poprawka weszła do pliku, który jedzie do auta
        2. log `GL_OES_texture_npot: yes/no` plus pełna lista rozszerzeń – diagnoza jako fakt
        3. **druga siatka**: kafelek z ≥ 99% alfy i < 1% koloru to podpis awarii, nie obraz
           (`overlay_tile_is_degenerate` w `overlay/overlay_fbo.h`). Sonda sprawdza to cicho już
           w pierwszej klatce treści i biegnie **przed** blitem, więc taki kafelek nie trafia na
           kombi ani na jedną klatkę; kolejny odczyt z treścią przywraca kompozycję
        4. `0x502` lokalizowany w `platform_swap()` przez pierwsze 150 klatek: czy podnosi go
           renderer, czy nasza sonda/kompozycja, w której klatce pierwszy raz i ile razy
  - [x] **Wizyta 7 wykonana (2026-10-28) — STRZAŁKA JEST NA KOMBI.** `GL_OES_texture_npot: no`
        (przyczyna czarnego prostokąta potwierdzona jako fakt), sonda: `rgb crop=23–27%`,
        najjaśniejszy piksel biały w wycinku. Przy małych zegarach kafelek wygląda dobrze. Przy
        **dużych** widać głównie podłoże jezdni — strzałka „wychodzi poza obszar". Błędy GL:
        5 na 150 klatek, wszystkie od renderera, sporadyczne — nie blokują niczego.
        Ciemnoszare tło pod strzałką to **fabryczne tło kafelka** (podkładka KDK 101/102 —
        nasz wycinek jest w ~76% przezroczysty), takie samo jak pod fabryczną strzałką.
        **Zostaje — decyzja użytkownika.**
  - [x] **Przyczyna 1: układ zegarów nie był wykrywany NIGDY.** `overlay_layout.h` czytał
        `V8H_LAYOUT` z `/tmp/carplay_hook.log`, a hook pisze tę linię przez `LOG_INFO`
        (`altscreen_session.c:1391`) przy loggerze z `.min_level = LOG_LEVEL_WARN`
        (`framework/logging.h:57`). W logu hooka są wyłącznie linie `[WRN]`. Kafelek stał więc
        zawsze w `x/y`, w obu układach — a ja opisywałem przełączanie po VIEW jako działające.
        Test hosta tego nie złapał, bo sprawdzał parser na liniach zmyślonych, nie na tym, co
        hook naprawdę zapisuje.
  - [x] **Przyczyna 2: etykiety pomiaru z 2026-10-26 były zamienione.** Pozycje są dokładne,
        ale sonda kazała tylko „przełączyć VIEW na drugi układ", a „duże zegary" dla (1091,110)
        było założeniem. Geometria: przy małych zegarach mały prędkościomierz zaczyna się ok.
        y≈296, więc kafelek kończący się na y=360 wszedłby na niego; przy dużych (1091,110)
        leży pod napisem odległości VC. **Duże zegary = (1055,207), małe = (1091,110).**
  - [x] **Etap 7h wdrożony (2026-10-28)**:
        1. układ z **pierścienia przechwytu DSI** (`/tmp/v8l-dsitracer-1798-NN.log`, uruchamiany
           przez `lvds-control` w każdej sesji), parserem **hooka** (`native/gauge_bap54.h`,
           BAP 50/54, tylko `2#`) i zapasowo `setZoomArea` (460 → duże, 700 → małe) do pierwszego
           BAP 54 — polityka hooka 1:1. Nie port 1798 na żywo: rekord stanu przyszedł o 52.424 s,
           renderer wystartował o 52.464 s. Nie zmiana poziomu logu w hooku: to biblioteka v8s
        2. **zamienione pozycje** w kodzie i na karcie
        3. pokrętło `visible_top_big/small` (domyślnie 0) → `render_set_visible_area()`, to samo
           wywołanie co `CMD_VISIBLE_AREA` z Javy na MHI2Q; na wypadek gdyby VC przy dużych
           zegarach zasłaniał górę kafelka, gdzie leży ramię skrętu
        **Sprawdzone na prawdziwym przechwycie z jazdy** (`run-30`): czytnik daje 12 zmian
        układu (49,7 s zoom → duże, potem BAP 54 na zmianę aż do 385,9 s), ostatni stan: duże
        zegary, kafelek w (1055,207). Podczas tej jazdy układ zmienił się 11 razy, a kafelek
        ani razu.
  - [x] **Wizyta 8 wykonana (2026-10-28) — DZIAŁA W OBU UKŁADACH.** Przy dużych zegarach strzałka
        w środku prawego zegara, przy małych w fabrycznym kafelku; `run-31` loguje każde VIEW
        (`layout slot=… (… clocks, bap54) panel at (…)`). Zostało jedno: po VIEW strzałka przez
        chwilę stoi w miejscu **poprzedniego** układu, potem przeskakuje.
  - [x] **Skąd ta chwila — zmierzone na śladzie:** klawisz VIEW to `DSIKeyPanelListener#updateKey2`
        z `int:4|int:35` (10 naciśnięć → 10 zmian BAP 54, 1:1; klawisze 36/37 układu nie
        zmieniają). VC przełącza się przy klawiszu, a raport BAP 54 przychodzi **~575 ms później**
        (9 z 10 razy 572–593 ms). Do tego dwa nasze opóźnienia: układ sprawdzany raz na sekundę
        i brak nowej klatki po zmianie układu (renderer Luki rysuje tylko przy zmianie sceny,
        w bezczynności pętla zwalnia do 3 Hz). Razem od ~0,6 s do ponad 2 s w złym miejscu.
  - [x] **Etap 7i wdrożony (2026-10-28)**: klawisz VIEW (dowolny słuchacz — `12#` przychodzi
        ~190 ms przed `1#`) chowa kafelek, raport BAP 54 pokazuje go **od razu w nowym miejscu**;
        spóźniona kopia klawisza (znacznik trackera ≤ znacznik ostatniego BAP 54) jest pomijana;
        bezpiecznik 1500 ms. Pierścień sprawdzany co 100 ms (sam `stat`, gdy nic nie przybyło),
        a przy każdej zmianie platforma sama kładzie ostatnią klatkę (`composite_panel` +
        `eglSwapBuffers`), bez czekania na `main.c`. **Na prawdziwym śladzie z `run-31`:**
        10 VIEW → 10 ukryć po 392–593 ms → za każdym razem pokazanie we właściwym układzie
  - [x] **Wizyta 9 wykonana (2026-10-28) — „Wszystko działa tak jak powinno".** Logi
        (`run-32`…`run-35`): każde VIEW kończy się `tile shown at (…) after N ms (bap54)`,
        N = 54–686 ms; **zero** bezpieczników (`timeout`), **zero** blokad kafelka; 1 błąd GL
        na 150 klatek (renderer, sporadyczny). Archiwum przebiegów `run-23`…`run-35` skopiowane do
        `work\auto_test_19\karta_mod\…\collected\`. **Etap 7 zamknięty.**
  - [ ] Fizyczna karta ratunkowa ma jeszcze stary `ratunek.sh`; **przed tą wizytą nie trzeba jej
        wgrywać** (Etap 7 nie dotyka Javy ani HMI), a poprzednia podmiana wymagała formatowania karty
  - [ ] **Etap 7d: karta rozmowy** – zrobione w `1.0.3-rc10`, czeka na test w aucie (wpis niżej). Historia: layout ze zdjęć (2026-10-26), brakowało renderowania tekstu
        (atlas glyphów; w rendererze Luki nie ma żadnego fontu). Wariant B sondy już odpowiedział:
        okno może wziąć całe płótno, więc karta rozmowy ma gdzie się zmieścić – zostaje sam tekst
- [ ] **Do wyjaśnienia:** świeży build v8s daje `b8da2da0…`, a w aucie jest `058413dd…`.
      **Nie jest to skutek Etapu 7** (build z wyciętymi blokami daje tę samą sumę). Dlatego paczka
      Etapu 7 nie zawiera v8s ani `vc_render` i nie rusza `work\sd_etap5\`.
- [ ] **Etap 5: Siri, touchpad, telefon, okładka** – plan i instrukcja: `docs\16_etap5_plan.md`
  - [x] Paczka 5.0 (sonda przesłaniania klas) `work\sd_etap5\`: `mib2high-k3346-overrides.jar`
        (1 klasa = fabryczna + znacznik), transakcja `INSTALL_ETAP5`/`ROLLBACK_ETAP5`, symulacja 142/142;
        karta ratunkowa usuwa też ten JAR (49/49)
  - [x] Karta instalacyjna przygotowana 2026-10-20 (`sprawdz_karte -Typ etap5`: 0 błędów, `INSTALL_ETAP5` uzbrojony)
  - [x] Karta ratunkowa: przy podmianie `ratunek.sh` karta przestała być czytelna (Windows: Warning) →
        pełne formatowanie FAT32 przez użytkownika, wgrana od nowa z `work\sd_ratunek\` (0 błędów, 238/238,
        uzbrojona, nowy `ratunek.sh` 65c8fbdc…). **Nie testować jej na zdrowym aucie z modem** – usunęłaby
        JAR v8s i `dsitracer_activated` (mapa na VC zniknie do rollbacku/reinstalacji). Stary log: `work\auto_test_5\`.
  - [x] Test 5.0 w aucie (2026-10-20): **PASS** – znacznik `class_loaded` + `updateActiveDeviceState`;
        J9 bierze klasy z naszego JAR-a (logi `work\auto_test_5\karta_mod\`, run-04)
  - [x] Paczka 5.1 (Siri krótkim PTT + touchpad D-pad): 3 podmienione klasy + 2 nowe, API = fabryczne,
        wywołania fabryczne zachowane, test D-pada 7/7, symulacja 149/149 – `docs\16`
  - [x] Test 5.1 w aucie (2026-10-20): **PASS** – krótki PTT = Siri, touchpad = D-pad w CarPlay
        (logi `work\auto_test_6\karta_mod\`, run-06)
  - [x] Paczka 5.2 (rozmowy CarPlay na VC): źródło = PPS `/ramdisk/pps/iap2/telephony` (stock iAP2
        już subskrybuje 0x4155, bez kodu natywnego); podmiana `ServiceManagerPhone` + `K3346GatedPhoneService`
        (port Luki bez list/akcji); testy 19/19 + 7/7, symulacja 151/151 – `docs\16`
  - [x] Test 5.2 w aucie (2026-10-20): **rozmowy widoczne na karcie telefonu VC** (przychodzące,
        wychodzące, aktywne, nazwa w pasku VC), brak komunikatu CarPlay; PPS tekstowy; logi `work\auto_test_7\`.
        Brak: okno rozmowy na zakładce mapy, odbierz/rozłącz z VC (→ 5.4)
  - [x] Paczka 5.3 = **sonda okładki** w COLLECT (`k3346-artwork-probe`, `iap2_nowplay_getartwork`, limit 15 s),
        symulacja 157/157 – `docs\16`
  - [ ] Test sondy 5.3 w aucie **[Ty]** (COLLECT przy grającej muzyce) → pełna okładka
  - [x] Test użytkownika: przycisk słuchawki na kierownicy odbiera/kończy rozmowę CarPlay (FLASH); przy
        telefonie BT okno rozmowy jest na zakładce mapy (stock potrafi – do ustalenia, co je wyzwala)
  - [x] Paczka 5.4 (odbierz/rozłącz z VC, wariant prosty = fabryczna ścieżka przycisku 13/14 + rezerwa
        FLASH 20): podmiana `MFLPhoneButtonsListener`, nowe `K3346CallKeys`/`K3346GatedPhoneListener`;
        test 29/29, symulacja 156/156, ratunek 49/49 – `docs\16`
  - [x] Test 5.4 w aucie (2026-10-20, `work\auto_test_8\`): VC „Odrzuć”/„Odłóż” działa; VC nie ma
        „Odbierz”; **regresja** – przycisk słuchawki = „call failed” (podwójne przełączenie: BAP accept +
        stock flash). Okno dzwonienia jest na mapie. Dźwięk tym razem OK (bez HFP). Sonda 5.3: `O_RDWR`
        zamiast `O_RDONLY` → EINVAL; demon ma okładkę (98 KB) – `docs\16`
  - [x] Poprawka 5.4 v2 (bez `ACCEPT_CALL`, deduplikacja z przyciskiem kierownicy) + sonda 5.3 v2;
        test 34/34, symulacja 157/157, ratunek 49/49
  - [x] v3: „Odbierz” w oknie dzwonienia VC (flaga `ACCEPT_CALL` + opcja rozmowy Accept), test 35/35
  - [x] Test v3 w aucie (2026-10-20, `work\auto_test_9\`): **5.4 PASS** (VC Odbierz/Odrzuć/Odłóż,
        przycisk słuchawki bez „call failed”); **sonda 5.3 PASS** – JPEG 600×600 z `iap2_nowplay_getartwork`
  - [x] Okno dzwonienia na mapie znika po zmianie zakładki – **tak samo fabrycznie z telefonem BT**
        (test użytkownika 2026-10-20) = zachowanie firmware VC, nie błąd modu
  - [ ] (kiedyś, **wybór użytkownika: wariant b**) Własna karta rozmowy rysowana na mapie CarPlay –
        ładniejsza niż fabryczne okno VC, widoczna zawsze przy dzwonieniu/rozmowie; robić razem z
        rendererem strzałki Etapu 4b (ta sama nakładka na mapie). Wariant (a) – ponowne wysłanie stanu
        dzwonienia przy powrocie na mapę – odrzucony
  - [x] Paczka 5.3 (okładka CarPlay na VC): narzędzie `k3346-coverart-fetch` + `K3346CoverArt` (PPS
        nowplaying) + podmiany `TerminalModeBapCombi`/`AppConnectorTerminalMode` (PictureManager, jak Luka);
        instalator zna nowy plik i stare kopie; symulacja 165/165, ratunek 49/49 – `docs\16`
  - [x] Test 5.3 w aucie (2026-10-21, `work\auto_test_10\`): okładka wysłana, VC pokazał obrazek zastępczy
        (JPEG 600×600 nieakceptowany) → v2: PNG 256×256 jak Luka
  - [x] Test 5.3 v3 w aucie (2026-10-21): **okładka CarPlay na VC działa** (`listRef=0` jak Luka)
  - [x] Etap 6 v5 w aucie (2026-10-21, `work\auto_test_14\`): lista z BT, dzwonienie z listy, rozmowy z sesji
        działają; lista dopiero po połączeniu BT
  - [ ] Etap 6b (`docs\17`): iPhone oddaje listę przez iAP2 jak u Luki? Hipoteza: brak
        `BluetoothTransportComponent` (MAC auta) w identyfikacji. Krok 1 zrobiony (adres BT tylko w chipie,
        podany przez użytkownika). Krok 2: `BT_MAC` na karcie → `[bluetooth]` z MAC w `iap2.cfg` (Etap 4),
        symulacja 192/192. Test 6b (run-18): komponent przyjęty, lista iAP2 nadal 0 → obalone.
        Etap 6c (run-19/20): wszystkie 5 wariantów 0x4170 → 0. Sprzątanie: biblioteka `ef800f54…`
        (jak v5), karta bez `BT_MAC`.
  - [x] **Korekta 2026-10-27 (`docs\17`, sekcja „Korekta”)**: pusta lista to **błąd iOS od 26.6**
        (Apple Community, Bimmerpost; ten sam objaw na fabrycznych BMW i Porsche), naprawiony według forum
        w iOS 27. Wniosek „Harman + stary chip” odwołany – chefranov ma listy przez iAP2 na MHI2 Harman.
        iPhone użytkownika: iOS 26.7.1. Kod v5 bez zmian (lista iAP2 ma pierwszeństwo).
  - [x] **Test na iOS 27.2 (2026-10-27): DZIAŁA** – BT wyłączony, kabel, historia połączeń z iPhone'a na VC
        od razu, bez zmian w kodzie (`docs\17`, „Wynik na iOS 27.2”). Logi jeszcze niezebrane.
  - [x] **Ulubione z iAP2 na VC – `1.0.2-rc1`: DZIAŁA w aucie (2026-11-02, `run-48`)** (`docs\17`, sekcja
        „Ulubione z iAP2”). iPhone już je wysyłał (`run-47`: count 1 + wpis), ginęły w parserze. Parser 6/7, osobny plik
        `/tmp/k3346-tel-favorites.txt`, Java `onFavorites`, bit 43. Test C 49/0, Java 62/0, instalator 392/0, ratunek 52/0.
        W aucie: `favorites n=1 skipped=0`, dwa wybrania z VC `result=OK` / `rc=0`, bez crashy. Logi:
        `work\auto_test_20\`; kopia karty sprzed zmiany: `work\karta_przed_102rc1\`. README uzupełnione lokalnie,
        **na GitHub nie wysłane (decyzja użytkownika)**.
  - [ ] **`1.0.3-rc1`: `showETA=yes` w URL-ach mapy VC (2026-11-02), W AUCIE NIETESTOWANE.** Pomysł z wydania Luki
        MHI2Q-2026-10-29, zmiana tylko w `native/gauge_layout_urls.h`. Do oceny w aucie: czy ETA nie dubluje się
        z paskiem VC (BAP) i czy ramka mieści się w obu układach. Wycofanie: `update.zip` z `1.0.2-rc1`.
        Test URL PASS, offsety ALL PASS, instalator 392/0, ratunek 52/0. Względem `1.0.2-rc1` różnią się
        `libmib2high-carplay-rgi.so` i `RELEASE`, a dwa JAR-y tylko datami wpisów. Kopia karty sprzed zmiany:
        `work\karta_przed_103rc1\`. **Test (`run-49`, logi `work\auto_test_21\`, zdjęcia, małe zegary):** ramka ETA jest
        w obu aplikacjach, bez dubla w pasku, ale zasłonięta przez lewy zegar, etykietę `75 m` i dolny pasek.
        Przyczyna: `view 1` miało x=132 i dół y=417, a ETA kotwiczy do dolnej krawędzi safe area.
  - [ ] **`1.0.3-rc2`: nowe obszary widoku, W AUCIE NIETESTOWANE.** `config/mib2high-lvds-viewareas.conf`:
        `view 1 270 90 702 290`, `view 2/3 490 90 460 300`, `view 0` bez zmian. Liczby z pomiarów Luki na panelu
        1440×542 (pasek VC od ok. y=390), nie z naszego auta; prawa krawędź 972 omija nasz kafel (x=1091). Karta manewru
        przy małych zegarach przesuwa się o ok. 138 px w prawo. **Czeka [Ty]:** zdjęcia obu układów, Apple i Google Maps.
        Wycofanie: `update.zip` z `1.0.3-rc1` albo `1.0.2-rc1`. Na GitHub nic nie wysłane. Nie testowane osobno,
        weszło do `rc3`.
  - [ ] **`1.0.3-rc3`: ukrycie wysokości, skali i kompasu na pasku VC, W AUCIE NIETESTOWANE.** Sam pas z zegarem
        i temperaturą rysuje VC, tego z MMI nie ruszymy. Elementy idą przez BAP (`CombiBAPListener` → nasza bramka):
        `K2161GatedCombiService.setNavStatusHidden`, włączane w `RouteGuidance.start()`, zwalniane przez
        `restoreBestEffort()`. Wartości: `updateAltitude(0x7FFF, 2)`, `updateMapScale(15, false, 0xFFFF, 0xFF, false)`,
        `updateCompassInfo(-1, -1)` (RE Luki, nie sprawdzone na naszym VC; kompasu Luka na stałe nie ukrywa).
        Skala przy ukryciu dalej wysyła status: odpowiedź na `MapScale_SetGet` z rolki. Ulica bez zmian.
        Test `java/test/routeguidance/K2161GatedCombiServiceTest.java` (kompilacja wg `build_java_release.sh`: `-source 1.4`
        dla źródeł, test zwykłym `javac`, classpath `work/lsd.jar`): 13/13, mutacje 12/12. **Czeka [Ty]:** zdjęcia,
        rolka, odłączenie kabla, logi (`NAV_STATUS_HIDDEN=1/0`). Wycofanie: `update.zip` z `1.0.3-rc2`.
        **Wynik (`run-50`, logi `work\auto_test_22\`, 4 zdjęcia):** wysokość, skala i kompas zniknęły, log
        `NAV_STATUS_HIDDEN` 1 → 0 po odłączeniu → 1 po podłączeniu, rolka działa, 0 `INITIALIZING`. Półprzezroczyste tło
        paska zostaje jako pusta kapsuła. Apple, małe zegary: `11:49 arrival` czytelne, dół ramki lekko pod kapsułą.
        Apple, duże zegary: karta na cały środek, bez mapy i ETA. Google, oba układy: karta jest, **ETA brak**, a przy `rc1`
        (`view 1` 840×332) było.
  - [ ] **`1.0.3-rc4`: `physical 292 110`, W AUCIE NIETESTOWANE.** Układ karty i próg ETA w CarPlaySupport zależą od
        rozmiaru fizycznego (Luka: `THIRD_PARTY_NAV_APPS.md`, ETA od ok. 202 px przy 292 mm). Przy 200 mm próg wychodzi
        z proporcji ok. 295 px: pasuje do małych zegarów (332 tak, 290 nie), ale **nie tłumaczy dużych (300 px, brak ETA)**,
        więc to hipoteza. **Czeka [Ty]:** ETA w Google i mapa w Apple przy dużych zegarach, czytelność (iPhone może rysować
        ok. 30% mniejsze elementy). Tekst za mały → np. `physical 240 90`. Brak ETA → powrót do `200 75` i wyższy obszar.
        Wycofanie: `update.zip` z `1.0.3-rc3`.
        **Wynik (`run-51`, logi `work\auto_test_23\`, 4 zdjęcia): DZIAŁA.** ETA w Google wróciło w obu układach, mapa
        w Apple przy dużych zegarach też. Rolka i wsteczny działają (użytkownik; logi tego nie pokazują). 0 `SIGSEGV`,
        0 `INITIALIZING`, `dec_err=0`, średnio 20,6 FPS (nie wiadomo, czy dzięki tej zmianie). Małe zegary: ramka ETA
        dotyka lewego zegara i ciemnej kapsuły.
  - [ ] **`1.0.3-rc5`: `view 1 300 90 672 265`, W AUCIE NIETESTOWANE.** ETA dalej od zegara (x=300) i kapsuły (dół y=355,
        oszacowane ze zdjęć). Prawa krawędź dalej x=972. Wysokość 265 > 202 px, szerokość 672 > 498 px przy 292 mm.
        **Czeka [Ty]:** zdjęcia małych zegarów w Google i Apple, kontrola Google przy dużych, rolka, wsteczny, kabel, logi.
        Wycofanie: `update.zip` z `1.0.3-rc4`. Karta albo ETA zniknie → `h=290` i tylko przesunięcie w prawo.
        Nie testowane osobno, weszło do `rc6`.
  - [ ] **`1.0.3-rc6`: godzina przyjazdu i czas jazdy w kapsule VC, W AUCIE NIETESTOWANE.** Tekst FctID 19:
        `11:49 · 12 min · <ulica>`, powyżej godziny `1 h 05`, bez trasy sama ulica. Samego tła nie da się usunąć
        z MMI (`NavStatus` w `hmi_app.elf` VC, a FctID 44 ukrywa też mapę; Luka, jego VC). `K3346NavBarText` + krok 4
        w `BAPBridge`, tekst wysyłany tylko po zmianie. Czas lokalny z `Navigation.getEnv().getFramework()
        .convertUTCTimeToLocalTime()`, bez niego godzina jest pomijana. **`BAPBridge.convertUtcToLocalMs` to no-op,
        więc FctID 22 dostaje UTC; świadomie bez zmian, do sprawdzenia, jeśli VC pokaże zły czas na karcie celu.**
        Bramka blokuje fabryczne FctID 19/20 podczas trasy, na starcie `updateTurnToInfo("", "")`. Pochodzenie `offroad`
        nieustalone (0 razy w naszym BAP i w `iap2.log`). Testy `java/test/routeguidance/`: bramka 15/15, tekst 28/28,
        mutacje wykryte. **Czeka [Ty]:** zdjęcia w Apple i Google, oba układy; czy `·` i godzina są poprawne. Wycofanie:
        `update.zip` z `1.0.3-rc5` albo sprawdzonej `1.0.3-rc4`.
        **Wynik (`run-52`, logi `work\auto_test_24\`, 4 zdjęcia): DZIAŁA.** Godzina zgodna z zegarem auta, `·` poprawne,
        0 `SIGSEGV`, 0 `INITIALIZING`, 0 `offroad`; rc5 (ramka ETA) też dobrze. Drobiazg: raz `14:19 · 0 min` (iOS wysyła 0,
        zanim zna czas), poprawione w `rc7`.
  - [ ] **`1.0.3-rc7`: trzy strefy w kapsule, W AUCIE NIETESTOWANE.** Małe zegary: `14:25 · 3 min` po lewej i `1,2 km`
        po prawej rysuje `vc_overlay` (`overlay/capsule_text.h`, `stb_truetype`, czcionka z firmware MMI), ulica w polu VC.
        Duże: `14:25 · 3 min · 1,2 km` w polu VC. Tekst nakładki leży **pod** półprzezroczystą kapsułą VC, więc będzie
        ciemniejszy. Pola tekstowego nie da się poszerzyć (ok. 359 px, RE Luki), boki VC przyjmują tylko liczby.
        Wymiana przez pliki `/tmp/vc-capsule-{text,slot,off}`: **`rename()` na `/tmp` nie działa**, więc jeden zapis
        i znacznik `END`. Brak pliku slotu = tekst jak w `rc6`; `draw=0` (brak czcionki, `capsule_text 0`, brama odmówiła)
        = bez ulicy, w polu VC. Położenie (`capsule_*` w `mib2high-overlay.conf`) to szacunek ze zdjęć. Testy: Java
        138/138, natywne 0 FAIL (`capsule_text` 112), mutacje wykryte, `sim_gate` 96, instalator 392, ratunek 52, offsety
        ALL PASS. **Czeka [Ty]:** czytelność za dnia, położenie, VIEW na stojąco, zdjęcia. Za ciemne → `capsule_text 0`.
        Wycofanie: `update.zip` z `1.0.3-rc6`. Potem `rc8`: opcja Kapsuła/Mapa/Obie w MMI (osobny plan; moduł CarPlay
        ma tylko ekrany stanu, wzór `UpmixToneUi` Luki, teksty w `lang_data.zip`).
        **Wynik (`run-53`, logi `work\auto_test_25\`, 4 zdjęcia): technicznie działa, ale ZA CIEMNE.** Położenie trafione
        (`capsule_*` z szacunku), VIEW w obie strony, 0 `SIGSEGV`, 0 błędów GL z naszego rysowania. Średnio 18,0 FPS przy
        19,8 w `run-52` (przyczyny nie ustalono, próby z różnych przejazdów). Tekst pod tłem paska jest słabo czytelny,
        tego nie poprawimy z MMI.
  - [ ] **`1.0.3-rc8`: boczne teksty wyłączone, W AUCIE NIETESTOWANE.** `capsule_text 0`: na obu układach
        `14:25 · 3 min · 1,2 km` w polu VC, bez ulicy (ulicę telefon zwykle pokazuje na mapie). Kod rysowania zostaje,
        włączany w `mib2high-overlay.conf`. **Ścieżka `draw=0` nie była jeszcze uruchomiona w aucie.** **Czeka [Ty]:**
        oba układy, VIEW. Wycofanie: `update.zip` z `1.0.3-rc6`. Nietestowane osobno, weszło do `rc9`.
  - [x] **`1.0.3-rc9`: opcja `Czas dojazdu CarPlay` (Kapsuła / Mapa / Obie), DZIAŁA W AUCIE.** Wiersz w fabrycznych
        opcjach Connectivity (`cMOPT`, obok „O Apple CarPlay”), bo moduł CarPlay nie ma własnych ustawień. Podmiana:
        `de.audi.tghu.connectivity.hmi.Activator` + `K3346EtaRow` w `overrides.jar`, każdy błąd zostawia fabryczny bank,
        fabrykę albo drawer. **`lang_data.zip` bez zmian:** teksty z nadpisanego `getText`. Wartość: magazyn HMI (1008/1000,
        dopisany do `FlushValuesDict`), kopia `/tmp/k3346-eta-mode`. Domyślnie Kapsuła. `altscreen_session.c` czyta plik co
        500 ms i przy zmianie wysyła `showUI` z `showETA=yes` albo bez niego; **czy iPhone reaguje od razu, gdy zmienia się
        tylko `showETA`, nie wiadomo** (inaczej działa po VIEW). `BAPBridge`: tryb Mapa = sama ulica. Testy: `K3346EtaRowTest`
        na klasach z `lsd.jar` 72/72, mutacje 33/33. **Czeka [Ty]:** czy wiersz się pokazuje, czy MMI startuje normalnie, wszystkie
        trzy pozycje, zapamiętanie po restarcie, logi `K3346_ETA_ROW` w `k3346-override-probe.log`. Wycofanie: `update.zip`
        z `1.0.3-rc6`.
        **Wynik (`run-54`, logi `work\auto_test_26\`, 5 zdjęć): DZIAŁA.** `row_added`, 10 wyborów `mirrored=true`, wszystkie
        trzy tryby, zdjęcia zgodne z tabelą w obu układach. 0 `SIGSEGV`, 0 `offroad`, `dec_err=0`, 0 `row_failed`.
        Ścieżka (potwierdzona przez użytkownika): MENU → Ustawienia (ostatnia pozycja) → Manager połączeń → pokrętło w prawo → na dole listy. Kursor na urządzeniu zmienia resztę listy
        (`PropertyObject` w `cMOPT`), nasz wiersz nie ma warunku, więc jest zawsze. **Nie potwierdzone:** zapamiętanie po
        restarcie (w logu jeden `boot mode=0`, restartu MMI po wyborze nie było). Średnio 16,3 FPS (19,8 w `run-52`, 18,0
        w `run-53`): rysowanie kapsuły jest wyłączone, więc to nie ono; próby na stojąco, bez porównywalnej jazdy.
  - [ ] **`1.0.3-rc10`: własna karta rozmowy na mapie VC, W AUCIE NIETESTOWANE.** Wariant A z wizualizacji:
        `overlay/call_card.h`, plik `/tmp/vc-call-card` z `K3346CallCard` (`onPps`), kropka stanu, nazwa, stan i licznik
        liczony w nakładce. Małe zegary (735, 300), duże (560, 305), 320×78, wartości w `mib2high-overlay.conf`.
        **Reguła dużych zegarów (pomysł użytkownika):** gdy widać kartę, w trybach Mapa i Obie iPhone dostaje URL bez
        `showETA`, a czas przechodzi do kapsuły; po rozmowie wraca sam, zapisana opcja bez zmian. Ta sama tabela w
        `native/eta_mode.h` i `K3346NavBarText.timesOnBar`, obie testowane. `K3346HmiLanguage` wspólny dla karty i wiersza
        opcji. Testy: `call_card` 84, `K3346CallCardTest` 33, `K3346EtaRowTest` 72, tabela i URL PASS, mutacje wykryte.
        **Nieznane:** czy fabryczne okno dzwonienia zasłania kartę (`call_card_ringing 0` = karta od odebrania), czy
        `showUI` w trakcie rozmowy przełącza ramkę płynnie, rysowanie na GPU. **Czeka [Ty]:** test z drugim telefonem,
        też czy `Czas dojazdu CarPlay` pamięta wybór po restarcie. Wycofanie: `update.zip` z `1.0.3-rc9`.
  - [ ] **`1.0.3-rc11`: profil Map 30 FPS, W AUCIE NIETESTOWANE.** `rc10` plus PPID `bf889dfdaea54096` (parametr 34)
        w Identify (`native/ident_hook.c`; pomysł z wydania Luki 2026-10-29). Błąd dopisania nie psuje Identify. Punkt
        wyjścia: 15–17 FPS na stojąco w `run-54`, średnie 16–20 w innych przebiegach, a `vc_render` dostaje tyle od telefonu.
        Luka: 29,7 zamiast 21,4 AU/s na wirtualnym iPhonie, **w aucie niezmierzone**. **Czeka [Ty]:** czy CarPlay się łączy,
        ok. 2 min na stojąco z trasą jak w `run-54`, `STATS` w `vc_render.log`, `K3346_MAPS30` w logu natywnym, parametr 34
        i `IdentificationAccepted` w `iap2.log`. Nie sięgnie ok. 25 FPS → dalej odbiór i USB (`WIRED_30FPS_INVESTIGATION.md`).
        Wycofanie: `update.zip` z `1.0.3-rc9`. `rc10` nie była instalowana, weszła tutaj.
        **Wynik (`run-55`, logi `work\\auto_test_27\\`, 2 zdjęcia): karta rozmowy DZIAŁA**, jasna i czytelna, reguła dużych
        zegarów też. `boot mode=1`: opcja pamięta wybór po restarcie. PPID wysłany i `IdentificationAccepted`, ale **bez
        zysku FPS**: 16,9 średnio, maks. 21, na stojąco. Zgłoszone: ramka ETA czasem wraca dopiero po VIEW; strzałka czasem
        w miejscu dużych zegarów (log `timeout - default place`).
  - [ ] **`1.0.3-rc12`: strzałka i ramka ETA, W AUCIE NIETESTOWANE.** (1) `/tmp/vc-layout-last`: pierścień DSI powstaje od
        nowa przy każdej sesji, a po ponownym podłączeniu kabla nikt nie powtarza układu aż do VIEW. Nakładka pamięta układ,
        a kod natywny bierze go z `/tmp/vc-capsule-slot`, dopóki nie przyjdzie FctID 54. (2) Zmiana samego `showETA` przez
        widok drugiego układu i z powrotem, jak VIEW (`k3346_eta_queue_view`, `eta_bounce`). **To hipoteza.** (3) `K3346_SHOWUI`
        i `K3346_SHOWUI_ACK` w logu, żeby ją rozstrzygnąć. **Czeka [Ty]:** kabel na obu układach, zmiana opcji bez VIEW,
        rozmowa zaczęta na dużych zegarach, FPS w jeździe. Wycofanie: `update.zip` z `1.0.3-rc11`.
  - [ ] **`1.0.3-rc13`: 30 FPS przez `features=2`, W AUCIE NIETESTOWANE.** `rc12` plus jedna zmiana. joeyQuery (Audi B9, MHI2
        ER AUG22 P3241, TRACE-012): iPhone sam dławi strumień 111 do ok. 16 FPS przy `features=0` bez urządzenia wejścia; po
        `features=2` z pokrętłem ok. 30 FPS. U nas było 16–20. `advertise_cluster_knob` (`native/altscreen_airplay.c`) kopiuje
        stockowe `Eso_Knob` jako `K3346_ClusterKnob`, wiąże je z ekranem 111 i ustawia `primaryInputDevice`. Tylko zgłoszenie:
        bez rejestracji urządzenia i bez raportów HID. Brak pokrętła lub błąd kopii = dotychczasowe `features=0`. `Eso_Knob` jest
        w `app.img`, ale lista urządzeń będzie dopiero w logu `K3346_KNOB`. **Uwaga przy pomiarze:** u joeyQuery licznik po
        ponownym podłączeniu kabla to 15–17, po zimnym starcie 29–30, a dwa przejęcia UI z rzędu też dławią (może dotyczyć
        odbicia ETA z `rc12`); harman-f ma 27,9 FPS przy `features=0`, ale inną drogą obrazu, więc to nie jedyny czynnik.
        **Czeka [Ty]:** FPS w pierwszej sesji po restarcie (postój i jazda), potem po kablu, pokrętło i dotyk na głównym ekranie,
        testy `rc12`. Wycofanie: `update.zip` z `1.0.3-rc11`.
        **Wynik (`run-56`, `run-57`, logi `work\auto_test_28\`):** strzałka po kablu dobrze, reszta działa. `K3346_KNOB pick=0
        advertised=1`, stock `Eso_Knob,Eso_Playback,Eso_Telephony,Eso_TrackPad`, główny ekran `features=2`, bez
        `primaryInputDevice`. **FPS bez zmian w liczniku: 16–18** (`rc11`: 16,9), raz 25–27 przez ok. 30 s przy 72 km/h, a użytkownik
        ocenia obraz jako płynny. `features=2` zostaje na razie (nie umie porównać, nic nie psuje). Ramka ETA: odbicie wykonane
        i potwierdzone (`status=0`), kroki co ok. 160 ms, ramka dalej dopiero po VIEW; chowanie działa.
  - [ ] **`1.0.3-rc14`: warianty ramki ETA, DIAGNOSTYKA, W AUCIE NIETESTOWANE.** `rc13` plus cykl sposobów pokazywania
        ramki (`k3346_eta_tries` w `native/eta_mode.h`), każda prośba bierze kolejny, log `K3346_ETA_TRY`: A odbicie z przerwą
        600 ms, B 1200, C 2500, D bez odbicia z powtórzeniem `showUI` po 1 s, E odbicie jak `rc13` z powtórzeniem, F sam `showUI`.
        Chowanie bez zmian. **Czeka [Ty]:** numery prób, przy których ramka pojawiła się sama. Potem zostawić jeden wariant
        i usunąć cykl. Test: `tests/test_k3346_eta_tries.c`. Wycofanie: `update.zip` z `1.0.3-rc11`.
        **Wynik `rc14` (`run-58`, logi `work\auto_test_29\`):** małe zegary: przełączenie 0,6 s nie, 1,2 s tak, 2,5 s nie, bez
        przełączania z powtórzeniem tak, szybkie przełączenie z powtórzeniem tak, sam `showUI` tak. Duże zegary: numeracja niepewna,
        bo kabel odpadł po `n=8`, licznik ruszył od nowa, a w logu jest 13 prób przy 12 wpisach. Wpis 11 „nie” bez mignięcia pasuje do
        bez przełączania z powtórzeniem. Samego `showUI` na dużych nie było, log kończy się na `E`. Przełączanie daje ramkę na chwilę
        w złym miejscu albo 0,5 s na środku; hipoteza „za krótka przerwa” odpada. Skąd ramka znika, nie wiadomo. FPS 16,9, maks. 24.
  - [ ] **`1.0.3-rc15`: ETA bez przełączania, opcja `Karta manewru CarPlay`, W AUCIE NIETESTOWANE.** (1) Pokazanie ramki na zmianę
        `F_same_only` i `D_same_resend_1000`, log `K3346_ETA_TRY`, licznik od nowa po podłączeniu kabla. Chowanie jednym `showUI`, też
        bez przełączania i też niesprawdzone. Kod odbicia usunięty. (2) Opcja dla dużych zegarów: `Pełna` (dziś), `Średnia` i `Mała`
        (`view 4/5`, w=400 i 340, joeyQuery `safe-areas.md`, progu „no space” na 292 mm nie znamy), `Brak` (`/map`, ramka ETA w Mapach
        Apple niesprawdzona). Klucz `1008/1001`, plik `/tmp/k3346-card-mode`, `/tmp/k3346-no-rgi` przy aplikacji bez wskazówek wraca
        do `Pełna`. `/map` dopisany do `altScreenURLs`, nie do `suggestUI`. **Czeka [Ty]:** 6 prób na dużych, 2 na małych, 2 rozmowy na
        dużych, zdjęcia każdej wartości karty. Potem zostawić jeden sposób i usunąć licznik. Testy: `tests/test_k3346_eta_tries.c`,
        `tests/test_k3346_card.c`. Wycofanie: `update.zip` z `1.0.3-rc11`.
        **Wynik `rc15` (`run-59`, logi `work\auto_test_30\`, 4 zdjęcia): ETA DZIAŁA:** 8 z 8 prób i 2 z 2 rozmów, sam `showUI` wystarcza,
        chowanie też. VIEW i `Mapa` po 7,7 s: ramki nie było, po 11,8 s i 17,7 s była (hipoteza, jeden przypadek). `Średnia` i `Mała`: karta prawie
        bez zmian, iPhone wyrzuca treść, odpadają. `Brak`: działa, ETA zostaje, ale karta nie wraca w tej sesji, także na małych
        zegarach, mimo właściwego URL. FPS 17,1, maks. 24.
  - [ ] **`1.0.3-rc16`, W AUCIE NIETESTOWANE.** (1) ETA: sam `showUI`, cykl usunięty. (2) Włączenie ramki czeka 12 s po zmianie układu
        (`k3346_eta_held`, log `K3346_ETA_WAIT`). (3) Karta `Pełna`/`Brak`, widoki z powrotem 4. (4) Wyjście z `/map` przez `stopUI` (log
        `K3346_STOPUI`), także przy VIEW na małe; **pierwsze użycie `stopUI` u nas, może wygasić mapę.** (5) Opcje `Strzałka na VC`, `Mapa na VC`,
        `HUD CarPlay`, klucze `1002–1004`, pliki `/tmp/k3346-{arrows,map,hud}-mode`. Strzałka Wył.: nakładka chowa kafelek i VC wychodzi z
        trybu wskazówek, bo tło kafelka rysuje VC (`vc_policy=off`, `run-39…42`); w trakcie trasy zwykły koniec i start wskazówek
        (`restartGuidance`), **pierwszy raz w trakcie sesji**. Mapa Wył. + Strzałka Wł.: nowy kontekst 902 `{98 102 101 33}`, bo fabryczny 74 ma
        złą strzałkę (20); układu jeszcze nie próbowaliśmy. Obie Wył.: 72 raz. Strumień iPhone'a i `vc_render` działają dalej.
        **HUD Wył.: tylko `updateHUDDisplayContent(false)`, NIESPRAWDZONE, auto bez HUD.** Wycofanie: `update.zip` z `1.0.3-rc11`.
        **Wynik (`run-60`, logi `work\auto_test_31\`):** ramka po VIEW 4 z 4, czekanie 5–10 s. Strzałka Wył./Wł. działa na obu układach, także w trakcie
        trasy. Mapa Wył.: mapa Audi z kafelkiem, czysty widok, powrót mapy CarPlay, na obu układach (kontekst 902 działa). `stopUI` potwierdzone 4 razy,
        karta nie wróciła. **Błąd: czarny kafelek bez strzałki po podłączeniu kabla.** iPhone wysłał dwie serie, a po resecie `5201` już bez listy
        (`present=0x1583fb`, 237 razy; w `run-58` i `run-59` lista w każdej). Reset wyczyścił naszą listę, więc nakładka dostawała „brak manewru”. Stan 0
        przyszedł przed `/map`, więc to nie skutek `Brak`.
  - [ ] **`1.0.3-rc17`, W AUCIE NIETESTOWANE.** (1) Lista przeżywa reset (`rgd_list_restore.h`, log `K3346_LIST_RESTORED` w `k3346-rgi-ipod-native.log`).
        Połączenie z `rgd_hook.c` sprawdzi tylko auto, na hoście jest sama logika. (2) `Brak` na obu układach, `stopUI` usunięte, karta ma wracać
        po ponownym podłączeniu kabla: **niesprawdzone**, główne pytanie testu. **Czeka [Ty]:** karta po restarcie, `Brak`, powrót po kablu, 5 razy
        kabel i strzałka. Wycofanie: `update.zip` z `1.0.3-rc11`.
        **Wynik (`run-61`, logi `work\auto_test_32\`), postój:** karta po restarcie jest, `Brak` działa na obu układach, karta wraca po kablu przy `Pełna`,
        strzałka wróciła w 5 z 5 podłączeń, własne testy użytkownika też dobrze; 11 połączeń bez błędu poleceń. **Poprawka listy nieudowodniona:** log `mm-ipod`
        obejmuje tylko ostatnią sesję, tam `K3346_LIST_RESTORED` 0 razy, bo iPhone wysłał dwie serie, ale sam powtórzył listę. FPS 16,1, maks. 25.
        **Czeka [Ty]:** normalna jazda z trasami i rozmowami. Czarny kafelek bez strzałki: od razu `run individual script`, bez odłączania kabla. Wydanie
        `1.0.3` dopiero potem i na polecenie.
  - [ ] **Na razie niewspierane (README, zgłoszone przez użytkownika):** układ S4 Sport, także tylko zakodowany: problemy z mapą, zacinający się HUD, a przy VIEW
        mapa CarPlay znika, pojawia się fabryczna i po chwili wraca CarPlay. Bezprzewodowe adaptery CarLink: dużo błędów, niestabilność.
  - [ ] **Otwarte: Mapy Apple przy dużych zegarach bez ETA na mapie** (`run-51`; mapa i karta są). Nie wiadomo, czy da się
        to zmienić z naszej strony. `rc6` daje tam czas w kapsule.
- [ ] **Etap 6 – ostatnie połączenia CarPlay na VC + dzwonienie z listy** (`docs\17`): paczka gotowa
      (grupa 0x41 w tabeli sterownika, 0x4170/0x4171/0x415A z hooka `mm-ipod`, Java lista + wybieranie);
      offsety ALL PASS, testy C/Java PASS, symulacja 173/173, ratunek 49/49. Książka adresowa przez CarPlay
      niewykonalna (tylko BT PBAP). Test w aucie **[Ty]** – karta typu `etap6` (4 uruchomienia).
  - [x] **Repozytorium publiczne (2026-10-20): (to repozytorium)**
        Aktualizacja **wyłącznie** przez `tools\export_public.sh publish\mhi2-k3346-carplay-vc` (czyści dane
        osobowe, skan blokuje publikację przy trafieniu; sam skrypt NIE trafia do repo), potem commit/push
        w `publish\mhi2-k3346-carplay-vc` (autor: noreply GitHub, nie prywatny e-mail). README/LICENSING:
        `publish_src\`. Nigdy push z katalogu projektu.
- [ ] Etap 5: kolejne funkcje (okładki, touchpad, telefon; zoom rolką już działa – Etap 3;
      wsteczny/PDC = fabryczne, nic do zmiany)

- [x] **Wydanie 1.0 dla laików (2026-10-29)** – kandydat 1.0.0-rc1 sprawdzony w aucie (run-36: ROLLBACK karty
      deweloperskiej + INSTALL_ALL, wszystkie funkcje OK, overlay context=901). 1.0.0 = rc1 bit w bit poza
      tekstem restartu w `command.sh` (było: gałka głośności – ZŁE dla B9; jest: duże pokrętło + NAV/MAP + RADIO
      ~10 s, wg tabeli wiki M.I.B.), `RELEASE` i README. Zip collect-logs + szablon zgłoszenia `.github/`.
      `custom.sh`: znacznik **`INSTALL_ALL`** (v8s → 4 → 5 → 7 jednym uruchomieniem, każdy etap dalej osobną
      transakcją; `SAFE TO REMOVE SD` tylko raz na końcu; przy błędzie `INSTALL_ALL_RESULT=FAIL` + co dalej;
      wznowienie ponownym uruchomieniem; pełna instalacja → `ALREADY_INSTALLED`), ponowna instalacja po pełnym
      `ROLLBACK` (stara kopia v8s odsuwana jak w etapach 4/5/7), `ROLLBACK` na karcie bez kopii →
      `NOTHING_TO_UNDO` zamiast „do not reboot”, plik `RELEASE` w wyniku i w `metadata.txt`.
      `tools\make_release.sh <wersja>` → `work\release\<wersja>\` (install.zip, uninstall.zip, SHA256SUMS).
      `sim_installer.sh` testuje teraz instalator ZE ŹRÓDEŁ (`/src`; wcześniej T1–T15 brały stary `command.sh`
      z `work\sd_etap5` z 2026-10-21!), nowe T17 (INSTALL_ALL) i T18 (prawdziwe zipy, `RELEASE_DIR`): 302/0.
      README EN + `README.pl.md` dla laika, `INSTALL.md` → `BUILDING.md` (stary w `work\INSTALL.md.stary`).
      Kandydat `1.0.0-rc1` zbudowany w całości ze źródeł. Kopia karty sprzed testu: `work\karta_przed_rc1\`.
      Pasy ruchu: niesprawdzone w aucie (brak jazdy z manewrami wielopasowymi) – opisane w README.

- [x] **1.0.0 z GitHuba sprawdzone w aucie (2026-10-30, run-37)**: uninstall.zip + install.zip pobrane z wydania,
      według INSTRUKCJA.md — wszystko działa, instrukcja bez uwag.
- [x] **Błąd #5 — PRZYCZYNA: brak łatki NavIgnore (2026-10-30).** Zgłaszający zainstalował w M.I.B.
      „dual navigation” (m.i.b. → navigation → NavActiveIgnore) i mapa zostaje. `NavActiveIgnore.jar`
      podmienia `org/dsi/ifc/carplay/AppState` (`getAppStateID()`/`getOwner()` → 0), więc HMI nie wie, że
      nawigację przejął iPhone. **Nasze auto ma go od 2023** (docs/01, docs/06), auto edzenisa też — dlatego
      nikt tego nie widział. SPROSTOWANIE: hipoteza „to wersja zegarów H11/0379” była błędna. Skutki:
      NavIgnore = wymaganie (README/INSTRUKCJA 4.3), instalator raportuje `NAVIGNORE=PRESENT/MISSING`
      (bez blokowania), `vc_policy` zostaje w kodzie (domyślnie rgi), niepotrzebny.
- [ ] ~~Błąd #5 (VC H11/0379, quepasafran): czarna mapa na VC przy prowadzeniu~~ (opis historyczny niżej) — u nas VC H15/0299 działa.
      Strona HU identyczna (vc_render bez błędów, BAP te same, `factoryRgType=4`); różnica: VC. Java przy starcie
      trasy robi `updateRgActive(true)` + `setFavoredViewMode(1)` (kod edzenisa; Luka favored nie rusza).
      1.0.1-rc1: polityka `/mnt/app/root/mib2high-vc.conf` `vc_policy=rgi|map|off` (domyślnie rgi = 1.0.0),
      znaczniki `VC_POLICY_RGI/MAP/OFF`, zipy `…-vc-policy-*.zip`, ROLLBACK usuwa plik. Natywne binarki = 1.0.0.
      **Test u nas (2026-10-30, VC H15/0299, run-39…42):** `rgi` = 1.0.0; `map` (`SET_FAVORED_SKIPPED`) —
      bez żadnej różnicy (mapa, strzałka, fabryczne tło kafelka); `off` (`vcEnabled=false`) — małe zegary:
      strzałka bez fabrycznego tła, duże zegary: brak strzałki → `off` odpada. Kolejka znaczników
      (ROLLBACK+INSTALL_ALL+MAP+OFF+RGI na jednej karcie) działa w aucie. rc1 padło na `printf` (patrz Pułapki),
      rc2 = poprawka. Czeka: test `map` u zgłaszającego (H11/0379).
- [ ] **Błąd #5b: brak strzałki u zgłaszającego** — przyczyna: jego iPhone nie powtarza listy manewrów (TLV 0x0D)
      po chwilowym `route_state=0` na starcie trasy, a `rgd_hook.c` (ramka resetu, kod Luki) ją kasuje →
      `active_man` pusty → nakładka dostaje tylko „clear”. Potwierdzone surowym `rx.bin` obu aut. Do zrobienia
      po czarnej mapie (plan: zapamiętać listę przez ramkę resetu i przywrócić, gdy 5202 wrócą).

- [ ] **1.0.1-rc3 (2026-10-30)** — dwa błędy potwierdzone w naszym aucie (1.0.0 + NavIgnore, run-43/44):
      1. **„Duch” strzałki po końcu trasy** (też #5): po 800 ms od `route_state=0` hook wysyłał Javie trasa=0
         (fabryczny kafelek znika), ale nakładce nie `clear` → ostatni manewr wisiał. Poprawka:
         `k3346_overlay_feed_clear("route end")` w `rgd_periodic_tick` (`rgd_hook.c`, tick 1 Hz z busa).
      2. **Pierwsza trasa po zimnym starcie: kafelek w miejscu dużych zegarów przy małych.** VC nie zgłasza
         układu (BAP 50/54 indication) do pierwszego VIEW; za to fabryczne HMI na starcie sesji wysyła
         `0#…DSIBAP#request#…|int:50|int:54|int:0|int:7|int:<stan>` i zawsze powtarza nim aktualny układ.
         `overlay_layout.h`: nowe źródło `hmi`, ważne do pierwszego raportu VC (BAP54 > HMI > zoom).
         Testy hosta na prawdziwych liniach z run-43/44: overlay_layout 104/0.
      Strzałki u zgłaszającego #5 z NavIgnore działają — teorii o gubionej liście manewrów nie ruszamy.
      **Test rc3 w aucie (run-45):** „duch” naprawiony (strzałka znika, fabryczny kafelek chwilę później).
      Pozycja częściowo: request HMI przyszedł 2,7 s PO pierwszym manewrze (48,4 s → 51,1 s), więc kafelek
      przez chwilę stał w miejscu domyślnym i sam przeskoczył. **rc4:** `overlay_layout_unknown_hold()` —
      przy nieznanym układzie kafelek schowany do 6 s (potem miejsce domyślne jak dawniej);
      overlay_layout 117/0.
      **rc5:** strzałka gasła ~1 s przed fabrycznym kafelkiem (run-45: nasz clear ~89,3 s, BAP RG status 0
      89,96/90,36 s) → `g_overlay_clear_pending`: clear w NASTĘPNYM ticku po „Route reset timeout”, kasowany
      przez nową trasę; rozłączenie gasi od razu. UWAGA: `rgd_hook.c` jest CRLF (kod Luki) — wstawki z rc3
      miały LF, ujednolicone; eksport do repo i tak normalizuje do LF.

- [x] **Błąd #6 (Q7 4M, VC H13/0378): touchpad w CarPlay / Carlinkit.** 1.0.1-rc6: `touchpad=dpad|stock`
      (`/mnt/app/root/k3346-touchpad.conf`, znaczniki `TOUCHPAD_*`, domyślnie dpad), log zakresu współrzędnych.
      **Test u nas (run-46/47, A4 B9):** dpad jak dotąd, zakres x=69..924 y=342..915; `mode=stock source=file`
      działa, ale **z iPhone'em po kablu touchpad w trybie stock nie reaguje wcale** (surowe współrzędne iOS
      ignoruje). Tryb stock NIE naprawia więc iPhone'a po kablu; ma sens tylko z przystawką.
      **Odpowiedź zgłaszającego (2026-11-01):** po kablu touchpad działa (przewijanie + klik), więc progi pasują do
      Q7; nie działa tylko kursor z Carlinkit → test `stock`. Opóźnienie HUD znikło po zmianie układu zegarów
      sport → classic. Waze bez strzałek: Waze nie wysyła RGI (Luka, `THIRD_PARTY_NAV_APPS.md`), opisane w README.
      Zipy jednoplikowe: `…-update.zip`, `…-update-touchpad-stock.zip`.
      **Wynik (2026-11-02):** `…-update-touchpad-stock.zip` na 1.0: kursor z przystawki z Androidem znów działa.
      Z iPhone'em po kablu touchpad w trybie stock nie reaguje, działa pokrętło; zgłaszający uważa, że w jego Q7
      zawsze tak było. Jeden tryb naraz, przełączenie plikiem `touchpad-*` + restart MMI. Zgłoszenie zamknięte.

- [ ] **Błąd #7 (A4 B9, 1.0.0, iOS 26.6.2, NavIgnore jest): czarna mapa na VC po ponownym podłączeniu USB.**
      Pierwsza sesja po restarcie MMI działa, po wyjęciu kabla na 15–20 s mapa jest czarna aż do restartu MMI; VIEW i
      zmiana aplikacji nawigacji nie pomagają. **Przyczyna nieznana, kodu nie zmieniamy bez dowodu.** Na naszym aucie mapa
      wraca (potwierdzenie użytkownika), więc szukamy różnicy między autami: VC, iOS, konfiguracja. Logi z maila nie dotarły;
      w zgłoszeniu poproszono o ponowne wysłanie według procedury + wersję VC (2026-11-01). Logi zapisywać do
      `Logi_bugow_uzytkownikow\issue7\` (poza eksportem). Wzorzec zdrowej drugiej sesji: `work\auto_test_19\…\collected\run-40`
      (przerwa 183 s): `hook connected`, `NvSSVideoOpen -> 0x0`, `dec_err=0`, `context=901`, Java `RESTORE` → `PREPARE` →
      `CONFIGURE`, sonda nakładki 12–13% alfy. **Odrzucone:** `/tmp/mib2high-lvds-reacquire` (nikt go nie czyta) i sama
      długość przerwy (u nas 8, 11, 183 s działa). Rozstrzyganie po logach, sesja 1 vs 2 i vs `run-40`:

      | Co widać w logach | Wniosek |
      |---|---|
      | `vc_render.log`: brak drugiego `hook connected`, błąd `NvSSVideoOpen`, `dec_err>0` | wideo nie dochodzi do dekodera |
      | `carplay_hook.log`: brak `SETUP_READY`/`FIRST_REAL_FRAME` w sesji 2, `V8S_CONTROL_TIMEOUT` | brak strumienia lub `showUI` |
      | dekodowanie jest, ale `STATS` stale bardzo mało KB/klatkę | iPhone wysyła pusty obraz |
      | realny ruch, `panel probe` z alfą bliską 100% | nakładka zasłania mapę |
      | `lvds-control.log`: `duplicate start ignored`, `declaration failed`, `staying on 77` | stan kontekstów lub skryptu |
      | Java: inny `CAPTURE_STATE_OK favored=`, brak `CONFIGURE`, wyjątek | stan VC po `RESTORE` |
      | wszystko jak w `run-40`, a mimo to czarno | VC/DisplayManager: porównać `sloginfo.txt` (`stream closed`, `window closed`, `new stream`), dopiero potem paczka diagnostyczna |

      `dmdt-gc.txt` urywa się na 11 264 B, przed kontekstami 900/901, więc nie potwierdza ich stanu.

## Gdzie co jest

| Dokument | Po co |
|---|---|
| **`00_postep.md`** (ten plik) | **Czytaj pierwszy.** Stan etapów + pułapki środowiskowe |
| `07_co_zostalo_do_zrobienia.md` | Co zrobione / co zostało, dla kogoś przejmującego projekt |
| `08_paczka_etap3_instrukcja.md` | Co paczka zmienia w MMI, instalacja, rollback, dowody jakości |
| `09_sciagawka_auto.md` | **Do auta**, na telefon: kolejność czynności i „kiedy przerwać" |
| `10_odtworzenie_builda.md` | **Jak zbudować wszystko od zera**: komendy, zależności, sumy kontrolne |
| `11_przeglad_etapu_2_i_3.md` | Przegląd poprawności: co zweryfikowano na binariach K3346 i jak, czego nie da się sprawdzić bez auta |
| `12_karta_ratunkowa.md` | Karta ratunkowa na bootloop HMI: jak działa, dowody, czego nie gwarantuje |
| `13_przygotowanie_kart.md` | **Jak przygotować obie karty** w Eksploratorze Windows, krok po kroku |
| `14_wyniki_etap3.md` | Wyniki pierwszych testów w aucie: co działa, analiza wstecznego, zerwania USB |
| `15_etap4_rgi.md` | Etap 4 (RGI w `mm-ipod`): co zmienia, dlaczego tak, bezpieczeństwo, instrukcja w aucie |
| `16_etap5_plan.md` | Etap 5 (Siri, touchpad, telefon, okładka): rozpoznanie K3346, kolejność, sonda 5.0 |
| `17_etap6_ostatnie.md` | Etap 6 (ostatnie połączenia, dzwonienie z listy): dlaczego iAP2 nie oddaje historii |
| `18_etap7_nakladka.md` | **Etap 7 (własna warstwa na VC: strzałka + pasy)**: dlaczego osobny renderer, port Luki, sonda, instrukcja |
| `19_displayable_na_mhi2.md` | **Jak powstaje displayable na MHI2**: CAPTION vs ID_STRING, protokół adopcji okna, `createImageDisplayable`, dowody z binarki z auta |
| `20_sciagawka_etap7b.md` | **Do auta, na telefon (Etap 7i)**: trzy uruchomienia, bez sondy ekranowej, na co patrzeć po podłączeniu telefonu, które linie logu rozstrzygają, kiedy przerwać |
| `01`–`06` | Analiza: backup, różnice K2161→K3346, HMI Java, odczyt z auta |

## Narzędzia zrobione po drodze (`tools\`)

| Narzędzie | Do czego |
|---|---|
| `qnx_build.sh` | buduje binarki QNX w kontenerze; obchodzi błąd 32-bitowych binutils na bind-mountach Windows |
| `make_sd_package.sh` | składa paczkę na SD, kontroluje CRLF i architekturę ARM, liczy sumy |
| `sim_installer.sh` | symulacja instalatora bez auta (62 testy) – **uruchamiać w kontenerze, nie na Windows** |
| `make_rescue_card.sh` | składa kartę ratunkową z M.I.B. 3.6.0; pilnuje, że podpisany `metainfo2.txt` i sumowany `finalScript.sh` są nietknięte |
| `sim_rescue.sh` | symulacja karty ratunkowej (45 testów, w tym warunki z auta: karta RO, ubogi `PATH`) – w jednorazowym kontenerze, pod `mksh`; `RESCUE=` / `TYLKO_R678=1` do testowania innej wersji |
| `sprawdz_karte.ps1` | sprawdza przygotowaną kartę SD (tylko odczyt): sumy względem paczki, FAT32, uzbrojenie, typowe pomyłki |
| `verify_port_offsets.py` | weryfikacja offsetów hooków przeciw prawdziwemu driverowi iAP2 |
| `verify_overlay_elf.py` | zależności binarek nakładki; blokuje `libiow`/`libscreen`, których w aucie nie ma |
| `run_overlay_map_diff.sh` | mapper manewrów C vs Java na 126 672 przypadkach – chroni przed rozjechaniem się dwóch kopii |
| `make_sd_etap7.sh` | składa paczkę Etapu 7 **obok** `sd_etap5` (nie nadpisuje plików już sprawdzonych w aucie) |
| `sim_gate.sh` | symulacja **bramy kontekstu kombi** (8 przypadków, 54 testy, dash + mksh): decyduje o przejściu 77 → 901. Sprawdza też **nieświeży** meldunek po rendererze ubitym na twardo. Potrzebuje `/mnt` i `/eso`, więc **tylko w kontenerze** |
| `sim_probe.sh` | symulacja sondy Etapu 7 (43 testy, dash + mksh): wymusza, by **każdy blok zostawił ślad** w logu i plikach — po tym, jak w aucie cały blok wykonał się „po cichu" i skrypt i tak zwrócił 0. Od Etapu 7b sprawdza też **odmowę przy żywym rendererze** (duplikat captionu 98) |

## Pułapki, o których trzeba pamiętać
- **Na MMI nie ma `printf`** (ani `sha256sum`, `cksum`, `md5sum`, `cmp`). 1.0.1-rc1 padło na tym w aucie
  (`printf: cannot execute`), choć symulacja dała 348/0 — w kontenerze `printf` jest. Od rc2 test T0
  w `sim_installer.sh` przeszukuje skrypty dla auta pod kątem tych poleceń.

- **Heredoc w tej powłoce zjada jeden backslash z `\\`.** Pisanie skryptu Pythona przez
  `cat > plik <<'EOF'` **nie jest przezroczyste**: `\\n` dochodzi jako `\n`, a Python zamienia to na
  prawdziwy znak nowej linii **wewnątrz literału C** – plik przestaje się kompilować, a podmiana
  wzorca zawierającego `\` po cichu nie trafia (0 wystąpień). Trafiło dziś 4 razy.
  Obejście: w skryptach, które generują kod, składać backslash przez `chr(92)`, a wzorców
  z backslashem (ścieżki Windows, kontynuacja linii w `sh`) **nie używać w ogóle** – ciąć plik
  po indeksie znacznika. **Po każdej podmianie oglądać literały**; `\n` przed `"` da się policzyć
  jednym `python -c`.
- **W Git Bash na tym komputerze nie ma `strings`.** `strings plik | grep X` nie zgłasza błędu
  w sposób, który widać w wyniku potoku – wygląda jak „nie znaleziono", czyli jak sukces przy
  sprawdzaniu, że czegoś **nie ma**. Liczyć bajty w Pythonie.
- **`$'\r'` w złożonym poleceniu psuje parsowanie** tej powłoki („unexpected EOF"). Zdarzyło się
  to przy `grep -c $'\r'` w jednej linii z heredokiem – całe polecenie nie wykonało się wcale,
  a plik nie powstał.
- **awk na Windows zjada `\r`.** Pliki HMI w aucie mają mieszane końce linii (CRLF + LF).
  Wszystko, co dotyka tych plików, musi iść przez kontener Linux albo przez auto.
- **QNX binutils (`nm`, `ld`) to 32-bitowe binarki bez obsługi dużych i-węzłów.** Na bind-mountach
  Docker Desktop na Windows linkowanie pada z „Value too large for defined data type”.
  Dlatego `qnx_build.sh` kopiuje źródła do systemu plików kontenera.
- **M.I.B. 3.6.0 uruchamia wyłącznie `/mod/command.sh`**, nie `custom.sh`.
- **Na MMI nie ma `sha256sum`, `cksum`, `md5sum` ani `cmp`** – dostępne tylko
  `cp awk find ls wc df mount uname`.
- `/mnt/app/eso/hmi/lsd/jars/` **jest katalogiem fabrycznym** (11 plików) – nie wolno go usuwać
  przy rollbacku.
- **Rollback kasuje możliwość zbierania logów.** `rollback()` usuwa `installed.ok` i tworzy
  `rolledback.ok`; dyspozytor wchodzi wtedy w `ALREADY_ROLLED_BACK` i nic nie zbiera.
  Dlatego: **logi przed rollbackiem**, a awaryjnie marker `COLLECT_LOGS`.
- **`/tmp` w aucie to RAM (`/dev/shmem`) — rozstrzygnięte 2026-10-27.** Dowód przyszedł z błędu
  renderera: zapis pliku działa (leżą tam wszystkie nasze logi), ale `rename()` na tym samym
  katalogu zwraca **`errno 18` (EXDEV)**. Tak zachowuje się wyłącznie `/dev/shmem`, który
  `rename()` po prostu nie implementuje. Czyli: **logi zbierać przed wyłączeniem auta**, a kod
  piszący do `/tmp` nie może zakładać `rename()` (`overlay\overlay_ready.h` pisze jednym
  `open`+`write`).
- Zmienna `COLLECT` w `custom.sh` to **katalog wyjściowy** (`$MOD/collected`), nie marker –
  dlatego marker nazywa się `FORCECOLLECT` / plik `COLLECT_LOGS`.
- **Kopia oryginalnych plików auta leży na karcie SD** (`mod\...\backup\pre-v8s\`), nie w aucie.
  Rollback poziomu 1 działa tylko z tą samą kartą. Po instalacji `mod\` z karty kopiować na dysk.
- **Znaczniki tworzone w Windows dostają `.txt`.** „Nowy → Dokument tekstowy" daje `ROLLBACK.txt`,
  którego skrypt nie rozpozna. Dlatego paczka ma gotowe puste pliki w `ZNACZNIKI\`.
- **Zielone menu (GEM) jest częścią Javy HMI** (`lsd.sh:218-225`). Gdy HMI nie wstaje, nie ma GEM,
  M.I.B. ani rollbacku przez `ROLLBACK` – zostaje karta ratunkowa (`docs\12`). GEM bywa niedostępne
  do 5 min po zimnym starcie – to jeszcze nie bootloop.
- **M.I.B. działa z karty, nie z auta.** W GEM jest tylko dowiązanie do `/net/mmx/fs/sda0/esd/...`
  (`apps/launcher:99`). Paczkę instalacyjną kopiować **na kartę z M.I.B.**, nie na czystą.
- **Launcher M.I.B. rozbraja autorun po każdym przebiegu** (`Swdlautorun.txt` → `_Swdlautorun.txt`,
  `apps/launcher:29-36`). Karta ratunkowa po teście musi zostać ponownie uzbrojona.
- **Tryb aktualizacji M.I.B. = RCC, karta RO, ubogi `PATH`.** Skrypty z `Launcher/stuff.sh` działają
  na RCC; narzędzia (`rm`, `cp`, `mount`…) leżą na MMX – trzeba ustawić `PATH` jak `apps/launcher:6`.
  Karta jest tylko do odczytu, dopóki M.I.B. nie zrobi `mount -uw` (`apps/mounts:20`). A polecenie
  z nieudanym przekierowaniem (`cmd >> log`) **w ogóle się nie wykonuje** – nigdy nie dawać działań
  za przekierowaniem do logu. Wykryte testem 1 karty ratunkowej (`docs\12`).
- **Launcher w trybie aktualizacji wczytuje `mod/command.sh`** (`apps/launcher:117-118`). Karta
  instalacyjna nie może mieć uzbrojonego `Swdlautorun.txt`, a karta ratunkowa nie może mieć `mod/command.sh`.
- **Przy aucie potrzebny laptop albo telefon z czytnikiem kart** – rollback i wymuszone
  zbieranie logów wymagają skopiowania pliku na kartę.
- **`vb_flush()` w rendererze Luki zostawia WŁĄCZONE tablice atrybutów wierzchołków**
  (`render.c`, gałąź flat-program: dwa `glEnableVertexAttribArray`, zero `Disable`). Każdy nasz
  własny `glDrawArrays` po jego klatce – np. przeniesienie panelu na płótno – pobierałby też z
  jego wskaźników, z jego stride. Dlatego `composite_panel()` wyłącza **wszystkie** atrybuty
  przed włączeniem swoich. Jego kodu to nie psuje: włącza je sobie z powrotem w każdej klatce.
  Znalezione czytaniem kodu 2026-10-27, nie awarią – i żaden test na komputerze by tego nie złapał,
  bo nie mamy tu GL-a.
- **ASan w tym kontenerze nie startuje mniej więcej raz na sześć uruchomień** – wypisuje
  `AddressSanitizer:DEADLYSIGNAL` i **kręci się w nieskończoność na 100% CPU** zamiast paść.
  To znana niezgodność ASan z entropią ASLR nowych jąder; obejścia nie ma pod ręką
  (`setarch -R` wymaga syscalla `personality`, którego seccomp tego kontenera zabrania, a sysctl
  siedzi w maszynie wirtualnej Docker Desktop). Zmierzone 2026-10-27: 1 na 6, bez zmiany po
  `ASAN_OPTIONS=detect_leaks=0`. Dwa razy jednego dnia wyglądało to na regresję w kodzie, którego
  te testy nawet nie kompilują, a jeden zapomniany kontener spalił 5 godzin CPU. Od tej pory
  `run_host_tests.sh` uruchamia każdy test pod `timeout -k 10 60` i **ponawia** przy awarii startu;
  prawdziwa porażka nie jest ponawiana, bo test, który się wykonał, wypisuje „N OK, M FAIL".
- **Testy hosta budują się do `/tmp` w kontenerze, nie do drzewa źródeł.** Drzewo przychodzi jako
  bind-mount dzielony przez wszystkie kontenery; budowanie do `overlay/tests/.hostbuild` sprawiało,
  że dwa przebiegi naraz nadpisywały sobie binarki **w trakcie wykonywania**.
- **`tools\sim_rescue.sh` bez `/bin/ksh` nie odmawia – CICHO zdaje część testów.** Obraz bazowy ma
  tylko `dash`; bez `ksh` `ratunek.sh` w ogóle się nie uruchamia, więc każdy przypadek typu „drzewo
  auta bez zmian" wychodzi OK, a reszta sypie się 34 razy i wygląda na regresję. Uruchamiać:
  `apt-get install -y mksh && ln -sf /bin/mksh /bin/ksh`. Od 2026-10-27 skrypt sam tego pilnuje
  i kończy się kodem 2, zamiast udawać wynik.
- **Każdą symulację uruchamiać w OSOBNYM kontenerze – inaczej `sim_gate` wisi w nieskończoność.**
  `sim_gate.sh` podmienia `/bin/sleep` na atrapę, która po 30 obrotach kończy sesję. Linux odmawia
  zapisu do pliku, który jest właśnie wykonywany (**ETXTBSY, „Text file busy"**), więc wystarczy,
  że w tym samym kontenerze działa jeszcze `sleep` zostawiony przez poprzednią symulację –
  np. 30-sekundowa atrapa sondy z `sim_probe.sh`. Podmiana cicho nie dochodzi do skutku, pętla
  czekania bramy **śpi naprawdę**, licznik nigdy nie rusza i symulacja wypisuje `dmdt sc 4 77`
  bez końca. Zdarzyło się 2026-10-27 przy łączeniu `sim_probe; sim_gate` w jednym `docker run`
  i zjadło 35 minut: komunikat `/bin/sleep: Text file busy` **był** na stderr, ale zginął
  w `| tail -20`, bo `tail` nie wypisuje nic aż do końca potoku. Dwa wnioski, oba wdrożone:
  `sim_gate.sh` sprawdza teraz marker `SIM_GATE_SLEEP_STUB` w `/bin/sleep` i kończy się kodem 2
  z wyjaśnieniem, a wyjścia długich przebiegów idą do pliku, nie przez `tail`.
- **Dane testowe biorą się z prawdziwego śladu z auta, nie z wyobrażenia o nim.** Parser
  `V8H_LAYOUT` przechodził testy hosta przez kilka rund, bo testy karmiły go liniami, które sami
  wymyśliliśmy – a hook tej linii nigdy nie zapisywał (`LOG_INFO` przy loggerze `WARN`). Od Etapu 7h
  dane do testów czytnika układu to linie skopiowane bajt w bajt z przechwytu DSI z auta, a każdą
  zmianę czytnika sprawdzamy dodatkowo przepuszczając przez niego cały przechwyt z jazdy.
- **„Działa" mówi log z auta, nie zdjęcie.** Przełączanie kafelka po VIEW było opisane jako
  działające na podstawie zdjęć, choć renderer ani razu nie zalogował zmiany układu. Zanim funkcja
  trafi do dokumentacji jako działająca, w logu z auta musi być linia, która to pokazuje.
