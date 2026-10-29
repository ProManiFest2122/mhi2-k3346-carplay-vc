# 18 – Etap 7: własna warstwa graficzna na VC (strzałka manewru + pasy)

Stan 2026-10-28: **ETAP 7 ZAMKNIĘTY — strzałka manewru działa na VC w obu układach zegarów i idzie
za VIEW bez chwili w złym miejscu** (wizyta 9). Pasy ruchu niesprawdzone w aucie: nie było jazdy
trasą z wieloma pasami. Paczka: `work\sd_etap7\`.

---

## Wynik wizyty 9 (2026-10-28, `run-32`…`run-35`) — etap zamknięty

Użytkownik: „Wszystko działa tak jak powinno". Logi:

| Przebieg | VIEW | pokazane po raporcie VC (`bap54`) | bezpiecznik | blokady kafelka | czas ukrycia |
|---|---|---|---|---|---|
| run-32 / 33 / 34 | 6 | 6 | 0 | 0 | 54–686 ms |
| run-35 | 10 | 10 | 0 | 0 | 54–681 ms |

`GL_OES_texture_npot: no`, `render 328x181 in RGBA8 (alpha OK)`, błędy GL: 1 na 150 klatek
(renderer, sporadyczny).

**Pasy ruchu** — ścieżka jest kompletna (`rgd_hook.c` → `k3346_overlay_feed_publish(upd, active_man,
active_lanes)` → `CMD_LANES_BEGIN/LANE/COMMIT` → `cr_lane_panel` u Luki), ale w żadnym przebiegu renderer
nie dostał pasów (0 linii `engine: lanes`): trasy testowe po osiedlu ich nie miały. Do sprawdzenia przy
okazji jazdy trasą z wielopasmowym skrzyżowaniem.

---

## Wynik wizyty 8 (2026-10-28, `run-31`)

Każde VIEW zalogowane, źródło `bap54`:

```
platform_harman: layout slot=1 (big clocks, bap54) panel at (1055,207)
platform_harman: layout slot=0 (small clocks, bap54) panel at (1091,110)
... (na zmianę, przy każdym naciśnięciu)
```

Zdjęcia: przy dużych zegarach strzałka w środku prawego zegara, przy małych w fabrycznym kafelku.
Ale tuż po VIEW przez chwilę nasz kafelek stoi jeszcze w miejscu poprzedniego układu, a fabryczne tło
już w nowym — a potem przeskakuje.

### Oś czasu jednego VIEW (ślad DSI z tej sesji)

```
89.326 12#…DSIKeyPanelListener#updateKey2#89324|int:4|int:35|int:1|…   wciśnięcie (słuchacz 12#)
89.515  1#…DSIKeyPanelListener#updateKey2#89324|int:4|int:35|int:1|…   to samo, słuchacz 1#
89.715 12#/1# …updateKey2#89523|int:4|int:35|int:0|…                   puszczenie
89.901  2#…DSIBAPListener#indication#89771|int:50|int:54|int:0|int:0|int:1   VC: nowy układ
```

Klawisz 35 urządzenia 4 to VIEW: w całej sesji 10 naciśnięć i 10 zmian BAP 54, 1:1; klawisze 36 i 37
układu nie zmieniają. **Od pierwszego rekordu klawisza do raportu mija ~575 ms** (9 z 10 razy
572–593 ms, raz 392 ms). VC przełącza się przy klawiszu; raport przychodzi później.

Do tego nasze dwa opóźnienia: układ sprawdzany najwyżej raz na sekundę, a po zmianie układu nic nie
rysowało kafelka od nowa — renderer Luki robi klatkę tylko przy zmianie sceny (`main.c:764`), a w
bezczynności pętla zwalnia do 3 Hz (`main.c:969-971`). Na postoju kafelek czekał w starym miejscu
na następną aktualizację manewru.

### Co zmienione (Etap 7i)

| Zmiana | Plik |
|---|---|
| rekord klawisza VIEW (dowolny słuchacz `N#`) ustawia `view_pending`, raport BAP 54 je zeruje; kopia klawisza ze znacznikiem trackera ≤ ostatniego BAP 54 jest pomijana | `overlay/overlay_layout.h` |
| na czas `view_pending` kafelek nie trafia na kombi (kolejny strażnik w `composite_panel`); bezpiecznik 1500 ms | `overlay/platform_harman.c` |
| przy ukryciu, pokazaniu i zmianie układu platforma sama kładzie ostatnią klatkę (`composite_panel` + `eglSwapBuffers`) — FBO panelu ją trzyma, nie trzeba czekać na `main.c` | `overlay/platform_harman.c` (`represent_last_frame`) |
| pierścień sprawdzany co 100 ms; plik nie jest otwierany, gdy nic nie przybyło | `overlay/overlay_layout.h`, `overlay/platform_harman.c` |

