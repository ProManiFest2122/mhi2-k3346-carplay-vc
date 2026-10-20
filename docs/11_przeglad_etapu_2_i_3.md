# 11 – Przegląd poprawności Etapów 2 i 3 (2026-10-20)

Pisane dla: Ciebie i dla kogoś, kto przejmie projekt i zechce wiedzieć, **na czym opiera się
zaufanie** do paczki przed pierwszą instalacją w aucie.

## Po co ten przegląd

Wcześniejsze weryfikacje (offsety 17/17, symulacja instalatora) sprawdzały to, o czym pomyślałem,
pisząc testy. Ten przegląd szukał rzeczy, których **ani kompilator, ani symulacja nie mogą
wykryć**: czy HMI w ogóle załaduje nasz kod, odwołania przez nazwy w stringach, założenia
o układzie kodu bibliotek K2161, różnice powłoki QNX, nieprawdziwe obietnice w dokumentacji.

Wszystko sprawdzone na **binariach K3346** (z backupu albo zebranych z auta – są identyczne).

## Wynik w skrócie

| Obszar | Wynik |
|---|---|
| Kod (Java, natywny, instalator) | **Bez błędów.** Wszystkie założenia z K2161 zachodzą w K3346 |
| Dokumentacja i procedura | **3 wady, poprawione** (kopia na karcie, `.txt`, `MIB_FORCE`) |
| Symulacja po poprawkach | **62 OK, 0 FAIL** pod `dash` **i** pod `mksh` |

---

## 1. Czy HMI załaduje nasz kod — TAK

`lsd.sh` w K3346 (`/mnt/app/eso/hmi/lsd/lsd.sh`):

- **linia 265:** gdy istnieją `lsd/dsitracer_activated`, `DSITracer.jar` i
  `bundles_dsitracer_enabled.properties`, HMI startuje z listą pakietów z tego pliku;
- **linie 393–399:** wszystkie `*.jar` z `lsd/jars/` trafiają do bootclasspath.

Instalator tworzy znacznik i JAR dokładnie w tych miejscach.

**Ryzyko sprawdzone osobno – podmiana całej listy startowej HMI.** Bez znacznika `lsd.sh`
nie podaje żadnej listy (linie 267–270), więc HMI używa listy wbudowanej w `lsd.jxe`.
Instalacja przełącza HMI na plik `bundles_dsitracer_enabled.properties`. Gdyby ten plik był
nieaktualny względem listy wbudowanej, HMI straciłoby pakiety – klasyczna droga do bootloopu.

Porównanie z `resources/bundles.properties` z `lsd.jar`:

| | Wbudowana | `bundles_dsitracer_enabled` |
|---|---|---|
| Pakietów | 160 | 161 |
| Brakujących względem wbudowanej | – | **0** |
| Inna klasa aktywatora | – | **0** |
| Różnice | – | tylko `serviceDelegateConfigurator` (DSITracer) + `DSITracer` w autostarcie |

Fabryczny `DSITracer.jar` K3346 zawiera obie klasy, których ta lista wymaga
(`TraceConfigurator`, `TraceActivator`). Aktywator naszego pakietu
(`org.mib2high.carplay.rgi.Activator`) zgadza się z tym, co wstawia `bundles.awk`.

## 2. Refleksja w Javie — wszystkie 5 miejsc zgodne z K3346

Refleksja przechodzi kompilację, a pada dopiero w aucie, jeśli w K3346 pole nazywa się inaczej.
Sprawdzone `javap -p` na `work\lsd.jar`:

| Miejsce | Cel | K3346 |
|---|---|---|
| `CarPlayLifecycle.java:118` | `TMInterAppHandler.deviceManager` | ✓ `private final IDeviceManager` |
| `BAPBridge.java:246` | `ClusterViewMode.favoredViewMode` | ✓ `private int` |
| `BAPBridge.java:248` | `ClusterViewMode.favoredViewModeReceived` | ✓ `private boolean` |
| `BAPBridge.java:310` | `ClusterViewMode.combiService` | ✓ `private ddp2.CombiService` |
| `BAPBridge.java:197,211` | `BAPDistance.getValue()/getUnit()` | ✓ `public int` |

