# 17 – Etap 6: ostatnie połączenia CarPlay na VC + dzwonienie z listy

Stan 2026-10-20: **w paczce, w aucie jeszcze nie testowane.** Paczka `work\sd_etap5` (ta sama co 5.3/5.4,
aktualizuje Etap 4 i 5 naraz).

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
