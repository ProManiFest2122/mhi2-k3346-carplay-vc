# 14 – Wyniki Etapu 3 w aucie (2026-10-20)

Źródło: zdjęcia od użytkownika i logi z karty instalacyjnej, skopiowane do
`work\auto_test_3\karta_mod\` (`collected\run-01` ok. 12:23, `run-02` ok. 12:25).
Logi zawierają dane osobowe (nazwa iPhone'a, adresy) — nie publikować.

Zegar MMX w logach to czas od startu (`Jan 01 00:MM:SS`). Start był ok. 12:00, więc
00:05 ≈ 12:05. Renderer liczy sekundy od startu, więc 341 s = 00:05:41.

---

## Co działa

| Funkcja | Stan | Dowód |
|---|---|---|
| Instalacja | PASS, ok. 20 s | ekran M.I.B., `installed.ok`, `backup\pre-v8s\complete` |
| Mapa CarPlay na VC | **działa** w obu układach (duża mapa / małe okno między zegarami) | zdjęcia; `FIRST_REAL_FRAME ... route=cluster` |
| VIEW | przełącza układy | zdjęcia |
| Renderer | 1440×542@30, faktycznie 15–23 kl/s, **0 błędów dekodowania** | `vc_render.log` |
| Kontekst kombi | `dc 900 58` za każdym razem OK, przy końcu sesji powrót do 72 | `mib2high-lvds-control.log` |
| Mapa na VC przy wstecznym | **gra dalej** (strumień 58 nie jest zamykany przy RVC) | niżej |

## Wsteczny — analiza

Zgłoszenie: „z nawigacją CarPlay na MMI po wrzuceniu wstecznego CarPlay znika, wraca po kilku
metrach jazdy do przodu". Na VC przy wstecznym nie ma grafiki PDC.

Oś czasu z `sloginfo` (run-02):

| Czas | Zdarzenie | CarPlay na MMI (strumień 59) | Mapa na VC (strumień 58) |
|---|---|---|---|
| 00:10:24 | `setRVCActive(true)` | `screen accessory`, zamknięty | renderer bez przerwy |
| 00:10:37 | `setRVCActive(false)` | `screen controller` **po 0,25 s** | bez przerwy |
| 00:10:39–00:10:48 | jeszcze dwa razy R → wyjście | za każdym razem wraca w <0,3 s | bez przerwy |
| 00:11:00 | `setRVCActive(true)` | zamknięty | bez przerwy |
| 00:14:35 | `setRVCActive(false)` (razem z `Clamp 15: off`) | wraca | bez przerwy |
| 00:19:20–00:20:33 | kamera ok. 73 s | wraca w chwili wyłączenia kamery | bez przerwy |

Wnioski:

1. **CarPlay na MMI wraca w tej samej chwili, w której auto wyłącza kamerę** (`RVC active:
   true -> false`). Sesja CarPlay się nie przerywa. iPhone dostaje standardowe
   `Modes changed: screen accessory` → `screen controller`.
2. To, **jak długo kamera jest włączona**, decyduje fabryczny system parkowania (`StMgr`,
   `RVC_ON/RVC_OFF` z CAPP), a nie mod. Mod nie dotyka arbitrażu ekranu MMI. Kamera, która zostaje
   po wyjściu z R do przejechania kilku metrów, to typowe zachowanie Audi. luka-dev
   (`PDC_SMALL_STAGE.md`) też celowo zostawia kamerę fabryczną.
3. **Cel z CLAUDE.md (punkt 6) jest spełniony dla VC.** Mapa CarPlay na zegarach działa przez
   całe cofanie, a po wyjściu z R CarPlay wraca bez reconnectu.
4. Opcja na przyszłość, do decyzji użytkownika: wcześniejszy powrót CarPlay na MMI po
   wyjściu z R. Wymagałoby to zmiany logiki parkowania w HMI, więc to kwestia bezpieczeństwa i nie
   priorytet.

**Potwierdzone przez użytkownika (2026-10-20):**
- 5A: po R → P na MMI zostaje kamera z PDC; **przycisk czujników parkowania od razu przywraca CarPlay**.
- 5B: CarPlay wraca po przejechaniu kilkunastu metrów do przodu — **tak samo było przed modem**.
- Zachowanie MMI przy wstecznym = fabryczne. Nic do naprawy.

## Pozostałe testy użytkownika (2026-10-20)

| Test | Wynik |
|---|---|
| VIEW | VC ma **dwa układy** z mapą CarPlay, przełącza poprawnie |
| Lewa rolka | **przybliża i oddala mapę w każdym układzie VC** (zoom rolką działa już teraz) |
| Odłącz/podłącz iPhone | CarPlay i mapa na VC wracają same |
| Wyłączenie VC i MMI, ponowne włączenie | CarPlay i mapa na VC wracają same |

Uwaga do ostatniego wiersza: w `sloginfo` wyłączenie widać jako `Clamp 15: off` przez ok. 40 s
bez restartu MMI (log jest ciągły). Ekrany zgasły, ale system się nie uruchamiał od nowa. Zimny start
MMI (auto zamknięte, MMI uśpione) nie jest jeszcze sprawdzony logami. Nowych logów po 12:25 na karcie
nie ma, więc analiza wyżej obejmuje te testy.

## Zdjęcia z 12:07–12:23

- Oba układy VC: mapa CarPlay wypełnia cały obszar, bez pasków i przesunięć. Geometria
  1440×542 pasuje, nie trzeba przełączać na wariant `b9-1440x540`.
- Zoom rolką: skala 200 m → 400 m → 500 m.
- R (12:21): mapa na VC zostaje w obu układach. MMI pokazuje fabryczną kamerę z PDC.
- Karta instrukcji CarPlay („towards <ulica>") w układzie z dużymi zegarami jest czytelna,
  u góry na środku.
- Przy dużej mapie lewy górny róg zasłania czarny prostokąt z samochodzikiem, a pod nim jest
  karta instrukcji (widać tylko jej turkusowy skrawek, `view 1` zaczyna się od x=132). To
  **fabryczne ostrzeżenie o otwartych drzwiach, bagażniku lub masce** (potwierdził użytkownik),
  więc w czasie jazdy go nie ma. **Geometria zostaje bez zmian.** Przesunięcie karty w prawo
  zabrałoby jej miejsce w normalnej jeździe dla przypadku, który zdarza się tylko na postoju.
- Muzyka (fabrycznie): tytuł, wykonawca, album, pasek postępu. **Okładka to zastępcza
  grafika** (cel na Etap 5).
- Telefon (fabrycznie): komunikat „iPhone jest w trybie CarPlay. Telefonowanie przez MMI nie jest
  możliwe…" (cel na Etap 5: rozmowy CarPlay na karcie telefonu VC).
- Pomarańczowa kontrolka silnika świeci na wszystkich zdjęciach. Nie ma to związku z modem MMI.

## Zerwania CarPlay — problem sprzed modu

W run-02 sesja CarPlay zrywa się 10 razy w ok. 24 min, za każdym razem tak samo:

```
USB-0.1:0: tid=4, vid=1a96, did=2018: Hardware workaround: treat as cable removed
<AirPlay> session ended: Dur=... Reason=-6753/0xFFFFE59F kConnectionErr
```

Wcześniej przychodzi `USB device suspend` ze strony telefonu albo łącza. Jedno z tych zerwań
(00:05:41) wypadło w trakcie kamery, co mogło wyglądać jak skutek wstecznego. Przyczyną było jednak
łącze USB.

**Log z Etapu 1 (przed instalacją, `work\sd_odczyt_1\K3346_ODCZYT_8519810\sloginfo.txt`) ma ten
sam wzorzec: 6 zerwań w ok. 15 min.** To nie jest skutek modu. Podejrzane są kabel, gniazdo USB albo
iPhone. Do sprawdzenia: inny (oryginalny) kabel, inne gniazdo USB w aucie.

## Inne obserwacje

- `K2161_RGI_RX base_symbols_missing` jest oczekiwane: biblioteka jest ładowana do `dio_manager`,
  a sterownik iAP2 (`ipod-drvr-iap2.so`) ładuje `mm-ipod`. RGI z telefonu wymaga wrappera
  `mm-ipod` (Etap 4).
- Java: `KNTMLifecycle` poprawnie uruchamia i zatrzymuje RouteGuidance przy każdym połączeniu
  i rozłączeniu (11 startów, 10 stopów). Wyjątków brak.
- `run-01` i `run-02` zebrano bez restartu MMI (Clamp 15 off/on ok. 40 s, `sloginfo` ciągły).
  Nie wiadomo więc jeszcze, czy `/tmp` przeżywa pełne uśpienie MMI.
- `date` w środowisku `lvds-control` zwraca pusty napis (linie logu bez czasu). Kosmetyka, na
  później.

## Poprawka po teście

Bramka wersji szukała `/ifs/LSD.JXE`, a w aucie plik nazywa się `/ifs/lsd.jxe` (QNX rozróżnia
wielkość liter). Sprawdzian był pomijany z komunikatem, więc bez wpływu na bezpieczeństwo.
Poprawione w `custom.sh`: małe litery, a gdy `/ifs/lsd.jxe` nie istnieje, sprawdzana jest też
`/net/mmx/ifs/lsd.jxe`. Symulacja: 62/62 (dash i mksh), sumy paczki OK.
Na karcie wystarczy podmienić `mod\command.sh`. Stan instalacji i kopia oryginałów zostają nietknięte.
Zainstalowanego modu ta zmiana nie dotyczy, bo działa tylko przy następnej instalacji.