`getDeclaredField` nie przeszukuje nadklas, więc sprawdzono też, że `ClusterViewMode` **nie ma
podklas** w `lsd.jar` (skan nadklas wszystkich klas: 0) – `getClass()` zwróci dokładnie tę klasę.
`V8MFactoryTrace.field()` przechodzi po nadklasach i łapie wszystko – tylko diagnostyka.

## 3. Klucz szyfrujący strumień mapy — założenie K2161 zachodzi w K3346

Najważniejsze znalezisko przeglądu. Hook przechwytuje klucz strumienia mapy (Stream 111)
tylko wtedy, gdy `AES_CBCFrame_Init` zostanie wywołane z **pierwszych 0x80 bajtów**
`AirPlayReceiverSessionSetSecurityInfo` (`altscreen_output.c:529`). To założenie o układzie
kodu `libairplay.so` z K2161 i **nie było objęte** weryfikacją 17/17 (ta dotyczy tylko drivera iAP2).

Gdyby nie zachodziło, zostaje drugi tor (`DeriveAESKeySHA512ForScreen`), ale iOS potrafi
zestawić strumień mapy **przed** strumieniem głównym (`altscreen_session.c:1862`) – wtedy klucz
przyszedłby za późno i mapy by nie było.

Deasemblacja `libairplay.so` z auta (`objdump` z toolchaina QNX):

```
00024c64 <AirPlayReceiverSessionSetSecurityInfo>:   rozmiar 0x6c
   24c7c:  bl 15514     -> AES_CBCFrame_Final(ctx)
   24c98:  bl 16990     -> AES_CBCFrame_Init(ctx, key, iv, 0)   adres powrotu 0x24c9c
   24cb4:  bl 15694     -> memcpy(..., 16)
   24cc4:  bl 15694     -> memcpy(..., 16)
```

Cele stubów PLT policzone ręcznie (ARM: `pc` czytany w instrukcji = adres + 8) i porównane
z relokacjami `R_ARM_JUMP_SLOT`:

| Stub | Obliczenie | Slot GOT | Symbol |
|---|---|---|---|
| 0x16990 | 0x16998 + 0x93000 + 0x2BC | 0xA9C54 | **`AES_CBCFrame_Init`** |
| 0x15514 | 0x1551C + 0x94000 + 0x64 | 0xA9580 | `AES_CBCFrame_Final` |
| 0x15694 | 0x1569C + 0x93000 + 0xF64 | 0xA9600 | `memcpy` |

Wniosek: wywołanie idzie przez PLT (więc `LD_PRELOAD` je przechwyci), z `encrypt=0`, a adres
powrotu leży pod przesunięciem **0x38 < 0x80**. Cała funkcja ma 0x6c bajtów, czyli mieści się
w oknie w całości.

## 4. Pozostałe elementy kodu

