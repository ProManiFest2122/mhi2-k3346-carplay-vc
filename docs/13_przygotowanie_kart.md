# 13 – Jak przygotować obie karty (krok po kroku, Windows)

Potrzebujesz dwóch kart: **ratunkowej** (nowa/pusta, do 32 GB) i **instalacyjnej** (Twoja karta
z M.I.B.). Kopiujesz w Eksploratorze Windows, a po każdej karcie **ja ją sprawdzam** — wkładasz
kartę do komputera i podajesz mi literę dysku. Sprawdzenie niczego na karcie nie zmienia.

Ścieżka projektu: `<projekt>\`

> **Nie mieszaj kart.** Plików karty ratunkowej nie kopiuj na kartę z M.I.B. i odwrotnie.
> Plików na kartach nie otwieraj ani nie zapisuj w Notatniku.

---

## Karta 1: RATUNKOWA (zrób ją pierwszą — w aucie też testujesz ją pierwszą)

**1. Formatowanie** (usuwa wszystko z karty — upewnij się, że nic na niej nie potrzebujesz)

1. Włóż kartę do czytnika.
2. Eksplorator → **Ten komputer** → prawy klik na karcie → **Formatuj…**
3. Ustaw:
   - **System plików: FAT32**
   - Rozmiar jednostki alokacji: domyślny
   - Etykieta woluminu: `RATUNEK`
   - ☑ **Szybkie formatowanie**
4. **Rozpocznij** → OK.

> Jeśli na liście nie ma FAT32 (tylko exFAT/NTFS) — karta ma więcej niż 32 GB. Przerwij i daj znać.

**2. Kopiowanie**

1. Otwórz folder `work\sd_ratunek\`.
2. **Ctrl+A** (zaznacz wszystko), **Ctrl+C**.
3. Otwórz kartę (sam dysk, np. `E:\` — nie żaden folder na niej), **Ctrl+V**.
   To 240 plików, 18 MB.

> **Zawartość, nie folder.** Nie przeciągaj folderu `sd_ratunek` na kartę — wchodzisz do niego
> i kopiujesz to, co w środku.

**3. Rzut oka** — w katalogu głównym karty widać m.in.:
`metainfo2`, `Swdlautorun`, `RATUNEK_README`, foldery `Launcher`, `ratunek`, `apps`, `esd`.
Nie ma folderu `sd_ratunek`.

(Jeśli Windows ukrywa rozszerzenia, `Swdlautorun.txt` wyświetla się jako `Swdlautorun` — to normalne.)

**4. Nie wyjmuj karty — napisz mi literę dysku.** Sprawdzę każdy plik sumą kontrolną.

**5. Po moim „GOTOWA"**: prawy klik na karcie → **Wysuń**, wyjmij, opisz kartę flamastrem:
**„RATUNEK – nie zostawiać w aucie"**.

---

## Karta 2: INSTALACYJNA (Twoja karta z M.I.B.)

**1. Kopia zapasowa całej karty** — zanim cokolwiek zmienisz

1. Włóż kartę z M.I.B. (tę z Etapu 1).
2. Utwórz folder `work\kopia_karty_mib_2026-10-20\`.
3. Otwórz kartę, **Ctrl+A**, **Ctrl+C**, wklej do tego folderu. Poczekaj do końca.

> Ta kopia zawiera backup M.I.B. z Twoimi danymi (VIN, kontakty). Zostaje tylko w `work\`.
>
> Jeśli pełną kopię karty już masz — tak jest u Ciebie: `ORYGINALNY BACKUP! NIERUSZAĆ\mib\`
> (sprawdzone: wszystkie pliki karty sprzed Etapu 1) — ten krok możesz pominąć.

**2. Kopiowanie paczki na kartę** — bez formatowania

1. Otwórz folder `work\sd_etap3\`.
2. **Ctrl+A**, **Ctrl+C**.
3. Otwórz katalog główny karty, **Ctrl+V**.
4. Jeśli Windows zapyta:
   - o folder `mod` — **scal** / potwierdź (M.I.B. ma swój `mod`, dokładamy do niego nasze pliki);
   - o plik `command.sh` — **Zamień plik w miejscu docelowym** (to stary skrypt z Etapu 1).

   Brak pytań też jest w porządku.

> Sprawdziłem: żaden nasz plik nie nadpisze pliku M.I.B. Jedyne zastąpienie to `mod\command.sh`
> z Etapu 1 — i tak ma być.

**3. Autorun musi być rozbrojony** — w katalogu głównym karty poszukaj pliku `Swdlautorun`:

- jest **`_Swdlautorun`** (z podkreśleniem na początku) → dobrze, nic nie rób;
- jest **`Swdlautorun`** (bez podkreślenia) → zaznacz, **F2**, dopisz `_` na początku, Enter.

**4. Nie ruszaj folderu `ZNACZNIKI`** i niczego z niego nie kopiuj — to na później (rollback, logi).

**5. Nie wyjmuj karty — napisz mi literę dysku.**

**6. Po moim „GOTOWA"**: **Wysuń**, wyjmij.

---

## Typowe pułapki (skrypt sprawdzający wyłapie każdą z nich)

| Pomyłka | Skutek w aucie |
|---|---|
| Skopiowany folder `sd_ratunek` / `sd_etap3` zamiast jego zawartości | nic się nie uruchomi |
| Antywirus usunął któryś plik M.I.B. (bywają oznaczane jako „narzędzia hakerskie") | tryb aktualizacji albo M.I.B. nie zadziała — sprawdź kwarantannę, skopiuj ponownie |
| Karta ratunkowa rozbrojona (`_Swdlautorun`) | w razie bootloopu nic nie zrobi |
| Karta instalacyjna uzbrojona (`Swdlautorun`) | tryb aktualizacji M.I.B. wczytałby instalator |
| Paczka na czystej karcie zamiast na karcie z M.I.B. | *run individual script* nie zadziała |
| Znacznik `ROLLBACK` skopiowany za wcześnie do folderu pakietu | zamiast instalacji uruchomi się rollback |
| Karta w exFAT/NTFS | jednostka jej nie odczyta |
| Wyjęcie karty bez „Wysuń" | może nie dograć ostatnich plików |

---

## Gdybyś chciał sprawdzić sam

W PowerShellu, w folderze projektu (zamień `E:` na literę karty):

```
powershell -ExecutionPolicy Bypass -File tools\sprawdz_karte.ps1 -Karta E: -Typ ratunkowa
powershell -ExecutionPolicy Bypass -File tools\sprawdz_karte.ps1 -Karta E: -Typ instalacyjna
```

Na końcu ma być zielone **„WYNIK: karta … GOTOWA"**. Czerwone `BLAD` mówi, co poprawić.

Ten sam skrypt przydaje się **po teście karty ratunkowej w aucie**: pokaże wynik z `RATUNEK_LOG.txt`
i przypomni, że karta się rozbroiła i trzeba ją ponownie uzbroić.

---

## Co dalej

Obie karty sprawdzone → jedziesz do auta wg `docs\09`: **najpierw punkt A (test karty
ratunkowej)**, instalacja tylko przy `WYNIK=OK`.
