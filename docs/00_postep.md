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
- [ ] **Etap 7 (dawniej 4b): własna warstwa graficzna na VC – strzałka manewru + pasy** → **`docs\18_etap7_nakladka.md`**
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
  - [x] Trop pochodzi z projektu **chefranov/mhi2-au37x-carplay** (MHI2 AU37x) — użyty **za zgodą autora**,
        adnotacja wpisana w `publish_src\README.md` i `LICENSING.md`. Żaden jego plik nie trafia do repo
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
  - [ ] **Wizyta 4 [Ty]** – 3 uruchomienia, telefon może być podłączony od początku.
        Rozstrzygające: `LISTEN` na 19800 w `netstat-an.txt`, `server: connected to 127.0.0.1:19800`
        w logu renderera, `scene: … commands=N` z N > 0, **strzałka w prawym panelu**, oraz przy
        wyłączonej nawigacji – czy czarny prostokąt zniknął (był nasz) czy został (podkładki)
  - [ ] Fizyczna karta ratunkowa ma jeszcze stary `ratunek.sh`; **przed tą wizytą nie trzeba jej
        wgrywać** (Etap 7 nie dotyka Javy ani HMI), a poprzednia podmiana wymagała formatowania karty
  - [ ] **Etap 7d: karta rozmowy** – layout ustalony ze zdjęć (2026-10-26), brakuje renderowania tekstu
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
  - [ ] Opcjonalnie: **ulubione z iAP2 na VC** – nasz parser je dziś ignoruje (0x4171 param 5–7);
        dopisać parser + Java `updateFavoriteList` (wzór Luka `GatedPhoneService`).
        Do zrobienia **[Ty]**: `ROLLBACK_ETAP4` + `INSTALL_ETAP4` (2 uruchomienia), restart MMI.
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
| `20_sciagawka_etap7b.md` | **Do auta, na telefon (Etap 7c)**: trzy uruchomienia, bez sondy, na co patrzeć w pierwszej sekundzie sesji, co sfotografować, kiedy przerwać |
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
