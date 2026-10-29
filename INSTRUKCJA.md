# Instrukcja instalacji krok po kroku

[English version](GUIDE.md) · [← wróć do README](README.pl.md)

Ta instrukcja zakłada, że **nie znasz się na tym w ogóle**. Rób dokładnie to, co jest napisane, po kolei,
niczego nie pomijaj. Przy każdym kroku jest napisane, co powinieneś zobaczyć. Jeśli zobaczysz coś
innego, **zatrzymaj się** i zajrzyj do rozdziału [7. Gdy coś nie działa](#7-gdy-coś-nie-działa).

Cała instalacja trwa ok. 30 minut, z czego w aucie ok. 15. Jeśli najpierw instalujesz M.I.B.
(rozdział 1), dolicz ok. 30 minut.

---

## 0. Zanim zaczniesz

### 0.1 Co to robi

Po instalacji na **Virtual Cockpit** (cyfrowe zegary za kierownicą) zobaczysz:

- **mapę z CarPlay** (Mapy Apple albo Google Maps z iPhone'a),
- obok mapy **strzałkę**, gdzie skręcić, i **odległość** do skrętu,
- **rozmowy z iPhone'a**: kto dzwoni, odbieranie i rozłączanie przyciskiem na kierownicy,
- **ostatnie połączenia** z iPhone'a,
- **okładkę** słuchanej piosenki.

Do tego **Siri** uruchamiasz krótkim wciśnięciem przycisku głosowego na kierownicy, a **touchpad** MMI
działa w CarPlay jak strzałki.

### 0.2 Czy to pasuje do Twojego auta

To działa **tylko** w Audi z systemem **MHI2 (Harman)** i oprogramowaniem
**`MHI2_ER_AUG22_K3346`**, **MU 1438**. Sprawdzone w Audi A4 B9.

Wersję oprogramowania zobaczysz w zielonym menu albo w M.I.B. (patrz niżej). Szukasz dwóch rzeczy:
napisu **`K3346`** i numeru **`1438`**.

- Widzisz `K3346` i `1438` → możesz instalować.
- Widzisz inne wartości → **nie instaluj**. Nic złego się nie stanie, jeśli spróbujesz: instalator
  sam sprawdza wersję i na innym aucie **odmawia, niczego nie zmieniając**. Szkoda tylko Twojego czasu.

### 0.3 Co to jest M.I.B. i czy go masz

**M.I.B. (More Incredible Bash)** to darmowe narzędzie na kartę SD, które pozwala uruchamiać skrypty
w MMI. Ta modyfikacja instaluje się **przez M.I.B.**, więc M.I.B. musi już działać w Twoim aucie.

Jak sprawdzić, czy masz M.I.B.:

1. Włącz zapłon i poczekaj, aż MMI się uruchomi.
2. Otwórz **zielone menu**. W A4 / A5 / Q5 (B9, 2017–2020) przytrzymaj jednocześnie przyciski
   **NAV/MAP** i **RADIO** (te dwa przy pokrętle MMI) przez kilka sekund, aż pojawi się ekran z zielonymi
   napisami. W innych modelach przyciski są inne — tabela w [wiki M.I.B.](https://wiki.mibsolution.one/en/MHI2).
3. Na liście w zielonym menu szukasz pozycji **`m.i.b.`**.

- Jest `m.i.b.` → masz M.I.B., idź dalej.
- Nie ma `m.i.b.` → najpierw zainstaluj M.I.B. według rozdziału
  [1. Karta SD i M.I.B.](#1-karta-sd-i-mib-tylko-jeśli-jeszcze-nie-masz-mib).

Aby wyjść z zielonego menu, przytrzymaj te same dwa przyciski albo wciśnij przycisk powrotu.

### 0.4 Co przygotować

- [ ] **kartę SD z M.I.B.**, tę samą, z której M.I.B. działa w Twoim aucie — albo, jeśli jeszcze
      nie masz M.I.B., **pustą pełnowymiarową kartę SD 8–32 GB** (rozdział 1),
- [ ] **komputer** (Windows albo Mac) z **czytnikiem kart SD**,
- [ ] **prostownik albo zasilacz** do akumulatora — silnik będzie wyłączony, a zapłon włączony
      przez kilkanaście minut; słaby akumulator w trakcie instalacji to najgłupszy sposób, żeby coś zepsuć,
- [ ] **iPhone** i **kabel**, którym zwykle podłączasz go do auta,
- [ ] **ok. 30 minut** bez pośpiechu.

### 0.5 Kopia zapasowa MMI (zalecane)

Zanim cokolwiek zmienisz, zrób w M.I.B. kopię zapasową MMI na kartę (w menu `m.i.b.` jest pozycja
do kopii zapasowej, a jej opis znajdziesz w [wiki M.I.B.](https://wiki.mibsolution.one/en/MHI2)).
Ta modyfikacja ma własną kopię i własne odinstalowanie, ale kopia M.I.B. to dodatkowa siatka
bezpieczeństwa. Po zrobieniu kopii przegraj ją z karty na komputer.

---

## 1. Karta SD i M.I.B. (tylko jeśli jeszcze nie masz M.I.B.)

Masz już działające M.I.B. (w zielonym menu jest `m.i.b.`)? **Pomiń ten rozdział** i przejdź do
rozdziału [2](#2-pobranie-pliku-na-komputerze).

M.I.B. to cudze narzędzie (autor: Mr-MIBonk). Poniżej jest najprostsza droga jego instalacji, opisana
w [wiki M.I.B.](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/wiki/Prepare-M.I.B-SD-card-&-install-M.I.B).
Ta modyfikacja była sprawdzana z **M.I.B. V3.6.0**.

### 1.1 Jaka karta

- **pełnowymiarowa karta SD** (taka jak do aparatu). **Nie** microSD w przejściówce — wiki M.I.B.
  wyraźnie tego zabrania;
- pojemność **8–32 GB** (minimum 4 GB; większe niż 32 GB Windows nie sformatuje w FAT32 zwykłym sposobem);
- najlepiej **nowa** albo pusta karta. Ta karta zostanie potem na stałe kartą z M.I.B. i z kopią
  oryginalnych plików auta — nie używaj jej do niczego innego;
- przełącznik blokady zapisu z boku karty ustaw w pozycji **odblokowanej** (przesunięty w stronę styków).

### 1.2 Sformatuj kartę na FAT32

**Formatowanie kasuje wszystko, co jest na karcie.** Jeśli coś na niej masz, najpierw to przegraj.

Windows:

1. Włóż kartę do czytnika.
2. Otwórz **Ten komputer** (Eksplorator plików → „Ten komputer” po lewej).
3. Kliknij **prawym** przyciskiem na dysk karty (np. `E:`) → **„Formatuj…”**.
4. **System plików:** wybierz **FAT32**. (Jeśli na liście nie ma FAT32, karta ma więcej niż 32 GB —
   użyj mniejszej karty.)
5. **Rozmiar jednostki alokacji:** zostaw „Domyślny”.
6. **Etykieta woluminu:** dowolna, np. `MIB`.
7. Zaznacz **„Szybkie formatowanie”**.
8. Kliknij **„Rozpocznij”** → **„OK”** na ostrzeżeniu → po chwili **„Formatowanie zakończone”** → **„OK”**.

Mac:

1. Włóż kartę do czytnika.
2. Otwórz **Narzędzie dyskowe** (Launchpad → Inne → Narzędzie dyskowe).
3. Z menu **Widok** wybierz **„Pokaż wszystkie urządzenia”**.
4. Po lewej zaznacz **kartę** (górną pozycję, samo urządzenie, nie wolumin pod nim).
5. Kliknij **„Wymaż”**. Nazwa: `MIB`. Format: **MS-DOS (FAT)**. Schemat: **MBR** (Master Boot Record — tak może być opisane po polsku: „Główny rekord rozruchowy”).
6. Kliknij **„Wymaż”** i poczekaj, aż się skończy.

### 1.3 Pobierz M.I.B.

1. Wejdź na stronę wydań M.I.B.:
   **https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/releases**
2. Znajdź wydanie oznaczone na zielono **„Latest”** (w chwili pisania: **V3.6.0 – QR Edition**).
   Nie bierz wydań oznaczonych „Pre-release”.
3. Pod nim, w sekcji **„Assets”**, kliknij **„Source code (zip)”**. (Przy M.I.B. to jest właściwy plik —
   M.I.B. nie ma osobnej paczki.)
4. Plik zapisze się w **Pobranych**.

### 1.4 Skopiuj M.I.B. na kartę

1. Rozpakuj pobrany zip **w Pobranych** (Windows: prawy przycisk → „Wyodrębnij wszystkie…” → „Wyodrębnij”;
   Mac: dwuklik).
2. Powstanie folder o nazwie podobnej do `M.I.B._More-Incredible-Bash-3.6.0`. Otwórz go. Jeśli w środku
   jest **jeszcze jeden** folder o podobnej nazwie, otwórz i ten.
3. Jesteś we właściwym miejscu, gdy widzisz m.in.: foldery **`apps`**, **`Launcher`**, **`mod`**,
   **`patches`** i pliki **`metainfo2.txt`**, **`_Swdlautorun.txt`**, **`VERSION`**.
4. Zaznacz **wszystko** w tym folderze (Windows: `Ctrl+A`; Mac: `Cmd+A`) i przeciągnij na **czystą kartę**,
   do jej głównego okna.
5. Poczekaj, aż kopiowanie się skończy.
6. **Sprawdzenie:** w głównym oknie karty (nie w żadnym podfolderze) widzisz `metainfo2.txt`,
   `_Swdlautorun.txt` i folder `Launcher`.

### 1.5 Włącz automatyczną instalację M.I.B.

Plik `_Swdlautorun.txt` z podkreślnikiem na początku jest wyłączony. Trzeba usunąć podkreślnik.

Windows najpierw ukrywa końcówki nazw plików, a to łatwo prowadzi do błędu:

1. W Eksploratorze na karcie kliknij u góry **„Widok”** → **„Pokaż”** → zaznacz **„Rozszerzenia nazw plików”**.
   (Windows 10: zakładka „Widok” → zaznacz „Rozszerzenia nazw plików”.)
2. Kliknij **prawym** przyciskiem na `_Swdlautorun.txt` → **„Zmień nazwę”**.
3. Usuń **tylko** pierwszy znak `_`. Ma zostać dokładnie: **`Swdlautorun.txt`**
   (nie `Swdlautorun.txt.txt`, nie `swdlautorun`).
4. Naciśnij `Enter`.

Mac: kliknij plik, naciśnij `Enter`, usuń pierwszy znak `_`, naciśnij `Enter`.

**Na tę kartę nie kopiuj jeszcze niczego od tej modyfikacji.** Najpierw M.I.B. musi się zainstalować
w aucie (instalator M.I.B. uruchamia plik `mod/command.sh`, jeśli go znajdzie).

Wysuń kartę (prawy przycisk na dysku → „Wysuń”; Mac: symbol wysuwania).

### 1.6 Zainstaluj M.I.B. w aucie

1. Auto stoi, silnik wyłączony, **prostownik / zasilacz podłączony**, **zapłon włączony**.
2. Poczekaj, aż MMI się uruchomi, i przejdź na **ekran główny** (Home).
3. Włóż kartę do **lewego** slotu — to jest **SD1** (w A4 / A5 / Q5 B9 sloty są w schowku przed pasażerem).
4. W ciągu **ok. 60 sekund** na ekranie MMI zacznie się **aktualizacja oprogramowania**. To jest
   instalacja M.I.B.
5. Teraz przez **5–10 minut**:
   - MMI **zrestartuje się trzy razy** — to normalne,
   - **nie wyjmuj karty, nie wyłączaj zapłonu, niczego nie naciskaj.**
6. Na końcu pojawi się podsumowanie aktualizacji. Wyjdź z niego przyciskiem powrotu.
7. Jeśli pojawi się pytanie **„Start diagnostic tester”**, wybierz **CANCEL** (Anuluj).
8. Jeśli po minucie od włożenia karty **nic się nie dzieje**: sprawdź na komputerze, czy plik nazywa się
   dokładnie `Swdlautorun.txt` i leży w głównym oknie karty. Druga droga (przez czerwone menu, bez tego
   pliku) jest opisana w [wiki M.I.B.](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/wiki/Prepare-M.I.B-SD-card-&-install-M.I.B).

### 1.7 Sprawdź M.I.B.

1. Po zakończeniu poczekaj **do 5 minut** — zielone menu bywa niedostępne zaraz po uruchomieniu.
2. Otwórz zielone menu (B9: **NAV/MAP + RADIO**, krok [0.3](#03-co-to-jest-mib-i-czy-go-masz)).
3. Na liście jest **`m.i.b.`** → M.I.B. działa.
4. Weź kartę do komputera i sprawdź, że plik w głównym oknie karty znów nazywa się **`_Swdlautorun.txt`**
   (z podkreślnikiem). M.I.B. robi to sam po instalacji. Jeśli nadal jest `Swdlautorun.txt`, dodaj
   podkreślnik z powrotem — inaczej przy każdym włożeniu karty zacznie się aktualizacja.
5. Zrób kopię zapasową w M.I.B. (krok [0.5](#05-kopia-zapasowa-mmi-zalecane)).

**Uwaga od autorów M.I.B.:** po instalacji M.I.B. serwis może przy diagnozie zobaczyć błąd SVM
(Software Version Management). M.I.B. ma do tego funkcję „SVM fix” — opis w
[wiki M.I.B.](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/wiki/M.I.B-functions).

Ta karta zostaje **kartą z M.I.B.** Na nią w następnych rozdziałach wgrasz tę modyfikację.

---

## 2. Pobranie pliku (na komputerze)

1. Wejdź na stronę **[Releases](../../releases/latest)** tego projektu.
2. Przewiń w dół do sekcji **„Assets”**.
3. Kliknij plik **`mhi2-k3346-carplay-vc-1.0.0-install.zip`**.
   **Nie** klikaj „Source code” — to jest kod dla programistów, nie do instalacji.
4. Plik zapisze się w folderze **Pobrane** (Downloads).
5. Pobierz od razu **`mhi2-k3346-carplay-vc-1.0.0-uninstall.zip`** i zachowaj go. Przyda się do
   odinstalowania.

**Nie rozpakowuj jeszcze niczego.** Zrobisz to w kroku 3, od razu na kartę.

---

## 3. Przygotowanie karty (na komputerze)

### 3.1 Włóż kartę do komputera

1. Wyjmij kartę z M.I.B. z auta.
2. Włóż ją do czytnika kart w komputerze.
3. Otwórz Eksplorator plików (Windows) albo Finder (Mac). Karta pojawi się jako nowy dysk, np. **`E:`**
   albo **`D:`** na Windows.
4. Otwórz kartę. Zobaczysz pliki i foldery M.I.B., m.in. folder **`mod`**.
   **Niczego tam nie kasuj i nie formatuj karty.**

### 3.2 Rozpakuj plik na kartę — Windows

1. W folderze **Pobrane** kliknij **prawym** przyciskiem myszy na plik
   `mhi2-k3346-carplay-vc-1.0.0-install.zip`.
2. Wybierz **„Wyodrębnij wszystkie…”** (Extract All…).
3. W oknie, które się pojawi, kliknij **„Przeglądaj…”** (Browse…) i wybierz **dysk karty** (np. `E:`),
   potem **„Wybierz folder”**.
4. **UWAGA — tu prawie wszyscy robią błąd.** Popatrz na pole ze ścieżką. Windows sam dopisuje na
   końcu nazwę folderu, np.:

   `E:\mhi2-k3346-carplay-vc-1.0.0-install`

   **Skasuj** tę końcówkę, tak żeby w polu zostało samo:

   `E:\`

   (z literą Twojej karty).
5. Kliknij **„Wyodrębnij”** (Extract).
6. Jeśli Windows zapyta, czy zastąpić pliki (np. `mod\command.sh` z innego moda), wybierz
   **„Zamień pliki w miejscu docelowym”** (Replace the files in the destination).

### 3.2 Rozpakuj plik na kartę — Mac

Na Macu **nie przeciągaj całego folderu `mod`** na kartę: Finder zaproponuje „Zastąp”, a to skasowałoby
pliki M.I.B. z folderu `mod` na karcie. Zrób to tak:

1. W folderze **Pobrane** kliknij dwukrotnie plik `mhi2-k3346-carplay-vc-1.0.0-install.zip`.
   Powstanie folder o tej samej nazwie. Otwórz go — w środku jest folder **`mod`** i plik
   **`README_CarPlay_VC.txt`**.
2. Przeciągnij plik **`README_CarPlay_VC.txt`** do głównego okna karty.
3. Otwórz folder **`mod`** z Pobranych (ten rozpakowany) i, w drugim oknie, folder **`mod` na karcie**.
4. Z rozpakowanego `mod` przeciągnij do `mod` na karcie **dwie rzeczy**: folder
   **`mib2high-carplay-rgi-v8s-simple`** i plik **`command.sh`**. Jeśli Finder zapyta o `command.sh`,
   wybierz **„Zastąp”** (dotyczy tylko tego jednego pliku).

### 3.3 Sprawdź, czy wszystko jest na swoim miejscu

To najważniejszy krok przed wyjściem do auta. Otwórz kartę i sprawdź:

1. W **głównym** oknie karty (tam, gdzie są foldery M.I.B.) jest teraz plik **`README_CarPlay_VC.txt`**.
2. Otwórz folder **`mod`**. Jest w nim folder **`mib2high-carplay-rgi-v8s-simple`** i plik **`command.sh`**.
3. Otwórz folder **`mib2high-carplay-rgi-v8s-simple`**. Są w nim:
   - plik **`INSTALL_ALL`**,
   - plik **`RELEASE`**,
   - folder **`payload`**,
   - folder **`source`**.

**Typowy błąd:** w głównym oknie karty jest folder **`mhi2-k3346-carplay-vc-1.0.0-install`**, a dopiero
w nim `mod`. To znaczy, że w kroku 3.2 nie skasowałeś końcówki ścieżki. Naprawa: otwórz ten folder,
zaznacz jego zawartość (`mod` i `README_CarPlay_VC.txt`), przeciągnij do głównego okna karty (Windows:
„Zamień pliki”; Mac: postąp jak w kroku 3.2 dla Maca), a potem skasuj pusty już folder `mhi2-k3346-carplay-vc-1.0.0-install`.

### 3.4 Wysuń kartę

1. Windows: prawy przycisk na dysku karty → **„Wysuń”**. Mac: przeciągnij kartę do kosza
   (albo kliknij symbol wysuwania przy karcie).
2. Dopiero wtedy wyjmij kartę z czytnika.

---

## 4. Instalacja (w aucie)

### 4.1 Przygotuj auto

1. Auto stoi, **silnik wyłączony**.
2. Podłącz **prostownik albo zasilacz** do akumulatora.
3. **Włącz zapłon** (bez uruchamiania silnika).
4. Poczekaj, aż MMI się uruchomi i ekran będzie normalnie działał.

### 4.2 Włóż kartę

1. Otwórz schowek przed pasażerem. W A4 / A5 / Q5 B9 są tam **dwa sloty na karty SD**.
2. Włóż kartę do **lewego** slotu — to jest **SD1**, ten sam, z którego działa M.I.B.
   **Nie** do prawego (SD2): z niego M.I.B. nie działa.
3. Wsuń kartę do końca, aż kliknie.

### 4.3 Uruchom instalację

1. Otwórz **zielone menu**: przytrzymaj jednocześnie **NAV/MAP + RADIO**, aż pojawi się zielony ekran
   (jak w kroku 0.3).
2. Pokrętłem MMI przejdź na pozycję **`m.i.b.`** i **wciśnij pokrętło**.
3. Przejdź na **`advanced settings`** i **wciśnij pokrętło**.
4. Przejdź na **`run individual script`** i **wciśnij pokrętło**.

### 4.4 Poczekaj — nic nie dotykaj

Na ekranie zaczną się pojawiać linie tekstu zaczynające się od **`[V8S]`**. To normalne. Kolejno:

1. Sprawdzenie wersji oprogramowania — linia **`VERSION_GATE=PASS`**.
2. Instalacja czterech części, po kolei. Po każdej pojawia się linia kończąca się na `RESULT=PASS`.
3. Na końcu dwie linie, na które czekasz:

   **`INSTALL_ALL_RESULT=PASS`**

   **`SAFE TO REMOVE SD`**

To trwa **kilka minut**. Przez ten czas:

- **nie wyłączaj zapłonu,**
- **nie wyjmuj karty,**
- **nie naciskaj niczego w MMI.**

Gdy zobaczysz **`SAFE TO REMOVE SD`**, instalacja jest skończona. Kartę możesz wyjąć albo zostawić
w aucie, bez różnicy.

**Jeśli zamiast tego** zobaczysz linię z **`ERROR`**, napis **`INSTALL_ALL_RESULT=FAIL`** albo
**`do not reboot`**, nie rób nic więcej i idź do rozdziału [7](#7-gdy-coś-nie-działa).

---

## 5. Restart MMI i pierwsze uruchomienie

### 5.1 Zrestartuj MMI

Bez tego nowe funkcje się nie włączą.

1. W A4 / A5 / Q5 B9 przytrzymaj jednocześnie **duże pokrętło MMI** (wciśnięte) **+ NAV/MAP + RADIO**
   przez ok. **10 sekund**, aż ekran MMI zgaśnie.
   (Inne modele: tabela przycisków w [wiki M.I.B.](https://wiki.mibsolution.one/en/MHI2).)
2. Puść przyciski. MMI uruchomi się od nowa — to trwa **1–2 minuty**. Poczekaj, aż ekran działa
   normalnie.

### 5.2 Podłącz iPhone'a

1. Podłącz iPhone'a **kablem** do tego samego gniazda USB, co zwykle.
2. Odblokuj iPhone'a. CarPlay uruchomi się na ekranie MMI jak zwykle.

### 5.3 Włącz trasę

1. W CarPlay na ekranie MMI otwórz **Mapy** (Apple) albo **Google Maps**.
2. Wpisz dowolny cel i **rozpocznij nawigację**.

### 5.4 Popatrz na zegary

1. Na zegarach przejdź na widok **nawigacji / mapy** — przyciskami na kierownicy, tak samo jak przy
   fabrycznej nawigacji.
2. **Co powinieneś zobaczyć:**
   - mapę z CarPlay na zegarach,
   - obok mapy **kafelek ze strzałką** pokazującą najbliższy manewr i odległość do niego.
3. Sprawdź jeszcze:
   - przycisk **VIEW** na kierownicy przełącza duże / małe zegary; strzałka przesuwa się razem z nimi
     (na ułamek sekundy znika — tak ma być),
   - **lewa rolka** na kierownicy przybliża i oddala mapę,
   - gdy ktoś zadzwoni, na zegarach na karcie **Telefon** widać, kto dzwoni, i można odebrać przyciskiem
     na kierownicy,
   - krótkie wciśnięcie przycisku głosowego uruchamia **Siri**.

Jeśli to wszystko działa — **gotowe**. Odłącz prostownik.

### 5.5 Zachowaj kopię oryginalnych plików

Podczas instalacji na karcie zapisała się **kopia oryginalnych plików auta**. Bez niej nie da się
odinstalować modyfikacji.

1. Weź kartę do komputera.
2. Skopiuj **cały folder `mod`** z karty na komputer (np. przeciągnij go na pulpit).
3. **Nie kasuj** na karcie folderu `mod\mib2high-carplay-rgi-v8s-simple\backup` i **nie formatuj** tej karty,
   dopóki modyfikacja jest zainstalowana.

---

## 6. Odinstalowanie (i przed każdą aktualizacją)

Odinstalowanie przywraca auto do stanu fabrycznego, plik w plik. **Musisz użyć tej samej karty**, na
którą instalowałeś, bo na niej jest kopia oryginalnych plików.

1. Włóż tę kartę do komputera.
2. Rozpakuj **`mhi2-k3346-carplay-vc-1.0.0-uninstall.zip`** na kartę **dokładnie tak jak w kroku 3.2**
   (Windows: skasuj końcówkę ścieżki, ma zostać samo `E:\`). Mac: rozpakuj zip w Pobranych i przeciągnij
   sam plik **`ROLLBACK`** (z `mod/mib2high-carplay-rgi-v8s-simple/`) do folderu
   `mod/mib2high-carplay-rgi-v8s-simple/` **na karcie**.
3. Sprawdź: w folderze `mod\mib2high-carplay-rgi-v8s-simple` jest teraz plik **`ROLLBACK`**.
4. Wysuń kartę (krok 3.4) i włóż ją do auta, do lewego slotu (**SD1**).
5. Prostownik, zapłon, MMI uruchomione (krok 4.1).
6. Zielone menu → **`m.i.b.`** → **`advanced settings`** → **`run individual script`** (krok 4.3).
7. Czekaj, nic nie dotykając, aż zobaczysz **`ROLLBACK_RESULT=PASS`** i **`SAFE TO REMOVE SD`**.
8. Zrestartuj MMI (krok 5.1).

Auto jest fabryczne.

**Aktualizacja do nowej wersji:** najpierw odinstaluj starą wersję (ten rozdział), potem zainstaluj nową
(rozdziały 2–5). Jeśli uruchomisz instalację nowej wersji na zainstalowanej starej, nic się nie zmieni,
a na ekranie zobaczysz `ALREADY_INSTALLED` z podpowiedzią, żeby najpierw odinstalować.

---

## 7. Gdy coś nie działa

| Co widzisz | Co to znaczy | Co zrobić |
|---|---|---|
| W zielonym menu nie ma `m.i.b.` | Nie masz zainstalowanego M.I.B. albo zielone menu jeszcze się nie włączyło (do 5 min po starcie). | Poczekaj 5 minut i spróbuj jeszcze raz. Dalej nic → zainstaluj M.I.B. (rozdział [1](#1-karta-sd-i-mib-tylko-jeśli-jeszcze-nie-masz-mib)). |
| Po `run individual script` nie pojawiają się linie `[V8S]` | Na karcie nie ma pliku `mod/command.sh` albo karta nie jest w slocie, z którego działa M.I.B. | Sprawdź kartę jak w kroku 3.3 (najczęściej: rozpakowane o jeden folder za głęboko) i slot SD1. |
| `ERROR VERSION_GATE …` | To nie jest oprogramowanie K3346 / MU 1438. | **Nic nie zostało zmienione.** Ta modyfikacja nie jest dla Twojego auta. |
| `INSTALL_ALL_RESULT=FAIL` | Coś przerwało instalację. Auto jest w **bezpiecznym stanie**: części zainstalowane wcześniej zostają, a ta, która się nie udała, została automatycznie cofnięta. | Powód jest w linii z `ERROR`. Zrób jej zdjęcie. Uruchom skrypt jeszcze raz (krok 4.3) — instalacja dokończy brakujące części. Jeśli znów jest `FAIL`, odinstaluj (rozdział 6) i zgłoś problem. |
| Linia z `do not reboot` | Automatyczne cofanie się nie udało. | **Nie restartuj MMI.** Od razu odinstaluj (rozdział 6, kroki 1–7). |
| Po restarcie nie ma mapy na zegarach | Najczęściej: zegary nie są na widoku nawigacji albo w CarPlay nie ma włączonej trasy. | Włącz trasę w CarPlay, przełącz zegary na nawigację. Jeśli dalej nic — zbierz logi i zgłoś (niżej). |
| MMI nie uruchamia się po restarcie (czarny ekran albo ciągłe restarty) | Bardzo mało prawdopodobne, ale możliwe. | Odczekaj 5 minut, zrestartuj jeszcze raz (krok 5.1). Jeśli dalej nie działa — zobacz „MMI nie uruchamia się” w [README](README.pl.md#mmi-nie-uruchamia-się). |

### Zbieranie logów i zgłoszenie problemu

1. Zaraz po tym, jak problem wystąpi, **bez restartu MMI**, uruchom skrypt jeszcze raz: zielone menu →
   `m.i.b.` → `advanced settings` → `run individual script`. Gdy nie ma nic do instalowania, skrypt
   **zbiera logi**.
2. Czekaj na **`COLLECT_RESULT=…`** i **`SAFE TO REMOVE SD`**.
3. Logi są na karcie w folderze `mod\mib2high-carplay-rgi-v8s-simple\collected\run-NN` (najwyższy numer
   to najnowsze logi).
4. Załóż zgłoszenie na GitHubie: zakładka **Issues** → **New issue** → **Bug report**, i wypełnij pola.

**Nie wrzucaj logów do zgłoszenia** — są w nich Twoje dane osobowe (VIN, nazwy kontaktów, numery
telefonów, miejsca z nawigacji). Napisz, że je masz; prześlesz je prywatnie, jeśli będą potrzebne.

Jeśli instalacja się **nie udała**, ponowne uruchomienie skryptu ponowi instalację zamiast zebrać logi.
Wtedy szczegóły są w [README → Zgłaszanie problemu](README.pl.md#zgłaszanie-problemu)
(plik `…-collect-logs.zip`).
