# 19 – Jak powstaje displayable na MHI2 (K3346)

Rozpoznanie offline, 2026-10-26, uzupełnione 2026-10-27. Powstało po tym, jak sonda z wizyty 1 nie
doczekała się adopcji okna, a projekt **chefranov/mhi2-au37x-carplay** podpowiedział, gdzie szukać.

**Stan: poprawka wdrożona 2026-10-27** (Etap 7b) — `kd_surface.c` ustawia CAPTION, brama w
`mib2high-lvds-control.sh` pyta renderera, karta odświeżona. Do rozstrzygnięcia w aucie została
tylko geometria (sekcja „Czego nadal nie wiem").

**Każde twierdzenie niżej ma dowód z naszego własnego firmware.** Tam, gdzie czegoś nie wiem,
jest to napisane wprost — przy wizycie 1 dwie moje hipotezy okazały się błędne i wolę ten błąd
powtórzyć w dokumentacji niż w aucie.

Dowody pochodzą z:
- `work\sd_odczyt_1\...\files\eso\bin\apps\displaymanager` — **binarka z naszego auta** (583 837 B),
- `work\lsd.jar` — Java HMI z naszego firmware,
- `work\sd_odczyt_1\...\files\mnt\system\etc\eso\production\displaymanager.json` — konfiguracja z auta,
- `work\auto_test_16\...\run-21\pidin-ar.txt` — lista procesów z auta.

---

## Wniosek najważniejszy

**Okno GL deklaruje swój numer displayable przez `KD_WINDOWPROPERTY_CAPTION` (69), jako napis
z liczbą.** Nie przez `ID_STRING`, nie przez `MANAGED`.

Nasza warstwa platformy ustawiała `KD_WINDOWPROPERTY_ID_STRING_QNX` (1002) i `MANAGED_QNX` (1013) —
**obu tych właściwości ten DisplayManager w ogóle nie czyta.** To jest przyczyna, dla której okno
nigdy nie zostało przyjęte.

Dowód — obecność i brak napisów w binarce z auta:

| Napis | W `displaymanager` |
|---|---|
| `ID_STRING` | **BRAK** |
| `MANAGED` | **BRAK** |
| `WINDOWPROPERTY_CAPTION` | **JEST** |
| `STREAMPROPERTY_DISPLAYABLE_ID` | **JEST** |
| `kdAcceptClientWindowNV` | **JEST** |
| `kdGetRequestedClientWindowPropertycvNV` | **JEST** |

Komunikaty diagnostyczne samego DisplayManagera:

```
KD_WINDOWPROPERTY_CAPTION (%p): %d
KD_WINDOWPROPERTY_CAPTION (%p): invalid window caption !!
CAPTION evaluated to 0. Most likely CAPTION string was invalid
CAPTION string seems too long
KD: new window available: NV: %p DM: %p
KD: new window ID: NV: %p DM: %p = %d
```

Czyli: caption jest zamieniany na liczbę (`%d`), **0 oznacza „nieprawidłowy"**, a zbyt długi napis
jest odrzucany. Numer `98` spełnia oba warunki.

## Niezależne potwierdzenie: Luka rozebrał TĘ SAMĄ binarkę

Znalezione 2026-10-27 w `repos/mib2q-carplay-rgi/docs/reference/MHI2_MU1439_PORT.md:198-256`.
To osobna, wcześniejsza praca — rozbiór w IDA firmware **MHI2_US_AUG22_K3347_MU1439**, czyli
amerykańskiego bliźniaka naszego K3346. I co najważniejsze:

| | Luka (MU1439) | My (K3346 / MU1438) |
|---|---|---|
| `displaymanager` rozmiar | 583 837 B | **583 837 B** |
| framework | `5.41.10.SR5 MIB2CLU6` | ten sam napis w binarce |

**To jest ten sam build.** Jego wnioski stosują się do naszego pliku bezpośrednio, a mój wynik
oparty na napisach i jego oparty na dekompilacji zgadzają się co do joty. Dwie metody, dwa
niezależne podejścia, jedna odpowiedź.

Fabryczny przepis, który podaje (`libdisplayinit.so` `display_create_window@0x136c`, tego samego
używa `/eso/bin/apps/loadandshowimage`):

```c
KDWindow *w = kdCreateWindow(egl_dpy, egl_cfg, NULL);
char cap[16]; sprintf(cap, "%d", 98);                 /* caption = numer displayable */
kdSetWindowPropertycv(w, KD_WINDOWPROPERTY_CAPTION /*0x45*/, cap);
KDint sz[2] = {328, 181};
kdSetWindowPropertyiv(w, KD_WINDOWPROPERTY_SIZE /*0x42*/, sz);
EGLNativeWindowType nw; kdRealizeWindow(w, &nw);
```

Zwróć uwagę, czego w nim **nie ma**: ani `ID_STRING`, ani `MANAGED`, ani `DISPLAY`. Dlatego nasz
`kd_surface.c` też już ich nie ustawia, a `display` domyślnie jest `-1` („nie ruszaj").

Jak zachowuje się handler captionu (`sub_15E588`, `atoi`, bez żadnej walidacji — nie sprawdza
konfiguracji, listy zajętych numerów ani duplikatów):

| Caption | Skutek |
|---|---|
| dowolna liczba niezerowa, np. `"98"` | zapisana jako numer displayable okna |
| `"0"` albo nie-liczba | loguje `CAPTION evaluated to 0`, ale **zapisuje id 0** |
| `"-1"` | okno **ignorowane** |
| dłuższy niż 64 znaki | odrzucony (`CAPTION string seems too long`) |

Stąd walidacja w `overlay_cfg_sanitize()`: dopuszczamy wyłącznie 1..109. Numer 0 dałby nam cudzy
displayable, `-1` wyrzuciłby okno bez słowa. Brak sprawdzania duplikatów znaczy, że dwa okna z tym
samym numerem to zachowanie nieokreślone — i że 98 jest bezpieczne, bo **nie występuje w żadnym
fabrycznym kontekście**.

Jedno zastrzeżenie, które Luka stawia sam i które trzeba powtórzyć: cała ta sekcja u niego jest
oznaczona `[decompile]` i **nie była testowana na aucie MHI2**. Zgodność z moim odczytem napisów
podnosi pewność mechanizmu, ale nie zastępuje próby w aucie.

## Protokół adopcji okna

DisplayManager jest **klientem NV window managera**. Importuje:

```
kdAcceptClientWindowNV      kdCloseClientWindowNV
kdGetRequestedClientWindowPropertycvNV   kdGetRequestedClientWindowPropertyivNV
kdSetClientWindowPropertyivNV            kdUnlockClientWindowBufferNV
kdAcceptClientStreamNV      kdManageStreamsNV     kdSetDesktopLayerPropertyivNV
```

Przebieg, odtworzony z napisów (`KD_EVENT_GUI_WINDOWAVAILABLE_NV`, `…WINDOWBUFFERS_NV`,
`…WINDOWCLOSED_NV`):

1. obcy proces tworzy okno KD i ustawia **CAPTION** na numer displayable,
2. DM dostaje `KD_EVENT_GUI_WINDOWAVAILABLE_NV`,
3. odczytuje żądaną właściwość okna (`kdGetRequestedClientWindowPropertycvNV`),
4. zamienia caption na liczbę — to jest numer displayable,
5. przyjmuje okno (`kdAcceptClientWindowNV`) i od tej pory składa je w kontekstach.

**Dwa różne kanały, zależnie od rodzaju źródła:**

| Źródło | Jak deklaruje numer |
|---|---|
| okno GL (nasza nakładka) | `KD_WINDOWPROPERTY_CAPTION` |
| strumień wideo (NvSS) | `KD_STREAMPROPERTY_DISPLAYABLE_ID_NV` |

To drugie wyjaśnia, dlaczego `vc_render --layer 58` działa od Etapu 3: NvSS ustawia właściwość
**strumienia**, nie okna. Dwie ścieżki, dwa mechanizmy — i myliliśmy je ze sobą.

Potwierdzenie z fabrycznego oprogramowania: `/eso/bin/apps/loadandshowimage` (ten, którego M.I.B.
używa do swoich splashy) woła `kdSetWindowPropertycv` z komentarzem `(caption)` w tablicy napisów,
a `eso/lib/libdisplayinit.so` robi to samo w `display_create_window`.

## Czy displayable musi być wcześniej zadeklarowany?

**Nie w konfiguracji.** Blok `"displayables"` w `displaymanager.json` to wyłącznie **właściwości
szczególne**, co mówi jego własny komentarz:

```
# list of displayables with special properties
# - annotations and filtering
# per default annotations are off (only Audi KDK uses them)
#             filtering is nearest
```

Jedyny wpis na naszym aucie to `{"id": 20, "annotation": true, "filtering": "nearest"}` — czyli
panel KDK, bo on jako jedyny używa adnotacji ECC. Komunikat
`ERROR displayable with id: %d not properly configured` dotyczy **błędnego wpisu w tej liście**,
nie nieznanego numeru.

**Nieznane:** czy istnieje górne ograniczenie numeru po stronie natywnej. Po stronie Javy jest
`MAX_DISPLAYABLES = 110` (`de.audi.tghu.fwhmi.DisplayManager`), więc 98 mieści się w zakresie,
ale to limit Javy, nie dowód na stronę natywną.

## Ścieżka `createImageDisplayable` — co to naprawdę jest

Istnieje i działa, ale to **osobna funkcja**: displayable zrobiony ze statycznego obrazka,
nie sposób na zarejestrowanie okna.

```java
// de.audi.tghu.fwhmi.DisplayManager  (javap na work\lsd.jar)
public void createImageDisplayable(ResourceLocator, int)
public void updateImageDisplayable(ResourceLocator, int)
private void setupImageDisplayable(ResourceLocator, int, boolean):
    if (getKombiType() == 4)                      // TT
        { log("… not supported for TT …"); return; }
    if (dsiDispMgmt != null)
        dsiDispMgmt.createImageDisplayable(rl, id);
```

Po stronie natywnej odpowiadają temu `CDSIDisplaymanagement::createImageDisplayable`,
`requestUpdateImageDisplayable`, `destroyImageDisplayable` oraz zdarzenia
`EVT_CREATE_IMAGE_DISPLAYABLE` / `EVT_UPDATE_IMAGE_DISPLAYABLE` / `EVT_DESTROY_IMAGE_DISPLAYABLE`.

Interfejs DSI (`org.dsi.ifc.displaymanagement.DSIDisplayManagement`) daje przy okazji pełną kontrolę
nad położeniem: `RT_SETPOSITION`, `RT_SETDIMENSION`, `RT_SETCROPPING`, `RT_SETSCALEMODE`,
`RT_SETOPACITY`, `RT_FADETOOPACITY`, `RT_GETEXTENTS`, `RT_SETUPDATERATE`.

**Co to znaczy dla nas:** nie potrzebujemy tej ścieżki, żeby okno zostało przyjęte. Może się jednak
przydać później — do ustawienia pozycji i rozmiaru naszego displayable zamiast polegania na
fabrycznym układzie.

**Uwaga o pochodzeniu tropu:** to właśnie opis instalatora chefranova („HMI tworzy displayable
nakładki z bitmapy") skierował mnie na `createImageDisplayable`. Trop okazał się *obok* właściwej
odpowiedzi dla naszego firmware, ale to on kazał mi otworzyć binarkę DisplayManagera i tam znalazł
się CAPTION. Bez tego nadal zgadywałbym przy `ID_STRING`.

## Pozostałe ustalenia

- **`use_all_mode: false`** w `displaymanager.json`. Komentarz w pliku: *„within 'all' mode the DM
  will render every client independent from any DSI context actions"*. Tryb `true` renderuje
  **każdego klienta niezależnie od kontekstów** — potencjalnie bardzo wygodne do diagnostyki,
  ale to zmiana w konfiguracji systemowej, więc tylko świadomie i z rollbackiem.
- `client buffer count exceeded. id: %d, count: %d, max: %d` — jest górny limit buforów na klienta.
- `available hardware layers: %d`, `KD failed to put window to layer 1` — DM sam zarządza warstwami
  sprzętowymi; nasza nakładka nie konkuruje o nie bezpośrednio.
- Proces w aucie: `/eso/bin/apps/displaymanager 2_lvds` (z `pidin` run-21).
- KombiType naszego auta: **nie TT** — konteksty mamy 77, a nie 77+79, co przy KombiType 4 byłoby
  inne (`DisplayManagerMIB2High.getMappedInternalContext`). Formalnego odczytu z auta brak.

## Geometria — też z naszego firmware, nie z oka

`work\mmx_app\navigation\resources\app\au\eu\pcconfig\komoviewstyle.conf` (1098 B) zna
dokładnie dwa style panelu manewru:

```
2; KVS_FPK;210x153
3; KVS_FPK;328x181
```

Pomiar ze zrzutów `dmdt ts 4` z wizyty 1 dał **210×153** — czyli styl 2, co do piksela. To domyka
korektę `docs/02_edzenis_vs_K3346.md:133`, gdzie stało „328×180": ta liczba była szacunkiem z innego
auta. Luka niezależnie notuje, że `komoviewstyle.conf` MU1439 jest identyczny z MU1316.

Pozycje (zmierzone, wizyta 1): **(1091,110)** duże zegary, **(1055,207)** po VIEW.

## Alfa — jest już napisana poprawnie, nic nie zmieniamy

DisplayManager **sam nie włącza `GL_BLEND`** i nie importuje `glBlendFunc`; składa materiałami
`premultiplied_alpha` / `blend_premult_alpha` z uniformem `uOpacity`. Czyli oczekuje od nas `rgb*a`.
Nasz renderer tak właśnie pisze:

- `EGL_ALPHA_SIZE 8` — `platform_harman.c:167`
- czyszczenie do `(0,0,0,0)` — `upstream/render.c`, sonda
- `glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`

Dokładny wzór siedzi w prekompilowanych shaderach (`glShaderBinary`, w binarce nie ma GLSL), więc
offline tego nie przeczytam. Dlatego sonda ma na **każdym** zrzucie schodek alfy: sześć stopni
0 / .2 / .4 / .6 / .8 / 1 nad czystym buforem.

## Co z tego wynikło — zrobione 2026-10-27

1. **Poprawka CAPTION** — `overlay/kd_surface.c` ustawia `KD_WINDOWPROPERTY_CAPTION` (69) z numerem
   jako napisem, `ID_STRING_QNX` i `MANAGED_QNX` usunięte, `DISPLAY_QNX` domyślnie nie ustawiane
   (`display -1`). Sprawdzone też w zbudowanej binarce: `vc_overlay` i `vc_overlay_probe` **nie
   zawierają** napisów `ID_STRING` ani `MANAGED`, zawierają `CAPTION`.
2. **Etap 7 został bez zmian w Javie HMI.** Pierwotne założenie projektowe się obroniło —
   displayable nie wymaga HMI, tylko właściwego kanału deklaracji.
3. **Brama przestała pytać `dmdt gd`.** Renderer po pierwszej wystawionej klatce pisze
   `/tmp/vc-overlay-ready` („id w h"), a `mib2high-lvds-control.sh` czeka na ten plik do 6 s,
   sprawdza numer i tylko wtedy bierze kontekst 901. Nieświeży meldunek jest usuwany przed startem
   nakładki. Osiem przypadków w `tools/sim_gate.sh`, 54 OK pod dash i mksh.
4. **Walidacja numeru** — `overlay_cfg_sanitize()` dopuszcza tylko 1..109, bo caption „0" dałby nam
   cudzy displayable, a „-1" wyrzuciłby okno bez słowa.
5. **Slot układu VC** czytany z loga hooka Etapu 3 (`V8H_LAYOUT slot=` w `/tmp/carplay_hook.log`)
   i logowany — **jeszcze nie stosowany**, patrz niżej. Biblioteka Etapu 3 nietknięta.

## Odpowiedzi z auta — sonda, 2026-10-27

Wszystkie trzy otwarte pytania zamknięte jedną wizytą. Dane: `collected/probe-run/`.

**Caption działa.** Z `A-vc-overlay-probe.log`:

```
kd_surface: CAPTION="98"
kd_surface: DISPLAY not set (stock recipe, conf display=-1)
kd_surface: SIZE=210x153
kd_surface: SOURCE_ALPHA=1 REFUSED (bv and iv)
kd_surface: VISIBILITY=1 via bv
kd_surface: window caption="98" 210x153 display=-1 alpha=1 realized native=beeb0006
```

**Geometria: DM honoruje rozmiar, nie nadaje pozycji.** Zrzuty rozebrane pikselowo (dekoder PNG
w czystym Pythonie, bez Pillow — w tym środowisku nie ma):

| Zrzut | Okno | Zmierzony bbox | Wniosek |
|---|---|---|---|
| `vc-10-A-ctx901.png` | 210×153 | `(0,0)-(209,152)` | rozmiar **co do piksela**, pozycja **(0,0)** |
| `vc-11-A-view2.png` | 210×153 | identyczny z A | VIEW nie ruszył ani nie zabrał okna |
| `vc-12-A-nobacking.png` | 210×153 | identyczny z A | podkładki KDK (101/102) nic nie zmieniają |
| `vc-20-B-full.png` | 1440×542 | `(0,0)-(1439,541)` | pełne płótno przechodzi, **bez obcięcia** |

Czyli: hipoteza „98 nie ma wpisu pozycji, więc wyląduje w (0,0)" potwierdzona. I nie ma o pozycję
jak poprosić — `dmdt gd`/`gs` zwracają na tej jednostce zero bajtów (`dm-gd*.txt`, `dm-gs.txt`
mają 0 B), a `dmdt gc` (11 264 B) wypisuje **tylko** listy displayable'ów na kontekst, bez żadnej
geometrii.

**Alfa miesza się per-piksel, premultiplikacja po naszej stronie jest poprawna.** Schodek alfy
sondy to sześć stopni `i·0,2` dla `i = 0..5`. Odczytane ze zrzutu wartości kanału:

```
0, 51, 102, 153, 204, 255
```

To jest `255·a` co do jedynki, w obu wariantach. Gdyby renderer pisał kolor **nie**premultiplikowany,
a DM składał materiałem `premultiplied_alpha`, wszystkie stopnie byłyby 255. Gdyby DM ignorował
alfę — też 255. Wyszedł dokładnie ten gradient, którego wymaga `src + dst·(1−a)` z `src = 255·a`.

Ciekawostka bez znaczenia praktycznego: `SOURCE_ALPHA` **na samym oknie KD** jest odrzucane
(`REFUSED (bv and iv)`), a mimo to mieszanie działa — bo robi je kompozytor, nie właściwość okna.

### Co z tego wynikło w kodzie

Okno bierze **całe płótno 1440×542**, a panel manewru jest w nim rysowany na offsecie zależnym od
slotu układu VC. Rozmiar źródła renderu zostaje 210×153, zgodnie z ostrzeżeniem Luki
(`HUD_RENDERER.md:138`): zmiana rozmiaru psuje aspect i FOV manewru.

Kodu Luki to **nie dotyka**. `render_init()` zapamiętuje jako „swój ekran" ten framebuffer, który
jest związany w chwili jego startu (`render.c`: `glGetIntegerv(GL_FRAMEBUFFER_BINDING,
&g_default_fbo)`), więc `platform_init()` podstawia mu własne FBO w rozmiarze panelu, a
`platform_swap()` przenosi je na płótno (`composite_panel()` w `platform_harman.c`).

## Czego nadal nie wiem

**Czy nasza przezroczystość przepuszcza mapę CarPlay.** Schodek alfy zmierzyliśmy nad **czarnym**
tłem, bo telefon był odłączony. Mapa jedzie warstwą sprzętową i **nie ma jej w kompozycie**
(ustalone na wizycie 1), więc żaden zrzut `dmdt ts` tego nie pokaże — ani teraz, ani później.
Odpowie wyłącznie oko przy działającym rendererze.

Dlatego `mib2high-overlay.conf` ma opisaną furtkę: wpisanie `canvas_width 210` / `canvas_height 153`
wraca do małego okna w rogu (brzydko, ale mapa wraca), bez przebudowy binarki.