**Sprawdzone na prawdziwym śladzie z tej sesji** (`run-31`), przepuszczonym przez czytnik:
10 naciśnięć VIEW → 10 ukryć po 392–593 ms → za każdym razem pokazanie od razu we właściwym układzie,
na zmianę małe/duże; żadnej zmiany układu bez ukrycia, nic nie zostaje wiszące na końcu.

---

## Wynik wizyty 7 (2026-10-28, `run-29` przed jazdą, `run-30` po jeździe)

```
platform_harman: GL_OES_texture_npot: no
platform_harman: render 328x181 in RGBA8 (alpha OK), crop 210x153 @(59,27) into canvas 1440x542
platform_harman: first GL error 0x502 raised by the renderer, in frame 6
platform_harman: panel probe 2/8 alpha canvas=13.4% crop=24.1% | rgb canvas=13.4% crop=24.1%
platform_harman: panel probe a=[0..255] in crop, brightest=(248,243,242) at canvas(164,40) INSIDE crop
platform_harman: GL errors in the first 150 frames: renderer 5 (first 0x502), ours 0 (first 0x0)
```

`GL_OES_texture_npot: no` — przyczyna czarnego prostokąta z wizyt 3–6 jest teraz faktem, nie
wnioskiem. Scena się rysuje i jest w wycinku. Błędy GL: 5 na 150 klatek, wyłącznie od renderera,
sporadyczne — strzałce nie przeszkadzają.

**Ciemnoszare tło pod strzałką** to fabryczne tło kafelka (podkładka KDK, 101 albo 102): nasz
wycinek jest w ~76% przezroczysty, więc to, co widać za strzałką, leży pod nami. Fabryczna strzałka
stoi na identycznym tle (zrzuty z wizyty 1). **Zostaje — decyzja użytkownika.** W tym dokumencie
„czarny prostokąt" oznacza wyłącznie stan sprzed Etapu 7g: cały kafelek nieprzezroczyście czarny,
bez strzałki.

### Układ zegarów nigdy nie był wykrywany

`overlay_layout.h` czytał `V8H_LAYOUT slot=` z `/tmp/carplay_hook.log`. Hook pisze tę linię przez
`LOG_INFO` (`native/altscreen_session.c:1391`), a jego logger ma `.min_level = LOG_LEVEL_WARN`
(`native/framework/logging.h:57`). W `carplay_hook.log` z auta są **wyłącznie** linie `[WRN]`;
`V8H_LAYOUT` istnieje tylko jako napis w binarce. Renderer nie zalogował ani jednego
`layout slot=`, więc kafelek stał w `x/y` w obu układach.

Test hosta tego nie złapał, bo sprawdzał parser na liniach zmyślonych — a nie na tym, co hook
naprawdę zapisuje. Nowy test bierze linie skopiowane bajt w bajt z przechwytu z auta.

### Etykiety pomiaru z 2026-10-26 były zamienione

Pomiar jest dokładny (dwa zrzuty `dmdt ts 4`, kafelek 210×153): A = (1091, 110),
B = (1055, 207). Ale sonda kazała tylko „przełączyć VIEW na drugi układ" (`probe-run/probe.log:9`),
nie zapisując stanu początkowego; „duże zegary" dla A było założeniem. Geometria mówi odwrotnie:

- **małe zegary**: mały prędkościomierz zaczyna się w prawym dolnym rogu ok. y≈296 — B, kończący się
  na y=360, wszedłby na niego; A kończy się na 263. Na zdjęciu kafelek w A wygląda właściwie;
