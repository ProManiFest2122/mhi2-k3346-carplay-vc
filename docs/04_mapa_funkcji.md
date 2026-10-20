# 04 – Mapa funkcji (wersja skrócona)

Szczegóły techniczne są w `docs\02_edzenis_vs_K3346.md` i `docs\03_hmi_java.md`. Plan wykonania: `docs\05_plan.md`.

Trudność: **N** niska, **Ś** średnia, **W** wysoka. Ryzyko dla auta: **N** najwyżej funkcja nie działa,
**Ś** restart MMI lub niestabilny CarPlay, **W** możliwe zawieszenie HMI (potrzebny rollback).

| # | Funkcja | Kto ją ma | Czy jest odpowiednik w moim K3346 | Trudność | Ryzyko | Etap |
|---|---|---|---|---|---|---|
| 1 | Mapa CarPlay na VC + karta instrukcji + VIEW | edzenis (MHI2), Luka i yuedizhibo (MHI2Q), harman-f (MHI2 MQB) | Tak: biblioteki CarPlay identyczne z referencją MU1438, kontekst kombi zgodny z B9. Do sprawdzenia w aucie: przełączanie kontekstu, sygnał VIEW, geometria 1440×540 | Ś | Ś | 3 |
| 2 | Strzałki / odległość / pasy / ETA na VC | edzenis (tylko HUD), Luka (VC + HUD, własny renderer) | Tak: interfejs `CombiBAPServiceNavi` istnieje (inny pakiet niż w K2161). Najprostsza droga to fabryczna grafika VC przez BAP. Własny renderer jak u Luki wymaga przepisania na inną grafikę (Tegra) | Ś (BAP) / W (renderer) | Ś | 4 |
| 3 | Wskazówki na HUD | edzenis, Luka | Nie dotyczy: brak HUD w aucie | – | – | – |
| 4 | Telefon CarPlay na VC (połączenia, ostatnie, ulubione) | Luka, yuedizhibo | Częściowo: klasa karty telefonu VC istnieje. Stan połączenia jest obsługiwany przez stock, listy (ostatnie/ulubione) wymagają dodatkowej pracy | W | Ś | 5 |
| 5 | Okładka albumu na VC | Luka (port na MU1439 opisany) | Tak: klasa okładek identyczna jak w MU1439, funkcja pobrania okładki jest w `libiap2client` | Ś | N | 5 |
| 6 | Zoom mapy rolką | edzenis, Luka, yuedizhibo | Tak (wejście z kierownicy dostępne w HMI). Kod rolki do potwierdzenia trace'em | N–Ś | N | 5 |
| 7 | Touchpad MMI → poruszanie po CarPlay | Luka | Prawdopodobnie tak (w aucie jest panel z rozpoznawaniem pisma). Wymaga dopasowania klas Java | Ś | Ś | 5 |
| 8 | Popup PDC nie zasłania CarPlay | Luka | Tak: te same klasy parkowania i te same stałe. Klasę menedżera popupów trzeba odtworzyć pod K3346 | Ś–W | W (zmiana klas HMI) | 5 |
| 9 | Cofanie: mapa na VC i sesja CarPlay działają dalej | nikt (Luka świadomie oddaje ekran przy kamerze) | Stock przy wstecznym tylko pożycza ekran główny, a sesji nie zrywa. Hipoteza: mapa na VC przetrwa bez zmian w kodzie | N (jeśli hipoteza się potwierdzi) | N | test w 3 |
| 10 | Dane auta do iPhone'a (prędkość, zasięg, temperatura) | Luka | Częściowo: mechanizmy są w sterowniku, ale wyłączone w konfiguracji | Ś | Ś | później |
| 11 | Siri krótkim wciśnięciem | Luka | Tak: klasy istnieją, różnice API | Ś | Ś | później |
| 12 | Dźwięk 5.1 (B&O) | Luka | Nie: rozwiązanie dla innego sprzętu audio (MHI2Q) | – | – | nie planowane |

## Cofanie (priorytet 6 z CLAUDE.md)

- Luka: obsługuje tylko popup czujników parkowania (PDC/OPS). Kamera, asystent parkowania i przyczepy nadal przejmują
  ekran (`java_patch/com/luka/carplay/pdc/PdcSmallStageGuard.java`).
- Stock K3346: przy wstecznym pożycza ekran główny od CarPlay i oddaje go po wyjściu z biegu. Szczegóły w `docs\03`, sekcja 4.
- Plan: najpierw sprawdzić w aucie (etap 3, test nr 4), czy mapa na VC i sesja przetrwają bez zmian.
  Dopiero jeśli nie, zająć się poprawką.
