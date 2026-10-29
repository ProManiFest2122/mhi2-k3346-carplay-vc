# Pochodzenie plików w `overlay/` (Etap 7)

Renderer nakładki na Virtual Cockpit to **port `maneuver_render` Luki**
(https://github.com/luka-dev/mib2q-carplay-rgi, **GPL-3.0**, © 2026 LuKa @LuKa_dev)
z MHI2Q na MHI2 Harman K3346.

Zasada tego portu: **kod rysujący zostaje jego, bajt w bajt.** Wymieniona jest wyłącznie
warstwa platformy, bo K3346 nie ma QNX Screen. Dzięki temu poprawki u Luki da się
wciągnąć zwykłym `diff`, a jego nazwisko zostaje tam, gdzie powinno.

NOTICE Luki wymaga zachowania nagłówków z prawami autorskimi przy redystrybucji —
w skopiowanych plikach są nietknięte.

## `upstream/` — kod Luki

| Plik | Rola |
|---|---|
| `main.c` | pętla renderu, maszyna stanów manewru, watchdog postępu |
| `maneuver.c/.h` | proceduralna geometria manewrów (zakręty, ronda, zjazdy, U-turn, przyjazd) |
| `render.c/.h` | potok GLES2: maski FBO, mesh trasy, SSAA 1.6 + FXAA, overlay pass |
| `route_path.c/.h` | ścieżka trasy → ekstrudowany mesh 3D |
| `route_progress.h` | postęp wypełnienia strzałki po projekcji centerline |
| `arrow_progress.h` | krytycznie tłumiony filtr wypełnienia + blink |
| `contact_shadow.h` | cień kontaktowy przy nachodzeniu strzałki |
| `visible_area.h` | animowana zmiana obszaru widocznego (250 ms, quintic) |
| `visual_style.h` | jedyne źródło prawdy dla kolorów i szerokości |
| `lane_guidance.h`, `lane_panel.h` | dekoder i panel pasów ruchu |
| `server.c/.h`, `protocol.h`, `maneuver_command.h` | protokół TCP 48 B |
| `gl_compat.h`, `platform.h` | nagłówki zgodności i interfejs platformy |
| `scene/*.cpp`, `scene/*.hpp`, `scene/scene.h` | kompilator sceny C++11 (bez C++ runtime) |
| `common/log_stamp.h`, `common/gl_program_cache.h` | znacznik czasu w logu, cache binarek shaderów |

Końce linii zamienione na LF (konwencja `port_k3346`), treść bez zmian.

### Zmiany w kodzie Luki — wszystkie, z powodem

Każda idzie przez `platform.h`, czyli szew, który Luka sam zostawił między kodem rysującym
a platformą. Kod rysujący (geometria, shadery, kompozycja) nie zmienił się ani o znak.

| Plik | Zmiana | Po co |
|---|---|---|
| `render.c` | `#define GLPC_DIR` owinięty w `#ifndef` | katalog cache'u shaderów podawany z linii poleceń. Domyślna wartość Luki (`/mnt/persist/var/app/luka_carplay_maneuver`) zostaje w pliku; nasz build podaje `/var/app/icab/tmp/k3346_overlay_glpc`, bo na MHI2 Harman `/mnt/persist` nie istnieje, a `/tmp` to `/dev/shmem`, gdzie `rename()` zawodzi (`docs/17`) |
| `main.c` | jedno wywołanie `platform_set_content_present(g_engine.has_current && !g_cleared)` tuż przed `platform_swap()` | okno K3346 to całe płótno kombi, więc platforma musi wiedzieć, czy klatka ma treść, zanim zdecyduje, czy w ogóle kłaść panel na mapę (Etap 7d, 2026-10-27) |
| `platform.h` | deklaracje `platform_set_content_present()` i `platform_fbo_type()` | interfejs dwóch powyższych |
| `render.c` | pięć alokacji buforów renderu (`glTexImage2D(..., NULL)`: SSAA, FXAA, dwie maski i ich odpowiedniki w `render_set_viewport`) bierze typ z `platform_fbo_type()` zamiast stałego `GL_UNSIGNED_BYTE`. Atlas flagi, który ładuje prawdziwe dane RGBA8, **nie** jest ruszany | na K3346 tekstura RGBA8 podpięta jako FBO może nie trzymać alfy (2026-10-28: panel odczytany jako alfa 255 w każdym pikselu tuż po wyczyszczeniu do zera). Platforma mierzy, który typ alfę trzyma, i wszystkie bufory biorą tę samą odpowiedź. Gdy RGBA8 działa, `platform_fbo_type()` zwraca `GL_UNSIGNED_BYTE` — czyli dokładnie to, co było (Etap 7f) |

| `render.c` | `GL_TEXTURE_WRAP_S/T = GL_CLAMP_TO_EDGE` na `g_ss_tex` i `g_fxaa_tex` — dwóch teksturach, które jako jedyne w pliku nie miały ustawionego zawijania | **to była przyczyna czarnej płyty przez cztery wizyty.** Oba bufory są NPOT (525×290), a w czystym GLES2 (§3.8.2) tekstura NPOT bez `CLAMP_TO_EDGE` jest niekompletna i próbkuje się jako `(0,0,0,1)`. Adreno w MHI2Q ma `GL_OES_texture_npot`, więc domyślne `GL_REPEAT` jest tam legalne i u Luki działa; Tegra 3 w MHI2 Harman nie. Każda inna tekstura w tym pliku już miała `CLAMP_TO_EDGE`. Zmiana jest neutralna wszędzie: oba bufory są czytane pełnoekranowo w zakresie [0,1]. (Etap 7g, 2026-10-28) |

Każda zmiana jest opisana komentarzem w samym pliku.

## Nasze pliki (GPL-3.0, ta sama licencja)

| Plik | Co zastępuje / po co |
|---|---|
| `platform_harman.c` | zastępuje `maneuver_render/platform_qnx.c` — EGL/GLES2 na OpenKODE zamiast QNX Screen |
| `kd_surface.c/.h` | zastępuje `common/cluster_surface.c` — okno deklaruje numer displayable przez `KD_WINDOWPROPERTY_CAPTION` (69) zamiast `screen_manage_window` + `SCREEN_PROPERTY_ID_STRING`; DisplayManager przyjmuje je sam. Patrz `docs/19_displayable_na_mhi2.md` |
| `overlay_cfg.h` | numer displayable i geometria z pliku na karcie, nie z `#define` |
| `overlay_fbo.h` | który typ tekstury FBO trzyma na tej jednostce alfę: decyzja (kolejność kandydatów, co zrobić, gdy żaden) oddzielona od GL-a i testowana na hoście; sam pomiar w `platform_harman.c` |
| `overlay_blit.h` | gdzie wyrenderowany kafelek ląduje na kombi: wycinek `CR_POPUP_*` z płótna renderera, dwa odwrócenia osi i mapowanie 1:1 w jednym, testowanym miejscu |
| `overlay_layout.h` | który układ zegarów VC jest na ekranie — z pierścienia przechwytu DSI (`/tmp/v8l-dsitracer-1798-NN.log`) parserem hooka (`native/gauge_bap54.h`, BAP 50/54) i zapasowo `setZoomArea`; do 2026-10-28 czytał log hooka, w którym ta linia nigdy się nie pojawiała |
| `overlay_ready.h` | meldunek `/tmp/vc-overlay-ready`, któremu ufa brama w `mib2high-lvds-control.sh` |
| `scene_compat_qnx65.hpp` | zgodność GCC 4.9.4 + nagłówki QNX 6.5 (Luka buduje na GCC 8.5) |
| `probe/vc_overlay_probe.c` | sonda kalibracyjna Etapu 7a |
| `build_overlay.sh`, `build_probe.sh` | build pod nasz toolchain |

## Zasoby

| Plik | Pochodzenie | Uwagi |
|---|---|---|
| `resources/flag_atlas.rgba` | `maneuver_render/resources/flag_atlas.rgba` Luki | 14 klatek 128×128 RGBA, animowana flaga celu. W jego NOTICE **nie** jest wymieniona jako materiał zewnętrzny, więc formalnie obejmuje ją GPL-3.0 projektu. Ale nazwy plików źródłowych (`Flagge65x65_000*.png`) wyglądają na zasób HMI auta. **Do publicznego repo nie trafia, dopóki tego nie wyjaśnię** — `tools/export_public.sh` ma ją wykluczać. W aucie działa normalnie. |

**Fontów Audi z repo Luki nie skopiowano.** Będą potrzebne dopiero przy karcie rozmowy
(Etap 7c, atlas glyphów) i są w jego NOTICE wprost oznaczone jako cudze — atlas
generujemy lokalnie i nie publikujemy.

## Czego NIE przenosimy

- `platform_qnx.c`, `common/cluster_surface.c` — QNX Screen, na K3346 nie ma `libscreen`.
- `platform_macos.c`, `test_harness.c` — backend hosta (do rozważenia później do podglądu
  klatek na PC, dziś niepotrzebny do paczki).
- Strona Javy Luki (`RendererServer.java`, `RendererMapper.java`) — u nas dane idą prosto
  z natywnego hooka w `mm-ipod`, bez zmian w Javie HMI. Logika mapowania jest portowana
  do C w `native/routeguidance/overlay_feed.c`.
