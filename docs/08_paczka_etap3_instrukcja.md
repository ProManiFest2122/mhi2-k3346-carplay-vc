# 08 – Paczka na Etap 3: mapa CarPlay na Virtual Cockpit (instrukcja)

Stan: **gotowa do instalacji, jeszcze nie uruchamiana w aucie.** Data: 2026-10-20.
Paczka: `work\sd_etap3\` (16 plików, 469 KB). Raport symulacji: `work\sim_app\sim_result.txt`.
Przegląd poprawności (co zweryfikowano na binariach K3346 i jak): `docs\11`.

> Nic z tego nie zostało jeszcze wgrane do auta. Decyzję o instalacji podejmujesz Ty.

> **Do auta weź `docs\09_sciagawka_auto.md`** – jedna strona na telefon, z drzewem decyzyjnym
> „kiedy przerwać”. Ten dokument jest pełną dokumentacją, tamten do czytania przy aucie.

---

## 1. Co ta paczka robi

Dodaje **mapę CarPlay na Virtual Cockpit** (drugi strumień H.264), kartę instrukcji
i przełączanie układu przyciskiem **VIEW**. To port projektu edzenisa (sprawdzonego na Audi Q7, K2161)
na Twój firmware **K3346 / MU1438**.

**Czego jeszcze nie robi:** strzałek manewrów i odległości na kombi (RGI). To wymaga dodatkowego
wrappera dla `mm-ipod` i zmian w `iap2.cfg` — osobny etap. Nie rusza dźwięku ani nawigacji fabrycznej.

## 2. Co dokładnie zmieni w MMI

**Dodawane pliki** (9) – wcześniej ich nie było:

| Plik | Po co |
|---|---|
| `/mnt/app/root/hooks/libmib2high-carplay-rgi.so` | hooki natywne iAP2 |
| `/mnt/app/eso/hmi/lsd/jars/mib2high-carplay-rgi.jar` | bundle OSGi w HMI |
| `/mnt/app/root/vc_render` | renderer obrazu na kombi |
| `/mnt/app/root/v8l-dsitracer-capture` | odczyt DSITracera (VIEW, rolka) |
| `/mnt/app/root/mib2high-lvds-control.sh` | sterowanie sesją i kontekstami kombi |
| `/mnt/app/root/mib2high-lvds-profile.conf` | geometria obrazu |
| `/mnt/app/root/mib2high-lvds-viewareas.conf` | geometria kart |
| `/mnt/app/carplay_altscreen` | znacznik |
| `/mnt/app/eso/hmi/lsd/dsitracer_activated` | znacznik |

**Modyfikowane pliki** (3) – zmiany punktowe, każdy z kopią zapasową:

| Plik | Zmiana |
|---|---|
| `bundles_dsitracer_enabled.properties` | 2 linie: dopisanie `MIB2HighCarPlayRGI` do autostartu + rejestracja aktywatora |
| `traceConfig.properties` | `use.appender=client` → `telnet,bluepirat` + odkomentowanie `=full` dla DSIBAP, DSICarKombi, DSIKeyPanel, DSIKombiSync, DSIKOMOView, DSIMapViewerControl |
| `smartphone_integrator.json` | 1 linia: `LD_PRELOAD=...libmib2high-carplay-rgi.so` |

`NavActiveIgnore.jar` **nie jest ruszany**.

## 3. Zabezpieczenie: blokada wersji

Instalator **sam odmówi pracy**, jeśli to nie jest Twój firmware. Na MMI nie ma `sha256sum`
ani `cmp` (sprawdzone collectorem), więc blokada używa `awk` i `wc -c`:

- `HMI_Version` musi być `HMI_AU_MIB2_H_EU_G22_C6_190300`
- `ipod-drvr-iap2.so` musi mieć dokładnie **739925 B** — względem tego pliku zweryfikowano offsety hooków (17/17 PASS). Inny plik ⇒ hooki pisałyby pod złe adresy
- `/ifs/LSD.JXE` musi mieć dokładnie **58131096 B** — względem niego skompilowano JAR

Przy niezgodności: komunikat `VERSION_GATE ... instalacja PRZERWANA` i **zero zapisów**.

## 4. Instalacja

**Przed:** prostownik/zasilacz podłączony, świeży backup M.I.B., karta z fabryczną paczką K3346 pod ręką,
**uzbrojona karta ratunkowa** sprawdzona wcześniej na zdrowym aucie (`docs\09` punkt A),
**laptop albo telefon z czytnikiem kart SD** (bez niego w aucie nie da się zrobić rollbacku –
wymaga skopiowania pliku na kartę).

1. Skopiuj **zawartość** `work\sd_etap3\` **na swoją kartę z M.I.B.** (tę z Etapu 1), bez formatowania.
   M.I.B. nie instaluje się do pamięci auta — jego pozycja w GEM to dowiązanie do karty
   (`apps/launcher:99`), a *run individual script* uruchamia `esd/scripts/individual.sh` **z karty**
   (`/net/mmx/fs/sda0/...`). Na czystej karcie z samą naszą paczką nie zadziała.
   Stary `mod\command.sh` z Etapu 1 zostanie zastąpiony — to zamierzone.
   W katalogu głównym karty ma być `_Swdlautorun.txt` (z podkreśleniem). Gdyby był
   `Swdlautorun.txt` bez podkreślenia, tryb aktualizacji M.I.B. wczytałby nasz instalator
   (`apps/launcher:117-118`) w niesprawdzonym kontekście.
   **Przed instalacją: test karty ratunkowej** – `docs\09` punkt A, `docs\12`.
2. Karta do slotu **SD1**, auto na zapłonie.
3. M.I.B. → GEM → *m.i.b.* → *advanced settings* → **run individual script**.
   M.I.B. 3.6.0 uruchamia wyłącznie `/mod/command.sh` — dlatego instalator jest pod tą nazwą.
4. Czekaj na **„SAFE TO REMOVE SD”**. Nie wyjmuj karty wcześniej.
5. Restart MMI ręcznie.

Na ekranie powinno pojawić się kolejno:
`VERSION_GATE=PASS` → `INSTALLING=...` (7 plików) → `INSTALL_RESULT=PASS` → `SAFE TO REMOVE SD`.

> **Kopia oryginalnych plików auta powstaje na tej karcie**, w
> `mod\mib2high-carplay-rgi-v8s-simple\backup\pre-v8s\`. Rollback poziomu 1 działa **tylko z tą
> kopią**, czyli z tą samą kartą. Po instalacji: nie formatuj karty, nie kasuj `backup\`, nie
> nadpisuj folderu `mod\` nową paczką, a przy pierwszej okazji **skopiuj cały `mod\` z karty na
> dysk** (`work\auto_test_3\karta_mod\`). Jeśli kopia zginie, zostają poziomy 2 i 3.

## 5. Lista testów (do odhaczania)

| # | Test | Wynik |
|---|---|---|
| 1 | MMI wstaje normalnie, CarPlay się łączy | ☐ |
| 2 | Mapa CarPlay pojawia się na VC podczas nawigacji | ☐ |
| 3 | **VIEW** przełącza układ VC | ☐ |
| 4 | Rolka na kierownicy – reakcja | ☐ |
| 5 | **Bieg wsteczny:** kamera na MMI, mapa na VC zostaje, po wyjściu z R CarPlay wraca bez ponownego łączenia | ☐ |
| 6 | Odłączenie i podłączenie iPhone'a | ☐ |
| 7 | Wyłączenie i włączenie auta – **dopiero po zebraniu logów** (kolejność: `docs\09`) | ☐ |
| 8 | Obraz na VC nie jest przesunięty i nie ma paska na krawędzi | ☐ |

Po testach uruchom skrypt **ponownie** – zbierze logi na kartę (tryb COLLECT, nie instaluje
po raz drugi). Skopiuj `collected\` do `work\auto_test_3\`.

> **Kolejność ma znaczenie – dwie pułapki:**
> 1. **Logi przed rollbackiem.** `rollback()` usuwa `installed.ok` i tworzy `rolledback.ok`,
>    po czym dyspozytor wchodzi w gałąź `ALREADY_ROLLED_BACK` i **nie zbiera niczego**.
> 2. **Logi przed wyłączeniem auta.** Logi naszych komponentów idą do `/tmp`. Na tym aucie logi
>    runtime siedzą w pamięci współdzielonej (`mm-ipod` → `/dev/shmem/iap2.log`), ale czy samo
>    `/tmp` to RAM (`/dev/shmem`) czy dysk (`/fs/tmpfs`) – **nie da się tego ustalić z zebranych
>    danych**. Zakładamy wariant gorszy.
>
> Obejście obu: plik **`COLLECT_LOGS`** w folderze pakietu wymusza zbieranie niezależnie
> od stanu – także po odmowie bramy wersji i po rollbacku. Gotowy pusty plik leży w
> `ZNACZNIKI\COLLECT_LOGS` – kopiuje się go do `mod\mib2high-carplay-rgi-v8s-simple\`.
> `ROLLBACK` zachowuje nad nim pierwszeństwo, żeby zapomniany marker nigdy nie zablokował
> ścieżki ratunku.

Co zbiera: kopie zainstalowanych plików, logi wszystkich naszych komponentów
(`mib2high-lvds-control.log`, `vc_render-launch.log`, `v8l-dsitracer-*`), log Javy z pamięci
współdzielonej RCC, `sloginfo`, `pidin ar`, `netstat -an`, przefiltrowany indeks zdarzeń oraz
**`dmdt-gc.txt` – stan kontekstów wyświetlacza**. To ostatnie jest kluczowe do diagnozy: punkt
odniesienia z Etapu 1 to 87 kontekstów i brak 900; po instalacji powinien pojawić się kontekst
`900` z displayable `58`, a `77` powinno zawierać `58`.

Tryb COLLECT **nie zmienia niczego w aucie** – potwierdzone w symulacji (test T10: ani jeden plik
w drzewie auta nie został dotknięty).

## 6. Jeśli coś nie tak – rollback

**Poziom 1 (najpierw to):** skopiuj gotowy plik `ZNACZNIKI\ROLLBACK` do
`mod\mib2high-carplay-rgi-v8s-simple\` → *run individual script* → restart.
Przywraca oryginalne pliki. Oczekiwany komunikat: `ROLLBACK_RESULT=PASS`.
Musi to być **ta sama karta**, na której była instalacja (tylko na niej jest kopia oryginałów).
**Najpierw zbierz logi** – po rollbacku zwykłe uruchomienie już nic nie zbiera.

Dlaczego gotowy plik zamiast „utwórz pusty plik": w Windows „Nowy → Dokument tekstowy" tworzy
`ROLLBACK.txt` (rozszerzenie często ukryte), którego skrypt nie rozpozna – zamiast rollbacku
zebrałby logi. Weryfikacja procedury w symulacji: test T12.

Instalator ma też **auto-rollback**: jeśli instalacja wysypie się w trakcie, sam cofa zmiany
(`AUTO_ROLLBACK_RESULT=PASS`). Jeśli zobaczysz `AUTO_ROLLBACK_RESULT=FAIL – do not reboot`,
**nie restartuj** i daj mi logi.

**Poziom 2:** przywrócenie plików z backupu M.I.B.
**Poziom 3:** pełne wgranie fabrycznej paczki K3346 z karty SD.

**Poziomy 1 i 2 wymagają zielonego menu, a zielone menu jest częścią Javy HMI**
(`lsd.sh:218-225`). Przy bootloopie HMI ich nie ma. Wtedy najpierw **karta ratunkowa**
(`work\sd_ratunek\`, `docs\12`): uruchamia się sama przez tryb aktualizacji (natywny `MIBRoot`),
bez HMI, usuwa `dsitracer_activated` i nasz JAR oraz przywraca fabryczny `traceConfig.properties`.
HMI wstaje, a resztę cofa poziom 1. Zielone menu może być niedostępne do 5 minut po zimnym
starcie — przed uznaniem, że to bootloop, odczekać.

Po rollbacku zostaje jeden **pusty** katalog `/mnt/app/root/hooks/` – to normalne i nieszkodliwe
(instalator celowo nie usuwa katalogów, bo `eso/hmi/lsd/jars/` zawiera 11 plików fabrycznych).

## 7. Jeśli obraz na VC jest przesunięty

Geometria jest czytana z pliku tekstowego w czasie działania – **nie wymaga przebudowy**.
Domyślnie `1440 542` (wartość sprawdzona przez edzenisa na MHI2 Harman, ten sam tryb LVDS
i te same konteksty kombi 72/77 co w Twoim aucie). Panel VC w A4 B9 bywa opisywany jako 1440×540.

Wariant B9 jest w paczce: `mod\...\alt-profiles\mib2high-lvds-profile.b9-1440x540.conf`.
Podmień nim `payload\mib2high-lvds-profile.conf` przed instalacją, albo już w aucie edytuj
`/mnt/app/root/mib2high-lvds-profile.conf`.

Format: `szerokość wysokość fps features safe_x safe_y safe_w safe_h` –
**tylko liczby, bez komentarzy** (każdy nieliczbowy znak powoduje awaryjny powrót do 800×480).
Wszystkie 4 widoki mieszczą się i w 542, i w 540, więc nic nie trzeba przeliczać.

## 8. Co zostało sprawdzone na komputerze (a czego nie)

**Sprawdzone – symulacja instalatora, 62 testy, 0 błędów**, pod `dash` i pod `mksh`
(`work\sim_app\sim_result.txt`, `sim_result_mksh.txt`),
na atrapie drzewa plików zbudowanej z **prawdziwych plików zebranych z Twojego auta**:

- instaluje się na zgodnym firmware, wszystkie 9 plików dodane, 3 zmodyfikowane punktowo
- **odmawia** przy złym `HMI_Version`, złym rozmiarze `ipod-drvr-iap2.so` i złym `LSD.JXE` – za każdym razem z zerem zapisów
- `ROLLBACK` przywraca wszystkie pliki **bit w bit, razem z trybami dostępu**
- **auto-rollback** po wymuszonej awarii w trakcie instalacji – też bit w bit
- `MIB_FORCE=1` obchodzi blokadę świadomie i to raportuje – **tylko przy uruchomieniu z powłoki**
  (symulacja, praca deweloperska). Z M.I.B. zmiennej nie da się ustawić, więc w aucie bramki
  nie da się obejść – celowo
- transformacje `awk` zachowują mieszane końce linii (CRLF) plików HMI
- tryb COLLECT działa i **nie dotyka drzewa plików auta**; brak `dmdt` daje `PARTIAL`, nie błąd
- marker `COLLECT_LOGS` wymusza zbieranie logów bez instalacji **i po rollbacku**
- `ROLLBACK` wygrywa z `COLLECT_LOGS`, gdy oba pliki są obecne (test bezpieczeństwa)

**Niesprawdzone – wymaga auta:** czy obraz faktycznie pojawia się na VC, czy VIEW przełącza układ,
jak zachowa się bieg wsteczny, czy geometria 542 jest właściwa dla B9.

## 9. Dowody jakości builda

| Artefakt | SHA-256 | Uwaga |
|---|---|---|
| `libmib2high-carplay-rgi.so` | `058413dd…` | ELF 32-bit ARM EABI5, 306904 B, `verify_current_native.py` PASS |
| `mib2high-carplay-rgi.jar` | `606e11a0…` | 29 klas, bytecode major 48 (Java 1.4) |
| `vc_render` | `fd5085d8…` | ELF 32-bit ARM EABI5 |
| `v8l-dsitracer-capture` | `07076bf1…` | **bit w bit identyczny z wydaniem edzenisa** |

Ostatni wiersz jest ważny: ta sama binarka co w oficjalnej paczce v1.0 dowodzi, że toolchain
w Dockerze (GCC 4.9.4 przez shim `qcc`) to **ten sam kompilator**, którego użył edzenis.
Pozostałe różnią się, bo zawierają nasze zmiany dla K3346 (`.so`, `.jar`) albo debug-info (`vc_render`).

Offsety hooków zweryfikowane przeciw driverowi **pobranemu z auta**: `tools\verify_port_offsets.py` → **17/17 PASS**.
