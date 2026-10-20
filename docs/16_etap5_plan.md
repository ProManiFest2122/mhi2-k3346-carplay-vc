# 16 – Etap 5: Siri, touchpad, telefon, okładka — plan, sonda 5.0, krok 5.1

Stan na 2026-10-20: **sonda 5.0: PASS. Krok 5.1 (Siri + touchpad) w aucie: PASS.**

## Wynik 5.1 w aucie (2026-10-20, logi `work\auto_test_6\karta_mod\…\collected\run-06`)

- Użytkownik: **przycisk głosowy włącza Siri, touchpad obsługuje CarPlay na MMI (wybór aplikacji).**
- `k3346-override-probe.log`: `K3346_SIRI updateActiveDeviceState active=true carplay=true`,
  3× `K3346_SIRI short_ptt -> long_ptt_path started` (także po ponownym podłączeniu iPhone'a);
  `K3346_DPAD carplay key controller active -> touchpad = dpad`, 20 zalogowanych tyków (limit logu)
  we wszystkich 4 kierunkach, zero błędów sinka.
Paczka: `work\sd_etap5\` (zawiera też v8s i Etap 4, które są już w aucie).

## Wynik sondy 5.0 (auto, 2026-10-20)

`work\auto_test_5\karta_mod\…\collected\run-04\logs\k3346-override-probe.log`:
`class_loaded TerminalModeSDSHandler`, `updateActiveDeviceState active=true`. HMI wstało, CarPlay
działa, krótki PTT → asystent Audi (fabrycznie). **J9 ładuje klasy z `lsd/jars/` zamiast z `lsd.jxe`**
→ podmiana fabrycznych klas działa jak u Luki.

## Krok 5.3 – okładka: SONDA (w paczce, w aucie jeszcze nie)

Fakty: iPhone wysyła okładkę do auta (`iap2.log`: `MediaItemArtworkFileTransferIdentifier`), stock
`dio_manager` jej nie pobiera. K3346 `libiap2client.so.1` eksportuje `iap2_connect`,
`iap2_nowplay_getartwork`, `iap2_disconnect` (API rozpoznane przez Lukę dla MHI2 MU1439:
`getartwork(h, NULL, 0)` = rozmiar, potem z buforem; bez id – bieżący utwór).

Sonda `diagnostics\k3346_artwork_probe.c` → `payload\k3346-artwork-probe` (QNX ARM, `dlopen` biblioteki,
tylko odczyt, jedno połączenie klienta). Uruchamiana **w COLLECT** z kopii w `/tmp`, limit 15 s (zawieszona →
zabita, `TIMEOUT`), tylko gdy jest `/dev/ipod0`. Wynik: `collected\run-NN\artwork\artwork-probe.txt`
(kody powrotu, magic `ffd8ff`=JPEG) i `artwork.bin`. **Nie wymaga instalacji** – wystarczy nowy `command.sh`
i plik sondy na karcie. Symulacja 157/157.

**Test:** CarPlay podłączony, **gra muzyka z okładką** (Spotify/Apple Music) → run individual script (COLLECT).
Jeśli `artwork.bin` = JPEG → pełne 5.3 wg Luki: pobieranie przy zmianie utworu (PPS `nowplaying`), PNG
256×256, Java `IPictureManager.responseCoverArt` (klasy K3346 = MU1316/1439). Jeśli pusto → plan B
(odczyt transferu w hooku `mm-ipod`).

## Krok 5.4 – odbierz/rozłącz z VC: PLAN (do zrobienia po teście przycisku na kierownicy)

1. **Test użytkownika:** czy fabryczny przycisk słuchawki na kierownicy odbiera/kończy rozmowę CarPlay
   (stock: `MFLPhoneButtonsListener` → FLASH 20 → HID `Eso_Telephony` do iPhone'a). Jeśli **tak** → wariant
   prosty: akcje VC (AcceptCall 30, HangupCall 29) → ta sama fabryczna ścieżka FLASH (Java, bez iAP2).
2. Jeśli **nie** → wariant iAP2 (jak Luka): `AcceptCall 0x415B` / `EndCall 0x415C` (param 0 = akcja 0, bez
   UUID) wysyłane z hooka w `mm-ipod` istniejącym prymitywem `send_message` (ten sam, który wysyła RGI
   `0x5200`), **w wątku sterownika** (kolejka z busa 19810, opróżniana w punkcie hooka); wpisy w
   `iap2_ctrl_msg_table` grupy 0x41 (dziś tylko 0x4154–0x4159), `[ident] sendmsgs += 0x415B,0x415C`
   (aktualizacja Etapu 4: `ROLLBACK_ETAP4` + `INSTALL_ETAP4`). Ryzyko: odrzucona identyfikacja = CarPlay
   się nie łączy → `ROLLBACK_ETAP4`.
3. Java: port `GatedPhoneListener` (przechwyt akcji VC przy bramce, odpowiedzi `*Result`), flagi
   ACCEPT/HANGUP w `MobileServiceSupport` dopiero gdy akcje działają.
4. Okno rozmowy na zakładce mapy – po teście użytkownika z telefonem BT (czy stock to robi).

## Wynik 5.2 w aucie (2026-10-20, logi `work\auto_test_7\karta_mod\…\collected\run-07`)

- **Działa:** karta telefonu VC bez komunikatu CarPlay („Telefon: Ulubione”), wychodzące
  („<kontakt>, Trwa wybieranie…”), przychodzące (okno „<kontakt> telefonuje” na karcie
  telefonu), aktywna rozmowa, rozłączenie; nazwa rozmówcy w górnym pasku VC na każdej zakładce.
- **PPS w aucie – format tekstowy:** `CallStateStatus::Connecting|Active|Disconnected`,
  `CallStateDirection::Incoming|Outgoing|Unknown`, `TelephonySignalStrength::2 bars`,
  `TelephonyRegistrationStatus::Registered Home`, `MobileOperator::Play WiFi Calling`
  (parser obsługuje). Plik pojawia się ~1 s po starcie sesji (`FileNotFound` przez pierwsze odczyty –
  nieszkodliwe).
- Bramka OFF/ON w trakcie testu = **zerwania USB** (`treat as cable removed` 03:17, 03:51, 04:06 –
  uszkodzony kabel); wrapper poprawnie oddaje stan stock i wraca.
- **Brakuje (zgłoszenie użytkownika):** (1) okno przychodzącej rozmowy nie pokazuje się na
  zakładce mapy, tylko na karcie telefonu; (2) „Odłóż”/odbieranie z VC nic nie robią – zgodnie
  z planem (akcje = 5.4). VC i tak pokazuje „Odłóż” dla rozmowy aktywnej/wybieranej (firmware VC
  dla stanów 2/3 zawsze oferuje Hangup – `VC_TELEPHONE_TAB.md` Luki).

## Krok 5.2 – rozmowy CarPlay na karcie telefonu VC

**Źródło danych – bez kodu natywnego.** Fabryczny sterownik iAP2 K3346 sam subskrybuje stan rozmów
(`iap2.log`: SEND `0x4154 StartTelephonyCallStateInformation`, RECV `0x4155 TelephonyCallStateInformation`
z `CallStatePhoneNumber/CallerName/Status/Direction/UniqueCallID`; `0x4158 TelephonyUpdate` z sygnałem
i operatorem) i publikuje je w PPS **`/ramdisk/pps/iap2/telephony`** (293 B w aucie, `rw-r--r--`;
nazwy atrybutów z `dio_manager.json`). `dio_manager` przekazuje to do DSI, gdzie K3346 Java wyrzuca
(`$DSICarplayListenerImpl.updateCallState` = pusty `return`). My czytamy PPS bezpośrednio z Javy.

| Klasa | Rola |
|---|---|
| `combi.bap.app.phone.ServiceManagerPhone` (podmiana) | fabryczna 1:1, usługa `CombiBAPServicePhone` rejestrowana przez wrapper; błąd → obiekt fabryczny |
| `K3346GatedPhoneService` (nowa) | port `GatedPhoneService` Luki: bramka = stock `FsgSetup.mobileConnectionType == 5` (CarPlay); w bramce mct 3 (HFP → znika komunikat „iPhone w trybie CarPlay”), Tel_State 15, FctID 22/23/24 (stan, nazwa/numer, czas rozmowy), sygnał/rejestracja/operator; wartości stock w cache i odtwarzane po wyjściu z CarPlay. Wątek co 300 ms czyta PPS tylko przy aktywnym CarPlay |
| `K3346PpsCall` (nowa) | parser PPS: wartości liczbowe (iAP2) lub nazwy z `dio_manager.json` |

Celowo **bez** list (ostatnie/ulubione) i akcji z VC (odbierz/rozłącz/wybierz/wycisz) – wymagają
wstrzykiwania iAP2 (5.4). Kombi nie dostaje flag tych funkcji, więc nie pokazuje martwych przycisków.
Odbieranie: telefon / ekran CarPlay na MMI / (fabrycznie) przycisk na kierownicy.

Weryfikacja: API `ServiceManagerPhone` = fabryczne, wywołania i napisy fabryczne zachowane; test
jednostkowy bramki i parsera 19/19 (atrapa usługi kombi); D-pad 7/7; symulacja 151/151 (+ zrzut PPS
w COLLECT: `pps\telephony.txt`, `pps\nowplaying.txt`, `pps\device.txt` – **dane osobowe**).

### Instrukcja 5.2

Karta: nowe `mod\command.sh` i `payload\mib2high-k3346-overrides.jar`, znaczniki `ROLLBACK_ETAP5` +
`INSTALL_ETAP5` w folderze pakietu (jak w 5.1: dwa uruchomienia skryptu, potem restart MMI).
Test (najlepiej z drugim telefonem): połączenie **przychodzące** (VC: dzwoni, nazwa/numer),
**odebrane** (VC: aktywna + licznik), **zakończone** (VC: brak rozmowy); połączenie **wychodzące**
z CarPlay; karta telefonu VC bez komunikatu CarPlay, z sygnałem/operatorem. Siri i touchpad dalej
działają. Potem COLLECT; w logu `K3346_PHONE gate ON`, `K3346_PHONE pps …`.

## Krok 5.1 – Siri krótkim PTT + touchpad D-pad (w paczce)

`mib2high-k3346-overrides.jar` (źródła `port_k3346\altscreen\java\overrides\src\`):

| Klasa | Zmiana względem fabrycznej K3346 |
|---|---|
| `sdsmanager…TerminalModeSDSHandler` | `isTerminalModeSpeechActive()` = true także przy aktywnym CarPlay (`CONN_METHOD_CARPLAY`=1) → krótki PTT idzie do CarPlay zamiast do asystenta Audi |
| `terminalmode…carplay.CarPlaySpeechRequestHandler` | `cancelPrewarm()` (krótki PTT) → `SDSAppHandler.pttLongDetected()` + `pttReleasedAfterLongPress()` = fabryczna ścieżka długiego PTT (mikrofon do telefonu, Siri START/STOP); błąd → fabryczny STOP |
| `terminalmode.keyevents.TMTouchscreenVBListener` | `touchPadPressed/Moved/Released` przy kontrolerze CarPlay → `K3346TouchpadDpad` → `updateKey(SOFTKEY_*, PRESSED/RELEASED)` → fabrycznie `BUTTON_UP/RIGHT/DOWN/LEFT` (7/6/8/5); ekran dotykowy, Android Auto, brak CarPlay – fabrycznie |
| nowe: `K3346TouchpadDpad`, `K3346Diag` | logika D-pada Luki 1:1 (progi 150/200, martwa strefa 30); wspólny log diagnostyczny |

Inaczej niż u Luki: nie podmieniamy `CarplayDSILifecycleController` (w K3346 zupełnie inny niż MU1316,
prywatne pola) – touchpad przechwytywany wyżej, w małej publicznej `TMTouchscreenVBListener`, a tyk idzie
fabryczną ścieżką klawisza (bez refleksji).

Weryfikacja: API 3 podmienionych klas = fabryczne (major 48); **wszystkie fabryczne wywołania
zachowane** (porównanie bytecode, także interfejsów); test D-pada 7/7; symulacja instalatora 149/149
(m.in. aktualizacja JAR-a: oba znaczniki naraz).

### Instrukcja 5.1

Karta instalacyjna: nowy `payload\mib2high-k3346-overrides.jar` + **oba** znaczniki
`ROLLBACK_ETAP5` i `INSTALL_ETAP5` w `mod\mib2high-carplay-rgi-v8s-simple\`.
1. Run individual script → `MODE=ROLLBACK_ETAP5` … `ETAP5_ROLLBACK_RESULT=PASS`.
2. Run individual script **jeszcze raz** → `MODE=INSTALL_ETAP5` … `ETAP5_INSTALL_RESULT=PASS`.
3. Restart MMI (karta ratunkowa pod ręką). Podłącz iPhone'a.
4. **Siri:** krótki PTT przy CarPlay → Siri (słucha, odpowiada)? Długi PTT → jak dawniej? Krótki PTT
   w trakcie Siri → kończy Siri? Bez iPhone'a → asystent Audi?
5. **Touchpad:** w CarPlay przesuwaj palcem po touchpadzie (góra/dół/lewo/prawo) – czy zaznaczenie się
   przesuwa, czy nie za szybko/za wolno. Ekran dotykowy MMI bez zmian.
6. Run individual script → COLLECT. W logu `k3346-override-probe.log`: `K3346_SIRI …`, `K3346_DPAD …`.

## Rozpoznanie (kod Luki/yuedizhibo + `javap`/CFR na `work\lsd.jar` K3346)

| Funkcja | Jak u Luki | Na K3346 | Trudność |
|---|---|---|---|
| Siri krótkim PTT | Java: `TerminalModeSDSHandler.isTerminalModeSpeechActive()` = true przy CarPlay; `CarPlaySpeechRequestHandler.cancelPrewarm()` odtwarza ścieżkę długiego PTT (`pttLongDetected` + `pttReleasedAfterLongPress`) | klasy są; brak `SiriAction` → `requestSIRIAction(int)` (0/1/2); handler nie trzyma `IContext` | mała |
| Touchpad → D-pad | Java: `$TerminalModeDSIKeyEventsController.updateTouchEvents` → `TouchpadController` → `postButtonEvent(5..8)`; natywny trackpad wyłączony (crash Apple Music) | outer `CarplayDSILifecycleController` zupełnie inny niż MU1316 → podmiana klasy wewnętrznej + refleksja; główny ekran już w trybie pokrętła (`primary_untouched=1`) | średnia |
| Telefon: stan rozmowy | stock `DSICarplay.updateCallState` → `GatedPhoneService` (5→3 HFP, Tel_State, FctID 22/23/24) | `$DSICarplayListenerImpl.updateCallState` to **pusty `return`** → parsować iAP2 **0x4155** w naszym hooku (`observer.c` widzi każdą ramkę); klasy BAP telefonu 1:1 | średnio-duża |
| Telefon: listy, sterowanie | 0x4170/0x4171, 0x415A–0x4160, wstrzykiwanie | grupa 0x41 w `iap2_ctrl_msg_table` ma tylko 0x4154–0x4159 (count 90) → rozszerzenie tabeli + `[ident]` | duża – później (5.4) |
| Okładka | natywnie z transferu plików iAP2 → PNG → `IPictureManager.responseCoverArt` | Java 1:1 (klasy i sygnatury identyczne); **Harmanowy `dio_manager` nie pobiera okładki** (importuje tylko `iap2_connect/…`, nie `getartwork`) → nowy kod natywny `iap2_nowplay_getartwork` przez `/dev/ipod0`, niepotwierdzone czy działa w sesji CarPlay | duża, z sondą |

**Kluczowe:** `lsd.sh:393-435` dokleja każdy `lsd/jars/*.jar` do `-Xbootclasspath` **przed** `lsd.jxe`.
Jeśli J9 rzeczywiście bierze klasę z naszego JAR-a (a nie z obrazu JXE), możemy podmieniać fabryczne
klasy tak jak Luka. Sprawdza to krok 5.0.

## Kolejność

1. **5.0 – sonda przesłaniania** (ta paczka).
2. 5.1 – Siri + touchpad (Java).
3. 5.2 – stan rozmowy na VC (natywny parser 0x4155/0x4158 + `GatedPhoneService`).
4. 5.3 – sonda okładki → okładka.
5. 5.4 – telefon: ostatnie/ulubione/sterowanie (osobny plan).

## Krok 5.0 – co jest w paczce

- `mib2high-k3346-overrides.jar` z **jedną** klasą: `de.audi.app.sdsmanager.apps.terminalmode.TerminalModeSDSHandler`
  – zachowanie identyczne z fabryczną (CFR z K3346), plus znacznik
  `/net/rcc/dev/shmem/k3346-override-probe.log` (`class_loaded`, `updateActiveDeviceState active=…`).
  Źródło: `port_k3346\altscreen\java\overrides\src\…`, build `java\build_overrides.sh`.
- Weryfikacja (`java\verify_overrides.py`): format Java 1.4 (major 48), **API publiczne/chronione
  identyczne z fabrycznym**; porównanie wywołań bytecode: wszystkie fabryczne wywołania te same,
  różnice wyłącznie w kodzie znacznika.
- Instalator: transakcja `INSTALL_ETAP5` / `ROLLBACK_ETAP5` (własna kopia `backup\pre-etap5\`),
  pełny `ROLLBACK` cofa 5 → 4 → v8s; COLLECT zbiera znacznik. Symulacja: **142/142** (dash, mksh).
- **Karta ratunkowa**: `ratunek.sh` usuwa teraz także `mib2high-k3346-overrides.jar`
  (bez tego bootloop od podmienionej klasy byłby nie do odratowania). Symulacja: **49/49**.

## Krok 5.0 – instrukcja

**Najpierw karta ratunkowa** (osobna karta): podmień `ratunek\ratunek.sh` na wersję z
`work\sd_ratunek\ratunek\ratunek.sh` (SHA-256 `65c8fbdc…`). Reszta karty bez zmian, ma zostać
uzbrojona (`Swdlautorun.txt`). Sprawdzenie: `tools\sprawdz_karte.ps1 -Karta <X>: -Typ ratunkowa`.

**Karta instalacyjna** (`I:\`): skopiuj z `work\sd_etap5\` z zastąpieniem: `mod\command.sh`, `README.txt`,
`ZNACZNIKI\INSTALL_ETAP5`, `ZNACZNIKI\ROLLBACK_ETAP5`, `mod\…\payload\mib2high-k3346-overrides.jar`;
potem `ZNACZNIKI\INSTALL_ETAP5` → `mod\mib2high-carplay-rgi-v8s-simple\`.
Sprawdzenie: `tools\sprawdz_karte.ps1 -Karta I: -Typ etap5` (0 błędów).

**W aucie** (silnik/prostownik, karta ratunkowa pod ręką):
1. GEM → run individual script → `ETAP5_INSTALL_RESULT=PASS` (zdjęcie).
2. Restart MMI (pokrętło głośności ~10 s). **HMI musi wstać.** Jeśli po 5 min nie ma ekranu
   głównego → karta ratunkowa (jak w teście z 2026-10-20), potem `ROLLBACK_ETAP5` przez GEM.
3. Podłącz iPhone'a, CarPlay jak zwykle. Sprawdź, że wszystko działa jak przed (mapa na VC, wskazówki).
4. Naciśnij krótko przycisk głosowy na kierownicy (zachowanie bez zmian – to tylko sonda; zapisz, co się dzieje).
5. Run individual script (bez znaczników) → COLLECT.

**Oczekiwany wynik:** `collected\run-0X\logs\k3346-override-probe.log` z `class_loaded` i
`updateActiveDeviceState active=true`. Brak pliku przy działającym HMI = J9 bierze klasę z `lsd.jxe` →
plan B dla 5.1–5.3: podmiana obiektów przez OSGi/refleksję z naszego bundla, bez podmiany klas.
