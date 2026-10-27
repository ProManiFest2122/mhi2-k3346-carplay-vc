# 18 – Etap 7: własna warstwa graficzna na VC (strzałka manewru + pasy)

Stan 2026-10-27: **wizyta 3 — nakładka jest na kombi, mapa widoczna dookoła niej.**
Strzałki nadal nie ma, i przyczyna jest już znana z pewnością: **oba końce łącza danych były
klientami, nikt nie nasłuchiwał**. Naprawione. Paczka: `work\sd_etap7\`.

---

## Wynik wizyty 3 (2026-10-27, `run-23` na karcie)

### Etap 7c zadziałał w całości

```
[lvds-control] overlay ready after 0s; context=901 {98,102,101,58} declared
platform_harman: READY /tmp/vc-overlay-ready -> 98 1440x542
render: init multi-layer 210x153 (default fbo=1)
kd_surface: health: VISIBILITY not readable on this DM - health check falls back to swap failures
```

Po kolei: meldunek bez `rename()` wychodzi; brama bierze kontekst 901 **w zerowej sekundzie**;
`default fbo=1` potwierdza, że renderer Luki rysuje do podstawionego mu FBO; a o nieczytelnym
`VISIBILITY` jest **jedna** linia zamiast piętnastu cykli niszczenia zdrowego okna.

Zdjęcia z auta dokładają odpowiedź na pytanie, którego żaden zrzut dać nie mógł: **mapa CarPlay
jest widoczna dookoła naszego okna**. Przezroczystość pełnego płótna dociera więc do warstwy
sprzętowej z mapą — to była ostatnia niewiadoma Etapu 7c i wyszła dobrze.

### Czemu nie było strzałki: dwóch klientów i żadnego serwera

| Plik | Rola | Co robił |
|---|---|---|
| `overlay/upstream/server.c:2` | „Single-thread TCP **CLIENT**" (Luka) | `connect()` 127.0.0.1:19800 |
| `native/routeguidance/overlay_feed.c` | `feed_connect()` | `connect()` 127.0.0.1:19800 |

Nikt nie wołał `bind()`/`listen()`. Rozstrzyga to `run-23/netstat-an.txt`: **portu 19800 nie ma
tam w ogóle** — ani nasłuchu, ani połączenia, przy 30 innych wpisach loopback. Stąd w logu
renderera `server: 127.0.0.1:19800 not yet listening, will retry` i `scene: … commands=0`.

Najgorsze w tym jest to, że komentarz w naszym własnym `overlay_feed.h` **opisywał topologię
odwrotnie niż kod**: twierdził, że serwerem jest renderer. Nikt nigdy renderera na serwer nie
przerobił, a hook został klientem. Dokumentacja zgadzała się sama ze sobą, więc nic nie zgrzytało.

### Co zmienione (Etap 7d)

| Zmiana | Plik |
|---|---|
| **feed nasłuchuje**, renderer zostaje klientem | `native/routeguidance/overlay_feed.c` |
| odtworzenie stanu po przyjęciu klienta | tamże |
| czytanie z gniazda: rozpoznanie rozłączenia + opróżnianie bufora | tamże |
| panel nie jest malowany, gdy nie ma treści | `overlay/platform_harman.c` |
| `platform_set_content_present()` przez istniejący szew | `overlay/upstream/platform.h`, `main.c` |
| recepta kontekstu 901 czytana z conf | `deployment/mib2high-lvds-control.sh` |
| `flag_atlas.rgba` instalowany, renderer startuje z `/mnt/app/root` | `deployment/`, `tools/make_sd_etap7.sh` |

**Dlaczego feed, a nie renderer, dostał rolę serwera:** to jest dokładnie topologia Luki —
u niego producent (Java) nasłuchuje, a renderer się łączy. Dzięki temu jego `server.c`, razem
z pętlą ponawiania co 200 ms i obsługą rozłączeń, **zostaje bez jednej zmiany**. `mm-ipod`
restartuje się przy każdym podłączeniu iPhone'a, więc nasłuch znika i wraca — czyli dokładnie
przypadek, dla którego ta pętla u niego istnieje.

Przy okazji wyszło, że link jest **dwukierunkowy**: renderer odsyła pakiet „frame ready"
(`upstream/server.c:96`). Nikt go nie czytał, więc bufor odbiorczy po naszej stronie rósłby,
aż zablokowałby rendererowi wysyłkę. Teraz jest opróżniany i wyrzucany — a przy okazji `recv()`
zwracające 0 to jedyny szybki sygnał, że renderer odszedł.

### Czego wizyta 3 NIE rozstrzygnęła

**Czyj jest czarny prostokąt** w miejscu panelu manewru, widoczny także przy wyłączonej nawigacji.
Albo nasz panel (blit kopiuje FBO z wyłączonym mieszaniem), albo fabryczne podkładki 101/102,
odsłonięte przez usunięcie displayable 20 z kontekstu. Zamiast zgadywać: renderer przestał malować
panel, gdy nie ma czego rysować — jeśli prostokąt zniknie, był nasz; jeśli zostanie, to podkładki
i wtedy wystarczy wpisać `context_recipe 58` w pliku na karcie.

---

## Wynik wizyty 2 (2026-10-27, `probe-run` + `run-22` na karcie)

### Główne pytanie Etapu 7 ma odpowiedź: tak

```
kd_surface: CAPTION="98"
kd_surface: window caption="98" 210x153 display=-1 alpha=1 realized native=beeb0006
```

Okno powstało i zostało zrealizowane. Zdjęć MMI nie ma (zapomniane) i **nie były potrzebne** —
nakładka jest na zrzutach `dmdt ts`, bo idzie przez kompozytor DisplayManagera. Zrzuty rozebrane
pikselowo (dekoder PNG w czystym Pythonie — w tym środowisku nie ma Pillow):

| Pytanie | Odpowiedź | Skąd |
|---|---|---|
| DM przyjmuje okno? | **tak** | `A-vc-overlay-probe.log` |
| rozmiar | **dokładnie** 210×153 | bbox `(0,0)-(209,152)` |
| pozycja | **(0,0)**, DM jej nie nadaje | ten sam bbox |
| pełne płótno | działa, 1440×542 | `vc-20-B-full.png`, bbox `(0,0)-(1439,541)` |
| alfa per-piksel | **tak**, `0, 51, 102, 153, 204, 255` = `255·a` | schodek alfy na obu zrzutach |
| VIEW | okno nie zniknęło ani nie drgnęło | `vc-11` identyczny z `vc-10` |

Szczegóły i wnioski: `docs\19`, sekcja „Odpowiedzi z auta".

### Dlaczego mimo to nie było strzałki — dwa nasze błędy

Z telefonem brama odmówiła (`overlay did not report a window within 6s; staying on 77`) i mapa
działała normalnie. Przyczyny widać w `run-22\logs\vc_overlay-launch.log`:

1. **`rename()` nie działa na `/tmp`.** `cannot write /tmp/vc-overlay-ready errno=18` (EXDEV).
   Sam `/tmp` jest zapisywalny — leżą tam wszystkie nasze logi (`trace\copied-files.txt`) — więc
   padało wyłącznie `rename()`. Przy okazji **rozstrzyga to starą niewiadomą z `docs\00`:
   `/tmp` to `/dev/shmem`, czyli RAM**, a ten system plików nie ma `rename()`.
2. **Health-check był nieprawdziwy.** `kdGetWindowPropertyiv(VISIBILITY)` na tym DM **nigdy** się
   nie udaje. Dowód jest w samym logu: drukowane `errno` to 18, 89, 236, 236, 236… — czyli śmieć
   po poprzednim wywołaniu, a nie błąd tego odczytu. Sonda loguje identyczne `lost=1` i mimo to
   rysuje 282 klatki, które widać na zrzucie. Renderer co 5 s niszczył zdrowe okno i kasował przy
   tym meldunek: 15 cykli na sesję, dwie sesje.
3. **Pochodna obu:** meldunek powstawał tylko przy swapie, a renderer swapuje dopiero mając scenę.
   Pierwszy (i jedyny) swap sesji był tym, przy którym zapis padł — i nic już nie wróciło spróbować.

To jest dokładnie ten rodzaj błędu, którego nie złapie żadna symulacja na komputerze: obie funkcje
zachowują się poprawnie na Linuksie. Złapał je dopiero log z auta — i dlatego renderer loguje
`errno` przy każdej nieudanej operacji.

### Co zmienione w odpowiedzi (Etap 7c)

| Zmiana | Plik |
|---|---|
| meldunek jednym `open`+`write`, bez `rename()` i bez `.tmp` | `overlay\overlay_ready.h` |
| nieudany odczyt `VISIBILITY` **nie** oznacza utraty okna | `overlay\kd_surface.c` |
| meldunek po wyrenderowaniu pustej przezroczystej klatki, nie po pierwszym manewrze | `overlay\platform_harman.c` |
| **okno = całe płótno 1440×542**, panel na offsecie w środku | `overlay\platform_harman.c` |
| slot układu VC **stosowany**, nie tylko logowany | `overlay\platform_harman.c` |
| brama czeka 10 s zamiast 6 | `deployment\mib2high-lvds-control.sh` |
| `canvas_width`/`canvas_height` + furtka awaryjna | `config\mib2high-overlay.conf` |

**Kod Luki pozostaje nietknięty.** `render_init()` zapamiętuje jako „swój ekran" framebuffer
związany w chwili jego startu (`render.c`: `glGetIntegerv(GL_FRAMEBUFFER_BINDING, &g_default_fbo)`),
więc `platform_init()` podstawia mu własne FBO w rozmiarze panelu, a `platform_swap()` przenosi je
na płótno. Rozmiar źródła renderu zostaje 210×153 — zmiana psułaby aspect i FOV manewru
(`HUD_RENDERER.md:138` u Luki).

### Czego wizyta 2 NIE rozstrzygnęła

Jednego: **czy nasza przezroczystość przepuszcza mapę CarPlay.** Schodek alfy mierzyliśmy nad
czarnym tłem, bo telefon był odłączony. Mapa jedzie warstwą sprzętową i **nie ma jej w kompozycie**,
więc nie pokaże tego żaden zrzut `dmdt ts` — ani teraz, ani przy kolejnej sondzie. Dlatego sonda
**wypada z wizyty 3**: nie umie odpowiedzieć na jedyne otwarte pytanie, a na resztę już odpowiedziała.
Odpowie oko, w pierwszej sekundzie sesji — renderer wystawia na starcie całkowicie przezroczystą
klatkę na całe płótno.

---

## Wynik wizyty 1 (2026-10-26, `probe-run` + `run-21` na karcie)

### Co się udało

| Fakt | Dowód |
|---|---|
| Cała sekwencja wykonała się w zaplanowanej kolejności | 4 znaczniki zużyte po jednym na uruchomienie, `etap7.installed.ok` |
| **Nic się nie zepsuło** | `vc_render` 15–21 kl/s, `rc=0x0`; mapa na VC działa |
| Brama zadziałała jak zaprojektowana | `displayable 98 not known to displaymanager; staying on 77` |
| DisplayManager **przyjmuje kontekst z numerem 98** | `dmdt dc 902 98 58` → `CTX902=OK` |
| Bez trasy panel manewru znika zupełnie | `vc-A-kdk-noroute.png` i `vc-after.png` praktycznie puste |

### Pomiar, którego dotąd nie mieliśmy

`dmdt ts 4` zrzuca **kompozyt DisplayManagera**: widać w nim displayable 20 (panel manewru z tą złą
strzałką), nie widać mapy CarPlay — bo mapa to warstwa sprzętowa, poza software'owym odczytem DM.
Z dwóch zrzutów, co do piksela:

| Układ | Panel manewru |
|---|---|
| duże zegary | **210 × 153 @ (1091, 110)** |
| po VIEW | **210 × 153 @ (1055, 207)** |

Płótno kombi potwierdzone jako 1440×542. To **koryguje `docs\02:133`**, gdzie przy dużym
kotwiczeniu stało „328×180" — oba kotwiczenia mają ten sam rozmiar, różni je tylko pozycja.
Wartość trafiła do `config\mib2high-overlay.conf`.

### Czego wizyta NIE rozstrzygnęła i dlaczego — trzy moje usterki

**A. `dmdt gd` i `dmdt gs` zwracają na tej jednostce zero bajtów.** `gc` działa (11 264 B).
Brama w `mib2high-lvds-control.sh` i sprawdzian `ADOPTED=` w sondzie pytały więc **ślepy przyrząd** —
odpowiedź „nie" padłaby niezależnie od tego, czy okno powstało. Bezpieczne, ale bezużyteczne jako dowód.

**B. Główny blok sondy w ogóle się nie wykonał.** Brak `VARIANT=`, `ADOPTED=`, `gd-A-kdk.txt`,
`vc-A-kdk.png`, `A-kdk-launch.log` — a wszystko przed i po zapisało się normalnie.
**Przyczyna nieustalona.** Sprawdzone i odrzucone:
`$*` po `shift 4` przy `set -u` (działa i w dash, i w mksh), uszkodzenie pliku na karcie
(skrypt czysty, LF, poprawna składnia), różnica powłok — **stara sonda w symulacji przechodzi pod obiema**.
Awarii nie da się odtworzyć poza autem. Jedyny ślad poszedł na ekran MMI i przepadł — i to jest
prawdziwy błąd projektowy.

**C. `collect()` nie zbierał logów nakładki**, więc nie wiadomo, co zrobił prawdziwy renderer,
choć skrypt go wystartował (`[lvds-control] overlay start`).

### Co z tego wynika dla wizyty 2

Nie zgadujemy przyczyny — zmieniamy skrypt tak, żeby **sam ją pokazał**:
cały stderr idzie do `probe.log` (`exec 2>>`), każdy blok loguje kod wyjścia i rozmiar wyniku osobno,
a głównym dowodem przyjęcia okna jest **log renderera** (`kd_surface.c` loguje `kdInitializeNV`,
`ID_STRING`, `MANAGED`, `kdRealizeWindow` z `errno`), nie lista z `dmdt`.
Nowa sonda jest sprawdzona symulacją `tools\sim_probe.sh` (43 OK, dash i mksh), która wymusza,
żeby **każdy blok zostawił ślad** — „skrypt zakończył się kodem 0" okazało się za słabym sprawdzianem.

---

Cel: strzałka turn-by-turn „jak u Luki” nad mapą CarPlay na Virtual Cockpit, zamiast fabrycznej
strzałki, która zawsze pokazuje prosto (`docs\15`, sekcja „Wynik w aucie”). Karta rozmowy na tej samej
warstwie to Etap 7d — dopiero po Etapie 7c.

---

## Dlaczego w ogóle potrzebny jest osobny renderer

Trzy fakty, każdy sprawdzony w kodzie, a nie założony:

1. **`vc_render` nie widzi ani jednego piksela.** `NvSSVideoDecode` dekoduje *i wyświetla* w jednym
   wywołaniu (`renderer/upstream/vc_render.c:196`). W naszej pamięci nigdy nie ma bufora obrazu — tylko
   bitstream H.264. Dorysowanie czegokolwiek „po drodze” jest niewykonalne.
2. **Firmware składa warstwy bez problemu.** Kontekst 77 to już cztery displayable ułożone nad mapą.
   Brakuje wyłącznie tego, żebyśmy własny displayable **produkowali**.
3. **Zła strzałka to displayable 20** — obrazek rysowany przez fabryczną nawigację MMI, która nie
   prowadzi. Nie da się go poprawić; da się go zastąpić.

## Czym to różni się od Luki

Jego `maneuver_render` stoi na **QNX Screen** (`-lscreen`, `screen_manage_window`).
**Na K3346 nie ma `libscreen`** (sprawdzone w całym firmware). Ale rozszerzenie **KD QNX** ma komplet
odpowiedników — `repos\qnx65-armv7-toolchain\...\KD\kd_qnx_window.h`:

| Luka (QNX Screen) | My (OpenKODE) |
|---|---|
| `screen_create_context` | `kdInitializeNV` |
| `screen_create_window` | `kdCreateWindow(EGLDisplay, EGLConfig, …)` |
| `SCREEN_PROPERTY_ID_STRING "98"` | `KD_WINDOWPROPERTY_CAPTION` (69) |
| `screen_manage_window(grupa)` | *(nic — DisplayManager przyjmuje okno sam)* |
| `TRANSPARENCY_SOURCE_OVER` | `KD_WINDOWPROPERTY_SOURCE_ALPHA_QNX` (1011) |
| `screen_create_window_buffers` | `kdRealizeWindow` → `EGLNativeWindowType` |

> **Poprawka z 2026-10-27.** Wiersze `ID_STRING` i `screen_manage_window` przez cały Etap 7a
> miały w prawej kolumnie **błędne** odpowiedniki
> i to dlatego sonda z wizyty 1 nie doczekała się adopcji okna. `ID_STRING_QNX` (1002) i
> `MANAGED_QNX` (1013) wyglądały na oczywiste odpowiedniki — i są udokumentowane w nagłówku SDP —
> ale **ten DisplayManager nie czyta ich w ogóle**: żaden z tych napisów nie występuje w jego
> binarce. Numer displayable jedzie w **captionie** okna, który DM zamienia na liczbę przez
> `atoi()`. Dowód z naszej binarki i niezależne potwierdzenie z rozbioru IDA tego samego builda
> (583 837 B) są w `docs/19_displayable_na_mhi2.md`.

**Kolejność jest odwrotna niż u niego.** Na jego Adreno okno musi powstać przed `eglGetDisplay`;
w OpenKODE `kdCreateWindow` bierze `EGLDisplay` i `EGLConfig` jako argumenty, więc najpierw EGL.
Wspólne zostaje tylko to, że `kdInitializeNV` idzie przed wszystkim (tak samo robi `vc_render`).

Że ta droga w ogóle istnieje na tym aucie, widać po dwóch fabrycznych binarkach, które robią dokładnie
tę sekwencję: `/eso/bin/apps/loadandshowimage` i `eso/lib/libdisplayinit.so` (obie: `kdCreateWindow` →
`ID_STRING` → `SIZE` → `kdRealizeWindow` → EGL → **GLES2 z shaderami**). M.I.B. używa tego mechanizmu
do swoich splashy.

## Co zostało z kodu Luki

**9345 linii bez zmian** — cała geometria manewrów, potok GLES2, mesh trasy, panel pasów, protokół.
Napisany od zera jest **tylko jeden plik**: warstwa platformy. Szczegółowy spis:
`port_k3346\altscreen\overlay\PROVENANCE.md`.

Jedyna zmiana w jego źródle: `render.c` — `#define GLPC_DIR` owinięty w `#ifndef`, bo katalog cache'u
shaderów nie może być ten sam na obu platformach (`/mnt/persist` u niego, `/var/app/icab/tmp` u nas).

