# 17 – Etap 6: ostatnie połączenia CarPlay na VC + dzwonienie z listy

Stan 2026-10-20: **w paczce, w aucie jeszcze nie testowane.** Paczka `work\sd_etap5` (ta sama co 5.3/5.4,
aktualizuje Etap 4 i 5 naraz).

## Wynik testu w aucie (2026-10-21, `work\auto_test_10\karta_mod\…\collected\run-11`)

- Instalacja 4 znacznikami: PASS, CarPlay łączy się, identyfikacja **przyjęta** z 0x415A/0x4170/0x4171
  (sterownik sam dopisał je też z rozszerzonej tabeli 0x41 – na liście są dwa razy, iPhone to przyjął).
- `tel_verify=PASS count=114`, `send msg=4170 … send_rc=0`, iPhone odpowiedział dwoma 0x4171:
  `00 05 00 00 01 | 00 06 00 02 00 00` (ostatnie: dostępne, **count 0**) i to samo dla ulubionych.
  W sesji nie było rozmowy, więc iPhone nie miał powodu wysłać zmiany. VC: „Lista połączeń” i „Ulubione”
  mają tylko fabryczną pozycję „Skrzynka mailowa”. Przyczyna pustych list nieznana (iOS?).
  **Poprawka v2:** ponowne 0x4170 przy pustej liście (po 20 s, potem 60 s) i 5 s po każdym 0x4155
  (zmiana stanu rozmowy), max 4 na sesję; test C PASS (UBSAN, ASAN). Następny test: **zadzwonić w trakcie
  sesji** i potem COLLECT – czy iPhone wyśle listę z wpisami.
- „Książka adresowa” na VC = fabryczna książka MMI z Bluetooth (PBAP); przy CarPlay pusta. Do ukrycia
  w przyszłości jak u Luki (`GatedAddressBook`: „brak książki” przy CarPlay).

## Wynik testu v2 (2026-10-21, `work\auto_test_11\`, run-12)

- 0x4170 wysłany 5× (pierwszy + 4 ponowne), iPhone zawsze `count 0` dla ostatnich i ulubionych; po
  rozmowie iPhone **sam** przysłał 0x4171 (subskrypcja działa) – nadal 0. Wniosek: iOS celowo nie oddaje
  historii połączeń temu akcesorium (przyczyna nieznana; hipoteza: brak powiązania iAP2 z parowaniem
  Bluetooth – fabryczny `iap2.cfg` ma sekcję `[bluetooth]` zakomentowaną).
- Książka adresowa na VC pokazała kontakty, choć iPhone nie był połączony przez Bluetooth → to kopia
  zapamiętana w MMI. **Poprawka v3:** przy pustej liście iAP2 wrapper przepuszcza listę połączeń
  zapamiętaną przez MMI (`stock combined n=…` w logu), zamiast ją zasłaniać.

## Wynik testu v3 (2026-10-21, `work\auto_test_12\`, run-13)

- **Okładka działa** (poprawka `listRef=0` z 5.3).
- Listy nadal puste. Log `K3346_PHONE stock combined`: przed CarPlay (iPhone na Bluetooth) MMI miało listę
  połączeń **n=102**; po `gate ON` (CarPlay – MMI wyłącza BT telefonu) samo ją czyści: 66 → 31 → 0.
  iAP2 dalej 0. **Poprawka v4 (JAR `9d94fdd7…`):** zapamiętana ostatnia pełna lista z BT (przy CarPlay
  przyjmowana tylko lista nie krótsza – czyszczenie pomijane) + rozmowy z sesji CarPlay dopisywane na
  górę z PPS (odebrana / nieodebrana / wybrana, czas = koniec rozmowy), max 50, `posID` 0..n-1; tak samo
  ulubione. Po wyjściu z CarPlay – lista MMI jak w fabryce. Lista z iAP2, gdyby kiedyś przyszła, ma
  pierwszeństwo. Warunek: iPhone łączy się najpierw przez Bluetooth (jak zwykle przy uruchomieniu).

