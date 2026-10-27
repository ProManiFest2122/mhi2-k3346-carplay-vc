# 20 – Ściągawka do auta (Etap 7c: strzałka manewru na VC)

Jedna strona do czytania na telefonie przy aucie. Dlaczego tak: `docs\19`. Pełny opis Etapu 7:
`docs\18`. Ściągawka Etapu 3 (instalacja mapy) to `docs\09` — ta jest osobna.

> **Co się zmieniło po wizycie 3.** Nakładka jest już na kombi i mapa jest widoczna dookoła niej —
> to działa. Nie było strzałki, bo **nikt nie nasłuchiwał na porcie, którym płyną dane o manewrze**:
> renderer i nasz hook oba się łączyły, zamiast jeden nasłuchiwać. Naprawione.
>
> Przy okazji: panel nie będzie już malowany, gdy nie ma czego pokazać, i doszła brakująca
> tekstura flagi celu. Procedura bez zmian: trzy uruchomienia, telefon może być podłączony
> od początku, sondy nie ma.

---

## 0. Przed wyjazdem

- ☐ **Karta `I:`** — sprawdzona (`sprawdz_karte -Typ etap7`).
- ☐ **Prostownik / zasilacz podłączony.** Bez tego nie zaczynaj.
- ☐ **Karta ratunkowa, uzbrojona** (`Swdlautorun.txt` **bez** podkreślenia) — na wypadek bootloopu.
- ☐ **Laptop albo czytnik kart.** Po zużyciu znaczników cofnięcie wymaga skopiowania pliku na kartę.
- ☐ iPhone + kabel. Tym razem może być podłączony od początku.

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

**Zanim włączysz nawigację** — popatrz na VC z samą mapą CarPlay:

- ☐ **Czy czarny prostokąt zniknął?** To jedyne pytanie, na które nie znam jeszcze odpowiedzi.
  Od tej wersji renderer nie maluje panelu, gdy nie ma manewru. Jeśli prostokąt **zniknął**, był
  nasz i sprawa zamknięta. Jeśli **został**, to fabryczne podkładki kombi — lekarstwo znane,
  jedna linijka w pliku na karcie, bez przebudowy.

Potem uruchom nawigację i daj jej chwilę — mapa na VC pojawia się dopiero, gdy nawigacja dwa razy
zgłosi gotowość:

- ☐ **Prawy panel**: czy jest strzałka i czy odpowiada karcie instrukcji na mapie
  (lewo / prawo / rondo / zjazd)?
- ☐ Czy strzałka jest **w miejscu fabrycznego panelu**, a nie w rogu?
- ☐ Czy pojawiają się **pasy ruchu**?
- ☐ Czy **odległość, ulica i ETA** są bez zmian? (Te rysuje VC z BAP, ich nie ruszamy — gdyby
  zniknęły, to regres.)
- ☐ Naciśnij **VIEW**: czy strzałka **przesuwa się razem z zegarami**?
- ☐ Mapa cały czas widoczna dookoła? (Na wizycie 3 była — gdyby kombi zrobiło się czarne,
  odłącz telefon, wszystko wraca od razu.)

---

## 3. Co sfotografować

| ☐ | Co | Dlaczego |
|---|---|---|
| ☐ | VC z mapą, **bez** włączonej nawigacji | czy czarny prostokąt zniknął — jedyne otwarte pytanie |
| ☐ | VC ze strzałką w prawym panelu | właściwy wynik całego Etapu 7 |
| ☐ | VC po naciśnięciu VIEW, drugi układ | czy strzałka podąża za układem |
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

Rozstrzygające tym razem:

- `netstat-an.txt` — ma być wpis **LISTEN na 19800** (na wizycie 3 nie było tam nic)
- `logs\vc_overlay-launch.log` — `server: connected to 127.0.0.1:19800` zamiast `not yet listening`
- tamże — `scene: ... commands=N` z **N większym od zera**
- tamże — **nie ma** `flag atlas open failed`

---

## Jedno zdanie na koniec

Droga danych o manewrze była przerwana w jednym miejscu: **oba jej końce próbowały się połączyć
i żaden nie nasłuchiwał**. Widać to w `netstat` z Twojego auta — portu 19800 nie było tam w ogóle.
Reszta łańcucha działa i jest to już sprawdzone na aucie: okno przyjęte, kontekst przełączony,
mapa widoczna dookoła nakładki. Ta wizyta sprawdza, czy po połączeniu tej jednej rzeczy pojawia
się strzałka.