---

## Co zmienia w aucie

| Plik | Zmiana |
|---|---|
| `/mnt/app/root/vc_overlay` | **nowy**: renderer nakładki (KD + EGL + GLES2) |
| `/mnt/app/root/vc_overlay_probe` | **nowy**: sonda kalibracyjna Etapu 7a |
| `/mnt/app/root/mib2high-overlay.conf` | **nowy**: numer displayable i geometria (do poprawienia po sondzie, **bez przebudowy**) |
| `/mnt/app/root/k3346-overlay-probe.sh` | **nowy**: sekwencja sondy |
| `/mnt/app/root/mib2high-lvds-control.sh` | podmiana (v8s): startuje nakładkę, deklaruje kontekst 901 |
| `/mnt/app/root/hooks/libmib2high-carplay-rgi-ipod.so` | podmiana (Etap 4): dosyła manewry do renderera |

**`vc_render` zostaje nietknięty.** Osobna transakcja `INSTALL_ETAP7` / `ROLLBACK_ETAP7` z własną kopią
w `backup\pre-etap7\`; cofa dokładnie te wersje plików, które zastała.

### Brama bezpieczeństwa, która sprawia, że porażka nic nie kosztuje

`mib2high-lvds-control.sh` przełącza kombi na kontekst 901 **tylko wtedy**, gdy renderer sam
zamelduje, że ma okno: po pierwszej wystawionej klatce pisze `/tmp/vc-overlay-ready` o treści
`id szerokość wysokość` (atomowo, przez `rename`). Skrypt czeka na ten plik do 6 s i sprawdza, czy
numer zgadza się z `mib2high-overlay.conf`. Bez meldunku — zostaje kontekst 77, czyli dzisiejszy
stan: mapa działa, fabryczna (zła) strzałka wraca, nic się nie psuje.

> **Pierwsza wersja bramy pytała `dmdt gd` i to było źle postawione pytanie.** Na tej jednostce
> `gd` zwraca zero bajtów, ale nawet gdyby drukowało listę, naszego okna by na niej nie było:
> DisplayManager wiąże numer displayable **z oknem**, biorąc go z jego captionu, i nie rejestruje
> go w niczym, co `gd` pokazuje (`docs\19`). Dowodem może być tylko renderer.

Trzy powody odmowy, każdy z własnym komunikatem w logu: brak meldunku w 6 s, meldunek z innym
numerem niż conf (np. **nieświeży**, po rendererze ubitym na twardo — dlatego skrypt usuwa plik
przed startem nakładki), odmowa `dmdt dc`. Osiem przypadków w `tools\sim_gate.sh`.

### Zasilanie danymi — bez zmian w Javie HMI

Świadome odejście od Luki (u niego serwerem jest Java). Java HMI to jedyny element, który potrafi
zabić auto bootloopem, a wszystkie potrzebne dane są już po stronie natywnej:

```
iPhone --iAP2 0x5202/0x5204--> hook w mm-ipod --TCP :19800 (48 B)--> vc_overlay
```

**Renderer jest serwerem, hook klientem** — odwrotnie niż u Luki, bo `mm-ipod` restartuje się przy
każdym podłączeniu iPhone'a, a `vc_overlay` żyje cały czas. Stabilny koniec musi być po stronie
renderera.

Pakiety idą **wyłącznie z własnego wątku z zablokowanym SIGPIPE** (`pthread_sigmask`, nigdy globalnie).
Zapis do gniazda po zamkniętym rendererze podniósłby SIGPIPE, a SIGPIPE w `mm-ipod` zabija CarPlay.
`MSG_NOSIGNAL` jest **nieużywalne**: QNX 6.5 definiuje makro, ale `send()` zwraca `ENOSYS` — raz już
po cichu zabiło cały bus (`framework/bus.c:211`).

---

## Dwie kopie mappera i co je pilnuje

Mapowanie iAP2 → BAP istnieje teraz dwa razy: w Javie (tekst i strzałka na VC przez BAP) i w C
(strzałka, którą rysujemy sami). Gdyby się rozjechały, auto pokazywałoby strzałkę przeczącą odległości
wypisanej obok niej.

Dlatego `tools\run_overlay_map_diff.sh` uruchamia **prawdziwą klasę Javy** i kod C na wyczerpującej
siatce i porównuje bajt w bajt:

> **126 672 przypadki** (58 typów manewru × 364 kąty × 3 typy skrzyżowania × 2 strony ruchu) — **identyczne**.

---

## Co sprawdzone na komputerze

| Sprawdzian | Wynik |
|---|---|
| Build renderera (QNX ARM, GLES2, bez C++ runtime) | OK, `vc_overlay` 602 KB |
| Build sondy | OK, `vc_overlay_probe` 52 KB |
| Zależności ELF (`tools\verify_overlay_elf.py`) | PASS — `libKD/libEGL/libGLESv2` są w aucie; **`libiow`/`libscreen` zablokowane** |
| Format transmisji, dekodowany **dekoderem Luki** (`cr_decode_maneuver`) | **54 OK, 0 FAIL** (ASan+UBSan) |
| Parser konfiguracji (CRLF, komentarze, śmieci, zakresy, numer 0/−1/110) | **69 OK, 0 FAIL** (ASan+UBSan) |
| Meldunek gotowości (`overlay_ready.h`) — także każdy sposób, w jaki może skłamać | **30 OK, 0 FAIL** (ASan+UBSan) |
| Czytanie slotu układu VC z loga hooka (`overlay_layout.h`) | **30 OK, 0 FAIL** (ASan+UBSan; w tym bufor kończący się dokładnie na znaczniku) |
| Mapper C ↔ Java | **126 672 przypadki identyczne** |
| **Symulacja bramy kontekstu** (`sim_gate.sh`) — pierwszy test tego kodu w ogóle | **54 OK, 0 FAIL** — dash i mksh |
| **Symulacja sondy** (`sim_probe.sh`, z odmową przy żywym rendererze) | **43 OK, 0 FAIL** — dash i mksh |
| **Symulacja instalatora** (`sim_installer.sh`, z blokiem T16 Etapu 7) | **236 OK, 0 FAIL** — dash i mksh |
| **Symulacja karty ratunkowej** (`sim_rescue.sh`, z R2b) | **52 OK, 0 FAIL** — mksh (ta symulacja wymaga `/bin/ksh`, pod dash się nie uruchamia) |
| `sprawdz_karte.ps1 -Typ etap7` | działa, sprawdzony na prawdziwej karcie |
| Składnia `sh -n` wszystkich skryptów | OK |
| Biblioteka v8s (`dio_manager`) po moich zmianach | **bit w bit bez zmian** — sprawdzone buildem z wyciętymi blokami |
| Biblioteka RGI Etapu 7 po poprawce CAPTION | **bit w bit bez zmian** (`4d81839730c0…`) — poprawka nie dotknęła hooka natywnego |
| Czy poprawka weszła do **binarki**, nie tylko do źródła | `vc_overlay` i `vc_overlay_probe` **nie zawierają** napisów `ID_STRING` ani `MANAGED`, zawierają `CAPTION` |
| Paczka SD (LF, architektura ARM, sumy) | OK, 11 plików |

### Trzy błędy, które złapały te zabezpieczenia

Warto je zapisać, bo każdy ugryzłby dopiero w aucie:

1. **Kolizja nazw w payloadzie.** Etap 4 i Etap 7 instalują ten sam plik docelowy
   (`libmib2high-carplay-rgi-ipod.so`). Gdyby dzieliły też nazwę w `payload/`, to
   `INSTALL_ETAP4` — uzbrojony na karcie użytkownika — wgrałby bibliotekę Etapu 7, a
   `ROLLBACK_ETAP7` przywracałby ją „sprzed Etapu 7”, czyli nie cofałby niczego.
   Etap 7 niesie ją teraz jako `libmib2high-carplay-rgi-ipod.etap7.so` (test T16i).
2. **Pełny `ROLLBACK` nie cofał Etapu 7.** Łańcuch kończył się na Etapie 5. Teraz leci
   7 → 5 → 4 → v8s, i Etap 7 **musi** być pierwszy: jego kopia zawiera bibliotekę z czasów
   Etapu 4, więc odtworzona po Etapie 4 nałożyłaby ją z powrotem na plik fabryczny (test T16h).
3. **Sonda miała zakodowaną ścieżkę** `/mnt/app/root/...` zamiast `pathof`, przez co w symulacji
   zawsze odmawiała, a błędu nie dałoby się wykryć inaczej niż w aucie (test T16g).

### Czego NIE sprawdzono

- **Niczego w aucie.** Cały etap jest nieprzetestowany na sprzęcie.
- **Czy DisplayManager MHI2 przyjmie nasze okno** — to jest główna niewiadoma i jedyny powód,
  dla którego istnieje sonda. Offline nie da się tego rozstrzygnąć.
- Geometria panelu KDK na B9 — dotąd **udokumentowana, nie zmierzona**
  (`docs\02:133`: małe (1055,207) 210×153, duże (1091,110) 328×180).
- **Fizyczna karta ratunkowa ma jeszcze starą wersję `ratunek.sh`.** Nowa (z usuwaniem plików
  Etapu 7) jest w `work\sd_ratunek\`. Przed tą wizytą **nie trzeba jej wgrywać**: Etap 7 nie
  dotyka Javy ani HMI, więc podstawowe zadanie karty się nie zmienia, a poprzednia podmiana
  `ratunek.sh` skończyła się koniecznością pełnego formatowania karty (`docs\00`).

---

## Dwie rzeczy znalezione po drodze, warte zapisania

1. **Świeży build biblioteki v8s nie wychodzi identyczny z tym w aucie.**
   W aucie / na karcie: `058413dd…`. Build dzisiaj: `b8da2da0…`. **To nie jest skutek moich zmian** —
   build z wyciętymi blokami Etapu 7 daje tę samą sumę `b8da2da0…`. Coś się rozjechało wcześniej
   (źródła albo stan kontenera) względem momentu, w którym składano kartę. Dlatego paczka Etapu 7
   **nie zawiera** v8s ani `vc_render` i nie rusza `work\sd_etap5\` — inaczej podmieniłaby w aucie
   plik przetestowany na nieprzetestowany. Do wyjaśnienia osobno.
2. **`dmdt gc` ucina wynik na 11 264 B** — wszystkie ~150 zebranych zrzutów kończą się na kontekście
   78 z 88. To limit po stronie `dmdt`/DisplayManagera, **nie** naszego skryptu, więc nie da się tego
   naprawić przekierowaniem. Dlatego COLLECT zbiera teraz dodatkowo `dmdt gd` (krótkie, z rozmiarami
   displayable) i `dmdt gs`.

---

## Instrukcja w aucie

Jedna strona do telefonu: `docs\20_sciagawka_etap7b.md`. Tu tylko to, czego tam nie ma.

### Trzy uruchomienia, bez sondy

| # | Znacznik | Wynik |
|---|---|---|
| 1 | `ROLLBACK_ETAP7` | `ETAP7_ROLLBACK_RESULT=PASS` |
| 2 | `INSTALL_ETAP7` | `ETAP7_INSTALL_RESULT=PASS`, potem **restart MMI** |
| — | podłącz iPhone, ustaw trasę | nakładka wstaje razem z sesją CarPlay |
| 3 | brak znacznika | `MODE=COLLECT` |

Oba znaczniki leżą już w katalogu pakietu na karcie, więc przy aucie **nie jest potrzebny czytnik
kart** — dyspozytor zużywa po jednym na uruchomienie, w ustalonej kolejności.

**Sondy nie ma w tej procedurze.** Odpowiedziała na wszystkie swoje pytania na wizycie 2, a na
jedyne otwarte (przezroczystość nad mapą) odpowiedzieć nie może: mapy nie ma w kompozycie
`dmdt ts`. Binarka i skrypt sondy nadal się instalują — zostają jako narzędzie regresji, gdyby
nakładka kiedyś przestała się pokazywać. Wtedy: `PROBE_ETAP7` z `ZNACZNIKI\`, iPhone odłączony.

### Co obejrzeć własnymi oczami

Kolejność pytań ma znaczenie, bo pierwsze widać **zanim** pojawi się jakikolwiek manewr — renderer
wystawia na starcie całkowicie przezroczystą klatkę na całe płótno:

1. **Czy mapa jest?** Jeśli kombi zrobiło się czarne, nasza warstwa zasłania mapę — odłącz telefon,
   wszystko wraca. To jedyne pytanie tej wizyty, na które nie znamy odpowiedzi.
2. Czy strzałka jest **w miejscu fabrycznego panelu**, a nie w rogu?
3. Czy strzałka odpowiada karcie instrukcji na mapie (lewo / prawo / rondo / zjazd)?
4. Czy po **VIEW** strzałka przesuwa się razem z zegarami? Od tej wersji slot układu jest stosowany.
5. Czy odległość, ulica i ETA (rysowane przez VC z BAP) są bez zmian? Ich nie ruszamy — znikłyby
   tylko przy regresie.

### Co powiedzą logi

W `collected\run-NN\logs\`:

| Plik | Czego szukać |
|---|---|
| `vc_overlay-launch.log` | `CAPTION="98"` i `realized native=` — okno powstało |
| | `READY /tmp/vc-overlay-ready -> 98 1440x542` — meldunek wyszedł |
| | **brak** `cannot write /tmp/vc-overlay-ready` — naprawa 1 działa |
| | **brak** serii `recreating window` — naprawa 2 działa |
| | `layout slot=N panel at (x,y)` — slot odczytany i zastosowany |
| `mib2high-lvds-control.log` | `overlay ready after Ns; context=901 {98,102,101,58} declared` |
| | zamiast `overlay did not report a window` z wizyty 2 |

### Ryzyko

Jedno, i jest odwracalne w sekundę: jeśli przezroczystość nie dociera do warstwy sprzętowej z mapą,
kombi pokaże czarne pole zamiast mapy na czas sesji CarPlay. **Odłączenie telefonu kończy sesję
i wszystko wraca.** Trwałe wyłączenie to `ROLLBACK_ETAP7` (wymaga skopiowania znacznika z
`ZNACZNIKI\` — stąd czytnik kart w liście rzeczy do zabrania).

Brama zostaje bez zmian i nadal jest głównym zabezpieczeniem: bez meldunku renderera kombi zostaje
na kontekście 77, a mapa, telefon, okładki i cofanie działają jak dziś. Dokładnie to zdarzyło się na
wizycie 2 — i dlatego nic się wtedy nie zepsuło.


## Budowanie (uzupełnienie `docs\10`)

```bash
bash tools/qnx_build.sh overlay/build_probe.sh   overlay/output/vc_overlay_probe
bash tools/qnx_build.sh overlay/build_overlay.sh overlay/output/vc_overlay
bash tools/qnx_build.sh build_rgi_ipod.sh output/libmib2high-carplay-rgi-ipod.so
# testy na komputerze (w kontenerze - host gcc + ASan/UBSan)
docker run --rm -v "$(pwd -W)/port_k3346/altscreen:/src" qnx65-armv7-toolchain:4.9 \
  bash -c 'cp -a /src /build && cd /build && bash overlay/tests/run_host_tests.sh'
