# 15 – Etap 4: wskazówki nawigacji CarPlay (RGI) — paczka i instrukcja

Stan na 2026-10-20: **zainstalowane w aucie ok. 13:35 (`ETAP4_INSTALL_RESULT=PASS`). Dane RGI docierają
do kombi; strzałka w panelu KDK jest zła, przyczyna znana** (wyniki niżej, logi
`work\auto_test_4\karta_mod\…\collected\run-03`).

## Wynik w aucie (2026-10-20, 13:37–13:44)

Działa (dowody w logach):
- CarPlay łączy się normalnie, identyfikacja z komponentem RGI zaakceptowana.
- `k3346-rgi-ipod-rx.log`: `driver_base_ready` → `ident_install_from_callsite` → `callsite_ready` →
  `bridge_first_ok`; **345× 0x5201, 38× 0x5202**.
- Java → BAP: `updateManeuverDescriptor` + `updateExitView` (rozdzielone, bo w K3346
  `updateManeuverDescriptorAndExitView` to atrapa – potwierdzone `javap`: tylko log „deprecated”).
- DSITracer (port 1798) widzi ramki BAP **LSG 50, FctID 23**: `13:64:0:0…` (TURN/lewo),
  `13:-64:0:0…` (TURN/prawo), `0:0:0…` (NO_SYMBOL). Odległość, ulica, flaga trasy na VC –
  poprawne (zdjęcia: 40 m, 200 m, 100 m zgodne z iPhone'em).

Nie działa: **strzałka w prawym panelu (KDK) zawsze prosto, na elipsie.**

Przyczyna (dowód: `run-03\dmdt-gc.txt`): mod przełącza kombi na stockowy kontekst
**77 = {20 `DISPLAYABLE_MAP_ROUTE_GUIDANCE`, 102, 101, 58 mapa CarPlay}**. Po Etapie 4 VC wie, że
trwa prowadzenie, i pokazuje panel KDK. Tekst odległości rysuje VC z BAP (stąd poprawny), ale
**obrazek manewru to displayable 20 renderowany przez fabryczną nawigację MMI**, która nie prowadzi
– stąd domyślna strzałka prosto. To jest „natywna grafika nawigacji obok mapy” ze znanych
problemów edzenisa. Luka na MHI2Q ma to samo i rozwiązał to własnym rendererem strzałki
(`maneuver_render`, displayable 98 zamiast 20 w swoim kontekście 80 = {98, 101, 102, 99}).
Brak bocznych ulic (`side=[]`, Google Maps nie podaje kątów skrzyżowania) nie jest przyczyną –
tego panelu kombi w ogóle nie rysuje z BAP.

Karta instrukcji CarPlay w mapie (iPhone) pokazuje strzałkę, odległość i ulicę poprawnie.
Paczka: `work\sd_etap4\` (zawiera też v8s, który jest już w aucie, i poprawkę `lsd.jxe`).

## Co to daje

iPhone wysyła do auta dane prowadzenia: manewr, odległość, pasy ruchu, ETA (iAP2,
wiadomości 0x5201/0x5202/0x5204). Bundel Javy z v8s (już działa w aucie) przekazuje je przez BAP
do fabrycznych wskazówek nawigacji na VC i ewentualnie na HUD. Dziś te dane nie docierają, bo
hook RGI siedzi w `dio_manager`, a sterownik iAP2 ładuje proces `mm-ipod`. W logach Etapu 3 widać
to jako `base_symbols_missing` (`docs\14`).

**Czego się spodziewać na zegarach:** u edzenisa (Q7) RGI było potwierdzone na HUD. Jak A4 B9
pokaże te dane na VC (fabryczne strzałki obok mapy, w zakładce nawigacji, na pasku), wyjdzie
dopiero w aucie. Znany problem edzenisa: natywna grafika nawigacji VC może się pokazać obok mapy
CarPlay.

## Co zmienia (dokładnie)

| Plik | Zmiana |
|---|---|
| `/mnt/app/root/hooks/libmib2high-carplay-rgi-ipod.so` | nowy: hooki RGI **bez kodu AltScreen** |
| `/mnt/app/root/mm-ipod-rgi.sh` | nowy: `LD_PRELOAD` tylko dla `mm-ipod`, potem `exec` fabrycznego `mm-ipod` z tymi samymi argumentami; bez biblioteki = fabryka |
| `/mnt/system/etc/usblauncher_otg.lua` | 1 linia: `return "mm-ipod -d iap2…` → `return "/mnt/app/root/mm-ipod-rgi.sh -d iap2…` |
| `/mnt/system/etc/mm/iap2.cfg` | dopisane na końcu: `[ident]`, `recvmsgs=0x5201,0x5202,0x5204`, `sendmsgs=0x5200,0x5203` |

Decyzje projektowe (sprawdzone w firmware K3346):
- **Osobna biblioteka dla `mm-ipod`.** Konstruktor AltScreen (`altscreen_output.c`) kasuje
  `/tmp/mib2high-lvds-active` w każdym procesie, który go załaduje. `mm-ipod` startuje przy każdym
  podłączeniu iPhone'a, więc wspólna biblioteka mogłaby zgasić mapę na VC. Pliki RGI nie zależą od
  AltScreen (`build_rgi_ipod.sh`, ten sam podział co RGI v1.1 edzenisa). Logi mają własne nazwy
  (`k3346-rgi-ipod-*`), żeby nie obcinać plików kopii z `dio_manager`. Biblioteka v8s po zmianie
  makr wychodzi **bit w bit taka sama** (`058413dd…`).
- **Launcher jako skrypt sh.** `usblauncher` uruchamia już skrypty jako sterowniki
  (`/etc/scripts/startncm.sh`), a `exec` zachowuje PID, więc `usblauncher` zarządza procesem jak
  dotąd. Poza tym jednym miejscem w lua nic w firmware nie odwołuje się do `mm-ipod` po nazwie
  (przeszukane `mmx_app` i `system`).
- **`/etc` = kopia `/mnt/system/etc` w RAM-dysku** (`startup.sh: move_etc_to_ramdisk`). Zmiany
  w `/mnt/system/etc` działają **po restarcie MMI**, tak samo jak `smartphone_integrator.json` w v8s.
- `usblauncher_otg.lua` czyta tylko drugi `usblauncher` (port OTG, iPhone). Karta SD i GEM od niego
  nie zależą, więc ratunek przez M.I.B. działa nawet przy błędzie w tym pliku. Składnię nowego
  pliku sprawdza `luac -p` w symulacji.
- Sterownik iAP2 K3346 ma klucze `ident`/`recvmsgs`/`sendmsgs` (stringi w `ipod-drvr-iap2.so`),
  tak jak K2161. Offsety hooków na prawdziwym sterowniku: ALL PASS.

## Bezpieczeństwo

- Instaluje się **tylko na zainstalowanym v8s**. Brama: wersja HMI + rozmiary jak w v8s, dodatkowo
  dokładne rozmiary stock `usblauncher_otg.lua` 19 907 B, `iap2.cfg` 5 852 B, `mm-ipod` 40 297 B
  i oczekiwane rozmiary po zmianie (19 928 B / 5 970 B). Każda niezgodność = odmowa, zero zapisów.
- Własna kopia oryginałów: `backup\pre-etap4\`. **`ROLLBACK_ETAP4`** cofa tylko Etap 4, mapa na VC
  zostaje. Pełny **`ROLLBACK`** cofa najpierw Etap 4, potem v8s.
- Awaria w trakcie = automatyczne cofnięcie Etapu 4 (`ETAP4_AUTO_ROLLBACK_RESULT=PASS`), v8s
  nietknięty.
- HMI i ekran MMI nie są ruszane, więc ryzyka bootloopu HMI nie ma. Najgorszy realny scenariusz:
  CarPlay przez kabel się nie łączy (np. iPhone odrzuci identyfikację). Wtedy `ROLLBACK_ETAP4`
  i restart.
- Symulacja `tools\sim_installer.sh`: **115 OK, 0 FAIL** (dash i mksh), w tym 53 testy Etapu 4.

## Instrukcja w aucie

**Na komputerze** (karta instalacyjna, ta z `backup\` i `installed.ok`):
1. Skopiuj **zawartość** `work\sd_etap4\` na kartę **z zastąpieniem plików, niczego nie usuwaj**.
   Na karcie muszą zostać `mod\mib2high-carplay-rgi-v8s-simple\backup\`, `installed.ok`
   i `collected\`.
2. Skopiuj `ZNACZNIKI\INSTALL_ETAP4` do `mod\mib2high-carplay-rgi-v8s-simple\`.
3. Sprawdzenie (tylko odczyt):
   `powershell -ExecutionPolicy Bypass -File tools\sprawdz_karte.ps1 -Karta I: -Typ etap4`.
   Ma być `0 BLEDOW` i komunikat „znacznik INSTALL_ETAP4 na miejscu”.

**W aucie** (silnik pracuje albo prostownik):
4. Karta w SD1 → GEM → m.i.b. → advanced settings → **run individual script**.
   Ma być `VERSION_GATE=PASS`, 4× `INSTALLING=…`, **`ETAP4_INSTALL_RESULT=PASS`**,
   `SAFE TO REMOVE SD`. Zrób zdjęcie ekranu.
5. **Restart MMI** (przytrzymaj pokrętło głośności ok. 10 s).
6. Podłącz iPhone'a. **Czy CarPlay się łączy?**
   - **Nie łączy się w ciągu 2 min** (albo komunikat o błędzie urządzenia) → zbierz logi (krok 9),
     potem `ZNACZNIKI\ROLLBACK_ETAP4` do folderu pakietu → run individual script → restart.
7. Mapa na VC jak dotąd (Etap 3). Ustaw w Apple Maps albo Google Maps **trasę z kilkoma
   manewrami** i przejedź kawałek (albo poczekaj na postoju). Zapisuj godziny.
8. Obserwuj i fotografuj:
   - VC w obu układach (VIEW): strzałka manewru, odległość, pasy, ETA, gdziekolwiek się pojawią;
   - czy nie pojawia się coś podwójnie albo dziwnie (fabryczna grafika nawigacji obok mapy);
   - pozostałe zakładki VC (jeśli przełączasz je przyciskami na kierownicy): czy któraś pokazuje
     wskazówki.
9. Logi: **run individual script** (bez znaczników) → `MODE=COLLECT`.

**Na komputerze:** cały folder `mod\` z karty → `work\auto_test_4\karta_mod\`.

## Co powiedzą logi (dla mnie)

- `k3346-rgi-ipod-rx.log`: `observer_loaded` → `driver_base_ready` → `callsite_ready` →
  `ident_install_from_callsite` → `msg=5201/5202 …` → `bridge_first_ok`.
- `k3346-rgi-ipod-native.log`, `k3346-rgi-ipod-hook.log`: szczegóły hooków.
- `iap2.log`: `IdentificationAccepted` czy `IdentificationRejected`, czy iPhone zaczyna wysyłać RGI.
  **Zawiera dane osobowe** (GPS), nie publikować.
- `mib2high-carplay-rgi-java-shmem.log`: BAPBridge z prawdziwymi wartościami zamiast `NO_SYMBOL` / `-1`.
- `live\usblauncher_otg.lua`, `live\iap2.cfg`: stan plików w aucie.

## Budowanie (uzupełnienie `docs\10`)

```bash
bash tools/qnx_build.sh build_rgi_ipod.sh output/libmib2high-carplay-rgi-ipod.so
bash tools/make_sd_package.sh            # -> work/sd_etap4/
# symulacja (lua5.1 dla luac -p): apt-get install -y lua5.1 mksh w kontenerze
```
