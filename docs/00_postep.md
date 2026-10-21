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
- [ ] **Etap 4b (później): strzałka manewru „1:1 jak u Luki”** – port `repos\mib2q-carplay-rgi\maneuver_render\`
      (u Luki OMX/GLES na MHI2Q, displayable 98 zamiast 20, kontekst {98,101,102,mapa}); u nas NvSS/Tegra
      (`renderer\vc_render`), kontekst 77 = {20,102,101,58} (`work\auto_test_4\…\run-03\dmdt-gc.txt`).
      Docs Luki: `docs\cluster-and-rgi\CLUSTER_CONTEXTS.md`, `KDK_GEOMETRY_AND_ANIMATION.md`, `VC_FPK_STATE_MACHINE.md`.
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
  - [ ] Test 5.3 w aucie **[Ty]**: okładka Spotify/Apple Music na VC, zmiana utworu
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

## Pułapki, o których trzeba pamiętać

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
- **Nie wiemy, czy `/tmp` w aucie to RAM czy dysk.** Logi runtime na pewno siedzą w pamięci
  współdzielonej (`mm-ipod` → `/dev/shmem/iap2.log`), ale na aucie istnieje też dyskowy
  `/fs/tmpfs` (`hd130t77`). `/tmp` nie jest osobnym punktem montowania i nie ma go w zebranych
  listach katalogów. Zakładamy wariant gorszy: **logi przed wyłączeniem auta.** Porównanie
  `run-01` (przed) i `run-02` (po wyłączeniu) z Etapu 3 ma to rozstrzygnąć.
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
