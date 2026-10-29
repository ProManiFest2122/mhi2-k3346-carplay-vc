# 20 – Ściągawka do auta (Etap 7i: strzałka manewru na VC)

> **Etap 7 zakończony (2026-10-28, wizyta 9).** Ta ściągawka zostaje na wypadek ponownej
> instalacji Etapu 7 z osobnej paczki; zwykła instalacja całości to od wersji 1.0 jedno
> uruchomienie z paczki wydania (README).

Jedna strona do czytania na telefonie przy aucie. Dlaczego tak: `docs\19`. Pełny opis Etapu 7:
`docs\18`. Ściągawka Etapu 3 (instalacja mapy) to `docs\09` — ta jest osobna.

> **Co się zmieniło po wizycie 8.** Strzałka działa w obu układach i idzie za VIEW. Zostało
> krótkie „mignięcie" w miejscu poprzedniego układu: VC przełącza się od razu przy klawiszu VIEW,
> a raport o nowym układzie przychodzi ~0,6 s później — a do tego my sprawdzaliśmy go raz na
> sekundę i czekaliśmy na następną klatkę.
>
> Teraz: przy naciśnięciu VIEW strzałka **znika**, a gdy VC zgłosi nowy układ, **pojawia się od
> razu we właściwym miejscu**. Nigdy w miejscu poprzedniego układu. Sprawdzone na zapisie z Twojej
> sesji: 10 naciśnięć, za każdym razem przerwa ~0,4–0,6 s i właściwe miejsce.
---

## 0. Przed wyjazdem

- ☐ **Karta `I:`** — sprawdzona (`sprawdz_karte -Typ etap7`).
- ☐ **Prostownik / zasilacz podłączony.** Bez tego nie zaczynaj.
- ☐ **Karta ratunkowa, uzbrojona** (`Swdlautorun.txt` **bez** podkreślenia) — na wypadek bootloopu.
- ☐ **Laptop albo czytnik kart.** Po zużyciu znaczników cofnięcie wymaga skopiowania pliku na kartę.
- ☐ iPhone + kabel. Telefon może być podłączony od początku.

---

## 1. Trzy uruchomienia

Wszystkie przez **GEM → m.i.b. → advanced settings → run individual script**.

