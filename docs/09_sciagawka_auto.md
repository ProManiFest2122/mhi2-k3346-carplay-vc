# 09 – Ściągawka do auta (Etap 3)

Jedna strona do czytania na telefonie przy aucie. Pełny opis: `docs\08`.

> **Dwie żelazne zasady, z których wynika cała kolejność niżej:**
> 1. **Logi zbieraj PRZED rollbackiem.** Rollback usuwa znacznik instalacji i skrypt przestaje
>    zbierać logi. Jest na to obejście (plik `COLLECT_LOGS`, punkt 5), ale prościej nie wpaść.
> 2. **Logi zbieraj PRZED wyłączeniem auta.** Logi siedzą w `/tmp` i nie wiem, czy to pamięć RAM
>    czy dysk. Jeśli RAM — wyłączenie auta je kasuje.

---

## 0. Przed startem — sprawdź, że masz

Jak przygotować karty: **`docs\13`** (każda sprawdzona skryptem `tools\sprawdz_karte.ps1` przed wyjazdem).

**Trzy karty SD:**

- ☐ **Karta instalacyjna = Twoja karta z M.I.B.** (ta z Etapu 1), na którą skopiowałeś zawartość
  `work\sd_etap3\` (bez formatowania — M.I.B. działa z tej karty; na czystej karcie
  *run individual script* nie zadziała). W katalogu głównym: `mod\`, `ZNACZNIKI\`, `README.txt`
  i pliki M.I.B. Ma tam być **`_Swdlautorun.txt` z podkreśleniem** — jeśli widzisz
  `Swdlautorun.txt` bez podkreślenia, zmień nazwę na `_Swdlautorun.txt`.
- ☐ **Karta ratunkowa** (`work\sd_ratunek\`), **uzbrojona**: w katalogu głównym
  `Swdlautorun.txt` **bez** podkreślenia. Osobna karta, nigdy nie zostaje w aucie.
- ☐ **Karta z fabryczną paczką K3346** — ostatnia deska ratunku.

Plus:

- ☐ **Prostownik / zasilacz podłączony.** Bez tego nie zaczynaj.
- ☐ Świeży backup M.I.B.
- ☐ **Laptop albo telefon z czytnikiem kart SD.** Bez niego w aucie **nie zrobisz rollbacku**
  ani wymuszonego zbierania logów — oba wymagają skopiowania pliku na kartę.
- ☐ iPhone + kabel (podłączysz **dopiero po** instalacji i restarcie)

> **Kopia oryginalnych plików auta powstaje NA TEJ KARCIE** (`mod\...\backup\pre-v8s\`).
> Rollback działa tylko z **tą samą kartą**. Po instalacji nie formatuj jej, nie kasuj `backup\`
> i nie nadpisuj folderu `mod\`. Gdy wrócisz do komputera — **skopiuj cały `mod\` z karty na dysk**.

---

## A. NAJPIERW: test karty ratunkowej (na zdrowym aucie, przed instalacją)

Po co: przy bootloopie zielone menu nie działa, więc zwykły rollback też nie. Karta ratunkowa
działa bez HMI — ale chcemy **wiedzieć**, że działa w Twoim aucie, zanim będzie potrzebna.
Na aucie bez moda niczego nie zmienia (sprawdzone w symulacji: drzewo plików bit w bit bez zmian).

1. MMI działa normalnie. Włóż **kartę ratunkową** do SD1. Czekaj.
2. Jak przy instalacji M.I.B.: do ~60 s jednostka sama przejdzie w tryb aktualizacji i może się
   kilka razy zrestartować. Jeśli pojawi się pytanie — kontynuuj. **Nie wyjmuj karty w trakcie.**
3. Gdy MMI wróci do normalnego ekranu — wyjmij kartę.
4. Na laptopie/telefonie otwórz na karcie **`RATUNEK_LOG.txt`** (albo daj mi kartę do sprawdzenia):
   - **`WYNIK=OK`** + na końcu `STUFF_KONIEC … ratunek.sh=jest` → działa. Możesz instalować (punkt 1).
   - **tylko linia `STUFF_KONIEC`**, bez `WYNIK` → M.I.B. zadziałał, ratunek nie. **NIE instaluj.**
   - **brak pliku** → nic się nie zapisało. **NIE instaluj.** Wracamy do komputera.
     (Tak było w teście 1 z wersją 1 karty — przyczyny usunięte, patrz `docs\12`.)
   - **`WYNIK=NIEPEWNY`** → pokaż mi log, nie instaluj.
5. Na karcie jest teraz `_Swdlautorun.txt` — karta **sama się rozbroiła**.
   **Uzbrój ją z powrotem:** zmień nazwę na `Swdlautorun.txt`. Bez tego w razie bootloopu nic nie zrobi.
6. Przy okazji sprawdź **twardy restart MMI**: przytrzymaj On/Off (gałkę głośności) ≥ 10 s.
   Zanotuj, czy MMI się zrestartowało. (Pewny sposób awaryjny: bezpiecznik albo akumulator.)

Szczegóły: `RATUNEK_README.txt` na karcie ratunkowej.

---

## 1. Instalacja

1. **Karta instalacyjna** do **SD1**, auto na zapłonie.
2. **GEM → m.i.b. → advanced settings → run individual script**
3. Czekaj.

Ma się pojawić, w tej kolejności:

```
VERSION_GATE: HMI_Version=... OK
VERSION_GATE: ipod-drvr-iap2.so=739925 OK
VERSION_GATE: LSD.JXE=58131096 OK
VERSION_GATE=PASS
INSTALLING=...           (7 razy)
INSTALL_RESULT=PASS
SAFE TO REMOVE SD
```

**Dopiero po „SAFE TO REMOVE SD”** wyjmij kartę i zrestartuj MMI ręcznie.

> **Nigdy nie wyciągaj karty wcześniej.** Jeśli skrypt wygląda, jakby stał — czekaj.
> Wyjęcie karty w trakcie zapisu to jedyny sposób, żeby naprawdę narobić sobie kłopotu.

---

## 2. Jeśli coś pójdzie nie tak — drzewo decyzyjne

| Co widzisz | Co to znaczy | Co robisz |
|---|---|---|
| `VERSION_GATE ... instalacja PRZERWANA` | Blokada zadziałała. **Nic nie zostało zapisane.** Auto nietknięte | Zdjęcie komunikatu. Zrób **punkt 5** (wymuszone logi) i koniec na dziś |
| `ERROR ...` + `AUTO_ROLLBACK_RESULT=PASS` | Instalacja padła, ale **sama się cofnęła**. Auto w stanie wyjściowym | Zdjęcie. Nie restartuj na siłę. Zrób **punkt 5**, potem koniec |
| `AUTO_ROLLBACK_RESULT=FAIL – do not reboot` | **Jedyny groźny przypadek.** Pliki w połowie | **NIE RESTARTUJ.** Zdjęcie ekranu. Zostaw zapłon, pisz do mnie |
| `INSTALL_RESULT=PASS` | Wszystko dobrze | Restart MMI → punkt 3 |

### Jeśli MMI nie wstaje po restarcie (bootloop)

**0. Odczekaj do 5 minut.** Zielone menu potrafi być niedostępne tyle czasu po zimnym starcie.

**Zielone menu działa?** → zbierz logi (punkt 5), potem **poziom 1**:
skopiuj `ZNACZNIKI\ROLLBACK` do `mod\mib2high-carplay-rgi-v8s-simple\` na **karcie instalacyjnej**
→ SD1 → *run individual script* → restart. Ma być `ROLLBACK_RESULT=PASS`.
(Ta sama karta co przy instalacji — tylko na niej jest kopia oryginałów.)

**Zielone menu NIE działa** (logo w kółko, czarny ekran) → **karta ratunkowa**:

1. Prostownik podłączony. Włóż **uzbrojoną** kartę ratunkową do SD1. Zapłon.
2. Czekaj na tryb aktualizacji. Może być: logo → czarny → logo… i może być potrzebny twardy
   restart. U luka-dev potem pojawiło się czerwone menu aktualizacji: **Continue**, potem **Cancel**.
3. Wyjmij kartę. HMI powinno wstać.
4. Zielone menu działa → zbierz logi (punkt 5) → **poziom 1** jak wyżej. Ratunek przywraca tylko
   start HMI; resztę moda cofa dopiero poziom 1.
5. Zachowaj `RATUNEK_LOG.txt` z karty ratunkowej.

**Masz przejściówkę D-Link DUB-E100?** → telnet i rollback bez HMI (`docs\12`, „Druga droga").

**Nic nie pomogło** → **poziom 3:** pełne wgranie fabrycznej paczki K3346.

> Backup M.I.B. (dawny „poziom 2") też działa z zielonego menu — przy bootloopie jest dostępny
> dopiero po karcie ratunkowej.

---

## 3. Testy — co klikać i co zapisać

Podłącz iPhone'a i włącz CarPlay.

> **Ważne:** mapa **nie pojawi się** od samego podłączenia CarPlay.
> Musisz **uruchomić nawigację** (Apple Maps albo Google Maps) do dowolnego celu i dać jej chwilę.
> System czeka, aż nawigacja zgłosi gotowość **dwa razy** — to może zająć kilkadziesiąt sekund.

| # | Test | Co zapisać / sfotografować |
|---|---|---|
| 1 | MMI wstaje, CarPlay się łączy | ☐ czy coś wygląda inaczej niż normalnie |
| 2 | Uruchom nawigację → **mapa na Virtual Cockpit** | ☐ **zdjęcie VC** (albo „nie pojawiła się") |
| 3 | Naciśnij **VIEW** kilka razy | ☐ czy układ VC się przełącza, ile różnych widoków |
| 4 | Pokręć **lewą rolką** na kierownicy | ☐ czy cokolwiek reaguje |
| 5 | **Wsteczny** — tylko na **pracującym silniku** (na samym zapłonie się nie da), patrz niżej | ☐ czy mapa na VC została ☐ kiedy CarPlay wrócił na MMI |
| 6 | Odłącz i podłącz iPhone'a | ☐ czy mapa wraca sama |
| 7 | Przyjrzyj się obrazowi na VC | ☐ czy przesunięty ☐ czy pasek/kreska na krawędzi |

**Test 5 krok po kroku** (podjazd, wolne miejsce, noga na hamulcu, **zapisuj godzinę**):

- **5A:** P → **R**, stój 10 s → zdjęcie zegarów i MMI → **P**. Jeśli na MMI została kamera,
  naciśnij przycisk czujników parkowania (P z falami) i zapisz, czy CarPlay wrócił od razu.
- **5B:** **R**, 1–2 m do tyłu → **D**, kilka metrów do przodu → zapisz, kiedy CarPlay wrócił na MMI.

Czego się spodziewać (potwierdzone logami 2026-10-20, `docs\14`): przy **R** MMI pokazuje
fabryczną kamerę, a **mapa na VC gra dalej**. Na zegarach nie ma żadnej grafiki PDC.
CarPlay wraca na MMI **w tej samej sekundzie**, w której auto wyłącza kamerę. Kiedy to się
dzieje, decyduje fabryczny system parkowania, nie mod. Po wyjściu z R kamera potrafi zostać
do przejechania kilku metrów albo do naciśnięcia przycisku PDC.

Test 7 jest ważny: pasek albo przesunięcie to kwestia geometrii — poprawka to **edycja jednej
linijki tekstu**, nie przebudowa. Mam gotowy drugi wariant w paczce.

**Test „wyłącz i włącz auto" jest celowo przeniesiony do punktu 6** — po zebraniu logów.

Jeśli mapa się **nie pojawi**, nie kombinuj w aucie — przejdź do punktu 4.
Logi powiedzą mi, na którym etapie się zatrzymało.

---

## 4. ZBIERZ LOGI (run-01) — nie pomiń tego

**Teraz, przed wyłączeniem auta.** Niezależnie od tego, czy wyszło. Może być na pracującym
silniku (ostrzeżenie M.I.B. o zasilaniu dotyczy flashowania, nie zbierania logów).

1. Karta z paczką do **SD1**.
2. **run individual script** jeszcze raz.
   Skrypt wie, że jest już zainstalowany, i zamiast instalować — **zbierze logi**.
3. Ma być `COLLECT_RESULT=PASS` (albo `PARTIAL` — też dobrze) i `SAFE TO REMOVE SD`.

Zbieranie **nie zmienia niczego w aucie** — sprawdzone w symulacji, ani jeden plik nie jest dotykany.

---

## 5. Wymuszone zbieranie logów (gdy punkt 4 nie działa)

Potrzebne, gdy **instalacja się nie udała** albo **już zrobiłeś rollback** — wtedy skrypt sam
z siebie powie tylko `ALREADY_ROLLED_BACK` i nic nie zbierze.

1. Skopiuj gotowy plik **`ZNACZNIKI\COLLECT_LOGS`** do folderu
   `mod\mib2high-carplay-rgi-v8s-simple\`
2. **run individual script** → ma się pokazać `FORCED_COLLECT=1` i `MODE=COLLECT`.
3. **Usuń `COLLECT_LOGS` z folderu pakietu, gdy skończysz.** Dopóki tam leży, każde uruchomienie
   tylko zbiera logi i **nie instaluje**. (Kopia w `ZNACZNIKI\` może zostać — tam skrypt jej nie widzi.)

> **Dlaczego gotowe pliki, a nie „utwórz pusty plik":** w Windows „Nowy → Dokument tekstowy"
> tworzy `COLLECT_LOGS.txt` / `ROLLBACK.txt`, a rozszerzenie bywa ukryte. Skrypt takiego pliku
> **nie rozpozna** — zamiast rollbacku zbierze logi albo nic nie zrobi. Kopiuj z `ZNACZNIKI\`.
>
> Nie kopiuj znaczników do folderu pakietu **przed** instalacją.

Nawet bez udanej instalacji dostaję w ten sposób `sloginfo`, listę procesów i stan kontekstów
wyświetlacza — często cenniejsze niż sama instalacja.

> `ROLLBACK` ma pierwszeństwo nad `COLLECT_LOGS`. Jeśli zostawisz oba pliki, wykona się rollback.
> Tak jest celowo: zapomniany `COLLECT_LOGS` nie może zablokować ratunku.

---

## 6. Dopiero teraz: wyłącz i włącz auto

1. Wyłącz auto, wyjmij kluczyk, odczekaj chwilę, włącz z powrotem.
2. ☐ Czy po zapłonie CarPlay i mapa na VC wracają same?
3. **Zbierz logi jeszcze raz** (punkt 4) → powstanie `run-02`.

Drugie zbieranie ma dodatkowy sens: porównanie `run-01` i `run-02` powie mi, **czy logi przeżywają
wyłączenie auta**. To wiedza, której dziś nie mam, a będzie potrzebna w Etapie 4.

---

## 7. Na komputerze

Skopiuj z karty **cały** katalog `mod\` do `work\auto_test_3\karta_mod\`.
Są w nim logi (`...\collected\run-01`, `run-02`, …) **i kopia oryginalnych plików auta**
(`...\backup\pre-v8s\`) — to druga sztuka ratunkowa, gdyby karta zginęła.

Nie kasuj niczego z karty, dopóki mod jest zainstalowany w aucie.

---

## Jedno zdanie na koniec

Instalator jest przetestowany: wgrywa, odmawia na złym firmware i cofa zmiany bit w bit
(**62 testy, 0 błędów**), a karta ratunkowa przywraca start HMI bez zielonego menu (**27 testów,
0 błędów**). Czego **nikt** nie wie do tej chwili — czy mapa faktycznie pojawi się na Virtual
Cockpit. Jeśli nie, to normalny etap pracy, nie porażka: logi powiedzą dlaczego.
