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
        Strzałki na HUD działają według zgłoszeń, a przy S4 Sport zacinają się i mają opóźnienie. Sami nie mamy auta z HUD, więc tego nie sprawdziliśmy.
  - [ ] **Zgłoszenia #8, #9, #10 (A4 B9 z zakodowanym S4, `1.0.1`; logi, zdjęcia i filmy lokalnie w `Logi_bugow_uzytkownikow`, poza eksportem).** Z filmów co 0,5 s:
        - **Mapa fabryczna przy zmianie układu, kabel:** 8 przełączeń, od ok. 0,5 s do ok. 3 s. Rozrzut zgadza się z wymuszaniem kontekstu co 3 s w `mib2high-lvds-control.sh`
          (`GATE_TICK`), ale to dalej hipoteza: nie mamy logu przełączeń kontekstu. Kafelek jest wtedy czarny, strzałka wraca razem z mapą CarPlay.
        - **Po zapłonie, kabel, 1 film:** ekran powitalny S4, potem ok. 1 s czarny kafelek, mapa fabryczna poniżej 1 s, potem CarPlay.
        - **Bezprzewodowo:** VC cały czas na mapie fabrycznej, na obu układach, a MMI pokazuje CarPlay z trasą. Kafelek czarny bez strzałki. Pasuje do logu (brak `SETUP_READY`), a nakładkę
          uruchamia dopiero skrypt sesji mapy, więc bez strumienia nie ma kto narysować strzałki.
        - **Okładki bezprzewodowo:** zielony zastępnik, po ok. 1,5 s przy pierwszym utworze i ok. 5 s przy drugim. Przyczyny nie sprawdzałem; brak filmu porównawczego po kablu.
        - **Obszar Sport, oszacowanie ze zdjęć pod kątem, ±30 px na płótnie 1440×542:** mapa po lewej od obrotomierza, ok. `60 50 440 320` (x y w h). Do pomiaru na prostym zdjęciu albo z klas HMI
          (`LayoutMIB2HighB9Sport`). Kafelek strzałki w Sport jest już we właściwym miejscu (slot dużych zegarów). Wykrywanie: `HMITerminalImplMIB2#setSkin` w `sloginfo` (skin 1 = Sport).
  - [ ] **Jazda na `rc17` (2026-11-04, `work\auto_test_33`, filmy `<zdjecia>`, poza eksportem):**
        - **Strzałka po pełnym starcie MMI:** kafelek na miejscu dużych zegarów nad małymi (`timeout - default place`, `run-61`, `run-63`). DSITracer startuje z sesją, więc stan FctID 54 ze startu umyka, a `/tmp/vc-layout-last` po starcie jest pusty.
          `rc18`: model `NAV_VIEW_SIZE_CHOICE` (402521) do `/tmp/vc-view-size`, w nakładce **najniżej**: raport > zapis > model. Czy model po starcie ma prawdziwą wartość, pokaże auto: `layout slot=… (viewsize)` i `VC view size=` w logu Javy. Jeśli nie: `IViewSizeManager` albo wcześniejszy tracer.
        - **`Brak` na małych zegarach oddala kamerę:** `rc18` wysyła samo `/map`, bez `maneuverLayout=leftAligned`. Hipoteza; jeśli nie pomoże, to decyzja Apple Maps.

  - [ ] **`1.0.3-rc18`: układ Sport i krótsze miganie, zastąpione przez `rc19`; Sport i miganie dalej nietestowane.** Kopia sprzed wgrania: `work\karta_przed_103rc18`. Wzór: Luka (`AltScreenModule`, `KDK_GEOMETRY_AND_ANIMATION.md`).
        Skin z `getLastmodeStorage().getSkin()` co 500 ms do `/tmp/vc-skin`; obszar `view 4 90 90 360 300` (zastępuje oszacowanie ze zdjęć); kafelek w Sport `984,139`, kadr `0,0,328×180`, wartości
        z `LayoutMIB2HighB9Sport` w naszym `lsd.jar`; karta rozmowy `110,305`. Plik skina przeżywa koniec sesji, żeby pierwszy `showUI` miał właściwy układ. Wymuszanie mapy co 1 s (hipoteza z filmów), `sc 4 900` przed nakładką,
        `K3346_SETUP` w logu dla adaptera. **Czeka [Ty]:** jazda na `rc17`, potem test na zakodowanym S4 i po odkodowaniu.
  - [ ] **Test `rc18` w aucie (2026-11-04, logi `work\auto_test_34\run-64`..`run-69`, filmy i screeny `<zdjecia>`, poza eksportem):**
        - **Strzałka: negatywny, mój błąd.** `run-64`: duże zegary, model 1, `actualViewSize=1`, nakładka slot 0. `run-65`: małe zegary, model 0, `actualViewSize=2`, nakładka slot 1. „1 = small” u Luki to mała **mapa**, czyli duże zegary (`MMICombiDisplayStatus`: `VIEW_SIZE_SMALL=1`, `VIEW_SIZE_LARGE=2`). Filtr `0/1` odrzucał 2, a 0 znaczy tylko „model nigdy się nie zmienił”: `currentViewSize` startuje od 2 i `setViewSize` przy tej samej wartości nie woła słuchaczy. Zły slot szedł też do hooka (`K3346_LAYOUT_FROM_OVERLAY`), więc mapa dostawała zły obszar.
        - **Ramka ETA, dwa objawy przy tym samym URL-u na obu układach.** Czasy `ETA.MP4` zgadzają się 1:1 z 18 przełączeniami `K3346_SHOWUI` z `run-69` (film t=2 s ↔ 2:43,77), wszystkie `/map?showETA=yes`. (1) Losowy kształt: wąska albo szeroka z `20 km` i napisem `Google Maps` na mapie. W `rc17` (`IMG_0710`) kształt był stały: wąska na małych (`leftAligned`), szeroka na dużych. (2) Po VIEW na duże ramka 0,5–0,75 s pod obrotomierzem (przebieg co 0,25 s), potem wraca. Przyczyna (1) to hipoteza: telefon może pomijać identyczny URL. Zmiana URL-a nie wymusza kształtu: `IMG_0714` t=8–12 po `leftAligned` → `/map` zostaje wąska.
        - **Kamera: bez oddalania, ale test niepełny.** `run-69` startuje przy `Brak` (`K3346_CARD mode=1`), karty nie było w całej sesji, a w `rc17` kamera ruszyła w chwili zniknięcia karty. Przy `leftAligned` bez `/map` (t=2–6) i przy `/map` (t=8–12) skala ta sama. `/map` razem z `leftAligned` nie było w tym teście.
        - **Logi testu 3 są:** cztery zebrania (`run-66`–`run-69`), w `run-67` pusty `dmdt-gc.txt` i brak kilku plików.
  - [x] **`1.0.3-rc19`: układ z HMI i test dwóch par URL, sprawdzone w aucie, zastąpione przez `rc20`.** Kopia sprzed wgrania: `work\karta_przed_103rc19`. Wycofanie: `update.zip` z `work\release\1.0.3-rc11\`.
        - **Układ:** `IViewSizeManager.getCurrentViewSize()` z `combiBAPListener.viewSizeManagerFPK`, ta sama droga co `V8MFactoryTrace` (`actualViewSize` jest w logach od `run-25`). `K3346CapsuleFile.viewSizeSlot`: 1 → slot 1, 2 → slot 0. `/tmp/vc-view-size` trzyma slot. Kolejność w nakładce: raport > HMI > zapis. Ograniczenie: przed odpowiedzią VC HMI ma domyślne 2, plik zmieni się później i kafelek się przesunie; znane odczyty 1 przy dużych: 51 s (`run-25`), 63 s (`run-64`). Log: `VC view size=… slot=… init=…`.
        - **Test tymczasowy, do usunięcia po wizycie (`k3346_eta_url_aligned`):** opcja `Mapa` = URL-e z `k3346Layout=small/big`, `Obie` = para z `rc17` (`leftAligned` na małych). Między nimi przełącza dopiero VIEW albo zmiana karty.
        - **Opóźnienie:** bez naszych 150 ms po BAP 54 (zostaje ok. 140 ms z 290 ms). Klawisz → BAP 54 to 440–470 ms i tego nie skrócimy bez reakcji na sam klawisz. Jeśli dalej przeszkadza: przewidywanie po klawiszu, z wycofaniem przy braku raportu.
        - **Komputer:** natywne, nakładka 153/255/234, Java 177 + 88, `sim_gate` 154, instalator 392, ratunek 52, offsety i ELF: 0 FAIL. Mutacje: 23 z 23. Odczyt z HMI sprawdzi tylko auto.
        - **Czeka [Ty]:** małe i duże zegary po restarcie bez VIEW; VIEW przy `Mapa` i `Obie`; kamera w sesji zaczętej przy `Pełna`, przy obu opcjach. **Decyzje:** która para zostaje; usunąć wariant; Sport i miganie dalej do sprawdzenia.
  - [ ] **Test `rc19` w aucie (2026-11-04, logi `work\auto_test_35\run-70`..`run-75`, poza eksportem):** `run-70` test 1, `run-71`–`run-73` test 2 (ta sama sesja; `run-71` niepełny, jak wcześniej `run-67`), `run-74` testy 3 i 4, `run-75` test użytkownika.
        - **Strzałka: dobrze na obu układach, od pierwszej trasy, bez VIEW.** `run-70`: `slot=0 (small clocks, viewsize)`. `run-73`: `VC view size=1 slot=1` o 47,66 s.
        - **Kafelek 7 s po strzałce (test 1), przyczyna w logu.** `run-70`: 29 wiadomości iPhone'a w 0,55 s (`t=64316–64862`), nakładka dostaje je od razu z kodu natywnego. Java przerabia każdą po ok. 150 ms: lista o 71,88 s, `Sent 1 maneuver` o 72,13 s, a kafelek rysuje VC dopiero po BAP. To samo przy innych startach: `run-75`, 509,1 → 514,1 s. Do tego 1,35 s na `onStop`/`onShutdown` przy nieaktywnym prowadzeniu.
        - **Długie wczytywanie strzałki (test 2), przyczyna w logu.** `run-73`: trasa o 47,83–48,26 s, szyna łączy się o 48,515 s. Java dostaje `rx=30`, o 50,13 s ma manewr, a potem `bus.c` wysyła zaległą kolejkę: „rozłączono” (zatrzymanie o 50,7 s) i od nowa `rx=1…30`, do 58,04 s. Nakładka startuje dopiero o 55,17 s: trzy wywołania `pidin`, `sloginfo` i deklaracja mapy przed nią. Mapa na dużych zegarach do 56,79 s w układzie małych.
        - **Ramka ETA:** co 2–3 s bardzo dobrze, szybciej potrafi się źle wyświetlić. Bez filmu. Polecenia dochodzą: 109 z 109 potwierdzeń, status 0, najdłużej 21 ms. `Mapa` i `Obie` (dwie pary adresów): użytkownik nie widzi różnicy.
        - **Kamera przy `Brak`:** oddalona przy obu parach (`run-74`, sesje zaczęte przy `Pełna`, małe zegary). Użytkownik akceptuje. **Zamknięte.**
        - **Karta po `Pełna` w tej samej sesji wraca przy następnym manewrze, nie tylko po kablu** (obserwacja użytkownika; `run-75`: `Pełna` o 586,89 s, następny manewr o 611,66 s, zgodne, ale to nie dowód). Poprzedni opis był niepełny.
  - [x] **`1.0.3-rc20`: strzałka i kafelek bez opóźnień, szybkie VIEW, sprawdzone w aucie, zastąpione przez `rc21`.** Kopia sprzed wgrania: `work\karta_przed_103rc20`. Wycofanie: `update.zip` z `work\release\1.0.3-rc11\`.
        - **`CarplayBus.java`:** starsza ramka trasy nie jest przerabiana, gdy w gnieździe czeka już nowsza. Tylko ramki z pełnym stanem (klucz `diag_rx_seq`); „rozłączono”, koniec trasy i inne typy zawsze zostają. Zależy od `available()` na J9: jeśli zwraca 0, wszystko jak dotąd, a w logu nie ma `route frames skipped=`.
        - **`RouteGuidance.java`:** „rozłączono” przy nieaktywnym prowadzeniu bez `onStop`/`onShutdown`. Bez testu na komputerze, bo `BAPBridge` wymaga HMI. Log: przed pierwszym `RG activate` nie ma `Shutdown`.
        - **`bus.c`:** przy połączeniu `q_drop_covered_nolock` usuwa z kolejki ramki zastąpione przez kopię najnowszego stanu, pod tą samą blokadą. Log: `snapshot replaces N queued frames`.
        - **`mib2high-lvds-control.sh`:** `pidin` raz (`PROCS`), drugi tylko gdy w pierwszym widać nakładkę, bo może to być ta z kończącej się sesji. Kolejność: renderer → nakładka → mapa (`sc 4 900`) → `sloginfo` → czekanie → 901. Bez `sleep 1` po starcie nakładki. Czasy to szacunek, bo w skrypcie `date` nic nie zwraca: mapa ok. 1 s, strzałka ok. 2–3 s wcześniej.
        - **Układ przed raportem VC:** hook czyta też `/tmp/vc-view-size` (nakładka > HMI, raport BAP ma pierwszeństwo przed oboma). Log: `K3346_LAYOUT_EARLY` zamiast `K3346_LAYOUT_FROM_OVERLAY`.
        - **`showUI` najwyżej co 2 s** (`K3346_SHOW_MIN_INTERVAL_MS`, log `K3346_SHOW_HELD`). Czeka tylko to, co wymaga `showUI`; sekwencja w toku kończy się, ostatnia wartość wygrywa, seria wracająca do tego samego układu nie wysyła nic. Hipoteza z obserwacji użytkownika. Przy szybkim klikaniu mapa spóźnia się za zegarami do 2 s. Testy sekwencji poleceń mają limit wyłączony, sprawdza go `test_k3346_show_limit.c`.
        - **Usunięty test tymczasowy:** `k3346_eta_url_aligned` i adresy z `k3346Layout`. Zostaje para z `rc17`. `Mapa` i `Obie` wysyłają to samo, różnią się czasem na pasku.
        - **Opis karty:** `publish_src/README*.md`, `eta_mode.h`.
        - **Komputer:** natywne, nakładka 153/255/234, nowe `bus_purge_test`, `test_k3346_show_limit`, `CarplayBusTest` 11, `sim_gate` 186, instalator 392, ratunek 52, offsety i ELF: 0 FAIL. Mutacje: 49 z 49 (38 natywne i Java, 11 skrypt), po dopisaniu testów dla 5 ocalałych.
        - **Pułapka:** w Git Bash `grep -c $'\r$'` zlicza wszystkie linie, więc nie mówi nic o CRLF. Końce linii sprawdzać w Pythonie. Spośród plików `rc20` tylko `native/framework/bus.c` ma CRLF.
        - **Czeka [Ty]:** małe zegary, trasa w trakcie sesji: odstęp strzałka–kafelek. Duże zegary, trasa przed podłączeniem kabla: czy strzałka szybciej i czy mapa nie przeskakuje. VIEW: 10 szybkich, potem pojedyncze. Sport i miganie dalej do sprawdzenia.
  - [ ] **Test `rc20` w aucie (2026-11-05, logi `work\auto_test_36\run-76`..`run-81`, filmy `<zdjecia>\IMG_0722.MP4`, `IMG_0723.MP4`, poza eksportem):** `run-76`, `run-77` testy 1, 2 po 7 h postoju; `run-78` test 3, ten sam start co `run-77`; `run-79` testy 1, 2 po 4–5 min postoju; `run-80`, `run-81` testy 3 i rozmowa, ta sama sesja co `run-79`.
        - **Co działa:** `route frames skipped=29`, `=14`, czyli `available()` na J9 zwraca dane. `K3346_SHOW_HELD` trzyma odstępy co najmniej 2 s. Przy `run-79` bez zauważalnego opóźnienia kafelka.
        - **Ramka wraca po czekaniu 12 s, a nie po aktualizacji czasu:** `run-81` 07:56,050 → 08:08,229 (12,18 s), 06:08,241 → 06:20,717 (12,48 s).
        - **Filmy** (czasy zgodne z `K3346_SHOWUI` z `run-80`; `IMG_0722` t = log − 121,2 s, `IMG_0723` t = log − 161,9 s):
          - `Pełna`, 15,8–16,1 s: po `showUI` ramka szeroka, ok. 0,3 s później, gdy przesuwa się mapa, znika i nie ma jej 7 s, do następnego `showUI` na małych. Tak samo przy 27,1 s. Wszystkie URL-e z `showETA=yes`.
          - `Brak`, duże zegary: dwa kształty przy tym samym `/map?showETA=yes`. Wąska z lewej i auto pod prędkościomierzem (t=9,5–22), szeroka pośrodku z `24 km` (t=25,5 i dalej).
          - Złe miejsce przy VIEW to ok. 0,4–0,8 s: VC zmienia zegary, a telefon reaguje ok. 0,4 s po `showUI`. Część z tego to limit 2 s (np. 0,36 s przy 13,1 s), reszty nie usuniemy.
        - **Hipoteza z innych projektów (najnowszy kod, `repos\` zaktualizowane 2026-11-05):** Luka od `2aedb95b` (2026-11-03): `updateViewArea` → potwierdzenie → `showUI`. U nas było odwrotnie. Jego RE: bez `maneuverLayout` szerokość obszaru powyżej ok. 498 px daje wąską kolumnę przy krawędzi. Stary obszar małych zegarów ma 672 px, nowy 460 px, więc wąska ramka pasuje do liczenia na starym obszarze. To RE z MHI2Q, u nas nie zmierzone.
        - **Kafelek i strzałka: VC nie zgłosił `FctID 44`.** `run-76`, `-77`, `-78`: 0 zgłoszeń, także przy działającym VIEW w `run-78` (samo `54`, a RE Luki opisuje `44 → 54 → 44`). `run-79`, `-81`, `-59`, `-64`, `-69`, `-70`, `-73`, `-75`: po ok. 0,6–1 s od `17`. U nas nakładka i BAP robią to samo w obu przypadkach. `chefranov` #44: ten sam objaw, „Twój VC nigdy nie podniósł kafelka”.
        - **„Postój” to niepewne:** dobre starty są na postoju (odległość −1), złe w jeździe (8,4 km, 4,6 km). Nie tłumaczą tego `updateManeuverState(1)` (jest w obu) ani kolejność `17`/`39` (`run-73`). `run-76`, `-77` kończą się po 2–4 min, bo logi zebrano na początku jazdy. Hipoteza: brak kafelka VC to jedna przyczyna obu objawów; na dużych zegarach zegar zasłania strzałkę.
  - [ ] **`1.0.3-rc21`: kolejność poleceń przy VIEW i ramka ETA, NA KARCIE (2026-11-05), CZEKA NA TEST W AUCIE.** Kopia sprzed wgrania: `work\karta_przed_103rc21`. Wycofanie: `update.zip` z `work\release\1.0.3-rc11\`.
        - **Kolejność przy zmianie układu w działającej sesji:** `updateViewArea` → `showUI` → `forceKeyFrame` (`control_view_first`). Start sesji, zmiana samej ramki i Sport bez zmian. Limit 2 s i `forceKeyFrame` celowo bez zmian.
        - **Ramka od razu w `showUI` zmiany układu.** Osobne włączenie dalej czeka 12 s (`k3346_eta_held`).
        - **Jedno powtórzenie po 12 s** po każdej zmianie układu z ramką (`eta_retry_due_ms`, log `K3346_ETA_RETRY`), URL z nieużywanym `&k3346Retry=1`. Kasuje je każdy `showUI`. Obejmuje też ramkę, która była widoczna, bo tak znikała na filmie.
        - **`Brak` na dużych:** `/map?showETA=yes&maneuverLayout=topAligned`. Tego w planie nie było, dodane po filmach. Tego adresu w aucie jeszcze nie było.
        - **Komputer:** natywne, nakładka 153/255/234, `CarplayBusTest` 11, `sim_gate` 186, instalator 392, ratunek 52, offsety i ELF: 0 FAIL. Mutacje natywne 25 z 25, po dopisaniu testów dla 2 ocalałych. Karta: 28 plików, 0 różnic.
        - **Sprawdzi tylko auto:** czy ramka przychodzi od razu, czy przestaje znikać, czy powtórzenie po 12 s nie miga, czy `topAligned` przy `/map` daje szeroką ramkę, czy mapa nie gaśnie przy nowej kolejności.
        - **Czeka [Ty]:** testy ramki, a dla strzałki start w ruchu (bez postoju) i rano na postoju, logi dopiero po VIEW. Sport i miganie dalej do sprawdzenia.
  - [ ] **Test `rc21` w aucie, część 1 (2026-11-05, logi `work\auto_test_37\run-82`..`run-84`, filmy `<zdjecia>\IMG_0724.MP4` = `Brak`, `IMG_0725.MP4` = `Pełna`, poza eksportem).** Testy 3 i 4 (kafelek) jeszcze przed nami. Kodu po tym teście nie zmieniano.
        - **Test 1, udany:** ramka od razu po VIEW z dużych na małe w trakcie rozmowy (`02:16,913` i `02:30,266`, `view=1 eta=1`). Powtórzenie po 12 s wyszło (`02:42,269`).
        - **`Brak`, film t = log − 259,0 s:** 22 `showUI`, 22 razy właściwy kształt, ramka w każdej klatce, auto na środku. Hipoteza o układzie w adresie potwierdzona w tym zakresie; nie wiadomo, czy dał to adres, kolejność, czy oba.
        - **Opóźnienie przy `Brak` to nasz limit 2 s.** Szybko: zegary zmienione przy 21,5 s, `showUI` przy 23,3 s, ramka 23,5 s. Wolno (bez `K3346_SHOW_HELD`): zegary 33,5 s, `showUI` 33,81 s, ramka 34,0 s, razem do 0,5 s. Ramka przesuwa się 0,1–0,3 s po `showUI`.
        - **`Pełna`, film t = log − 430,75 s:** 26 `showUI`, ramka po 6 (11,2; 13,6; 22,2; 32,6; 47,1; 66,9 s). Duże 4 z 13, małe 2 z 13. Wszystkie polecenia z `showETA=yes`, potwierdzone. Bez VIEW sama nie przychodzi (54–58 s, 58–62 s).
        - **Korekta `rc21`: kolejność nie naprawiła `Pełna`.** Na starej kolejności (`IMG_0722`) ramka była po ok. 5 z 10, ale bez zielonej karty na mapie, więc tych liczb nie da się wprost porównać. Znikanie 0,3 s po ruchu mapy nie było więc całą przyczyną.
        - **Testy to Mapy Google, nie Apple.** Dla aplikacji zewnętrznej karta i ramka to jeden panel rysowany przez `CPSInstrumentClusterCardViewController` (dekompilacja Luki, iOS 26.6, u nas nie sprawdzone). `Brak` = `/map`, sama mapa bez karty, ramka zawsze. `Pełna` = adres z kartą, ramka bywa albo nie. Przyczyna nieznana.
        - **Odrzucone dla `Pełna`:** stała wysokość obszaru (próg ponad 202 px, mamy 265 i 300, brakuje na obu) i wiadomości trasy (co ok. 1 s, nie tłumaczą). Inne dane z telefonu, np. sam czas dojazdu, nie są wykluczone. Luka przy aktywnej strzałce używa `/map`, `chefranov` tej ramki nie używa, więc obu brak danych z auta. `run-59` (`rc15`): 8 z 8 przy przełączaniu ramki off → on na tym samym układzie.
        - **Możliwe, bez dowodu:** rozmiar obszaru. `chefranov` 1.1.4: Mapy Google rysowały pustą kartę przy obszarze o 20 px za niskim. My na małych 672×265, on 720×280, Luka 780×294.
        - **Nie wiadomo, czy powtórzenie po 12 s przywraca ramkę przy `Pełna`:** `08:34,716` wypadło po końcu filmu. Do sprawdzenia bez zmiany kodu.
  - [ ] **Test `rc21` w aucie, część 2 (2026-11-05, logi `work\auto_test_38\run-86` = test 5, `run-87` = test 6, `run-85` niepełny; filmy `<zdjecia>\1.MP4`, `2.MP4` = test 5, `3.MP4` = test 6, poza eksportem).** Testy 3 i 4 (kafelek) dalej przed nami.
        - **Test 5, `Pełna`, Mapy Google, film 2 t = log − 203,5 s (dopasowane do zmian zegarów):** ramka po 1 z 4 zmian układu (`03:25` duże nie, `03:49` małe tak, `04:07` duże nie, `04:29` małe nie). Dwie zmiany na małe (`03:49` i `04:29`) mają ten sam adres, a wynik inny.
        - **Powtórzenie po 12 s nie przywraca ramki:** 3 z 3 bez ramki zostały bez ramki (`03:37`, `04:19`, `04:41`), a widoczna została (`04:01`). Na klatkach co 1 s nic nie miga. Powtórzenie z `&k3346Retry=1` wygląda na bezskuteczne, skoro `showETA` się nie zmienia (wniosek, nie pomiar). Skracanie powtórzenia z planu `rc22` odpada.
        - **Klatka po klatce (co 0,125 s):** przy złym przypadku ramka nie pojawia się w żadnej klatce, a nie znika po chwili. Przy dobrym pojawia się razem z przestawieniem karty, ok. 0,45 s po `showUI`. Na dużych znika razem z przestawieniem karty.
        - **Start trasy (film 1):** przy `view 0` ramka pojawia się razem z trasą. Tego dnia nie sprawdzono, czy to zasługa obszaru, czy startu trasy.
        - **Przyczyna dalej nieznana:** w `run-84` odstęp od poprzedniego `showUI` nie tłumaczy ramki (dobre przy 2,05–4,52 s, a złe przy podobnych). Ślad z `rc15` (8 z 8, `run-59`) dotyczy przejścia `showETA` wył. → wł.; w `run-59` przejście 7,7 s po zmianie układu nie dało ramki, 11,8 s dało. `run-58`: 0,6 s nie, 1,2 s tak, 2,5 s nie.
        - **Test 6, `Brak`, Mapy Apple, film 3 t = log − 450,4 s:** mapa i ramka są na obu układach, 20 `showUI` w filmie, 15 `K3346_SHOW_HELD`. Na małych szeroka ramka stoi pośrodku mapy nad paskiem z ulicą, `leftAligned` nic tu nie przesuwa. Na dużych wąska, tylko `19:05 arrival`. Przy szybkim VIEW zostaje na chwilę poprzednia ramka: wąska przy małych zegarach, a szeroka częściowo schowana za obrotomierzem przy dużych. Co dokładnie użytkownik uważa za złą pozycję: **do potwierdzenia**.
  - [x] **Test `rc21` w aucie, część 3: testy 3 i 4, kafelek (2026-11-06, logi `work\auto_test_39\run-88`, `run-89`, ten sam start systemu, zebrane 06:14 i 06:16).** Użytkownik: strzałka szybko, razem z czarnym kafelkiem, we właściwym miejscu. VC zgłosił kafelek (`FctID 44`, `0:1:0:0:3`) w obu, 12 i 24 razy; w `run-76`–`run-78` (`rc20`) 0. **Błąd się nie powtórzył.** Może pomogła kolejność `updateViewArea` → `showUI` z `rc21`, ale to jedna próba, bez dowodu. Kodu kafelka nie zmieniano. Jeśli wróci: od razu `run individual script`, bez odłączania kabla.
  - [x] **`1.0.3-rc22`: sprawdzone w aucie (2026-11-06, `run-90`), zastąpione przez `rc23`.** Zbudowane 2026-11-05; po testach 3 i 4 bez poprawki kafelka. Kopia karty sprzed wgrania: `work\karta_przed_103rc22` (1070 plików, bez `collected`). Karta: 28 plików, 0 różnic, `INSTALL_ALL` i `ROLLBACK` jak przy `rc21`, `collected` zostało (90). Paczka: `work\release\1.0.3-rc22\`. Kopia źródeł sprzed zmian: `work\przed_rc22_src\`. Wycofanie: `update.zip` z `work\release\1.0.3-rc11\`. Plan: `trzeba-doda-obs-ug-ulubionych-wobbly-pascal.md`.
        - **`Pełna`: zmiana układu bez ramki, ramka po czasie (zgoda użytkownika).** Polecenie zmiany układu idzie z `showETA` wyłączonym, a ramkę włącza po czasie istniejąca ścieżka z `rc16` (`k3346_eta_held`, `eta_mode_poll`). Czas to test: kolejne zmiany układu przy `Pełna` biorą 3, 6, 12 s na zmianę (`k3346_eta_delay_ms`, log `K3346_ETA_DELAY ms= try= view=`). Potem zostaje jedna wartość. `Brak`, start sesji, chowanie przy rozmowie bez zmian. Skórka Sport zawsze 12 s.
        - **Powtórzenie po 12 s usunięte** (`eta_retry_*`, `K3346_ETA_RETRY`, `&k3346Retry=1`).
        - **Limit `showUI` 0,5 s** zamiast 2 s.
        - **Obszary:** `view 1` `300 88 480 294` (było `300 90 672 265`), `view 2`/`3` `490 88 460 294` (było `490 90 460 300`). `view 0` (bez trasy, bez ramki) i `view 4` (Sport, nietestowany) bez zmian. Węższe małe przesuwają ramkę Map Apple w lewo, do miejsca ramki Google (prośba użytkownika, także strzałka auta w lewo, dla wszystkich aplikacji). Ryzyko: przy 460 px Mapy Apple dają wąską ramkę z samym czasem przybycia; jeśli 480 też, następny krok 520–560.
        - **Grafika manewrów Luki `036cc755`** w `overlay/upstream/` jego diffem, bez konfliktów. Sprawdzone: przed zmianą kopia była zgodna z jego `52c3002f`, po zmianie z `036cc755`, poza naszymi 3 plikami. **Korekta:** `6e9f34d5` w `PROVENANCE.md` to commit `stb_truetype`, nie Luki; numeru commita Luki wcześniej tam nie było, teraz jest. Doszło: połączenie z serwerem po starcie EGL/GL i szybsze ponawianie (`main.c`, `server.c`).
        - **Nie przenosimy z `MHI2Q-2026-11-04`:** animacji, startu i gotowości `altscreen_render` (u nas inny renderer), startu USB.
        - **Komputer:** natywne (mock, URL-e, ETA, karta, skórka, log, limit, pokrętło, lista, `bus_purge`), nakładka 153/255/234, `CarplayBusTest` 11, offsety, ELF, instalator 392, ratunek 52, `sim_gate` 186: 0 FAIL. Mutacje 22 z 22, po dopisaniu testu skórki dla 1 ocalałej. W bibliotece 0 × `K3346_ETA_RETRY`/`k3346Retry`, 1 × `K3346_ETA_DELAY`.
        - **W aucie sprawdzi:** czy ramka przy `Pełna` przychodzi po 3, 6 lub 12 s i nie miga; czy ramka Map Apple na małych stoi po lewej i jest szeroka; czy Mapy Google przy `Brak` i `Pełna` dalej wyglądają dobrze; szybkie VIEW co ok. 1 s; ronda i zawracanie na strzałce.
  - [x] **Test `rc22` w aucie (2026-11-06, log `work\auto_test_40\run-90`, bez filmu; zdjęcie Map Apple w rozmowie, poza eksportem).** Użytkownik: testy 1–4 „bez zastrzeżeń, prawie wszystko wzorowo”, test 5 (strzałka na rondzie) nie zrobiony.
        - **Test 1, `Pełna`:** w logu 11 × `K3346_ETA_DELAY` (3 s ×4, 6 s ×4, 12 s ×3 po kolei). Każde opóźnienie, którego nie przerwał VIEW, kończy się `showUI` z `showETA=yes` (`K3346_ETA_WAIT on=0`), i użytkownik widział ramkę za każdym razem. Wniosek: wystarczy 3 s.
        - **Test 2, Mapy Apple, `Brak`, małe zegary:** ramka **wąska** (`17:23 arrival`) i razem ze strzałką auta na środku obszaru `view 1` (`300 88 480 294`). Użytkownik chce szeroką (godzina, minuty, km) i przy obrotomierzu, jak w Mapach Google. Przy 672 px (`rc21`, `run-87`) była szeroka, przy 480 px wąska. Przy `Pełna` Apple stawia ramkę z lewej, pod kartą (kadr z filmu `rc21`), więc problem dotyczy tylko `Brak`.
        - **Nazwa aplikacji jest w danych trasy:** iAP2 `0x5201`, TLV `0x0013` SourceName. W `k3346-rgi-ipod-rx.bin` są „Apple Maps” i „Google Maps”. `rgd_hook.c` parsował to pole, ale go nie używał.
        - **Testy 3, 4:** bez uwag. Szybkie VIEW: 2 × `K3346_SHOW_HELD` (330 i 344 ms), potem poszło.
  - [x] **`1.0.3-rc23`: osobny obszar dla Map Apple, 3 s przy `Pełna`, sprawdzone w aucie (2026-11-07, `run-91`, `run-92`), zastąpione przez `rc24`.** Kopia karty sprzed wgrania: `work\karta_przed_103rc23` (`rc22`, 1106 plików bez `collected`). Karta: 28 plików, 0 różnic, `INSTALL_ALL` i `ROLLBACK`, `collected` zostało (91). Paczka: `work\release\1.0.3-rc23\`. Wycofanie: kopia karty albo `update.zip` z `work\release\1.0.3-rc11\`. Plan: `trzeba-doda-obs-ug-ulubionych-wobbly-pascal.md`.
        - **Flaga Apple z `mm-ipod`:** `rgd_hook.c` zapisuje `/tmp/k3346-nav-apple` (`1` = Apple). Zapis następuje po zmianie aplikacji i ponownie, gdy pliku nie ma, bo skrypt sesji kasuje go razem z `CAPSULE_FILES`. Rozpoznanie: `native/nav_app.h`, nazwy „Apple Maps”, „Maps”, „Mapy”. Log `K3346_NAV_APP apple= name=` w `k3346-rgi-ipod-native.log`.
        - **Obszar 5:** `view 5 170 88 672 294`. Szerokość 672 ma dać szeroką ramkę, x=170 przesuwa środek z 636 na 506. Obszar dotyczy tylko Apple bez karty (`Brak`), na małych zegarach z trasą (`k3346_wire_view(view, sport, apple_free)`, `k3346_apple_free`). Telefon dostaje 6 obszarów (`ALTSCREEN_VIEW_COUNT` 6). Przy zmianie aplikacji idzie sam `updateViewArea` + `forceKeyFrame`, bez `showUI`, tak jak przy starcie trasy (w `run-90` Apple przestawił wtedy ramkę). Log `K3346_NAV_APPLE apple= free= queued=` w `carplay_hook.log`. Google, `Pełna` i duże zegary bez zmian. x=170 to wartość z kadrów pod kątem, ±40 px.
        - **`Pełna`: 3 s na stałe** (`K3346_ETA_DELAY_MS`). Cykl i `eta_delay_try` usunięte, log `K3346_ETA_DELAY ms=3000 view=`.
        - **Komputer:**
            - testy natywne z nowym `tests/test_k3346_nav_apple.c` (nazwy, czytnik pliku, sekwencja: Apple daje obszar 5, Google 1, `Pełna` 1, duże bez zmian, plik skasowany, zmiana w trakcie sekwencji);
            - pokrętło (test liczy 6 obszarów);
            - nakładka 153/255/234;
            - `CarplayBusTest` 11;
            - offsety, ELF;
            - instalator 392, ratunek 52, `sim_gate` 186: 0 FAIL;
            - mutacje 33 z 33.
        - **W aucie sprawdzi:** Mapy Apple przy `Brak` na małych zegarach: czy ramka jest szeroka i stoi przy obrotomierzu; czy po przełączeniu na Mapy Google ramka i auto wracają na miejsce z `rc22`; czy Mapy Apple przy `Pełna` się nie zmieniły; czy Mapy Google przy `Pełna` dają ramkę po ok. 3 s; czy duże zegary się nie zmieniły.
  - [x] **Test `rc23` w aucie (2026-11-07, logi `work\auto_test_41\run-91`, `run-92`, zdjęcia `<zdjecia>\1–6.JPEG`, film `pelne.MP4`, poza eksportem).**
        - **Nazwa aplikacji działa:** `K3346_NAV_APP apple=1 name=Apple Maps` / `apple=0 name=Google Maps` w `k3346-rgi-ipod-native.log`, `K3346_NAV_APPLE` w `carplay_hook.log`.
        - **Test 1, Apple, `Brak`, małe (zdj. 1):** ramka szeroka (`17:05 arrival 2 min 450 m`), ale godzina dotyka obrotomierza. Użytkownik: trochę w prawo.
        - **Test 2, Google:** bez uwag.
        - **Test 3, Apple, `Pełna`, małe (zdj. 2):** ramka wąska (`17:04 arrival`) pod kartą, na obszarze 480 px. Użytkownik chce pełną.
        - **Test 4, Google, `Pełna` (film):** na dużych zegarach ramki nie ma ani na chwilę (8–16 s, 26–32 s filmu), choć karta ma jedną linię i miejsca jest dość. Na małych ramka pojawia się 3–4 s po VIEW. W `run-92` (00:21:47–00:22:49) po każdym VIEW jest `K3346_ETA_DELAY ms=3000` i 3 s później `showUI` z `showETA=yes`, wszystko z ACK. To samo zadziałało w `rc22` (`run-90`, test 1 był na Google według kolejności nazw w `rx.bin`). Wniosek: „ramka po czasie” nie jest niezawodna. `Brak` działa na obu układach.
        - **Test 5, Apple, `Pełna`, duże (zdj. 4):** brak ramki, jak `run-51`. Zdjęcia 5 i 6 to Google na dużych (zielona karta, „ETA”): szeroka ramka przy `Brak` i przy `Pełna`. Do zdj. 6 doszło przez zmianę `Brak` z ramką → `Pełna` z ramką (`run-92` 00:20:32): karta doszła, ramka została.
        - **Rozpoznanie Apple/Google po wyglądzie:** Apple ma ciemną kartę i „arrival”, Google zieloną kartę i „ETA”.
  - [x] **`1.0.3-rc24`: ramki ETA po teście `rc23`, sprawdzone w aucie (2026-11-09, `run-94`, `run-95`), zastąpione przez `rc25`.** Kopia karty sprzed wgrania: `work\karta_przed_103rc24` (`rc23`, 1142 pliki bez `collected`). Karta: 28 plików, 0 różnic, `INSTALL_ALL` i `ROLLBACK`, `collected` zostało (93). Paczka: `work\release\1.0.3-rc24\`. Kopia źródeł sprzed zmian: `work\przed_rc24_src\`. Wycofanie: kopia karty albo `update.zip` z `work\release\1.0.3-rc11\`. Plan: `trzeba-doda-obs-ug-ulubionych-wobbly-pascal.md`.
        - **Obszary Apple:** `view 5 200 88 672 294` (Apple, `Brak`, małe, 30 px w prawo od `rc23`). Nowe `view 6 300 88 672 294` (Apple, `Pełna`, małe): karta przy x=300 jak dotąd, szerokość jak w `Brak`. `k3346_apple_area` (NONE/FREE/CARD) liczy się z wybranej opcji karty, nie z chwilowego URL-a. Telefon dostaje 7 obszarów.
        - **Apple, `Pełna`, duże: czas na pasku** (decyzja użytkownika), jak przy rozmowie. Natywnie `k3346_eta_map_effective(..., apple_card)` nie prosi o ramkę. Java: `K3346NavBarText.timesOnBar(..., appleCard)`, a `BAPBridge.pollAppleCard` czyta `/tmp/k3346-nav-apple` i `/tmp/k3346-card-mode`.
        - **`Pełna`, zmiana układu: najpierw ramka, potem karta.** `showUI` zmiany układu idzie jak `Brak` (`/map?showETA=yes`), a po 1,5 s (`K3346_CARD_AFTER_BOX_MS`, `card_due_ms`) URL z kartą i ramką. Log `K3346_CARD_AFTER ms=1500 view=`. Usunięte `K3346_ETA_DELAY_MS` i log `K3346_ETA_DELAY`. 12 s z `run-59` zostaje dla skórki i dla ramki włączonej tuż po VIEW.
        - **Komputer:**
            - testy natywne (`eta_tries` przepisany, `nav_apple` rozszerzony, URL-e, karta, skórka, limit, pokrętło 7 obszarów);
            - nakładka 153/255/234;
            - `CarplayBusTest` 11;
            - Java `K3346NavBarText` 117 i `K3346CapsuleFile` 90 OK;
            - offsety, ELF;
            - instalator 392, ratunek 52, `sim_gate` 186: 0 FAIL;
            - mutacje 40 z 40 natywnych (po dopisaniu testu dla 1 ocalałej) i 5 z 5 Javy.
        - **W aucie sprawdzi:**
            - Apple `Brak` małe: ramka dalej od obrotomierza;
            - Apple `Pełna` małe: szeroka ramka pod kartą;
            - Apple `Pełna` duże: czas na pasku, bez ramki;
            - Google `Pełna` przy VIEW: ramka od razu, karta ok. 1,5 s później, nic nie miga;
            - `Brak` bez zmian.
  - [x] **Test `rc24` w aucie (2026-11-09, logi `work\auto_test_43\run-94`, `run-95`, film `<zdjecia>\IMG_0775.MOV`, poza eksportem).** Mapy Apple, karta `Brak`, czas dojazdu „Mapa”, auto stoi przez cały film.
        - **2–3 s filmu, małe zegary, start trasy:** ramka szeroka (`22:27 arrival · 14 min · 12 km`), przy obrotomierzu. Do telefonu poszła sama zmiana obszaru, bez `showUI` (`run-95`, 00:13:16).
        - **7–16 s, duże:** ramka wąska (`22:27 arrival`), pośrodku. Obszar → od razu `showUI` (`topAligned`), 00:13:53.
        - **17–21 s, małe:** ramka wąska, środek mniej więcej dobry, zostaje do końca filmu. Obszar → od razu `showUI` (`leftAligned`), 00:14:02.
        - **Wniosek:** wąska ramka na małych = po przejściu przez duże zegary, szeroka = trasa zaczęta na małych. To tłumaczy różne zdjęcia z `rc24`. „Postój” jako przyczyna odpada.
        - **Decyzja użytkownika (2026-11-10):** ramka ma być pełna (godzina, pozostały czas, dystans) na obu układach, także po VIEW, jak w Mapach Google.
        - **Czego nie wiadomo:** dlaczego `showUI` 2–3 ms po potwierdzeniu obszaru zostawia wąską ramkę. Logi WRN nie pokazywały numeru obszaru, który poszedł po kablu.
        - **Luka `1299619d` (2026-11-09; po `git pull` 2026-11-10: Luka `219bf3ae`, chefranov `d229c03`):** adres mapy bez kart zmieniony na `maneuverLayout=topAligned` w każdej strefie. Jego uzasadnienie (analiza kodu Apple, bez testu w aucie): układy boczne rezerwują szerokość kolumny karty także bez kart. U nas `Brak` na małych idzie z `leftAligned`. Dowodu z auta nie ma.
  - [x] **`1.0.3-rc25`: WERSJA PRÓBNA, pełna ramka Map Apple na obu układach, sprawdzona w aucie (2026-11-10, `run-96`, `run-97`).** Kopia karty sprzed wgrania: `work\karta_przed_103rc25` (`rc24`, 1179 plików bez `collected`). Karta: 28 plików, 0 różnic, `INSTALL_ALL` i `ROLLBACK`, `collected` zostało (96). Paczka: `work\release\1.0.3-rc25\` (zbudowana od nowa, próbna z 2026-11-07 usunięta). Kopia źródeł sprzed zmian: `work\przed_rc25_src\`. Wycofanie: kopia karty albo `update.zip` z `work\release\1.0.3-rc11\`. Plan: `chce-zmieni-w-pojazdach-sprz-tach-expressive-diffie.md`. Bez eksportu publicznego.
        - **Obszar 7:** `view 7 384 88 672 294`: Apple, `Brak`, duże zegary z trasą. Szerokość jak na małych (672), środek x=720 jak dotąd. `K3346_VIEW_APPLE_BIG`, `k3346_wire_view` (widok 3 + Apple bez karty → 7; Sport ma pierwszeństwo, obszar 4). Telefon dostaje 8 obszarów (`ALTSCREEN_VIEW_COUNT` 8). Zmiana aplikacji Google ↔ Apple przestawia sam obszar także na dużych (3 ↔ 7). Obszar wchodzi ok. 100 px pod każdy duży zegar: do obejrzenia w aucie.
        - **Cykl czterech sposobów zmiany układu** (tylko Apple + `Brak` + ramka na mapie + zmiana układu z trasą, nie Sport). Licznik `g_apple_try` rośnie przy przejściu na duże, powrót na małe bierze ten sam sposób. Po starcie procesu: para 1 NOW, 2 TOP, 3 GAP, 4 BOX_AGAIN, 5 NOW i dalej w kółko. Log `K3346_APPLE_TRY n= kind= view=` (`kind`: 0 NOW, 1 TOP, 2 GAP, 3 BOX_AGAIN).
            - **NOW:** jak `rc24`, obszar → od razu `showUI` → `forceKeyFrame`.
            - **TOP:** adres `topAligned` na obu układach (reguła Luki). `showUI` idzie tylko wtedy, gdy telefon ma inny adres albo nie ma ramki. Powrót na małe to sam obszar 5 + `forceKeyFrame`, jak start trasy. Pole `ui_url_slot` pamięta adres, który telefon naprawdę ma. Gdy aplikacja przestaje być „Apple bez karty” (Google, `Pełna`), a adres nie pasuje do układu, idzie pełne `showUI`.
            - **GAP:** obszar → 1000 ms przerwy (`K3346_APPLE_SHOW_GAP_MS`, `show_after_ms`) → `showUI`. VIEW w trakcie przerwy zeruje ją.
            - **BOX_AGAIN:** obszar → `showUI` bez ramki → po 1000 ms (`K3346_APPLE_BOX_AGAIN_MS`) `showUI` z ramką. Ramka znika na ok. 1 s.
        - **Różnice względem zatwierdzonego planu:** sposób TOP zostawia w telefonie adres `topAligned`, więc para po nim (GAP) zaczyna z tym adresem na małych. VIEW naciśnięty w trakcie przerwy przy powrocie nie zwiększa licznika.
        - **Diagnostyka na stałe (WRN, `carplay_hook.log`):** `K3346_VIEWAREA view= wire= apple= rc=` przy każdym `updateViewArea` (`wire` = numer obszaru, który poszedł po kablu) i `K3346_VIEWAREA_ACK view= status= valid=`.
        - **Weszły teksty „silnik wył. ~1 min / wł.”** w `custom.sh` i `README_CarPlay_VC.txt` (punkt niżej).
        - **Komputer:**
            - testy natywne hosta 12 z 12 (`nav_apple` rozszerzony o obszar 7 i cztery sposoby, URL-e, karta, skórka, pokrętło: 8 obszarów);
            - mutacje 43 z 43, po uproszczeniu jednego nadmiarowego warunku (`control_skip_show`);
            - nakładka: `overlay_cfg` 293, `overlay_ready` 41, `overlay_layout` 153, `overlay_blit` 234, `overlay_fbo` 223, kapsuła 76, karta rozmowy 56, `overlay_feed` 62, `rgd_list_restore` OK, `OVERLAY_HOST_TESTS=PASS`;
            - `CarplayBusTest` 11, Java `K3346NavBarText` 117 i `K3346CapsuleFile` 90 (na klasach z paczki; źródła Javy bez zmian, wpisy w obu JAR-ach identyczne z `rc24`);
            - offsety `ALL PASS`, ELF `PASS`;
            - instalator 392, ratunek 52, `sim_gate` 186 (mksh; pod dash 93): 0 FAIL;
            - w bibliotece napisy `K3346_APPLE_TRY`, `K3346_VIEWAREA`, `K3346_VIEWAREA_ACK`; conf w paczce i na karcie ma 8 linii `view` (wartości obszarów nie sprawdza żaden test, kontrola ręczna).
        - **Dwie wady testów znalezione przy okazji (nie regresje):**
            - `overlay/tests/run_host_tests.sh` kończył się `FAIL: rgd_list_restore nie wystartowal`, choć test przechodził: `run_test` szuka podsumowania „N OK, M FAIL”, a ten test wypisuje tylko `rgd list restore OK`. Poprawione: test idzie bez `run_test` (nie ma ASan). Kopia sprzed zmiany: `work\przed_rc25_src\overlay_tests\`.
            - `tests/test_k3346_lifecycle_log.c` nie linkuje się sam (`undefined reference to altscreen_unregister_session`), tak samo na źródłach `rc24`. Uruchomiony z atrapą tej funkcji. Do naprawy w źródłach testu.
        - **W aucie sprawdzi:** 8 par VIEW (duże 6 s, małe 6 s) na Mapach Apple przy `Brak`, z filmem. Przy której parze ramka jest pełna na dużych i po powrocie na małe; czy na dużych ramka albo mapa nie zachodzi na zegary.
        - **Co dalej (`rc26`):** pełna już przy NOW → zostaje sam obszar 7, cykl do usunięcia. Pełna dopiero przy TOP, GAP albo BOX_AGAIN → ten sposób na stałe. Pełna, ale zachodzi na zegary → zwężać obszar 7 (560, potem 520). Wąska zawsze → log `K3346_VIEWAREA`, potem inna wysokość obszarów na małych i dużych (jak w `rc21`) i animacja 250 ms jak u Luki.
  - [x] **Test `rc25` w aucie (2026-11-10, logi `work\auto_test_44\run-96`, `run-97` (ta sama sesja, `run-97` ma 1 linię więcej), film `<zdjecia>\IMG_0776.MOV`, 103 s, poza eksportem).** Mapy Apple, `Brak`, postój. Film t = log − 186,6 s (11 odstępów między VIEW zgadza się do 0,5 s). Kadry co 1 s z całego filmu i co 0,25 s wokół par 3 i 4.
        - **Użytkownik:** ramka pełna na dużych i po powrocie na małe, na dużych nic nie wchodzi pod zegary. W połowie filmu dwa razy „coś się rozjechało”.
        - **6 par VIEW, wszystkie w filmie:** 1 NOW (film 6 s / 15 s), 2 TOP (22,5 / 30,5), 3 GAP (39,5 / 46,5), 4 BOX_AGAIN (56,5 / 66,5), 5 NOW (77,5 / 84,5), 6 TOP (90,5 / 95,5). Po kablu zawsze obszar 7 na dużych i 5 na małych (`K3346_VIEWAREA wire=`), wszystko z `rc=0`.
        - **Ramka pełna (`12:59 arrival · 3 min · 2,1 km`) w każdej parze, na obu układach**, także przy NOW (2 z 2 na duże i 2 z 2 na małe). Sposób wysyłania nie ma znaczenia.
        - **Wniosek:** wąską ramkę w `rc24` dawał wąski obszar na dużych zegarach (460 px), a telefon zostawiał ją po powrocie na małe. Obszar 7 (672 px) to naprawia.
        - **„Rozjechało się” = para 4 (BOX_AGAIN):** ramka znika ok. 1 s po VIEW i wraca po ok. 1,5 s (film 57,75–58,5 s na dużych i 67,5–68,75 s na małych). To zamierzone działanie tego sposobu, nie błąd. Przy GAP mapa przestawia się ok. 1 s po zmianie układu, ramka zostaje.
        - **Przy każdym VIEW** przez 1 klatkę (0,25 s) widać ramkę w starym miejscu, częściowo za obrotomierzem. Tak samo przy każdym sposobie.
        - **Do `rc26` (zgoda użytkownika 2026-11-10):** zostaje obszar 7 i sposób NOW, cykl czterech sposobów usunięty. Log `K3346_VIEWAREA` zostaje.
  - [x] **`1.0.3-rc26`: pełna ramka Map Apple na obu układach, bez cyklu próbnego, sprawdzona w aucie (2026-11-10, `run-98`), zastąpiona przez `rc27`.** Kopia karty sprzed wgrania: `work\karta_przed_103rc26` (`rc25`, 1214 plików bez `collected`). Karta: 28 plików, 0 różnic, `INSTALL_ALL` i `ROLLBACK`, `collected` zostało (98). Paczka: `work\release\1.0.3-rc26\`. Kopia źródeł sprzed zmian: `work\przed_rc26_src\` (`rc25`). Wycofanie: kopia karty albo `update.zip` z `work\release\1.0.3-rc11\`. Bez eksportu publicznego.
        - **`altscreen_session.c` = plik z `rc24` (`work\przed_rc25_src`) plus trzy rzeczy z `rc25`:** `ALTSCREEN_VIEW_COUNT` 8; logi `K3346_VIEWAREA view= wire= apple= rc=` i `K3346_VIEWAREA_ACK`; zmiana aplikacji przestawia sam obszar na obu układach (`last_ui_slot >= 0`, na dużych 3 ↔ 7). Usunięte: `g_apple_try`, `apple_try`, `ui_url_slot`, `control_url_slot`, `control_skip_show`, `show_after_ms`, parametr `url_slot`, log `K3346_APPLE_TRY`.
        - **`eta_mode.h`:** zostaje `K3346_VIEW_APPLE_BIG 7` w `k3346_wire_view`; usunięte `K3346_TRY_*`, `K3346_APPLE_SHOW_GAP_MS`, `K3346_APPLE_BOX_AGAIN_MS`, `k3346_apple_try_kind`, `k3346_apple_try_applies`. `altscreen_airplay.c` (`ALTSCREEN_VIEW_COUNT_MAX 8`) i `mib2high-lvds-viewareas.conf` (8 obszarów) bez zmian względem `rc25`.
        - **Testy:** `test_k3346_nav_apple.c`: zamiast par z czterema sposobami 3 pary VIEW Apple bez karty (obszar 7 → `topAligned` z ramką, obszar 5 → `leftAligned` z ramką), rozmowa na dużych w obszarze 7, karta (obszar 6 i kolejność z `rc24`), Google ↔ Apple na dużych (3 ↔ 7, sam obszar), Sport (obszar 4). `test_v8r_layout_urls.c`: bez asercji cyklu, tabela obszaru 7 zostaje.
        - **Komputer:**
            - testy natywne: mock, URL-e, `eta_tries`, karta, skórka, limit, `nav_apple`, `lifecycle_log` (atrapa w `stubs.c`), pokrętło 8 obszarów, `rgd_list_restore`, `bus_purge`;
            - mutacje 47 z 47 (40 z `rc24`, 7 nowych dla obszaru 7 i liczby obszarów; mutacja `ALTSCREEN_VIEW_COUNT_MAX 7` łapie test pokrętła);
            - nakładka `OVERLAY_HOST_TESTS=PASS` (cfg 293, ready 41, layout 153, blit 234, fbo 223, kapsuła 76, karta rozmowy 56, feed 62, `rgd_list_restore`);
            - `CarplayBusTest` 11, Java `K3346NavBarText` 117, `K3346CapsuleFile` 90;
            - offsety `ALL PASS`, ELF `PASS`;
            - instalator 392, ratunek 52, `sim_gate` 186: 0 FAIL;
            - w bibliotece `K3346_APPLE_TRY` 0, `K3346_VIEWAREA` jest, `K3346_VIEWAREA_ACK` 1; conf na karcie: 8 linii `view`, obszar 7 `384 88 672 294`.
        - **Pułapki bramki:** `run_host_tests.sh` nakładki zapisuje pliki w katalogu źródeł, więc nie na `/src:ro` (kopia w kontenerze). `overlay_feed` pod ASan: `AddressSanitizer:DEADLYSIGNAL` od startu przy losowaniu adresów; działa z `setarch x86_64 -R` i `--security-opt seccomp=unconfined`.
        - **W aucie sprawdzi:** Mapy Apple, `Brak`, kilka razy VIEW: ramka pełna na obu układach, bez znikania ramki po VIEW (jak para 4 w `rc25`). Google i `Pełna` bez zmian.
  - [x] **Test `rc26` w aucie (2026-11-10, logi `work\auto_test_45\run-98`, bez zdjęć).**
        - **Mapy Apple, `Brak`, VIEW:** bez błędu, „taki efekt, jaki chciałem”. Pełna ramka na obu układach, bez znikania.
        - **Mapy Google, `Brak`, VIEW:** przełączanie dobre, ale na małych zegarach ramka **bez dystansu** (godzina i pozostały czas). W logu Google na małych: obszar 1 (`300 88 480 294`), `leftAligned`.
        - **Uwaga użytkownika:** w Mapach Apple znacznik auta na małych stoi mocno w lewo, przy obrotomierzu. Apple stawia ramkę i znacznik na środku obszaru, więc obszarem nie da się ich rozdzielić. **Decyzja: Apple zostaje bez zmian.**
  - [x] **`1.0.3-rc27`: pełna ramka Map Google na małych zegarach przy `Brak`, nie wgrany do auta, wszedł w `rc28` (2026-11-10).** Kopia karty sprzed wgrania: `work\karta_przed_103rc27` (`rc26`, 1250 plików bez `collected`). Karta: 28 plików, 0 różnic, `INSTALL_ALL` i `ROLLBACK`. Paczka: `work\release\1.0.3-rc27\`. Kopia źródeł sprzed zmian: `work\przed_rc27_src\`. Wycofanie: kopia karty albo `update.zip` z `work\release\1.0.3-rc11\`. Bez eksportu publicznego.
        - **Zmiana:** Google (każda aplikacja inna niż Apple), karta `Brak`, małe zegary z trasą: obszar `300 88 672 294` (geometria obszaru 6, numer 6; lewa krawędź jak obszar 1, szerokość jak dla Apple). Ramka Google stoi przy lewej krawędzi, więc ma zostać przy obrotomierzu. Znacznik auta Google przesunie się ok. 100 px w prawo. Google `Pełna` dalej obszar 1, duże zegary dalej 3, Sport 4.
        - **Kod:** `eta_mode.h`: `K3346_OTHER_FREE` (3) z `k3346_apple_area(0, K3346_CARD_NONE)`, `K3346_VIEW_OTHER_FREE` = 6 w `k3346_wire_view` dla widoku 1. `altscreen_session.c` tylko komentarz: istniejące ścieżki (`send_view_event`, `v8s_control_tick`, przestawianie obszaru w `eta_mode_poll`) liczą obszar z tej funkcji. Przełączenie `Pełna` → `Brak` w trakcie czekania na kartę wysyła teraz sam obszar 6 (wcześniej nic). Conf: wartości bez zmian, tylko komentarze.
        - **Testy:** `test_v8r_layout_urls.c` (tabela z `K3346_OTHER_FREE`), `test_k3346_nav_apple.c` (Google `Brak` małe 6, `Pełna` 1, Google ↔ Apple 6 ↔ 5, VIEW 3 ↔ 6, Google `Pełna` z ramką najpierw na obszarze 1), `test_k3346_eta_tries.c` (sam obszar 6 przy `Brak` w trakcie czekania), pomocnicze `wire_of()` w `card`, `show_limit`, `eta_tries`.
        - **Komputer:**
            - testy natywne wszystkie OK;
            - mutacje 55 z 55 (8 nowych dla Google; `ALTSCREEN_VIEW_COUNT_MAX 7` łapie test pokrętła, plik bez zmian);
            - nakładka `OVERLAY_HOST_TESTS=PASS`;
            - `CarplayBusTest` 11, Java 117 i 90;
            - offsety `ALL PASS`, ELF `PASS`;
            - instalator 392, ratunek 52, `sim_gate` 186: 0 FAIL;
            - conf w paczce: 8 obszarów, wartości jak `rc26`.
        - **W aucie sprawdzi:** Google `Brak` małe z trasą: ramka z godziną, czasem i dystansem, przy obrotomierzu; VIEW na Google i Apple jak w `rc26`; Google `Pełna` małe jak dotąd.
  - [x] **`1.0.3-rc28`: kafelek rozmowy jako pastylka (Przydymiona / Jasna / Wył. w menu MMI) + zmiana z `rc27`, sprawdzony w aucie (2026-11-11, `run-99`), zastąpiony przez `rc29`. Kafelek rozmowy: bez relacji z testu.** Kopia karty sprzed wgrania: `work\karta_przed_103rc28` (`rc27`, 1252 pliki bez `collected`, `rc27` nieinstalowany: brak nowych logów po `run-98`). Karta: 28 plików, 0 różnic, `INSTALL_ALL` i `ROLLBACK`. Paczka: `work\release\1.0.3-rc28\`. Kopia źródeł sprzed zmian: `work\przed_rc28_src\`. Wycofanie: kopia karty albo `update.zip` z `work\release\1.0.3-rc11\`. Bez eksportu publicznego.
        - **Skąd:** użytkownikowi nie pasował kolor i kształt kafelka z `rc10` (granatowa szyba 320×78, dwie linie). Wizualizacje: `work\wizualizacje\kafelek_rozmowy.png` (A–D), `kafelek_rozmowy_pastylka.png` (B1–B3, kolory z `IMG_0777`). Wybór: B2 i B1 do wyboru w menu MMI, plus wyłączenie. Podgląd z kodu projektu: `kafelek_rozmowy_rc28.png`.
        - **Menu MMI (`K3346EtaRow`):** wiersz „Karta rozmowy na VC” / „Call card on VC”, model `2500397`, fokus `2509915`, klucz `1005`, teksty `2509925–2509927` + `TEXT_OFF`. Wartości: 0 Przydymiona (domyślnie), 1 Jasna, 2 Wył. Plik `/tmp/k3346-callcard-mode` (`"N\n"`).
        - **Rysowanie (`overlay/call_card.h`):**
            - pastylka 320×44, promień połowy wysokości, jedna linia;
            - kropka x=22 w kolorach systemowych CarPlay: zielony 48,209,88, pomarańczowy 255,159,10, szary 152,152,157;
            - nazwa pogrubiona 24 px, potem „ · licznik” albo „ · Dzwoni/Łączenie” 22 px;
            - nazwa przycinana do miejsca, które zostaje po liczniku;
            - przydymiona `(84,92,106)` krycie 0,62, jasna biel krycie 0,36;
            - `call_card_render(..., style, ...)`, `CALL_CARD_STYLE_*`.
        - **Nakładka (`platform_harman.c`):** `call_card_poll` co odczyt czyta opcję przez nowe `overlay_option_read(path, previous, max)` (`overlay_layout.h`). Zmiana wersji przerysowuje kafelek od razu. Przy Wył. kafelek nie jest rysowany, a licznik liczy dalej. Log `call card style a -> b`.
        - **Wył. wszędzie:** natywnie `call_card_shown_read()` → `k3346_call_card_shown(call, mode)` (`eta_mode.h`, `K3346_CALLCARD_MODE_PATH`, `K3346_CALLCARD_OFF`). Java `BAPBridge.pollCallCard` przez `K3346CapsuleFile.readCallCardMode`/`callCardShown`. Przy Wył. rozmowa nie zabiera ramki z mapy na dużych zegarach i nie przenosi czasu na pasek.
        - **Położenie (`mib2high-overlay.conf`, domyślne w `overlay_cfg.h`):** małe `734,310`, duże `560,322`, Sport `110,322` (środek starej karty). Walidacja: wysokość 32–160. Do dostrojenia po zdjęciu.
        - **Komputer:**
            - nakładka `OVERLAY_HOST_TESTS=PASS` (cfg 303, layout 163, karta rozmowy 97, kapsuła 112);
            - natywne wszystkie OK, z nowym przypadkiem Wył./Przydymiona/Jasna w trakcie rozmowy na dużych;
            - Java: `K3346EtaRowTest` 216, `K3346CapsuleFile` 102, `NavBarText` 117, `CarplayBusTest` 11;
            - mutacje: natywne 60 z 60, nakładka 19 z 19, Java 13 z 13;
            - offsety, ELF;
            - instalator 392, ratunek 52, `sim_gate` 186: 0 FAIL;
            - napis pliku opcji w `vc_overlay`, bibliotece i obu JAR-ach.
            - `BAPBridge.pollCallCard` sam nie ma testu, testowane są jego funkcje w `K3346CapsuleFile`.
        - **Pułapka bramki:** testy rysowania (karta rozmowy, kapsuła) potrzebują czcionek z `/work/mmx_app`. Bez `-v work:/work` dają `SKIP` i mniej przypadków (karta 56 zamiast 97, kapsuła 76 zamiast 112).
        - **W aucie sprawdzi:**
            - nowy wiersz w menu z trzema wartościami;
            - wygląd i położenie przy Przydymiona na małych i dużych;
            - przełączenie na Jasna w trakcie rozmowy;
            - Wył.: kafelek znika, ramka czasu zostaje na dużych;
            - pamięć wyboru po restarcie;
            - plus test z `rc27`: Google `Brak` małe z dystansem.
  - [x] **Test `rc28` w aucie (2026-11-11, logi `work\auto_test_46\run-99`, filmy `<zdjecia>\apple_pelne.MP4`, `google_pelne.MP4`, `google_brak.MP4`, poza eksportem).** Czas dojazdu „Mapa”. Kadry co 0,5 s z trzech filmów.
        - **Apple, `Brak`:** dobrze.
        - **Apple, `Pełna`, małe:** ramka telefonu tylko `06:43 arrival`, pod kartą manewru, w kolumnie karty. Szeroki obszar 6 (672 px) nic nie dał. Duże: czas na pasku `06:43 · 15 min · 12 km`, dobrze.
        - **Google, `Pełna`, małe:** ramka `07:34 ETA · 49 min` bez dystansu, karty Google nie widać. Ok. 2 s po VIEW cała mapa znika na jedną klatkę: drugie `showUI` z kartą (`K3346_CARD_AFTER`). Duże: raz karta bez ramki, raz ramka bez karty, na pasku nic („Mapa”).
        - **Google, `Brak`, małe:** ramka bez dystansu mimo obszaru 6 z `rc27`. Na dużych (obszar 460 px, `topAligned`) pełna: `07:35 ETA · 49 min · 45 km`. Różnicą jest `maneuverLayout`, nie szerokość (zgodne z notatką w `gauge_layout_urls.h` i z Luką `1299619d`).
        - **Menu:** wiersz `2500397` dodany (`row_added`), wybór `key=1005` zapisany.
        - **Wniosek:** kształt i obecność ramki telefonu zależą od aplikacji, układu i karty. Nie da się ich wymusić, a próby dają miganie.
        - **Decyzje użytkownika:** ETA ma być na mapie przy zegarach (nie na pasku). Własna ramka czasu zawsze, także przy `Brak`. Wygląd jak kafelek rozmowy.
  - [x] **`1.0.3-rc29`: własna ramka czasu w nakładce zamiast ramki telefonu, sprawdzona w aucie (2026-11-11, zdjęcia, bez logów), zastąpiona przez `rc30`.** Kopia karty sprzed wgrania: `work\karta_przed_103rc29` (`rc28`, 1286 plików bez `collected`). Karta: 28 plików, 0 różnic, `INSTALL_ALL` i `ROLLBACK`. Paczka: `work\release\1.0.3-rc29\`. Kopia źródeł sprzed zmian: `work\przed_rc29_src\`. Podgląd z kodu projektu: `work\wizualizacje\ramka_czasu_rc29.png`. Wycofanie: kopia karty albo `update.zip` z `work\release\1.0.3-rc11\`. Bez eksportu publicznego.
        - **Telefon bez ramki:** `k3346_eta_map_effective(mode, call, slot, no_rgi)` prosi o `showETA` tylko dla aplikacji bez wskazówek (`K3346_NO_RGI_PATH`, zmienna `g_no_rgi`). Apple i Google: `Pełna` = `instrumentcluster?maneuverLayout=…`, `Brak` = `instrumentcluster/map`. Usunięte: sekwencja „ramka najpierw” (`card_due_ms`, `K3346_CARD_AFTER_BOX_MS`, `k3346_eta_after_layout`, log `K3346_CARD_AFTER`) i szeroki obszar Google z `rc27` (`K3346_OTHER_FREE`). VIEW = obszar → jedno `showUI` → `forceKeyFrame`. Obszary Apple 5/6/7 zostają (znacznik auta i karta na miejscu).
        - **Ramka (`overlay/eta_pill.h`, nowy):**
            - plik `/tmp/vc-eta-pill` w formacie kapsuły (`godzina\nczas · dystans\nEND\n`), puste linie = brak;
            - pastylka o szerokości tekstu + 2 × 20 px w pasku 400×44, godzina pogrubiona 24 px, reszta 22 px;
            - tło wspólne z kafelkiem rozmowy (`call_card_glass`, `call_card_fill` wydzielone z `call_card.h`), styl z `/tmp/k3346-callcard-mode` (1 = jasna, inaczej przydymiona);
            - wyrównanie `eta_pill_align`: lewa na małych i w Sport, środek na dużych Classic;
            - `eta_pill_hidden_by_call`: na dużych kafelek rozmowy zajmuje to miejsce;
            - `/tmp/vc-eta-pill-ok`: nakładka pisze go, gdy może rysować (czcionki, `eta_pill 1`), i usuwa przy wyjściu.
        - **Nakładka (`platform_harman.c`):** `eta_pill_init`, `eta_pill_poll`, `composite_eta_pill` (między kapsułą a kafelkiem rozmowy). Log `eta pill on/off`.
        - **Konfiguracja:** `eta_pill 1`, `eta_pill_w 400`, `eta_pill_h 44`, małe `316,310`, duże `520,322`, Sport `110,322`. Czcionki i rozmiary z `call_card_*`. `CAPSULE_FILES` w `mib2high-lvds-control.sh` sprząta oba pliki.
        - **Java:**
            - `K3346NavBarText.etaPill(etaMode, pillReady, timeValid, arrival, remaining, distance)`: tekst przy „Mapa”/„Obie”, gotowej nakładce i poprawnym czasie;
            - `timesOnBar(etaMode, callVisible, slot, pillReady)`: czas na pasku przy „Kapsuła”/„Obie”, przy rozmowie na dużych i gdy nakładka nie rysuje;
            - `BAPBridge`: `writePill` (ponowny zapis, gdy skrypt sesji usunął plik), `pollPillReady`, czyszczenie razem z kapsułą; usunięte `navApple`, `cardMode`, `pollAppleCard`;
            - `syncEtaModeFromStorage` synchronizuje też opcję kafelka rozmowy (klucz `1005`).
        - **Komputer:**
            - natywne wszystkie OK (`eta_tries` przepisany, `nav_apple` z wersji `rc26` + blok `rc29`, karta, skórka, limit, URL-e);
            - nakładka `OVERLAY_HOST_TESTS=PASS` (cfg 367, layout 163, karta rozmowy 97, ramka czasu 71, kapsuła 112);
            - Java: `NavBarText` 158, `CapsuleFile` 108, `EtaRow` 216, `CarplayBusTest` 11;
            - mutacje: natywne 46 z 46, nakładka 44 z 44, Java 18 z 18 (+13 z `rc28`);
            - offsety, ELF; instalator 392, ratunek 52, `sim_gate` 186: 0 FAIL;
            - w bibliotece `K3346_CARD_AFTER` 0; napisy ramki w `vc_overlay` i w JAR-ze `rgi`.
            - **Bez testu na komputerze:** sklejenie w `platform_harman.c` (GL) i `BAPBridge.publishBar` (potrzebuje HMI). Testowane są ich funkcje składowe.
        - **W aucie sprawdzi:**
            - ramka z godziną, czasem i dystansem przy zegarach: Apple i Google, `Pełna` i `Brak`, małe i duże; zdjęcia położenia;
            - VIEW bez migania;
            - wygląd razem z opcją „Karta rozmowy na VC”;
            - rozmowa: na małych obie pastylki obok siebie, na dużych kafelek rozmowy zamiast ramki i czas na pasku;
            - „Kapsuła”: ramka znika, czas na pasku.
        - **Ryzyko:** położenie to szacunek; elementy telefonu (etykieta ulicy, wysoka karta z pasami) mogą wejść pod ramkę, wtedy zmniejszyć obszar mapy od dołu. Karta Google przy `Pełna` na małych nie była widoczna na filmie `rc28`: osobny temat.
        - **Do porządków później:** obszary Apple i flaga `/tmp/k3346-nav-apple` służyły ramce telefonu; komentarze w `mib2high-lvds-viewareas.conf` opisują jeszcze ramkę telefonu.
  - [x] **Test `rc29` w aucie (2026-11-11, zdjęcia `<zdjecia>\IMG_0783…0788.JPEG`, poza eksportem; logów brak, użytkownik zgłosił brak miejsca na karcie, a na karcie jest 24,9 GB wolnego).**
        - **Działa:** ramka czasu wyświetla się wszędzie (Apple i Google, `Pełna` i `Brak`, oba układy), mapa nie miga przy VIEW, wygląd zmienia się z opcją „Karta rozmowy na VC”, na małych ramka i kafelek rozmowy obok siebie, na dużych kafelek rozmowy i czas na pasku, „Kapsuła” chowa ramkę.
        - **Do poprawy: położenie.** Pomiar ze zdjęć w pikselach płótna (skala z dwóch znanych punktów: początek ramki x=316 i kafelek strzałki x=1091):
            - ramka na y=310–354 (małe) i 322–366 (duże), dolny pasek od ok. y=384;
            - Mapy Google: znacznik auta (duży krąg) ze środkiem ok. y=293 i etykieta ulicy pod nim do ok. y=360, przy obszarze kończącym się na 382. Ramka leży na znaczniku i etykiecie na obu układach (`IMG_0783`, `0784`, `0787`, `0788`);
            - Mapy Apple: znacznik wyżej, bez etykiety (`IMG_0785`, `0786`);
            - kafelek rozmowy na małych x=744–1048 (`IMG_0788`).
        - **Decyzje użytkownika:** ramki tuż nad dolnym paskiem; znacznik auta i nazwa ulicy wyżej; kafelek rozmowy na małych po prawej stronie mapy. W ankiecie zaznaczone też „Inne” bez treści: do dopytania.
  - [x] **`1.0.3-rc30`: położenie ramki czasu i kafelka rozmowy, sprawdzone w aucie (2026-11-11, zdjęcie `IMG_0789`, bez logów), zastąpione przez `rc31`.** Kopia karty sprzed wgrania: `work\karta_przed_103rc30` (`rc29`, 1322 pliki bez `collected`). Karta: 28 plików, 0 różnic, `INSTALL_ALL` i `ROLLBACK`. Paczka: `work\release\1.0.3-rc30\`. Kopia źródeł sprzed zmian: `work\przed_rc30_src\`. Podgląd: `work\wizualizacje\polozenie_rc30.png` (położenie znacznika to przewidywanie). Wycofanie: kopia karty albo `update.zip` z `work\release\1.0.3-rc11\`. Bez eksportu publicznego.
        - **Pastylki (`mib2high-overlay.conf`, `overlay_cfg.h`):** `eta_pill_y_*` i `call_card_y_*` = 334 (dół 378, pasek od ok. 384), `call_card_x_small` 756 (koniec 1076, kafelek strzałki od 1091). `x` ramki czasu bez zmian.
        - **Obszary (`mib2high-lvds-viewareas.conf`):** obszary z trasą kończą się na y=334 (`h=246`): 1, 5, 6, 7 i nowy 8 (`490 88 460 246`: duże, aplikacja inna niż Apple, bez karty). Obszar 3 (duże z kartą, każda aplikacja) zostaje `490 88 460 294`, bo karta u góry potrzebuje wysokości (`0.3*h > 87`). Obszary 0, 2 i 4 bez zmian.
        - **Kod:** `K3346_OTHER_FREE` wraca jako rodzaj „inna aplikacja bez karty”, `K3346_VIEW_OTHER_BIG` 8 w `k3346_wire_view` tylko dla widoku 3 (widok 1 zostaje 1, Sport dalej 4). Telefon dostaje 9 obszarów (`ALTSCREEN_VIEW_COUNT`, `_MAX`).
        - **Komputer:**
            - natywne wszystkie OK (obszar 8 w `nav_apple`, `eta_tries`, `card`, `show_limit`; 9 obszarów w `card`, `skin`, pokrętle);
            - nakładka `OVERLAY_HOST_TESTS=PASS` (cfg 376);
            - Java bez zmian w kodzie: `NavBarText` 158, `CapsuleFile` 108, `EtaRow` 216, `CarplayBusTest` 11;
            - mutacje: natywne 53 z 53, nakładka 50 z 50;
            - offsety, ELF; instalator 392, ratunek 52, `sim_gate` 186: 0 FAIL;
            - wartości obu plików conf w paczce sprawdzone ręcznie.
        - **W aucie sprawdzi:**
            - `Brak`, Apple i Google, oba układy: ramka tuż nad paskiem, znacznik i nazwa ulicy nad nią;
            - `Pełna`, małe: czy karta manewru dalej jest i wygląda jak dotąd (obszar niższy o 48 px);
            - `Pełna`, duże: karta jak dotąd, w Google etykieta może wchodzić pod ramkę;
            - rozmowa na małych: kafelek po prawej; na dużych zamiast ramki;
            - logi.
        - **Ryzyko:** karta z boku przy `h=246` (w `rc5` działała przy 265); znacznik może podnieść się mniej niż wyliczone 33–52 px; Sport: tylko pastylki niżej, obszar bez zmian.
  - [x] **Test `rc30` w aucie (2026-11-11, zdjęcie `<zdjecia>\IMG_0789.JPEG`, poza eksportem, bez logów).** Użytkownik: „wszystko elegancko”. Na zdjęciu (Google, `Brak`, małe, dzwonienie): znacznik auta i etykieta ulicy są nad ramką czasu, czyli niższy obszar (`h=246`) zadziałał. Kafelek rozmowy po prawej potwierdzony. Karta przy `Pełna` na małych: bez uwag użytkownika, zdjęcia nie ma.
        - **Prośba:** kafelek rozmowy delikatnie bliżej prędkościomierza (bez wchodzenia pod niego), ramka czasu z takim samym odstępem od obrotomierza.
        - **Pomiar (wiersz y=334–378):** krawędź obrotomierza ok. x=334 (ramka od x=316 chowała lewy koniec pod zegarem, tekst zaczynał się przy krawędzi). Zegary są lustrzane względem x=720, więc prędkościomierz ok. x=1106 (osobny pomiar: 1096–1116). Kafelek rozmowy kończył się na 1076. Niepewność ±6 px, zdjęcia pod kątem.
  - [x] **`1.0.3-rc31`: równe odstępy pastylek od małych zegarów, sprawdzone w aucie (2026-11-11): „wszystko dobrze”, bez logów i zdjęć.** Kopia karty sprzed wgrania: `work\karta_przed_103rc31` (`rc30`, 1358 plików bez `collected`). Karta: 28 plików, 0 różnic, `INSTALL_ALL` i `ROLLBACK`. Paczka: `work\release\1.0.3-rc31\`. Kopia źródeł sprzed zmian: `work\przed_rc31_src\`. Podgląd: `work\wizualizacje\polozenie_rc31.png`. Wycofanie: kopia karty albo `update.zip` z `work\release\1.0.3-rc11\`. Bez eksportu publicznego.
        - **Zmiana (tylko `mib2high-overlay.conf` i domyślne w `overlay_cfg.h`):** `eta_pill_x_small` 316 → 348, `call_card_x_small` 756 → 772. Odstęp 14 px od obu zegarów: ramka czasu od x=348, kafelek rozmowy do x=1092. Niezmiennik w teście: `eta_pill_x_small + call_card_x_small + call_card_w == 1440`.
        - **Komputer:** nakładka `OVERLAY_HOST_TESTS=PASS` (cfg 379), mutacje nakładki 52 z 52, natywne bez zmian OK, offsety, ELF, instalator 392, ratunek 52, `sim_gate` 186: 0 FAIL. Wartości w conf w paczce sprawdzone.
        - **W aucie sprawdzi:** małe zegary z trasą i rozmową: równe odstępy, żadna pastylka pod zegarem; zdjęcie na wprost; logi.
  - [x] **Nadchodząca wersja to 2.0.0, nie 1.0.3 (decyzja użytkownika, 2026-11-11: „dużo zostanie wprowadzone”).** `1.0.3-rc1…rc31` to historia testów; stan kodu = `rc31`. Następne paczki testowe `tools/make_release.sh 2.0.0-rcN`, wydanie `2.0.0` dopiero na polecenie. Numer wersji jest tylko w pliku `RELEASE` paczki, w kodzie go nie ma.
  - [x] **Publikacja stanu `rc31` jako wersji testowej 2.0 na GitHubie (2026-11-11, na polecenie użytkownika).** README EN/PL: „Coming in 2.0” / „W wersji 2.0” z tabelami „Nowe” i „Poprawione”, lista nazw opcji MMI, krok „wyłącz silnik na minutę” w instalacji (README, GUIDE, INSTRUKCJA). `tools/export_public.sh`: scrub ścieżki zdjęć `Desktop\mmi` i ulic `<ulica>`, `<ulica>`. Bez wydania w Releases, PR #12 i #3 bez zmian.
  - [ ] **Po instalacji: restart MMI nie wystarcza, potrzebny silnik wył./wł. (obserwacja użytkownika, 2026-11-07).**
        - **Co było:** po wgraniu `rc22` i restarcie MMI przez chwilę wszystko działało, potem strzałka bez ciemnego tła kafelka. Nie pomogło odpięcie kabla ani restart MMI. Pomogło: silnik wył., chwila przerwy, silnik wł. Od tego czasu (`rc22`, `rc23`) działa dobrze.
        - **Hipoteza:** tło kafelka rysuje VC (fabryczna podkładka KDK 101/102, wizyta 7), a strzałka bez tła to obraz z `vc_policy=off` (`run-39…42`). VC nie był w trybie wskazówek. Restart MMI restartuje tylko jednostkę główną, VC zachowuje stan. Dopiero uśpienie magistrali po wyłączeniu silnika go zeruje.
        - **Brak dowodu:** `run-90` to już dobra sesja (`vcEnabled=true`, `SET_FAVORED_RGI_RETURNED_OK`, `MAINTAIN`), złej sesji nie ma w logach. Jeśli się powtórzy: logi **przed** wyłączeniem silnika, porównanie `VC_TRACE` z `run-90`.
        - **Zmiana tylko w tekstach:** `custom.sh` (`INSTALL_ALL`, `ROLLBACK`, `VC_POLICY`, `TOUCHPAD`: linia „engine off, wait until the gauges and MMI go dark (~1 min), engine on”) i `README_CarPlay_VC.txt` (krok 6 EN/PL, odinstalowanie, punkt „strzałka bez tła” w „GDY COŚ PÓJDZIE NIE TAK”). Kopia sprzed zmian: `work\przed_tekst_silnik\`. Weszło z `rc25` (2026-11-10).
        - **Komputer:** instalator 392, ratunek 52: 0 FAIL, `command.sh` = `custom.sh` (paczka `rc25`).
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

- [ ] **Szybkie ponowne podłączenie: #7 (czarna mapa, 15–20 s), #11 pkt 2 (MMI dziwnie działa lub się zawiesza, ok. 30 s; po > 3 min nie), nasze auto (zawieszenie raz).**
      Logów z tych przypadków brak. `rc18` dostaje linie WARN: `TEARDOWN`, `K3346_CLEANUP begin/join/done`, `NvSS stub open/close`, `session start return`, `output_scope_busy`, `others=` w `K3346_SETUP`,
      oraz `session #N since boot` z PR #12 (Gooliver2). **Odczyt:** `begin`/`join` bez `done` = zablokowanie w `dio_manager`; `others>0` = nowa sesja zastała starą; `already_active`, `listener bind failed` = stary stan.
      W naszych 53 unikalnych logach: 114 `SETUP_READY`, 1 `session start failed rc=-1` (`run-06`, ok. 9 s po rozłączeniu, bez ponowienia), 0 stanów z PR #12. Watchdog z PR #12 **nie wchodzi** bez dowodu; PR otwarty.
      Trwała kopia logu po resecie: nie w tej wersji (`/mnt/icab` jest, ale nie sprawdzono zapisu). Odpowiedzi w #7, #11, PR #12 wysłane 2026-11-03.

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

- [ ] **Zgłoszenie #13 (Q7 4M, pusty szablon, 2 zdjęcia, 2026-11-03): M.I.B. 3.8.0 uruchomił własną zaślepkę `mod/custom.sh`,**
      nasz instalator się nie uruchomił (3.8.0 nie czyta `mod/command.sh` w „run custom script”). Decyzja właściciela: bez zmian
      w paczce i dokumentacji; zgłaszający ma użyć M.I.B. 3.6.0 według instrukcji i wypełnić szablon. Odpowiedź w zgłoszeniu do wysłania.

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