bash tools/run_overlay_map_diff.sh          # C vs Java, 126672 przypadki
python tools/verify_overlay_elf.py port_k3346/altscreen/overlay/output/vc_overlay
bash tools/make_sd_etap7.sh                 # -> work/sd_etap7/

# symulacje - W KONTENERZE (/work to mount; na Windows sypie Permission denied)
docker run --rm -v "$(pwd -W)/work:/work" -v "$(pwd -W)/tools:/tools" \
  qnx65-armv7-toolchain:4.9 bash -c 'bash /tools/sim_installer.sh'       # 229/229
# ratunek wymaga /bin/ksh - bez mksh ta symulacja w ogole nie rusza
docker run --rm -v "$(pwd -W)/work:/work" -v "$(pwd -W)/tools:/tools" \
  qnx65-armv7-toolchain:4.9 bash -c 'apt-get update -qq && apt-get install -y -qq mksh &&
    ln -sf /bin/mksh /bin/ksh && ln -sf /bin/mksh /bin/sh && bash /tools/sim_rescue.sh'  # 52/52
```

## Etap 7d – karta rozmowy (po Etapie 7c)

Mechanizm gotowy, layout ustalony ze zdjęć użytkownika (2026-10-26), brakuje renderowania tekstu
(w rendererze Luki nie ma żadnego fontu — trzeba dorobić atlas glyphów).

Ze zdjęć wynikły trzy rzeczy, które przesądzają o projekcie karty:
- panel manewru ma **dwa miejsca**: wewnątrz prawego zegara (duże zegary) i jako pływająca karta
  po prawej (duża mapa) — to para in-tube/popup z dokumentacji Luki;
- ostrzeżenie o otwartych drzwiach **nie zasłania panelu manewru**, ale w układzie z dużą mapą siedzi
  lewo-środkowo na mapie → karta rozmowy musi iść w prawo;
- **bez nawigacji panelu nie ma w ogóle** → karta rozmowy nie może mieszkać wyłącznie w nim, i stąd
  wariant pełnopłótnowy (B) w sondzie.

Awatara dzwoniącego nie da się dostać — iAP2 `CallStateUpdate` nie przenosi obrazu.