## Wynik testu v4 (2026-10-21, `work\auto_test_13\`, run-14)

- **Lista połączeń na VC jest** (101 pozycji zapamiętanych z BT, z datami z telefonu).
- Wybór pozycji → „Błąd sieci”: `vc dial … result=FAILED` – `File.renameTo` w `/tmp` zawodzi (na MMX
  `/tmp` = `/dev/shmem`, bez zmiany nazwy). Ten sam błąd miał natywny zapis listy iAP2 (nigdy
  `recents_written`). Okładka pisze do `/var/app/icab/tmp` – działa.
- Rozmowa z sesji: data 1.1.1970 (zegar MMX nieustawiony, PPS bez czasu rozmowy); po ponownym
  podłączeniu kabla znikała (czyszczona przy `gate OFF`).
- Porównanie z Luką: u niego iOS wysłał 12 ostatnich + 4 ulubione w 21 s na **to samo** żądanie
  (`CARPLAY_TELEPHONY.md`, `[car]`); jego warunek „auto na liście sparowanych CarPlay” u nas przechodzi
  (`available=1`), a lista i tak 0 – przyczyna po stronie iOS **otwarta**. Dzwonienie: Luka używa
  `requestUI2("tel:")`, K3346 ma tylko `requestUI(int)` → iAP2 `InitiateCall 0x415A` (z jego tabeli).
- **Poprawka v5:** polecenie wybierania zapisywane wprost (bez `renameTo`), hook bierze je dopiero z `\n`
  (bez połowy numeru); natywny zapis listy z zapasową ścieżką bez `rename`; rozmowy z sesji bez daty,
  zostają do restartu MMI, usuwane gdy świeża lista z BT ma ten sam numer i tryb (5 najnowszych).
  JAR + biblioteka `mm-ipod` (aktualizacja Etapu 4 i 5, 4 znaczniki). Testy PASS, symulacja 173/173.

## Wynik testu v5 (2026-10-21, `work\auto_test_14\`, run-16)

- Dzwonienie z listy działa (`initiate_call digits=12 rc=0`), rozmowy z sesji na górze bez daty, zostają po
  ponownym podłączeniu kabla. Lista z BT pojawia się dopiero po połączeniu iPhone'a przez Bluetooth
  (`stock combined` 0 → 36 → 71 → 101). Użytkownik odrzucił trwałą kopię listy na MMI: „Luki napewno to jakoś
  rozwiązał”.

## Etap 6b: dlaczego iPhone oddaje Luce listę, a nam nie (2026-10-21)

- Luka i yuedizhibo (V3.8, `tel_hook.c`) nie trzymają żadnej kopii: iPhone przez iAP2 wysyła 12 ostatnich
  i ulubione, auto nie ma wtedy BT. Nasze 0x4170 w `iap2.log` jest **bajt w bajt** jak ich (różnica tylko
  max 25/0 zamiast 20/20; iOS i tak tnie do 12). Odpowiedź iPhone'a zawsze `available=1, count=0`.
- Różnica w identyfikacji 0x1D01 (run-14): mamy USBHostTransport, Location, RG; **brak
  `BluetoothTransportComponent` (17) z MAC auta**. Hipoteza: iOS nie wiąże akcesorium USB z autem sparowanym
  przez BT i oddaje pustą historię. Niepotwierdzona.