- **duże zegary**: A leży w górnej połowie prawego zegara, pod napisem odległości VC („● 100m"),
  i tam strzałka znikała; B to środek wewnętrznego okna zegara.

**Duże zegary (slot 1) = (1055, 207). Małe (slot 0) = (1091, 110).**

### Skąd teraz brać układ

Z tego samego strumienia, który czyta hook — z jego kopii, którą `mib2high-lvds-control.sh`
(linie 50–55) trzyma w każdej sesji: pierścień `/tmp/v8l-dsitracer-1798-00..03.log`
(4 × 512 KB, rotacja przez `fopen("w")`, każda linia z prefiksem `sekundy.ms `). Parser jest
**hookowy** (`native/gauge_bap54.h`, włączony, nie skopiowany): BAP 50/54, tylko rekordy `2#`,
bit 0: 0 = duże, 1 = małe. Do pierwszego BAP 54 zapasowo `1#…DSIMapViewerZoomEngine#setZoomArea`
(460 → duże, 700 → małe), potem ignorowany — polityka hooka 1:1.

Nie port 1798 na żywo, bo VC podaje stan tylko przy starcie sesji i przy VIEW — w `run-30` rekord
przyszedł o 52.424 s, renderer wystartował o 52.464 s. Nie zmiana poziomu logu w hooku, bo to
biblioteka v8s Etapu 3, a jej świeży build nie wychodzi bit w bit taki jak ten w aucie.

**Sprawdzone na prawdziwych danych:** przechwyt z jazdy (`run-30`, 309 KB) przepuszczony przez
nowy czytnik daje 12 zmian układu — 49,7 s zoom → duże, potem BAP 54 na zmianę: 52,4 małe,
65,3 duże, 67,8 małe, 70,1 duże, 72,8 małe, 228,5 duże, 234,7 małe, 263,5 duże, 265,5 małe,
385,9 duże. Pełny czytnik po plikach pierścienia kończy na „duże, bap54" i kafelku w (1055,207).
Podczas tej jazdy układ zmienił się jedenaście razy; kafelek ani razu.

### Ramię skrętu leży w górze wycinka

Mapki sondy pokazują, że przy skręcie ramię strzałki leży w samej górze wycinka (odczyt 8/8,
skręt w lewo: wiersz 1 = `...######`). Gdyby VC przy dużych zegarach zasłaniał górę kafelka,
zniknęłoby właśnie ono. W pozycji (1055,207) fabryczny kafelek ma pełne tło, więc nie powinien —
ale na wszelki wypadek jest pokrętło `visible_top_big` w pliku na karcie: liczba górnych wierszy
do pominięcia, przekazywana do `render_set_visible_area()` (to samo wywołanie, którym na MHI2Q
steruje Java przez `CMD_VISIBLE_AREA`); renderer przekadrowuje manewr płynnie.
---

## Wynik wizyty 6 (2026-10-28, `run-27` na karcie)

```
platform_harman: window alpha_bits=8
platform_harman: fbo alpha test RGBA8: alpha OK (alpha_bits=8, dst.a=0 -> rgb 0, dst.a=1 -> rgb 255, readback a 0/255)
platform_harman: render 328x181 in RGBA8 (alpha OK), crop 210x153 @(59,27) into canvas 1440x542
scene: kind=0 fallback=0 commands=34
platform_harman: GL error 0x502 pending before the readback
platform_harman: panel probe 1/8 alpha canvas=100.0% crop=100.0% | rgb canvas=0.0% crop=0.0%
platform_harman: panel probe a=[255..255] in crop, brightest=(0,0,0) at canvas(0,180) outside crop
platform_harman: panel map |................|        (x8, całe płótno)
                 (to samo po 3 s i po 10 s)
```

### Bufor alfę ma — więc wniosek z wizyty 5 był błędny

Test z Etapu 7f pyta o alfę przez RGB i odpowiada jednoznacznie: RGBA8 w FBO alfę trzyma.
A mimo to w stanie ustalonym — trzy odczyty w ciągu 10 s, wszystkie po wygaszaniu — panel to
dokładnie `(0,0,0,255)` w każdym pikselu całego 328×181.

### `(0,0,0,1)` to wartość niekompletnej tekstury

Specyfikacja GLES 2.0, §3.8.2: próbkowanie niekompletnej tekstury zwraca `(R,G,B,A) = (0,0,0,1)`.
Tekstura NPOT jest niekompletna, gdy jej tryb zawijania nie jest `GL_CLAMP_TO_EDGE` — chyba że
sterownik ma `GL_OES_texture_npot`. Domyślny tryb to `GL_REPEAT`.

| Tekstura w `render.c` | Rozmiar | Zawijanie | Na Tegrze 3 |
|---|---|---|---|
| maski (`fbos_init`) | 1838×1016 | `CLAMP_TO_EDGE` | kompletna |
| atlas flagi | 1792×128 | `CLAMP_TO_EDGE` | kompletna |
| **`g_ss_tex`** | 525×290 | **domyślne `REPEAT`** | **niekompletna** — FXAA czyta `(0,0,0,1)` |
| **`g_fxaa_tex`** | 525×290 | **domyślne `REPEAT`** | **niekompletna** — ostatni przebieg czyta `(0,0,0,1)` |

Ostatni przebieg `render_end_frame` kopiuje `g_fxaa_tex` do panelu z wyłączonym mieszaniem —
panel dostaje `(0,0,0,1)` wszędzie, nasz blit kładzie to na kombi. Scena (34 komendy) rysuje się
do SSAA poprawnie, tylko nikt nie umie jej przeczytać. Na Adreno w MHI2Q `REPEAT` na NPOT jest
legalne (`GL_OES_texture_npot`), więc u Luki działa, a jego aktualny `render.c` ma ten sam kod.

To tłumaczy też wizyty 3–5: SSAA było zawsze NPOT (336×245, potem 525×290). Płyta była ta sama;
zmieniały się tylko nasze hipotezy — przesunięcie kadru, rozmiar płótna, brak alfy.

Na wizycie 7 log z auta potwierdził to wprost: `GL_OES_texture_npot: no`.

### Co zmienione (Etap 7g)

| Zmiana | Plik |
|---|---|
| `CLAMP_TO_EDGE` na `g_ss_tex` i `g_fxaa_tex` | `overlay/upstream/render.c` (piąta zmiana w kodzie Luki, `PROVENANCE.md`) |
| log `GL_OES_texture_npot: yes/no` + pełna lista rozszerzeń | `overlay/platform_harman.c` |
| **druga siatka**: kafelek ≥ 99% nieprzezroczysty i < 1% oświetlony nie trafia na kombi; sonda sprawdza to cicho już w pierwszej klatce i biegnie **przed** blitem | `overlay/overlay_fbo.h` (`overlay_tile_is_degenerate`), `overlay/platform_harman.c` |
| `0x502`: kto podnosi (renderer czy my), w której klatce pierwszy raz, ile razy w 150 klatkach | `overlay/platform_harman.c` (`platform_swap`) |

---

## Wynik wizyty 5 (2026-10-28, `run-25` i `run-26` na karcie)

### Etap 7e zadziałał

```
platform_harman: displayable=98 render=328x181 crop=210x153@(59,27) canvas=1440x542
maneuver_render: framebuffer 328x181
render: SSAA 525x290 -> 328x181
server: connected to 127.0.0.1:19800
engine: reveal from clear icon=2
scene: kind=0 fallback=0 commands=34
platform_harman: panel drawn
```

Po ujawnieniu silnik przechodzi ~25 cykli `queued`/`promoting` — treść jest obecna przez dłuższą
chwilę. Strzałki na kombi nie ma, czarny prostokąt jest w **obu** układach.

### Sonda: alfa 255 w każdym pikselu

> **SPROSTOWANIE (wizyta 6).** Wniosek tej sekcji — „bufor nie ma kanału alfa" — był **błędny**.
> Test z Etapu 7f pokazał, że RGBA8 w FBO alfę trzyma. `(0,0,0,255)` to wartość, którą GLES2
> zwraca przy próbkowaniu **niekompletnej tekstury**: bufory SSAA/FXAA Luki są NPOT bez
> `CLAMP_TO_EDGE`. Tekst niżej zostaje jako zapis rozumowania, które zawiodło.

```
panel probe 1/8 canvas=100.0% crop=100.0% a=[255..255] brightest=(0,0,0) at crop(0,152)
panel map |################|        (osiem takich linii)
```

To nie jest obraz, który renderer narysował — to obraz bufora, **w którym nie ma kanału alfa**.
Odczyt padł zaraz po tym, jak `render_end_frame` wyczyścił bufor do `(0,0,0,0)` i wrzucił nań
teksturę FXAA. Gdyby kanał istniał, tło musiałoby wyjść jako 0. Wyszło 255, jednolicie, na
całym 328×181.

A blit kopiuje panel na płótno kombi z **wyłączonym** mieszaniem — to jest poprawne, bo panel
trzyma już kolor premultiplikowany — więc alfa panelu staje się alfą płótna. Bez kanału:
255 na całym wycinku, czyli **nieprzezroczysta czarna płyta**, niezależnie od tego, co renderer
narysował. Dokładnie to widać na zdjęciach.

Okno (FBO 0) alfę ma: EGL prosi o `EGL_ALPHA_SIZE 8`, a schodek alfy z Etapu 7a wrócił na
zrzucie jako dokładnie `255·a`. Problem dotyczy **tekstury podpiętej jako FBO**. GLES2 gwarantuje
alfę tylko dla `GL_RGBA4` i `GL_RGB5_A1`; `GL_RGBA8` w FBO to rozszerzenie
(`GL_OES_rgb8_rgba8`), a sterownik może przyjąć żądanie i kanał po cichu zgubić.

### Czego sonda nie powiedziała — trzy moje błędy

1. **Odczyt w pierwszej klatce treści.** `main.c` ustawia wtedy `g_fade_alpha = 0` i wygasza
   przez 8 klatek. `brightest=(0,0,0)` w tej jednej klatce niczego nie dowodzi o stanie ustalonym.
2. **Drugi odczyt wyłączył sondę na całą sesję** (`glReadPixels refused`). `glGetError()` zwraca
   *pierwszy* błąd w kolejce, nie błąd ostatniego wywołania — złapał coś zostawionego przez kod
   Luki. Że w kolejce w ogóle jest błąd GL, to osobna informacja, której nie zapisaliśmy.
3. **Mapka liczona tylko w wycinku.** Przy zepsutej alfie pokrycie alfy wynosi wszędzie 100%,
   więc „scena rysuje się obok wycinka" i „nie rysuje się wcale" dają identyczny log.

### Co zmienione (Etap 7f)

| Zmiana | Plik |
|---|---|
| format FBO **mierzony**: `RGBA8` → `RGBA4` → `RGB5A1`, wygrywa pierwszy, który zachowa alfę | `overlay/overlay_fbo.h` (nowy, decyzja), `overlay/platform_harman.c` (pomiar) |
| test alfy **przez RGB**: czyść do `(0,0,0,A)`, biały kwadrat z `glBlendFunc(GL_DST_ALPHA, GL_ZERO)`, czytaj RGB | `overlay/platform_harman.c` |
| **brak potwierdzonej alfy = panel nie trafia na kombi** — zostaje obraz fabryczny | `overlay/platform_harman.c` (`composite_panel`) |
| bufory Luki (SSAA, FXAA, maski) biorą ten sam typ przez `platform_fbo_type()` | `overlay/upstream/platform.h`, `overlay/upstream/render.c` (5 alokacji) |
| sonda: odczyt po 1 s, czyszczenie i logowanie kolejki błędów GL, ponawianie, całe płótno + pokrycie RGB | `overlay/platform_harman.c` |
| decyzja przetestowana na wszystkich 64 kombinacjach werdyktów | `overlay/tests/overlay_fbo_test.c` (nowy) |

**Dlaczego test pyta przez RGB.** Sterownik, który gubi alfę w FBO, może też zwracać 255 jako
alfę w `glReadPixels` — wtedy dwa różne błędy wyglądają identycznie. Mieszanie z
`GL_DST_ALPHA` przenosi pytanie do kanału RGB, któremu ufamy: GLES2 definiuje `GL_DST_ALPHA`
jako 1, gdy bufor alfy nie ma, więc przy `A=0` wraca 255 zamiast 0.

**Dlaczego brak blitu, a nie „i tak spróbuj".** Brama kontekstu pilnuje przypadku *renderer
nie wstał*. Ten warunek pilnuje przypadku *renderer wstał, ale nie umie się wtopić*. Wcześniejsza
wersja w tym przypadku degradowała się do stanu **gorszego niż fabryczny**; nakładka bez
strzałki to słabsza funkcja, nakładka z czarną płytą to gorsze auto.

---

## Wynik wizyty 4 (2026-10-27, `run-24` na karcie)

### Etap 7d zadziałał — łącze stoi

```
00:00:49  server: 127.0.0.1:19800 not yet listening, will retry
          render: flag atlas loaded 1792x128 (14 frames) tex=6 gl_err=0x0
          scene: kind=1 fallback=0 commands=0
          platform_harman: panel blank (nothing to show)
00:01:31  server: connected to 127.0.0.1:19800
          engine: reveal from clear icon=2
          scene: kind=0 fallback=0 commands=34
          platform_harman: panel drawn
          engine: first frame ready
```

Hook nasłuchuje, renderer się wpiął, manewr dotarł, scena ma 34 komendy rysowania, atlas flagi
się ładuje (po wizycie 3 brakowało go w aucie), a bramka „nie maluj pustego panelu" robi swoje —
**bez nawigacji czarny prostokąt zniknął**. Z nawigacją wraca i strzałki nie ma.

### Czemu nie ma strzałki: renderer nie umie rysować na podanym mu płótnie

`CR_DEFAULT_WIDTH/HEIGHT` z `upstream/protocol.h` (328×181) to **nie** domyślny rozmiar okna.
To układ współrzędnych całego renderera Luki, który w ogóle się nie skaluje:

| Miejsce | Co robi z tymi stałymi |
|---|---|
| `protocol.h:116` | `CR_POPUP_` = 59, 27, 210, 153 — „big-screen popup crop within the 328x180 content area" |
| `render.c:238` | domyślny `visible_area` = ten wycinek |
| `render.c:1219` | środkowanie manewru **w wycinku**, liczone z `CR_DEFAULT_HEIGHT` i `CR_POPUP_Y` |
| `render.c:1280` | przesunięcie MVP dzielone przez `CR_DEFAULT_WIDTH/HEIGHT` |
| `render.c:1407` | nożyce: `clip.x * g_ss_w / CR_DEFAULT_WIDTH` — poprawne tylko gdy `g_ss_w = 328 * SSAA` |
| `render.c:1425` | **każdy** element 2D przeliczany na NDC przez `CR_DEFAULT_WIDTH/HEIGHT` |
| `scene/lane_panel.cpp:286` | `render_build_framing_matrix(m, 328.f/181, dolly)` — aspekt wpisany na sztywno |
| `scene/lane_panel.cpp:179` | pojemność listy pasów liczona z `fminf(w, 328.f)` |
| `scene/style.hpp:13` | `constexpr float SourceWidth = 328, SourceHeight = 181;` — nazwane wprost |

`platform_get_framebuffer_size()` zwracało `width/height` z konfiguracji, czyli **210×153**
(w logu: `maneuver_render: framebuffer 210x153`, `render: SSAA 336x245 -> 210x153`). Wszystkie
powyższe wzory liczyły wtedy w złym układzie — nożyce tnące 336-pikselowy bufor miarką 328,
elementy 2D poza kadrem, solver kadrowania dobierający dolly pod aspekt 1,81 dla bufora
o aspekcie 1,37.

**Minę opisaliśmy sami i sami na nią weszliśmy.** `overlay_cfg.h:14` mówił
`width 328 # render source size`, a trzy linijki niżej ostrzegał, że zmiana tego rozmiaru psuje
aspekt i FOV. Sonda z Etapu 7a ma `REF_CROP_X/Y/W/H` = 59/27/210/153 z komentarzem *„Luka's popup
crop on MHI2Q … so the screenshot shows at a glance whether the VC on this car crops our source
the same way his does"*. Po pomiarze fabrycznego kafelka (2026-10-26, `dmdt ts 4`, 210×153)
wpisaliśmy pomiar **w rozmiar renderu zamiast w rozmiar wycinka**.

Wniosek do zapamiętania: gdy komentarz nazywa pole *render source size*, pomiar ekranu do niego
nie należy — nawet jeśli liczby wyglądają na „ten sam rodzaj rzeczy".

### Co zmienione (Etap 7e)

| Zmiana | Plik |
|---|---|
| trzy prostokąty rozdzielone: `render_*` (płótno), `crop_*` + `width/height` (wycinek), `x/y` (pozycja) | `overlay/overlay_cfg.h`, `config/mib2high-overlay.conf` |
| `platform_get_framebuffer_size()` zwraca rozmiar **renderu**; FBO panelu też | `overlay/platform_harman.c` |
| matematyka blitu (dwa flipy + mapowanie 1:1) w jednym, testowalnym miejscu | `overlay/overlay_blit.h` (nowy) |
| sonda: renderer odczytuje własny panel i loguje pokrycie alfy + mapkę 16×8 | `overlay/platform_harman.c` |
| 221 asercji na blicie, klamry wycinka, zgodność starej karty | `overlay/tests/overlay_blit_test.c` (nowy), `overlay_cfg_test.c` |

Wycinek `(59, 27, 210×153)` to dokładnie prostokąt, w który renderer wkadrowuje gotowy manewr,
a 210×153 to zmierzony fabryczny kafelek — więc blit jest **1:1**, bez skalowania. Że te dwie
liczby są równe, nie jest przypadkiem: oba kombi biorą kafelek manewru z tego samego katalogu
stylów KVS_FPK.

### Czyj jest ciemny prostokąt — rozstrzygnie sonda, nie oko

Dwie możliwości: nasz panel z nieudanym kadrem (ciemna jezdnia ze sceny wypełnia kafelek) albo
fabryczne podkładki 101/102, odsłonięte po wyrzuceniu displayable 20. **Zdjęcie ich nie rozróżni**
i dwie wizyty poszły na tę niejednoznaczność — mapa CarPlay jedzie warstwą sprzętową i nie ma jej
na `dmdt ts`, a ciemny prostokąt na nocnym zdjęciu wygląda tak samo w obu przypadkach.

Dlatego renderer czyta teraz własny panel (`glReadPixels`, kilka razy na sesję, nigdy przy pustym
panelu) i wpisuje do logu: pokrycie alfy na całym płótnie i osobno w wycinku, zakres alfy,
najjaśniejszy piksel i mapkę 16×8. Treść poza wycinkiem, ale nie w nim, to podpis błędu kadru;
nic nigdzie to martwa scena; pełny wycinek przy alfie 255 znaczy, że prostokąt jest nasz.

**`context_recipe` zostaje `102 101 58`.** Zdjęcie bez nawigacji pokazuje, że na tych podkładkach
siedzi fabryczna chrome mapy („85m", „<ulica>", „200m", „offroad"), więc wpisanie samego `58`
kosztowałoby te elementy. Zielona karta („towards <ulica>") jest **po angielsku przy polskim
UI auta** → rysuje ją iPhone w strumieniu CarPlay (58), nie kombi.

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
| ~~duże zegary~~ **małe zegary** (sprostowanie 2026-10-28) | **210 × 153 @ (1091, 110)** |
| ~~po VIEW~~ **duże zegary** | **210 × 153 @ (1055, 207)** |

> **Sprostowanie (2026-10-28).** Pozycje są dokładne, etykiety były zamienione: sonda nie zapisała,
> od którego układu auto zaczęło. Uzasadnienie w sekcji „Wynik wizyty 7".

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
| Build renderera (QNX ARM, GLES2, bez C++ runtime, **bez ostrzeżeń** `-Wall -Wextra`) | OK, `vc_overlay` 624 KB |
| Build sondy | OK, `vc_overlay_probe` 54 KB |
| Zależności ELF (`tools\verify_overlay_elf.py`) | PASS — `libKD/libEGL/libGLESv2` są w aucie; **`libiow`/`libscreen` zablokowane** |
| Format transmisji, dekodowany **dekoderem Luki** (`cr_decode_maneuver`) | **62 OK, 0 FAIL** (ASan+UBSan; w tym topologia gniazda: feed nasluchuje, renderer sie laczy) |
| Parser konfiguracji (CRLF, komentarze, śmieci, zakresy, numer 0/−1/110, wycinek w płótnie, stara karta sprzed rozdzielenia `render_*`) | **142 OK, 0 FAIL** (w tym zamienione pozycje i `visible_top_big/small`) (ASan+UBSan) |
| Meldunek gotowości (`overlay_ready.h`) — także każdy sposób, w jaki może skłamać | **41 OK, 0 FAIL** (ASan+UBSan) |
| Układ zegarów VC (`overlay_layout.h`) — z pierścienia przechwytu DSI, parserem hooka; dane testowe to linie skopiowane bajt w bajt z przechwytu z auta; rotacja pierścienia, linie niedokończone, duplikaty `3#`, priorytet BAP 54 nad zoom-area, klawisz VIEW (`12#` i `1#`, wciśnięcie i puszczenie, klawisz 36 i rolka jako nie-VIEW, spóźniona kopia klawisza pomijana) | **87 OK, 0 FAIL** (ASan+UBSan). Do tego dwa przechwyty z jazdy przepuszczone przez czytnik: `run-30` — **12 zmian układu**, stan końcowy zgodny; `run-31` — **10 VIEW → 10 ukryć po 392–593 ms**, za każdym razem pokazanie we właściwym układzie. Poprzednia wersja tego testu (30 OK) sprawdzała parser na liniach, których hook nigdy nie zapisywał |
| **Geometria blitu** (`overlay_blit.h`): teksel `(crop_x+i, crop_y+j)` ma wyjść w pikselu kombi `(dst_x+i, dst_y+j)` — oba odwrócenia osi, skala 1:1, oba układy zegarów, karty przycięte przez `sanitize` | **221 OK, 0 FAIL** (ASan+UBSan) |
| **Co wolno pokazać na kombi** (`overlay_fbo.h`): panel tylko wtedy, gdy któryś format naprawdę zachował alfę (wszystkie 64 kombinacje werdyktów), i nigdy kafelek „cały nieprzezroczysty, prawie nieoświetlony” (granice 99% / 1% z obu stron, siatka 101×101, w tym dosłowny odczyt z auta) | **223 OK, 0 FAIL** (ASan+UBSan) |
| Mapper C ↔ Java | **126 672 przypadki identyczne** |
| **Symulacja bramy kontekstu** (`sim_gate.sh`) | **62 OK, 0 FAIL** — dash i mksh |
| **Symulacja sondy** (`sim_probe.sh`, z odmową przy żywym rendererze) | **43 OK, 0 FAIL** — dash i mksh (25 to przebieg pod samym dash, gdy w kontenerze brak `mksh`) |
| **Symulacja instalatora** (`sim_installer.sh`, z blokiem T16 Etapu 7) | **236 OK, 0 FAIL** — dash i mksh |
| **Symulacja karty ratunkowej** (`sim_rescue.sh`, z R2b) | **52 OK, 0 FAIL** — mksh (ta symulacja wymaga `/bin/ksh`, pod dash się nie uruchamia) |
| `sprawdz_karte.ps1 -Typ etap7` | działa, sprawdzony na prawdziwej karcie |
| Składnia `sh -n` wszystkich skryptów | OK |
| Biblioteka v8s (`dio_manager`) po moich zmianach | **bit w bit bez zmian** — sprawdzone buildem z wyciętymi blokami |
| Biblioteka RGI Etapu 7 po poprawce CAPTION | **bit w bit bez zmian** (`4d81839730c0…`) — poprawka nie dotknęła hooka natywnego |
| Czy poprawka weszła do **binarki**, nie tylko do źródła | `vc_overlay` i `vc_overlay_probe` **nie zawierają** napisów `ID_STRING` ani `MANAGED`, zawierają `CAPTION` |
| Paczka SD (LF, architektura ARM, sumy) | OK, 12 plików, 1,9 MB |
| Czy rozmiar renderu weszedł do **binarki** | `vc_overlay` zawiera `render %dx%d`, `crop %dx%d`, `panel probe`; **nie zawiera** starego `panel %dx%d into canvas` |
| Czy poprawka NPOT weszła do **binarki** | zliczenie instrukcji `movw` ze stałymi GL w kodzie ARM: `WRAP_S` **3 → 5**, `WRAP_T` **3 → 5**, `CLAMP_TO_EDGE` **6 → 10**; binarka zawiera `GL_OES_texture_npot: %s`, `TILE IS A BLACK SLAB`, `GL errors in the first %u frames` |
| Czy test alfy i nowa sonda weszły do **binarki** | zawiera `fbo alpha test`, `window alpha_bits`, `NO FORMAT KEEPS ALPHA`, `panel NOT composited`, `rgb canvas=`, `GL error … pending`; **nie zawiera** `glReadPixels refused` (komunikatu, który wyłączał sondę na całą sesję) |

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
| | `maneuver_render: framebuffer 328x181` i `render: SSAA 525x290 -> 328x181` — renderer dostał **swoje** płótno (na wizycie 4 było `210x153`, i to była cała przyczyna braku strzałki) |
| | `render 328x181, crop 210x153 @(59,27)` — co wycinamy i skąd |
| | `server: connected to 127.0.0.1:19800` i `scene: … commands=N` z N > 0 — dane o manewrze dochodzą |
| | `panel probe N/8 canvas=… crop=…` — **ile treści jest w wycinku**; powyżej zera znaczy, że strzałka jest w kadrze, a zero przy niezerowym `canvas=` znaczyłoby, że scena rysuje się obok wycinka |
| | `panel map \|…\|` — osiem linii mapki alfy; widać na nich kształt strzałki i to, czy wypełnia cały kafelek (wtedy ciemny prostokąt jest nasz) |
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
