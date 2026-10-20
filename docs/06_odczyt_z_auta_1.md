# 06 – Wyniki pierwszego odczytu z auta (etap 1, tylko odczyt)

Data: 2026-10-20. Zestaw: `work\sd_odczyt_1\mod\command.sh` + collector edzenis v1.2 (bez zmian).
Uruchomiony przez M.I.B. 3.6.0 → *run individual script* (pierwsza próba dała „Nothing to do!”, bo M.I.B. 3.6.0
szuka pliku `mod/command.sh`, a nie `mod/custom.sh`; poprawione).
Wyniki: `work\sd_odczyt_1\K3346_ODCZYT_8519810\` i `work\sd_odczyt_1\MIB2_SOFTWARE_COLLECTOR_v1.2_PID_8523907\`
(kopia jest też na karcie SD). Zawierają dane z auta i telefonu, nie publikować.

## Najważniejsze ustalenia

| Pytanie | Wynik | Znaczenie |
|---|---|---|
| Czy collector v1.2 działa na prawdziwym MMI? | **Tak**: `COMPLETE_WITH_LIMITATIONS`, 157 plików (74 MB), „Vehicle firmware writes: none”. Na tym MMI są `wc`, `df`, `mount`, `uname` | Pierwszy test collectora v1.2 na prawdziwym aucie (wcześniej tylko offline) |
| Czy pliki w aucie = backup? | **150/150 identycznych**, 0 różnic | Backup jest wierny i analiza offline jest aktualna |
| `displaymanager`, czyli brakujący element backupu | zebrany, SHA-256 `8082efd7…` = **referencja MU1438 harman-f** | Statyczne ustalenia harman-f o DisplayManagerze dotyczą mojego auta |
| `/ifs/LSD.JXE` w aucie | identyczny z `lsd.jxe` z backupu (`e43d80e7…`) | `work\lsd.jar` to właściwy JAR zależności |
| Nowe pliki (spoza backupu) | `displaymanager`, `libdsidisplaymanagementproxy.so`, `libdsidisplaycontrollerproxy.so`, `libasimmxvideoovermost(md)proxy.so`, `/ifs/LSD.JXE` | w `files\` collectora |
| `/mnt/app` w aucie a fabryczna paczka K3346 | 6810 plików identycznych, **jedyna różnica: `NavActiveIgnore.jar`** (dodany w 2023) | Brak resztek innych modów CarPlay/VC. Paczka jest pełnym punktem odniesienia do odzysku |
| Tryb kombi (persistence `3221356656`) | `03 00 00 03 …` → **03 = LVDS** (wg `clusterCoding*.sh`) | Mapa VC po LVDS, zgodnie z założeniem edzenis |
| `displaymanager` – tryb | `/eso/bin/apps/displaymanager 2_lvds` | jw. |
| `dmdt` w aucie | **`gc` działa** (87 kontekstów). `gs` i `gd` nie zwróciły nic | Narzędzie edzenis do przełączania kontekstów działa (u Zykkiego `dmdt` nie odpowiadał, prawdopodobnie przez brak `LD_LIBRARY_PATH`) |
| Konteksty kombi (runtime) | 72={33}, 73={20,102,101}, 74={20,102,101,33}, 75={}, 76={58}, **77={20,102,101,58}**, 78={16}. **Brak 79+** | Dokładnie wariant „SysConst541==2 / LVDS”, ten sam co u edzenis (`sc 4 77` / `sc 4 72`) |
| Kamera cofania | kontekst -2 = {17 REAR_VIEW_CAM}, kontekst 3 = {16 HMI, 17}. Ekran główny | RVC nie korzysta z kontekstów kombi 72–78 |
| Start `mm-ipod` | z `usblauncher_otg.lua`: `mm-ipod -d iap2,config=/etc/mm/iap2.cfg,… -t usbdevice,path=/dev/io-usb/otg_dev -l /dev/shmem/iap2.log -vvvvvv` | Zgodnie z analizą: preload RGI musi trafić tutaj (etap 4) |
| Identyfikacja iAP2 (stock) | wysyłane: 5000, 5002, 5003, AE00, AE02, AE03, FFFB, 4154, 4156, 4157, 4159; odbierane: 5001, AE01, FFFA, FFFC, 4155, 4158, 4E09, 4E0A. Komponenty: USBHostTransport (16), **LocationInformation (22)** | Brak 0x52xx (RGI), co jest oczekiwane. Komponent 22 to punkt zaczepienia hooka identyfikacji edzenis |
| Log systemowy | 2× `Change NavStateOfOperation to 5` (sygnał wykorzystywany przez edzenis). Kilka połączeń i rozłączeń CarPlay w okresie logu (8× `connectDevice … CARPLAY`) | do wyjaśnienia: kabel czy adapter bezprzewodowy? |

## Co pozostaje nieznane (wymaga zmian konfiguracji lub testu w etapie 3)

- Sygnał przycisku VIEW i rolki: edzenis odczytuje go z DSITracera (porty 1798/20991), który trzeba włączyć
  w konfiguracji HMI. To nie jest już „tylko odczyt”, więc zostaje na etap 3, z rollbackiem.
- Geometria karty instrukcji dla układów B9 (1440×540).
- Zachowanie przy biegu wstecznym z aktywną mapą na VC (test nr 4 z etapu 3).

## Status etapów

- Etap 0: 0.1 ✔ (paczka K3346 w `firmware_K3346\`, `release = MHI2_ER_AUG22_K3346`, `MUVersion = 1438`,
  zawiera wariant `FM2-P-TNL-EU-AU-MLE`), 0.2 ✔ (brakujące elementy pozyskane z auta collectorem, dekodowanie LZOZ
  paczki niepotrzebne), 0.3 ✔, 0.4 ✔.
- Etap 1: ✔ zakończony.
- Następny: etap 2. Wymaga Twojej zgody na instalację narzędzi (JDK 8, Docker Desktop z kompilatorem QNX).
