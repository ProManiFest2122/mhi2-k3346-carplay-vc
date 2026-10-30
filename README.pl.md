# CarPlay na Virtual Cockpit Audi — MHI2 K3346

[English](README.md) | **Polski**

> ## 📖 [Instrukcja instalacji krok po kroku → INSTRUKCJA.md](INSTRUKCJA.md)
> Pierwszy raz? Czytaj tę. Każde kliknięcie opisane, z tym, co powinieneś zobaczyć po każdym kroku.

Wyświetla Apple CarPlay na **Virtual Cockpit** (cyfrowych zegarach) w Audi z systemem **MHI2 (Harman),
oprogramowanie `MHI2_ER_AUG22_K3346`, MU 1438**. Sprawdzone w Audi A4 B9 z CarPlay po kablu.

> ### ⚠️ Tylko dla oprogramowania K3346 / MU 1438
> Inne oprogramowanie: **nie instaluj**. Nie musisz tego sprawdzać ręcznie dla bezpieczeństwa —
> instalator sam to sprawdza i odmawia, niczego nie zmieniając — ale oszczędzisz sobie drogi do auta.
> Jak sprawdzić: [Czy zadziała w moim aucie?](#czy-zadziała-w-moim-aucie)

## Co dostajesz

| | Funkcja |
|---|---|
| 🗺️ | **Mapa CarPlay na zegarach**, w obu układach; przycisk **VIEW** przełącza układ |
| 🔍 | **Przybliżanie mapy** lewą rolką na kierownicy |
| ➡️ | **Strzałka manewru 3D** obok mapy, do tego odległość, nazwa ulicy i flaga celu |
| 🔙 | Mapa działa dalej przy **cofaniu** (MMI pokazuje kamerę jak zwykle) |
| 📞 | **Rozmowy CarPlay na karcie telefonu w zegarach**: kto dzwoni, odbierz / odrzuć / rozłącz, przycisk na kierownicy działa |
| 🕘 | **Ostatnie połączenia** w zegarach podczas CarPlay, dzwonienie z listy |
| 🎵 | **Okładka** odtwarzanego utworu w zegarach |
| 🎙️ | **Siri** krótkim wciśnięciem przycisku głosowego |
| 👆 | **Touchpad MMI jako strzałki** w CarPlay |

## Co dalej?

| | Funkcje |
|---|---|
| 🔧 | BugFixs |
| 🎧 | CarPlay 5-głośnikowy MixUp (Dzięki yety-ua za prace) |
| ➡️ | Strzałki skrętów na HUD'ie  |
| 📞 | Własna karta połączenia na mapie VC (fabryczna znika po odebraniu połączenia na mapie VC) |

## Czy zadziała w moim aucie?

Potrzebne jest **wszystko** z listy:

- Audi z **MHI2 (Harman)** i **Virtual Cockpit** (nie MHI2Q, nie MIB2 Standard),
- oprogramowanie **`MHI2_ER_AUG22_K3346`**, **MU 1438** — widać je w zielonym menu albo na ekranie
  wersji w M.I.B.,
- **iPhone z CarPlay** (po kablu),
- [**M.I.B. – More Incredible Bash**](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash) na karcie
  SD, już działające w Twoim aucie (zielone menu → *m.i.b.* się otwiera).
- łatka M.I.B. **NavIgnore** („podwójna nawigacja”: *m.i.b. → navigation → NavActiveIgnore → Enable dual
  navigation*). Bez niej mapa na zegarach robi się czarna przy prowadzeniu. Zob. [INSTRUKCJA.md, krok 4.3](INSTRUKCJA.md#43-włącz-podwójną-nawigację-navignore--wymagane). Sprawdzone z **M.I.B. 3.6.0**.
  Nie masz M.I.B.? Jak przygotować kartę SD (FAT32) i zainstalować M.I.B.: [INSTRUKCJA.md, rozdział 1](INSTRUKCJA.md#1-karta-sd-i-mib-tylko-jeśli-jeszcze-nie-masz-mib).

## Zanim zaczniesz

- **prostownik / zasilacz** podłączony do auta — silnik będzie wyłączony ok. 20 minut,
- świeża **kopia MMI w M.I.B.** (M.I.B. → backup),
- pobierz z [**Releases**](../../releases/latest): `…-install.zip` i `…-uninstall.zip`.

## Instalacja (5 kroków)

*Skrót dla tych, którzy już to robili. **Pierwszy raz? Użyj [instrukcji krok po kroku](INSTRUKCJA.md).***

1. **Rozpakuj** `…-install.zip` do **katalogu głównego karty z M.I.B.**; zgódź się na zastąpienie plików.
   Dodaje własny folder i podmienia tylko `mod/command.sh`.
2. Karta do slotu, z którego działa M.I.B. (**SD1**), **zapłon włączony**, **prostownik podłączony**.
3. Zielone menu → **m.i.b.** → **advanced settings** → **run individual script**.
4. Czekaj na **`INSTALL_ALL_RESULT=PASS`**, a potem **`SAFE TO REMOVE SD`** (kilka minut).
   Nie wyłączaj zapłonu wcześniej.
5. **Restart MMI** ([jak](#jak-zrestartować-mmi)). Podłącz iPhone'a i włącz trasę
   w nawigacji CarPlay.

> **Zachowaj tę kartę.** Instalator zapisuje na niej oryginalne pliki auta
> (`mod/mib2high-carplay-rgi-v8s-simple/backup/`), a odinstalowanie ich potrzebuje. Skopiuj też cały
> folder `mod/` na komputer.

### Jak zrestartować MMI

![Przyciski MMI w A4 / A5 / Q5 B9: czerwone menu, zielone menu, restart, zrzut ekranu](images/audi-b9-mmi-buttons.png)

*Przyciski w A4 / A5 / Q5 (B9, 2017–2020), zaznaczone na czerwono. Od lewej: czerwone menu, **zielone menu**, **restart**, zrzut ekranu. Przyciski naciska się na ekranie głównym MMI. Źródło: [wiki M.I.B.](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/wiki/%22hidden-menus%22---key-combinations).*

**A4 / A5 / Q5 (B9, 2017–2020):** przytrzymaj jednocześnie **duże pokrętło MMI + NAV/MAP + RADIO**
przez ok. 10 sekund, aż ekran zgaśnie i MMI uruchomi się od nowa. Inne modele: tabela przycisków
w [wiki M.I.B.](https://wiki.mibsolution.one/en/MHI2).

## Odinstalowanie (i przed każdą aktualizacją)

*Krok po kroku: [INSTRUKCJA.md, rozdział 6](INSTRUKCJA.md#6-odinstalowanie-i-przed-każdą-aktualizacją).*

1. Rozpakuj `…-uninstall.zip` do katalogu głównego **tej samej** karty (dodaje jeden pusty plik `ROLLBACK`).
2. Uruchom skrypt jak w kroku 3. Czekaj na **`ROLLBACK_RESULT=PASS`** i **`SAFE TO REMOVE SD`**,
   potem restart MMI. Auto wraca do stanu fabrycznego, plik w plik.

**Aktualizacja do nowej wersji:** odinstaluj starą, potem zainstaluj nową. Instalacja uruchomiona na
już zainstalowanym modzie niczego nie zmienia i podpowiada, żeby najpierw odinstalować.

## Gdy coś pójdzie nie tak

| Widzisz | Co to znaczy / co zrobić |
|---|---|
| `ERROR VERSION_GATE …` | To nie jest oprogramowanie K3346 / MU1438. **Nic nie zostało zmienione.** Ten mod nie jest dla Twojego MMI. |
| `INSTALL_ALL_RESULT=FAIL` | Auto jest w **bezpiecznym stanie**: ukończone części zostają, część, która się nie udała, została cofnięta. Powód jest w linii z `ERROR`. Uruchom skrypt jeszcze raz albo odinstaluj. |
| linia z `do not reboot` | **Nie restartuj MMI.** Od razu odinstaluj. |
| Coś nie działa w aucie | Zbierz logi i zgłoś: zob. [Zgłaszanie problemu](#zgłaszanie-problemu). |

### MMI nie uruchamia się

Mało prawdopodobne (sprawdzone w aucie autora), ale to modyfikacja komputera w aucie, więc: jeśli po
kroku 5 ekran zostaje czarny albo się restartuje, odczekaj 5 minut i zrestartuj jeszcze raz. Jeśli to
nie pomoże, zielone menu jest niedostępne, więc odinstalowanie przez M.I.B. nie zadziała. Drogi powrotu:

- **karta ratunkowa** opisana w [BUILDING.md](BUILDING.md#rescue-card) — uruchamia M.I.B. w trybie
  aktualizacji i usuwa część moda w HMI (wymaga komputera i trochę doświadczenia),
- ponowne wgranie oprogramowania **K3346** z karty SD,
- warsztat, który potrafi wgrać oprogramowanie MMI.

## Zgłaszanie problemu

**1. Wywołaj problem**, a potem w ciągu kilku minut i **bez restartu MMI** (restart kasuje większość logów):

**2. Zbierz logi** — zielone menu → m.i.b. → advanced settings → **run individual script**:

- mod zainstalowany i działa (wcześniej było `INSTALL_ALL_RESULT=PASS`): po prostu uruchom — gdy nie ma
  nic do instalowania, skrypt zbiera logi;
- instalacja się nie udała (`INSTALL_ALL_RESULT=FAIL`): ponowne uruchomienie ponowiłoby instalację, więc
  najpierw rozpakuj na kartę `…-collect-logs.zip` z [Releases](../../releases/latest) (dodaje jeden pusty
  plik `COLLECT_LOGS`), potem uruchom skrypt. Plik nie znika sam i dopóki leży na karcie, każde
  uruchomienie tylko zbiera logi — usuń go z `mod/mib2high-carplay-rgi-v8s-simple/`, gdy skończysz.

Czekaj na `COLLECT_RESULT=…` i `SAFE TO REMOVE SD`. Każde uruchomienie dodaje nowy folder
`mod/mib2high-carplay-rgi-v8s-simple/collected/run-NN/` (najwyższy numer = najnowszy), a w nim: log
systemowy (`sloginfo.txt`), logi moda (`logs/`: hooki CarPlay, prowadzenie po trasie, Java, nakładka ze
strzałką), lista procesów, stan wyświetlania na zegarach (`dmdt-*.txt`) i `trace/metadata.txt` (wersja
moda z `RELEASE`, które etapy są zainstalowane).

**3. Załóż zgłoszenie** na GitHubie (Issues → New issue → *Bug report*) i napisz: wersję (plik
`mod/mib2high-carplay-rgi-v8s-simple/RELEASE`), model i rocznik auta, wersję iOS, co robiłeś, czego
oczekiwałeś, co się stało, ile minut przed zebraniem logów, ewentualnie zdjęcie zegarów/MMI.

> **Nie dołączaj logów do publicznego zgłoszenia.** Zawierają dane osobowe: VIN, nazwę telefonu, numery
> i nazwy kontaktów, pozycje z nawigacji. Spakuj folder `run-NN` i wyślij go dopiero na prośbę, prywatnie.

## Znane ograniczenia

- **Pasy ruchu** (który pas wybrać) są podłączone, ale **jeszcze niewidziane w aucie** — na trasach
  testowych nie było manewrów z wieloma pasami. Daj znać, jak to wygląda.
- **Ostatnie połączenia:** iOS 26.6–26.7.x wysyła po kablu pustą listę (błąd iOS, dotyka też fabrycznych
  zegarów BMW/Porsche); wtedy pokazywana jest lista, którą MMI pobrało przez Bluetooth. iOS 27.2 działa.
  Pełna książka adresowa przez CarPlay nie jest możliwa.
- Po wciśnięciu **VIEW** strzałka znika na maks. ~0,7 s i wraca w nowym miejscu.
- Bez trasy w nawigacji kafelek strzałki jest celowo ukryty.

**Na własne ryzyko.** Bez żadnej gwarancji. Zewnętrzne zasilanie, świeża kopia M.I.B. i oryginalne
oprogramowanie powinny być pod ręką.

## Dla programistów

Jak to działa, budowanie ze źródeł, znaczniki pojedynczych etapów i karta ratunkowa:
**[BUILDING.md](BUILDING.md)** (po angielsku). Dziennik prac: [`docs/`](docs/), zacznij od
[`docs/00_postep.md`](docs/00_postep.md).

## Podziękowania

* **edzenis** – [mib2high-carplay-altscreen-rgi](https://github.com/edzenis/mib2high-carplay-altscreen-rgi),
  baza tego portu (MHI2 Harman, K2161 / Q7 4M).
* **LuKa (@LuKa_dev)** – [mib2q-carplay-rgi](https://github.com/luka-dev/mib2q-carplay-rgi): parser i szyna
  RGI, mapowanie manewrów, przeniesiony tu renderer strzałki 3D, projekty Siri/touchpada/telefonu,
  dokumentacja. Jego `docs/reference/MHI2_MU1439_PORT.md` (analiza IDA MHI2 MU1439, którego
  `displaymanager` ma co do bajta rozmiar naszego) rozstrzygnął najtrudniejsze pytanie etapu strzałki:
  okno KD deklaruje swój displayable przez `KD_WINDOWPROPERTY_CAPTION`, DisplayManager składa obraz
  z alfą premultiplikowaną, a style panelu to 210×153 / 328×181 — wszystko potwierdzone w aucie.
  Zob. [`docs/19`](docs/19_displayable_na_mhi2.md).
* **Eugene Chefranov (@chefranov)** – [mhi2-au37x-carplay](https://github.com/chefranov/mhi2-au37x-carplay),
  bliźniaczy projekt dla MHI2 AU37x. Jego publiczny opis instalatora (na MHI2
  displayable nakładki w zegarach tworzy HMI z bitmapy) sprawił, że przestaliśmy zgadywać i otworzyliśmy
  plik `displaymanager`, w którym znalazł się `KD_WINDOWPROPERTY_CAPTION`. Jego projekt jest też dowodem,
  że ten zestaw funkcji jest osiągalny na MHI2 Harman, nie tylko na MHI2Q. Żaden jego plik, binarka, jar,
  shader ani skrypt nie jest tu użyty.
* **joeyQuery** – MHI2-altScreen (źródło `vc_render`)
* **harman-f** – badania mhi2_altscreen_carplay
* **Mr-MIBonk** – M.I.B.

Licencje: [LICENSING.md](LICENSING.md).