| Element | Wynik |
|---|---|
| Walidacje układu w kodzie natywnym | Stałe offsetów tylko w `ident_hook.c`, `message_table.c`, `send_hook.c`, `observer.c` – **wszystkie objęte** 17/17. Pozostałe wzmianki „K2161" to komentarze, nazwy funkcji i liczników |
| `observer.c:279` (okno adresu w `memcpy`) | Offset RX drivera iAP2 `0x125e8` – objęty 17/17. W `dio_manager` driver nie jest załadowany, więc hook jest bezczynny; zadziała w Etapie 4 w `mm-ipod` |
| Uruchamianie `mib2high-lvds-control.sh` | Hook robi `spawnve()` po `access(X_OK)` (`altscreen_output.c:925`). Wymaga trybu 755 (instalator nadaje) i **LF** – przy CRLF shebang byłby `#!/bin/sh\r`. `make_sd_package.sh` to pilnuje |
| `smartphone_integrator.json` | Plik ma komentarze `#` (to nie ścisły JSON – w stocku też). Po ich usunięciu: zmiana to **dokładnie jeden wpis** `LD_PRELOAD` w `children.carplay.envs` (`dio_manager`). W stocku `carplay` nie miał żadnego `LD_PRELOAD` – brak kolizji |
| `wc -c` w bramce wersji | Collector **na tym aucie** wykonał `$(wc -c < plik)` i porównał wynik jako string z `10` i `31` – przeszło. Czysta liczba, bez spacji |
| `awk` w bramce | Gawk w `/mnt/app/armle/usr/bin`, dostępny przez odziedziczony `PATH` (collector go nie ustawia i znalazł `awk`; instalator dokleja swoje katalogi przed odziedziczony). Gdyby `awk` nie było – pusty `HMI_Version`, przerwanie **przed** zapisem |
| Powłoka | QNX `/bin/sh` to rodzina ksh. Pełna symulacja pod `mksh` (MirBSD Korn shell): **62 OK, 0 FAIL** – tak samo jak pod `dash` |
| Uruchomienie przez M.I.B. | Repo M.I.B. lokalnie puste, więc nie sprawdzono, czy M.I.B. uruchamia czy wczytuje skrypt. Bez znaczenia: skrypt nie zależy od `$0`, ścieżki bierze z `MIB_SD`, komunikat końcowy wypisuje sam |

## 5. Wady znalezione i poprawione

**a) Kopia oryginałów auta leży na karcie – nigdzie o tym nie pisałem.**
`BACKUP="$MOD/backup/pre-v8s"`, czyli na karcie SD. Wyczyszczenie karty albo nadpisanie
folderu `mod\` nową paczką uniemożliwia rollback poziomu 1.
→ Ostrzeżenia w `README.txt`, `docs\08`, `docs\09`; instrukcja skopiowania `mod\` na dysk.

**b) Rollback wymaga komputera przy aucie, a instrukcja kazała „utworzyć pusty plik".**
W Windows „Nowy → Dokument tekstowy" daje `ROLLBACK.txt` (rozszerzenie często ukryte), którego
skrypt nie rozpozna – zamiast rollbacku zebrałby logi.
→ Paczka ma teraz gotowe puste pliki `ZNACZNIKI\ROLLBACK` i `ZNACZNIKI\COLLECT_LOGS` (w katalogu
głównym karty, poza `mod\`, więc skrypt ich nie widzi). Instrukcja: skopiuj, nie twórz.
Laptop / telefon z czytnikiem kart dopisany do listy rzeczy przed startem.
Procedura sprawdzona w symulacji dosłownie, krok po kroku (test T12).

**c) `MIB_FORCE=1` opisane jako obejście bramki – z M.I.B. nie da się go ustawić.**
Niegroźne (bramki nie da się przypadkiem obejść), ale wprowadzało w błąd.
→ Opis poprawiony: działa tylko z powłoki (symulacja, praca deweloperska).

## 6. Czego ten przegląd NIE dowodzi

- Że mapa **pojawi się** na VC. To zależy od zachowania renderera, MOST/LVDS i HMI w czasie
  działania – do sprawdzenia tylko w aucie.
- Że **VIEW i rolka** zadziałają. Kod VIEW był strojony pod dwa układy zegarów Q7
  (`altscreen_session.c:1370`: „K2161 Q7 supports the two layouts; slot 2 untested"),
  a komentarz w `altscreen_session.c:1355` wprost wspomina, że sygnał zoomu z **A4** wyglądał
  inaczej niż na Q7. VC w B9 może mieć inne tryby.
- Że **geometria 1440×542** jest właściwa dla panelu B9 (wariant 1440×540 w paczce).
- Zachowania narzędzi QNX innych niż sprawdzone (`wc -c` i `awk` – sprawdzone; `ls -ld` w
  `filemode()` – skalibrowane przez edzenisa i collector na tym aucie: kolumna rozmiaru = 5).