| # | Co robisz | Ma się pokazać |
|---|---|---|
| 1 | uruchom skrypt | `ETAP7_ROLLBACK_RESULT=PASS` + `SAFE TO REMOVE SD` |
| 2 | uruchom **drugi raz** | `ETAP7_INSTALL_RESULT=PASS` + `SAFE TO REMOVE SD` |
| — | **restart MMI** — przytrzymaj gałkę głośności ~10 s | MMI wstaje |
| — | **podłącz iPhone, uruchom nawigację** | patrz niżej |
| 3 | uruchom **trzeci raz** | `MODE=COLLECT` |
| — | wyjmij kartę, w domu skopiuj cały `mod\` | — |

**Nigdy nie wyciągaj karty przed `SAFE TO REMOVE SD`.** Jeśli skrypt wygląda, jakby stał — czekaj.

Krok 1 cofa to, co zostało z poprzedniej wizyty. Krok 2 wgrywa nową wersję. Restart między 2 a
podłączeniem telefonu to jedyna czynność, której skrypt nie zrobi za Ciebie.

---

## 2. Na co patrzeć po podłączeniu telefonu

Uruchom nawigację (Apple Maps / Google Maps) do dowolnego celu i daj jej chwilę — mapa na VC
pojawia się dopiero, gdy nawigacja dwa razy zgłosi gotowość.

- ☐ **Naciśnij VIEW kilka razy** przy aktywnej trasie: strzałka ma na chwilę **zniknąć** i
  pojawić się **od razu we właściwym miejscu** — nigdy w miejscu poprzedniego układu. To jest
  główne pytanie tej wizyty.
- ☐ Czy przerwa jest krótka (ok. pół sekundy) i wygląda jak przejście, a nie jak błąd?
- ☐ Duże i małe zegary: czy strzałka dalej siedzi tak dobrze jak na wizycie 8?
- ☐ Czy strzałka odpowiada karcie instrukcji na mapie (lewo / prawo / rondo / zjazd)?
- ☐ Czy jest **w miejscu fabrycznego panelu**, a nie w rogu i nie uciętą połową?
- ☐ Czy pojawiają się **pasy ruchu**?
- ☐ Czy **odległość, ulica i ETA** są bez zmian? (Te rysuje VC z BAP, ich nie ruszamy — gdyby
  zniknęły, to regres.)
- ☐ Mapa cały czas widoczna dookoła? (Na wizytach 3 i 4 była — gdyby kombi zrobiło się czarne,
  odłącz telefon, wszystko wraca od razu.)

---

## 3. Co sfotografować

| ☐ | Co | Dlaczego |
|---|---|---|
| ☐ | VC tuż po VIEW (jeśli zdążysz) | czy w momencie przełączania nie ma strzałki w złym miejscu |
| ☐ | VC z nawigacją, gdyby strzałki **nie** było | porównanie z mapką z sondy |
| ☐ | Każdy ekran MMI z `ERROR`, `FAIL` albo `AUTO_ROLLBACK` | diagnoza |

---

## 4. Kiedy przerwać

| Co widzisz | Co robisz |
|---|---|
| `AUTO_ROLLBACK_RESULT=FAIL – do not reboot` | **Jedyny groźny przypadek.** NIE RESTARTUJ, zdjęcie, zostaw zapłon, pisz do mnie |
| `ERROR` + `AUTO_ROLLBACK_RESULT=PASS` | Samo się cofnęło. Zdjęcie, nie restartuj na siłę, zrób krok 3 |
| `VERSION_GATE ... instalacja PRZERWANA` | Blokada zadziałała, **nic nie zapisano**. Zdjęcie, krok 3, koniec na dziś |
| Kombi czarne zamiast mapy | Odłącz telefon — wraca od razu. Zdjęcie, krok 3 |
| VC zgłupiało, ale MMI żyje | `ROLLBACK_ETAP7` — mapa, RGI, telefon i okładka zostają |
| MMI nie wstaje (bootloop) | Odczekaj **do 5 min** (GEM bywa niedostępne). Potem: zielone menu → `ROLLBACK_ETAP7`; brak zielonego menu → **karta ratunkowa** (`docs\12`) |

**Co gwarantuje brama:** jeśli renderer nie zamelduje żywego okna w 10 sekund, kombi zostaje na
kontekście 77 i mapa, telefon, okładki i cofanie działają dokładnie jak dziś. Dokładnie to zdarzyło
się na wizycie 2 — i właśnie dlatego nic się wtedy nie zepsuło.

---

## 5. Na komputerze

Skopiuj z karty **cały** katalog `mod\` do `work\auto_test_19\karta_mod\`. Są w nim logi
(`collected\run-*`) **i kopia oryginalnych plików auta** (`backup\pre-v8s\`). Nie kasuj niczego
z karty, dopóki mod jest zainstalowany.

Rozstrzygające tym razem, wszystko w `logs\vc_overlay-launch.log`:

- przy każdym VIEW para linii: `VIEW pressed - tile hidden until the cluster reports the new
  layout` i `tile shown at (…) after N ms (bap54)` — N powinno wyjść ok. 400–600
- `(timeout - no layout report)` zamiast `(bap54)` znaczyłoby, że VC po VIEW nie zgłosił nowego
  układu — wtedy strzałka wróciła po 1,5 s w poprzednie miejsce
- `layout slot=… (… clocks, bap54) panel at (…)` dalej przy każdej zmianie
---

## Jedno zdanie na koniec

Strzałka już działa w obu układach; teraz na czas przełączania VIEW znika i wraca od razu we
właściwym miejscu, zamiast przez chwilę stać tam, gdzie była.