- RE sterownika K3346 (`work\re_bt\`, `ipod-drvr-iap2.so`):
  - `init_features`: `[bluetooth] enable=yes` → bit 4 cech („Bluetooth connection enabled”);
  - `ident_info_sendmsgs/recvmsgs`: bit 4 dopisuje komunikaty 0x4E0x do identyfikacji;
  - `ident_info_tspbt@0xd490`: komponent 17 = id (`id=`, domyślnie 0), nazwa (`name=`, domyślnie
    „bluetooth”), opcjonalnie TransportSupportsiAP2Connection, **MAC: env `IAP2_BT_STATUS_MAC_ADDR`
    (17 znaków) albo `mac=`** (`%hhx:%hhx:…`). Bez MAC-a komponentu nie ma wcale;
  - `iap2_start_features`: po akceptacji `bt_send_info(status)` → 0x4E01 BluetoothComponentInformation
    (status z PPS `btconnectstatus` albo `connectstatus=enable`), potem `bt_start_updates`.
  - Wniosek: sekcję dodawać **tylko razem z poprawnym MAC** (komunikaty bez komponentu = ryzyko odrzucenia
    identyfikacji).
- Krok 1 (karta 2026-10-21): COLLECT zapisuje `bt-addr/listing.txt` i `bt-addr/objects.txt` (tylko odczyt;
  PPS, /var, /mnt, /net, /dev i to samo na RCC). Celem jest znalezienie lokalnego adresu BT MMI. Symulacja
  179/179 (dash, mksh).
- Wynik kroku 1 (run-17): w systemie nie ma adresu. RE `btstack` (`work\re_bt\`): radio Marvell MV8787 (SDIO);
  `/var/bdaddr` to tylko zapas (losowy adres, gdy chip zgłosi domyślny), na MMX go nie ma. Adres siedzi w chipie,
  idzie wyłącznie do trace („Local BtAddress is…”) i do HMI (`updateLocalBtAddress`). Użytkownik odczytał go
  z innego urządzenia. Adres nie trafia do repo ani docs.
- Krok 2 (paczka 2026-10-21):
  - plik `BT_MAC` na karcie (`mod/…/BT_MAC`, jedna linia, tylko na karcie);
  - `INSTALL_ETAP4` sprawdza format; zły albo pusty adres → stop bez zmian, brak pliku → jak dotąd;
  - `iap2.awk -v btmac=` dopisuje po `[ident]` sekcję `[bluetooth]` (`enable=yes`, `id=1111`,
    `name=Bluetooth`, `connectstatus=enable`, `mac=`): iap2.cfg 6141 B zamiast 5991;
  - log `ETAP4_BT=ON/OFF` bez adresu; `ROLLBACK_ETAP4` przywraca fabryczny plik.
  - Testy: symulacja 192/192 (dash, mksh, nowe T13-BT), ratunek 49/49. `sprawdz_karte.ps1 -Typ etap4`
    porównuje z `sd_etap5` i sprawdza `BT_MAC`.
  - Test w aucie: `ROLLBACK_ETAP4` + `INSTALL_ETAP4` (2 uruchomienia), restart MMI, iPhone **bez BT**
    na kablu. Oczekiwane w `iap2.log`: `BluetoothTransportComponent` w 0x1D01, `IdentificationAccepted`,
    0x4E01. W `k3346-rgi-ipod-native.log`: `list_update … count>0`.
- **Wynik testu 6b (2026-10-21, `work\auto_test_15\`, run-18):**
  - komponent przyjęty: `BluetoothTransportComponent` z MAC w 0x1D01, `IdentificationAccepted`, 0x4E01,
    iPhone odpowiada 0x4E03/0x4E04 (BluetoothConnectionUpdate). CarPlay i reszta działają;
  - lista z iAP2 **nadal `available=1, count=0`** → hipoteza BT obalona;
  - lista na VC pochodziła z pobrania przez BT w tej sesji (`stock combined` 27 → 36 → 71 przy `gate ON`,
    potem MMI czyści, wrapper trzyma `kept=71/101`);
  - dzwonienie z listy i rozmowy z sesji (na górze, bez daty) działają.
- **Etap 6c (paczka 2026-10-21):** warianty 0x4170 (`k3346_tel_build_start_lists_variant`). Kolejne prośby
  w sesji: v0 xPaiiN (25/0, combine), v1 Luka (20/20, combine), v2 0/0 (domyślny limit iOS), v3 12/12 bez combine,
  v4 same grupy właściwości. Po pierwszej liście z wpisami (`variant_ok=N`) kolejne prośby tylko tym wariantem.
  Log: `start_list_updates … variant=N`, `list_update … variant=N`. Test C 45/45 (UBSAN, ASAN), offsety ALL PASS,
  symulacja 192/192. Biblioteka `mm-ipod` `83950873…`. `BT_MAC` zostaje na czas testu. Test: CarPlay
  podłączony **5 minut** (ponowienia po 20 s i 60 s).
- **Wynik testu 6c (2026-10-21, `work\auto_test_16\`, run-19/20):**
  - wszystkie 5 wariantów (w tym bajt w bajt jak Luka) → `available=1, count=0`, także po rozmowie (#16);
  - lista na VC: tylko rozmowa z sesji (`session call mode=3 n=1`), zostaje po ponownym podłączeniu kabla.
- **Wniosek – koniec tropu iAP2:** ten iPhone nie oddaje historii połączeń temu MMI przez kabel, niezależnie
  od treści żądania i od komponentu BT. Różnica jest po stronie akcesorium (MHI2 Harman + stary chip
  autoryzacji, certyfikat z 2015, zamiast MHI2Q Cinemo) i nie da się jej obejść bez łatania binarek.
  Decyzja użytkownika: lista zostaje z BT w danej sesji + rozmowy z sesji (bez trwałej kopii).
- **Sprzątanie:**
  - warianty 6c usunięte, biblioteka `mm-ipod` wraca do `ef800f54…` (bit w bit ta z testu v5);
  - z karty usunięty `BT_MAC`, więc iap2.cfg znów ma 5991 B;
  - opcja `BT_MAC` w instalatorze zostaje, opisana jako nieskuteczna;
  - test C 34/34, symulacja 192/192.

## Zakres i ograniczenia

- **Ostatnie połączenia** (odebrane / wybrane / nieodebrane, do 12 – limit iOS dla CarPlay) na karcie
  Telefon VC, z nazwą kontaktu i datą; wybór pozycji → iPhone dzwoni.
- **Pełna książka adresowa – niewykonalna przez CarPlay.** iAP2 przenosi tylko ostatnie i ulubione; kontakty
  w MMI pochodzą z Bluetooth (PBAP), a CarPlay wyłącza Bluetooth telefonu (`DSICarplay.responseBTDeactivation`).
  Tak samo u Luki („Directory has no CarPlay source”).
- Ulubione iPhone też wysyła (to samo żądanie co w aucie xPaiiN), ale na razie są ignorowane.

## Jak działa

| Warstwa | Zmiana |
|---|---|
| `iap2.cfg [ident]` (Etap 4) | `recvmsgs=…,0x4171`, `sendmsgs=…,0x415a,0x4170` (5991 B, było 5970) |
| `native/message_table.c` | grupa 0x41 „Telephony”: stock 90 wpisów (0x54–0x59 użyte) → nasza kopia 0x72 wpisów + `0x5A InitiateCall` (1), `0x70 StartListUpdates` (1), `0x71 ListUpdate` (2); fail-closed (slot + 6 wpisów jak w fabryce), niezależnie od RGI |
| `native/telephony/k3346_tel.c` (nowy) | builder 0x4170 **bajt w bajt jak zweryfikowane w aucie żądanie xPaiiN** (recents max 25, combine, favorites max 0), parser + scalanie 0x4171 (port Luki), plik `/tmp/k3346-tel-recents.txt`, polecenia z Javy `/tmp/k3346-tel-cmd` (`dial <numer>`) → 0x415A (Type 0, numer, Service 1) |
| `native/send_hook.c` | wysyłka przez stock pulę pakietów (`sendlist_pop` + `packet_addparam_blob/nodata` + `link_send_message`) pod blokadą 0x5200, w wątku sterownika po stock 0x2700; 0x4170 2 s po identyfikacji, ponowienia co 15 s (max 3) do pierwszego 0x4171 |
| `native/observer.c` | instalacja tabeli 0x41 po RGI; każdy pakiet łącza → `k3346_tel_observe` (0x1D02/0x41xx = gotowość, 0x4171 = lista; ucięte ramki pomijane) |
| `build_rgi_ipod.sh` | `-DK3346_TEL_LISTS` + moduł; **biblioteka v8s bez zmian** (`058413dd…`) |
| Java `K3346GatedPhoneService` | czyta plik listy z wątku PPS, przy CarPlay `updateCombinedNumbers` (typ iAP2 1/2/3 → BAP received 2 / dialed 3 / missed 1, data z czasu), flagi `COMBINED_NUMBERS` + `DIAL_NUMBER`; stock lista w cache i odtwarzana po CarPlay |
| Java `K3346GatedPhoneListener` | `dialNumber` / `dialNumberFromAdbEntry` przy CarPlay → plik polecenia (tylko cyfry `+*#`, ≤32), `dialNumberResult` |

## Weryfikacja na PC

- Offsety na prawdziwym `ipod-drvr-iap2.so` K3346: **ALL PASS** (w tym grupa 0x41, 6 wpisów, `packet_addparam_blob`).
- Test C `native/tests/k3346_tel_test.c`: bajty 0x4170 = xPaiiN, 0x415A, 3 prawdziwe ramki 0x4171 (fixtures Luki),
  scalanie, ulubione ignorowane, plik, polecenia, sesja (2 s / 15 s / koniec po 0x4171), ucięte ramki,
  200 000 losowych ramek – PASS (UBSAN 30/30, ASAN bez ASLR 30/30; ASAN z ASLR w Docker Desktop zawiesza się
  na `DEADLYSIGNAL` – znana niezgodność środowiska, nie kodu).
- Java: telefon PASS (lista, typy, daty, wybieranie, poza CarPlay → stock), okładka PASS, D-pad PASS;
  `OVERRIDES_VERIFY=PASS`. JAR `7d68d27a…`, biblioteka `mm-ipod` `f4d7037d…`.
- Symulacja instalatora 173/173 (dash i mksh, T15 = aktualizacja 4 znacznikami), ratunek 49/49.

## Instalacja (karta typu `etap6`)

Znaczniki w folderze pakietu: `ROLLBACK_ETAP5`, `ROLLBACK_ETAP4`, `INSTALL_ETAP4`, `INSTALL_ETAP5` →
**run individual script 4 razy** (kolejno: cofa 5, cofa 4, instaluje 4, instaluje 5), potem restart MMI.
Weryfikacja karty: `tools\sprawdz_karte.ps1 -Karta I: -Typ etap6`.

**Ryzyko:** iPhone odrzuci nową identyfikację → CarPlay przez kabel się nie łączy → `ROLLBACK_ETAP4`
(mapa zostaje) i zgłoszenie. Nic w HMI ani na ekranie MMI nie zależy od Etapu 4.

## Test w aucie

1. CarPlay łączy się jak dotąd (mapa, RGI, telefon, okładka 5.3).
2. VC → Telefon → lista ostatnich: nazwy/numery, ikony odebrane/wybrane/nieodebrane, daty.
3. Wybór pozycji → iPhone dzwoni do tej osoby.
4. COLLECT. Logi: `k3346-rgi-ipod-native.log` (`tel_verify=PASS`, `start_list_updates … rc=0`,
   `first_list_update`, `recents_written n=…`, `initiate_call … rc=0`), `iap2.log` (IdentificationAccepted,
   0x4170/0x4171), `k3346-override-probe.log` (`K3346_PHONE recents n=…`, `vc dial …`).
