# 01 – Inwentaryzacja backupu M.I.B. (MHI2_ER_AUG22_K3346 / MU1438)

Stan na 2026-10-20. Analiza wyłącznie offline: `backup_do_edycji\` był otwierany tylko do odczytu.
Wszystkie kopie i rozpakowane obrazy są w `work\`. Folderu `ORYGINALNY BACKUP! NIERUSZAĆ` nie otwierałem.

> **Prywatność:** numer FAZIT jednostki występuje w nazwie katalogu backupu. W tym dokumencie zastąpiłem go
> ciągiem `<FAZIT>`. Katalog `docs\` nie jest w `.gitignore`, a `backup_do_edycji\`, `work\` i oryginalny backup są.

## 1. Identyfikacja jednostki – potwierdzona

Źródła: `MU1438-version.txt`, `MU1438-allversions.txt`, `SWDL\`.

| Pole | Wartość | Źródło |
|---|---|---|
| Train | **MHI2_ER_AUG22_K3346** | version.txt / allversions.txt |
| MainUnit (MU) | **1438** | version.txt |
| Samochód | **Audi A4 (8W0)**, model ID 019402, czyli B9. CLAUDE.md miało „A4/A5/Q5 – do potwierdzenia” | allversions.txt |
| Numer części / HW | 8W0035036C / HW 052 | allversions.txt |
| Wariant | `FM2-P-TNL-EU-AU-MLE`, EEPROM `MU-P-LN-EU` | version.txt |
| Aplikacja MMX | `CLU6_MMX2_AU_ER_G22_314PROD`, branch `rel/CLU6_KD4`, CL 43498891 | allversions.txt |
| Framework MMX / RCC | 5.41.10.SR5 MIB2CLU6 I165 / 5.41.0 MIB2MAIN I158 | allversions.txt |
| J9 / DSI | 1.15 / 2_60 | allversions.txt |
| RCC | U9044, `MIB2CLU6-Trunk_AU_rcc2_B1-18122B` | version.txt, rcc_version_info.txt |
| Kombi (VC) | `KI_FPK_AU491` (FPK = freiprogramowalne kombi, rodzina AU491 = A4/A5/Q5 B9) | version.txt |
| Inne moduły z listy wersji | `FC1H49xE` (FCB/FCP/FCR/**HREU**, prawdopodobnie panel MMI z rozpoznawaniem pisma, czyli touchpad), `DUA305` (ekran), GPS, LTE-ALS6, Tuner, MuINIC, IOC | version.txt |
| Mapa nawigacji | P370_N60S5MIBH3_EU_NT_Q3.23 | allversions.txt |

Na liście wersji **nie ma modułu HUD**, co sugeruje, że auto nie ma HUD (do potwierdzenia, patrz pytania w 05).

### Zgodność z referencyjnym MU1438 z repo harman-f

harman-f (`docs/research/MU1438_MU1440_OFFLINE_COMPARISON_2026-09-28.md` i `AUDI_B9_MU1438_COMPATIBILITY_LEAD`)
przeanalizował statycznie dokładnie ten firmware (pliki dostarczył użytkownik z A4 B9 K3346). Porównałem hashe:

| Element | SHA-256 (mój backup) | Zgodny z referencją harman-f |
|---|---|---|
| `lsd.jxe` | `e43d80e7…b99ce` | **TAK** |
| `libairplay.so` | `34503a9f…7e949` | **TAK** |
| `dio_manager.json`, `smartphone_integrator.json`, `displaymanager.json` | (pełne w tabeli niżej) | **TAK** (wszystkie 3) |
| `dio_manager`, `libdsicarplayproxy.so`, `libiap2client.so.1`, `libnvss_video.so`, `videoovermost`, `smartphone_integrator`, `mm-ipod`, `dmdt`, `libdmdt_core.so`, `libirc_mmx_adapter.so` | — | **TAK** (11/11 komponentów z `app.img`) |
| `displaymanager`, `devp-iso-mmx-mib2` | brak w backupie (stage2 MMX) | nie dało się sprawdzić |

Wniosek: mój firmware jest bajt w bajt tym samym MU1438, który opisuje harman-f. Ich ustalenia statyczne
(konteksty DisplayManagera, offsety w libairplay, B9 = mapa VC po LVDS) można stosować do mojej jednostki bez zastrzeżeń
co do wersji.

## 2. Co zawiera backup

| Część | Co to jest | Uwagi |
|---|---|---|
| `advanced\MU1438-FW-images\MMX2\app\70\default\app.img` | Obraz partycji **/mnt/app** MMX (QNX6, 972 MB = partycja 1 na `mnand0`) | Rozpakowany przez `tools\qnx6_extract.py` do `work\mmx_app\` (479 katalogów, 6811 plików, 937 linków, 678 MB) |
| `advanced\MU1438-system\` | Katalog **/etc** MMX (= `/mnt/system/etc`): `eso/production/*.json`, `mm/iap2.cfg`, `usblauncher*.lua`, skrypty | Skopiowany do `work\system\` |
| `MU1438-lsd.jxe` | HMI Java (normalnie `/ifs/lsd.jxe`, ładowane przez `lsd.sh`) | Kopia w `work\lsd.jxe`, skonwertowana do `work\lsd.jar` |
| `MU1438-ifs-root-part2-0x00ba0000.ifs` | Wbrew nazwie to **druga część IFS RCC** (QNX IFS, LZO), a nie MMX: `AudioProcess`, `TunerApp`, `car`, `MIBRoot`, `devp-iso-rcc`, proxy DSI RCC | Rozpakowany przez `tools\qnx_ifs_extract.py` do `work\mmx_ifs_stage2\` (149 wpisów; nazwa katalogu myląca, to RCC) |
| `advanced\MU1438-FW-images\RCC\*` | IPL, ifs-emergency, ifs-root, efs-system (QNX EFS/F3S), DSP RCC | Nierozpakowane: nie są potrzebne do CarPlay/VC |
| `MU1438-MMX_fs0.bin` / `MU1438-RCC_fs0.bin` | Surowe zrzuty flash NOR (64 MB) | MMX: partycje EFS pod 0x36000c4 i 0x3dc00c4, nierozpakowane |
| `MU1438-folders.txt`, `MU1438-Partition.txt` | Listingi `ls -alv` (MMX/RCC) i `fdisk mnand0` | Przydatne do sprawdzania dostępnych poleceń powłoki |
| `HMI\rhmi\`, `FEC\`, `SWDL\`, `EEProm`, `storage*.raw`, `shadow*`, `datasets.csv` | Dane jednostki, kody FEC, dane RemoteHMI, EEPROM | **Wrażliwe**, nie publikować |
| Gracenote2, SpeechRes, SpeechAppRes, RadioStationDB (≈3 GB) | Bazy danych | Nieistotne dla projektu |
| `logs\MU1438-<FAZIT>.log` | Log M.I.B. z wykonywania backupu (174 linie) | Wszystkie etapy „done”, „MMX dump not fully implemented yet” |

### Czego w backupie brakuje (ważne dla dalszej analizy)

- **Stage2 IFS MMX** (`/ifs`, `/etc` z IFS, `/usr`, `/lib`, `/bin`), a w nim **`displaymanager`** i `devp-iso-mmx-mib2`.
  M.I.B. sam o tym pisze („MMX dump not fully implemented yet”). Tę lukę może uzupełnić collector edzenis
  (skanuje `/net/mmx/ifs`, `/net/mmx/etc`, `/net/mmx/lib` …), patrz 02.
- Stan runtime: lista procesów (`pidin ar`), `sloginfo`, montowania, wartości persistence (KombiType, SysConst 541,
  tryb kombi `3221356656`), aktualny kontekst DisplayManagera.

### Dane wrażliwe zauważone przy inwentaryzacji

- `MU1438-shadow`, `advanced\MU1438-system\shadow`, `passwd`: hasła systemowe.
- `advanced\MU1438-system\eso\production\gal\gal_privatekey.key`: klucz prywatny (Android Auto / GAL).
- `FEC\*.fec`: kody aktywacji funkcji (powiązane z VIN), `EEProm.bin/.txt`, `storage*.raw`, `HMI\rhmi\Live\`.
- Numer FAZIT w nazwach katalogów.
- `lsd.sh` zawiera „hack VIN” `-DmyAudi.VIN=…`. To fabryczny ciąg testowy wpisany w skrypt, nie VIN tego auta.

## 3. Narzędzia użyte do inwentaryzacji (wszystkie tylko do odczytu)

| Skrypt | Co robi |
|---|---|
| `tools\qnx6_extract.py` | Parser QNX6 (Power-Safe) w czystym Pythonie, według `fs/qnx6` z jądra Linuxa. Otwiera obraz przez `mmap` w trybie read-only |
| `tools\qnx_ifs_extract.py` | Parser QNX IFS + dekompresor LZO1X w czystym Pythonie. Rozmiar po dekompresji zgadza się co do bajtu z nagłówkiem (`0x2976504`) |
| `tools\inventory_backup.py` | Generuje poniższe tabele (rozmiary + SHA-256), maskuje FAZIT |
| `tools\check_iap2_offsets.py`, `tools\derive_k3346_iap2_offsets.py` | Analiza ELF `ipod-drvr-iap2.so`, opisana w 02 |

Zainstalowano tylko `pyelftools 0.33` (pip), zgodnie z pozwoleniem.

## 4. Tabele: rozmiary i SHA-256

Ścieżki „na jednostce” podaję tak, jak widzi je QNX (`/mnt/app/...` = `work\mmx_app\...`,
`/etc/...` = `work\system\...`).

### Rozmiar

`backup_do_edycji/`: **4.3 GB** w **1871** plikach.

### Drzewo katalogów (do głębokości 4, z rozmiarem poddrzew)

```text
backup_do_edycji/  [4.3 GB, 1871 plików]
  MU1438-MHI2_ER_AUG22_K3346-<FAZIT>/  [4.3 GB, 1868 plików]
    FEC/  [9.9 KB, 10 plików]
      archiv/  [8.0 KB, 1 plików]
      backup/  [894.0 B, 3 plików]
      garbage/  [0.0 B, 0 plików]
      incoming/  [0.0 B, 0 plików]
      tmp/  [0.0 B, 0 plików]
    HMI/  [3.8 MB, 507 plików]
      rhmi/  [3.8 MB, 506 plików]
        Live/  [3.7 MB, 496 plików]  (…)
        generic/  [0.0 B, 0 plików]  (…)
        smartphone/  [80.6 KB, 10 plików]  (…)
        transient/  [0.0 B, 0 plików]
    SWDL/  [51.7 KB, 50 plików]
      FileCopyInfo/  [13.8 KB, 8 plików]
      Log/  [15.6 KB, 31 plików]
        service/  [0.0 B, 0 plików]
        user/  [15.1 KB, 28 plików]  (…)
      info/  [24.0 B, 2 plików]
    advanced/  [4.1 GB, 1282 plików]
      MU1438-FW-images/  [4.0 GB, 564 plików]
        Gracenote2/  [1004.4 MB, 9 plików]  (…)
        MMX2/  [972.0 MB, 3 plików]  (…)
        MUConsistency/  [6.8 KB, 1 plików]  (…)
        MuTnrRef/  [1.7 KB, 1 plików]  (…)
        RCC/  [64.0 MB, 6 plików]  (…)
        RadioStationDB/  [112.6 MB, 2 plików]  (…)
        SpeechAppRes/  [83.8 MB, 342 plików]  (…)
        SpeechRes/  [1.9 GB, 198 plików]  (…)
      MU1438-fileinfo/  [3.4 KB, 108 plików]
      MU1438-special/  [26.6 MB, 361 plików]
        mmx/  [860.7 KB, 9 plików]  (…)
        rcc/  [25.8 MB, 351 plików]  (…)
      MU1438-system/  [1.1 MB, 249 plików]
        autorunner/  [14.2 KB, 3 plików]
        boot/  [30.4 KB, 2 plików]
        config/  [965.0 B, 2 plików]
        db/  [85.3 KB, 11 plików]
        default/  [429.0 B, 1 plików]
        dhcp/  [7.1 KB, 4 plików]
        efu/  [256.0 B, 2 plików]
        eso/  [732.0 KB, 144 plików]  (…)
        memmonit/  [1.2 KB, 1 plików]
        mm/  [70.7 KB, 8 plików]
        opt/  [0.0 B, 0 plików]
        ppp/  [3.2 KB, 2 plików]
        scripts/  [20.0 KB, 28 plików]
        slf/  [4.5 KB, 2 plików]
        systracker/  [0.0 B, 0 plików]
  logs/  [4.2 KB, 2 plików]
```

### Pliki w katalogu głównym backupu (`MU1438-MHI2_ER_AUG22_K3346-<FAZIT>/`)

| Plik | Rozmiar [B] | SHA-256 |
|---|---:|---|
| `MU1438-AppDevelopment.jar` | 245793 | `75a2a1f62ac6bdd52461b4bc94cae9f067b205b00862ed89c5664dbb5510b326` |
| `MU1438-EEProm.bin` | 8192 | (hash pominięty – plik wrażliwy) |
| `MU1438-EEProm.txt` | 28989 | (hash pominięty – plik wrażliwy) |
| `MU1438-MMX_fs0.bin` | 67108864 | `1f4d0472d9ae5ea75b4e7f719bc6e6a150d5624538a53a8dbb7b99e6ed283f7f` |
| `MU1438-Partition.txt` | 11757 | `e4525578e42277ed725cc5f28c66c60b1b9d6ceefc81e1aae7d48be44215f930` |
| `MU1438-RCC-Flashlock.log` | 474 | `ace65c380250bd5098f2437598dcd312e8adf3a8ac7a04f224991e869faae8af` |
| `MU1438-RCC_fs0.bin` | 67108864 | `5abb1b073f51ba8048abc5d550783f75dd4d51493d8ffec8432b3a0a0f77cc03` |
| `MU1438-StartupImageConfig.txt` | 2843 | `b7e47b2a4517fbd3680cc4a6612b2db7628ea6297b2207a66b9f2dcf5bd4c1f7` |
| `MU1438-allversions.txt` | 3309 | `fac509ef8b0cc3c8b1b226509e15d3597b201c39a3312b7de5b5d90febef36c9` |
| `MU1438-datasets.csv` | 74948 | (hash pominięty – plik wrażliwy) |
| `MU1438-folders.txt` | 33920 | `9fb9e7f49916b6b69e2b5ab1fe154e36881ae8a945a73327f8d1e64238059181` |
| `MU1438-ifs-root-part2-0x00ba0000-OFFSET.txt` | 28 | `82c8316a9932f648822a251fa894717108e8deffb9ae45efbc1abe198d201bfa` |
| `MU1438-ifs-root-part2-0x00ba0000.ifs` | 18481152 | `3ed0e0ef87741825f98917785c1af3792c4e88d15adfb255a32495b047fe3262` |
| `MU1438-lsd.jxe` | 58131096 | `e43d80e7eeb803d6a7db29908562b9545e7b17138d4660ce3a6efafa974b99ce` |
| `MU1438-shadow` | 56 | (hash pominięty – plik wrażliwy) |
| `MU1438-shadow_rcc` | 27 | (hash pominięty – plik wrażliwy) |
| `MU1438-storage1.raw` | 19445 | (hash pominięty – plik wrażliwy) |
| `MU1438-storage2.raw` | 19445 | (hash pominięty – plik wrażliwy) |
| `MU1438-version.txt` | 13057 | `9ee172dc2fe3e45384ff77c01b1158b5de9ff0ea8f72adf61d9dc2083d346f8a` |

### Obrazy partycji (advanced/MU1438-FW-images)

| Ścieżka | Rozmiar [B] | SHA-256 |
|---|---:|---|
| `MU1438-MHI2_ER_AUG22_K3346-<FAZIT>/advanced/MU1438-FW-images/MMX2/app/70/default/app.img` | 1019183104 | `860592c82299707c4c69f28283ed3860afaa6b6e2741de3dfdcb588829c6e6df` |
| `MU1438-MHI2_ER_AUG22_K3346-<FAZIT>/advanced/MU1438-FW-images/RCC/dsp/21/default/AUDI_MIB_DSP.bin.bgz` | 3145728 | `35d2b2878a91dd4f2c8660732c4c2108bdddaf7275974c932f809a7cdd3392b6` |
| `MU1438-MHI2_ER_AUG22_K3346-<FAZIT>/advanced/MU1438-FW-images/RCC/efs-system/21/default/efs-system.efs` | 33292288 | `1e4dad1d5433ef3365a926e8419509a12132a6f9ac7fb939931da891a1d16890` |
| `MU1438-MHI2_ER_AUG22_K3346-<FAZIT>/advanced/MU1438-FW-images/RCC/ifs-emg/21/default/ifs-emergency.ifs` | 5373952 | `72964e8527379332b39fd72d23cb6915c43b69f4c7dee29bfbb29463496bfe85` |
| `MU1438-MHI2_ER_AUG22_K3346-<FAZIT>/advanced/MU1438-FW-images/RCC/ifs-root/21/default/ifs-root.ifs` | 25165824 | `d279b1cc9caf057d0657e8b29a601793c96fa46714892d5d95f60de9351ecc91` |
| `MU1438-MHI2_ER_AUG22_K3346-<FAZIT>/advanced/MU1438-FW-images/RCC/ipl/21/default/ipl-mib2.bin` | 131072 | `ecfc5d2793c605ec875eab14885435b818b35603b811eba447286500c4111e0f` |

### Pliki istotne z `app.img` (partycja `/mnt/app` MMX, wyodrębnione do `work/mmx_app/`)

| Ścieżka na jednostce | Rozmiar [B] | SHA-256 |
|---|---:|---|
| `/mnt/app/armle/bin/usblauncher` | 398221 | `c7a90c0a5bb8a8bebc5b9cca77e07e998a83e9b2103d8a6d6ff55bebd6a11457` |
| `/mnt/app/armle/lib/dll/deva-ctrl-ipod.so` | 18052 | `4bf8e4d056438145f6404f3b502b26bb67385829051be289a9a087eed1dba622` |
| `/mnt/app/armle/lib/dll/devu-iap2-tegra3-ci.so` | 26886 | `d28728eb39f6aa1c1f487da1a14e1f0bc17ba47ebfc448d2084c26498bdfc120` |
| `/mnt/app/armle/lib/dll/devu-iap2ncm-tegra3-ci.so` | 27238 | `ffcce5ffe499f5b501ab469f2cc748f3cbc3f372e5375b3e9dd5ce692e28b5ec` |
| `/mnt/app/armle/lib/dll/iofs-i2c-ipod.so` | 10830 | `f7b4296713836403de3d257e4e1b0376ef8add8147871a043ab4564054bc401a` |
| `/mnt/app/armle/lib/dll/iofs-ipod.so` | 291231 | `7e1985b314273819915f511d2e1bb108e0798dbf9ee119b22497c67878c3101b` |
| `/mnt/app/armle/lib/dll/iofs-ser-ipod.so` | 17779 | `b12e321422ede2900475b01b01b3668891fcab05e29ec30d81d0ab0467ced9ea` |
| `/mnt/app/armle/lib/dll/iofs-usb-ipod.so` | 22474 | `dbaa7ea8c170c57d6d971776e30ef7638f6ad2ebfde12b35f8765c0c2a035ea6` |
| `/mnt/app/armle/lib/dll/ipod-acp-i2c.so` | 11298 | `26bb9b07e4eaedc40ad26375c02b65a096e41e58b977ac690b466bf3a247856c` |
| `/mnt/app/armle/lib/dll/ipod-drvr-iap2.so` | 739925 | `0ac13f43d3fb259eee328c272fa1211bb558b6e2cd97c6e38ef38e4e71469d51` |
| `/mnt/app/armle/lib/dll/ipod-transport-usbdevice.so` | 19298 | `18ed46420ca6212398b0c6f9b30fd53d8270d0f0274f1e808f1140afc8d474a9` |
| `/mnt/app/armle/lib/dll/ipod-transport-usbhost.so` | 20912 | `bf40026caad0c7a31811eacf8d8d08edba7f0b23cb6a79bace25024a802b8777` |
| `/mnt/app/armle/lib/dll/mm-mdp-ipod.so` | 16645 | `2a0fe86682e7ffb5ce07ffabacc9cff2021598033bc13b87b01bb388941314a7` |
| `/mnt/app/armle/lib/dll/mss-ipodgeneric.so` | 84653 | `cf4d1a197477f8c324e5fc409111c1fb5c86195e2650ce6fdc1309d6953045f9` |
| `/mnt/app/armle/lib/dll/mss-ipodiap2.so` | 74037 | `1d4620b3524aac7b58cbd9ab65b86c8ddd7d7a1198a53eac563e126754e98f59` |
| `/mnt/app/armle/lib/dll/mss-ipodpb.so` | 83449 | `e8514a9cee676c6dc818f9d7af90bdd2c97288ca13977b1204b5fa2f2bfc6137` |
| `/mnt/app/armle/lib/dll/mss-ipoduid.so` | 82157 | `e3e090878057c4c2b26643fc46d405417c640f38a2591e18863fccef891bc9a4` |
| `/mnt/app/armle/lib/dll/pubs/pp-ipod.so` | 11249 | `888f33efbb313b65307e766b4ca5a729eba16a645f10fe6efcd5eb8be284e14c` |
| `/mnt/app/armle/lib/libnvss_video.so` | 73304 | `99be82415f51c7139a93a4c4081cffb4d3937826d41a669e1b505ba6760e234d` |
| `/mnt/app/armle/usr/bin/iap2cli` | 29969 | `51f4838ce87d89f73bfdca4de92e12dab6c72a0e6a8112cb61aab8757476e4f6` |
| `/mnt/app/armle/usr/bin/ipodcli` | 20727 | `300f8bbeef2da3c0a18f9a3d88ff3d3f83b765928843f1457cffb87151b4287a` |
| `/mnt/app/armle/usr/lib/libiap2client.so.1` | 18613 | `33efe4fe95f07e1d3d5abb6a39edfb300430e417f30eb61b1600eaaaded9f31d` |
| `/mnt/app/armle/usr/lib/libipod.so.1` | 38355 | `44e48861dbb493d476fa77934f6bc57e8f2aa15ee4e99f0d047532736292d38c` |
| `/mnt/app/armle/usr/sbin/mm-ipod` | 40297 | `57ef5e7010f2a8b574ad8b681f05004e1b337e67c4471da4c6a7153db0bcfb4f` |
| `/mnt/app/eso/bin/apps/airplayutil` | 338468 | `fcc2a21b3c2299a9ac266b25b4119eda27e2d5e66925c9cb7319b28bb481084c` |
| `/mnt/app/eso/bin/apps/dio_manager` | 657337 | `bd42c9ee43021eca4db1dbd42550d86f7c7df60d350063564e985667a63bfe70` |
| `/mnt/app/eso/bin/apps/dmdt` | 11880 | `4e62b48055fabceb38d4693454bc7e806abc1602347b22104b1593cf4c99738b` |
| `/mnt/app/eso/bin/apps/iap` | 89807 | `77cdc9b936a8dfa8fa18cf4bce8d8defab95ab10b0c83a833a834d1dadd92102` |
| `/mnt/app/eso/bin/apps/smartphone_integrator` | 808938 | `dabdac3fb3625547b4aec91abdc3c217ab369edf7779c72c7823759fcae8ccf7` |
| `/mnt/app/eso/bin/apps/videoovermost` | 131424 | `4f29d43222ad0218dcaaa50698deb3ae90f9e841f95897cdb3c055b1e9d7dc8c` |
| `/mnt/app/eso/bin/apps/videoovermost.sh` | 363 | `a8f8ab7d3473356b3e5fb2197c5d9ddc2a744eb0e1f85e72034f05bd760c8fc3` |
| `/mnt/app/eso/bin/java.sh` | 2645 | `147f3ea49555b8d4ef76b12994c7ca684dfa172cc72b0493ff9ff6c783199510` |
| `/mnt/app/eso/bin/runHMI.sh` | 1360 | `32aa6803754defbdd3cb9b9ca65bda7e31aebe62c69b368dfd71959913d689f7` |
| `/mnt/app/eso/bin/runLSD.sh` | 2103 | `694d4c576e92b42473b8bcbcdbbfa6e0097f30bbd7f414759de88d60459701d3` |
| `/mnt/app/eso/bin/runMMX.sh` | 2454 | `0d6fb4034f8f5f0973dec4771cf12b89e078453409a94d2a4d84b4dcb79c67c3` |
| `/mnt/app/eso/hmi/engdefs/MostInfo_prod.esd` | 560 | `340de6dc7040e7c894473b553d12abd67ed77cd7793fcbbd75033a71532f5ade` |
| `/mnt/app/eso/hmi/engdefs/scripts/SCALE/MOST_activate.sh` | 195 | `1bdbbbd288991fb4e7e646bf96131fa95e4eeeb328a39c6713bad62afb10d01c` |
| `/mnt/app/eso/hmi/engdefs/scripts/SCALE/MOST_deactivate.sh` | 199 | `a34a10e94960ceb7944482ef149f76456e4ba87aa45bffa22f2e085b9868fe3e` |
| `/mnt/app/eso/hmi/engdefs/scripts/SCALE/clustermap_activate.sh` | 503 | `6d6e708cfb9f44f0c3f156d60926391e98b98979ef74a18f50451201f8d54d88` |
| `/mnt/app/eso/hmi/engdefs/scripts/SCALE/clustermap_deactivate.sh` | 505 | `6cada69561cc31e88a00956e5c0894d9457bb891ad4dd41e6c7af37b7466ff69` |
| `/mnt/app/eso/hmi/engdefs/scripts/activateDSITracer.sh` | 836 | `f185baface1bd3e797708481c49aa1b262978894e4f641753e4a726222199f50` |
| `/mnt/app/eso/hmi/engdefs/scripts/activateDisplayManagerSplitscreen.sh` | 156 | `252b8cf0fafe6674175fdc8f393c8a1a574cc7ba81732e521bfce003e0c85c4f` |
| `/mnt/app/eso/hmi/engdefs/scripts/activate_CarPlay.sh` | 1416 | `d946426a4be43c9c29f5cc36b547b52453d8ccb85d9f8d1195ebb33e68693321` |
| `/mnt/app/eso/hmi/engdefs/scripts/activate_ipod_logging.sh` | 156 | `70d74d0d01c16c2e8ddb4c7f6311a1adc7f5bb0c23c28a779bac4786087543e8` |
| `/mnt/app/eso/hmi/engdefs/scripts/check_ipod_logging.sh` | 145 | `23504e297e5751be3bdee1cd0a08a5e986dfffd1bd77f5e484b4f83064ad8ba9` |
| `/mnt/app/eso/hmi/engdefs/scripts/clusterCodingLVDS.sh` | 501 | `3a51c60bd7da1e91f2231103a6ce69345f407c29cc0515149498607dfed0bd07` |
| `/mnt/app/eso/hmi/engdefs/scripts/clusterCodingMMI.sh` | 516 | `345088758499d5a87c2a124332b519e21f46438e31cc8846d270b1943f8da8a0` |
| `/mnt/app/eso/hmi/engdefs/scripts/clusterCodingMOST.sh` | 501 | `a642ccbb4c8ea497778830e37309c7c4242759c14210656a073ab0feabb31712` |
| `/mnt/app/eso/hmi/engdefs/scripts/clusterCodingOFF.sh` | 500 | `8acd74282b9be6b307a19045160a47a7b091cca40fa6b7d37190878defc57365` |
| `/mnt/app/eso/hmi/engdefs/scripts/connectedNavImportOtaList.sh` | 293 | `90eee75349979abba1c2be5b81c63e885620c1dbb3841f0ff5b04ed51276c299` |
| `/mnt/app/eso/hmi/engdefs/scripts/deactivateDSITracer.sh` | 842 | `95704a3aa3eeefbd9225840e59576bef293421b2e80991325165deba04c7b1dc` |
| `/mnt/app/eso/hmi/engdefs/scripts/deactivateDisplayManagerSplitscreen.sh` | 159 | `3a2ec09233a302b398977b21fa32ad190522063402c1ae976e96e77cf226acd0` |
| `/mnt/app/eso/hmi/engdefs/scripts/deactivate_CarPlay.sh` | 1424 | `9e7eab1f7cbb24bcb6ff1182161b51e490bc30ed02f7b82415e108983d4df158` |
| `/mnt/app/eso/hmi/engdefs/scripts/deactivate_ipod_logging.sh` | 157 | `ac842c5b1f82cf19a63448b8b9c0e95ebe9cdb294d361838c4d6dce55dcfdcfc` |
| `/mnt/app/eso/hmi/engdefs/scripts/displayManagerSplitscreen.sh` | 165 | `28c1adca4b6f09cfe58408202c4f946ef2a81a4b4fdff68449bb94ccaf9f8ab8` |
| `/mnt/app/eso/hmi/engdefs/scripts/enableKombiMapMOST.sh` | 453 | `3c4dae272caee8653fc92367be441d4670d1b92e6fd26447d6adce73d0a6fee3` |
| `/mnt/app/eso/hmi/engdefs/scripts/naviAppDisable.sh` | 235 | `f4ddfe4078800ad5c09e859904388ce96809f6c860a51b94c3fbb8e8515b975c` |
| `/mnt/app/eso/hmi/engdefs/scripts/naviAppEnable.sh` | 234 | `138037fb56657a09aece3bf839443b413ee6132b8d727ba003e52ce1662750d8` |
| `/mnt/app/eso/hmi/engdefs/scripts/naviAppRestoreDefault.sh` | 255 | `56bfa526cff4e968df40c09b13678560bb5a6c7e24ecf8e96384b70093ddd8fe` |
| `/mnt/app/eso/hmi/engdefs/scripts/showKombiMapMode.sh` | 336 | `7a1b62ef12d3f5a408b9c4bc11acb085f34733d46dd3102f33672573d15621b3` |
| `/mnt/app/eso/hmi/engdefs/scripts/showKombiMapModeAlwaysOn.sh` | 156 | `65401e3a81a3694050ab3a607c025edd9381eff92d31be5431ee39d78532d67e` |
| `/mnt/app/eso/hmi/lsd/DSITracer-clientData.bin` | 786069 | `23afd1c4c8a606a976c907c9c3d635b26da3122de688ff79b2d4306eb7794ad1` |
| `/mnt/app/eso/hmi/lsd/DSITracer.jar` | 1161684 | `570e51da6ecbd166de909beeefa2c722a27d9605fec7f73c08cc5762230636b1` |
| `/mnt/app/eso/hmi/lsd/bundles.properties` | 10034 | `53d16655998f931f43299989e703c13f9d99e674c2db64aaf1bce5c451d7e993` |
| `/mnt/app/eso/hmi/lsd/bundles_dsitracer_disabled.properties` | 10034 | `53d16655998f931f43299989e703c13f9d99e674c2db64aaf1bce5c451d7e993` |
| `/mnt/app/eso/hmi/lsd/bundles_dsitracer_enabled.properties` | 10043 | `0b8694a9bda79354d8b02073f7e130cc199155fe3dc549590c857a460be0da34` |
| `/mnt/app/eso/hmi/lsd/bundles_headless.properties` | 10066 | `fb915febe816517959608fb1d0581fa38c6f256a0dc2f3bbb5cbdd88265f67f9` |
| `/mnt/app/eso/hmi/lsd/jars/AppDevelopment.jar` | 245793 | `75a2a1f62ac6bdd52461b4bc94cae9f067b205b00862ed89c5664dbb5510b326` |
| `/mnt/app/eso/hmi/lsd/jars/DevelopmentStubEvo.jar` | 23585 | `2bc60da51df94ff291b5cfc74a8662ea047f3912a11cd219ca86904508b20bff` |
| `/mnt/app/eso/hmi/lsd/jars/NavActiveIgnore.jar` | 2645 | `c962834de249459209f7c1aa4210f6b4179e563bad84e39e1312e4848278fd46` |
| `/mnt/app/eso/hmi/lsd/jars/RemoteHMI-Libs-Ext.jar` | 164348 | `ca38dc2fbb90f6809e8eb6a8c9111ed0fb66f3eebeb2a349af745b783a4ef1e7` |
| `/mnt/app/eso/hmi/lsd/jars/diag.jar` | 485347 | `836610ea26bb9a64cbf6bb33350a3de74e2eaa787f36e143bdc6b3d430b6f863` |
| `/mnt/app/eso/hmi/lsd/jars/lang_data.zip` | 2901025 | `82558f1e6808f9f27a6c1f0102f8421c9aac86c9498a928b7d76428de54991cf` |
| `/mnt/app/eso/hmi/lsd/jars/locale.zip` | 2205704 | `cbbf5ab240e7e6dec765058212d85cbbc997d53db6e83e60e39112b6df07d440` |
| `/mnt/app/eso/hmi/lsd/jars/xEventRecorder.jar` | 30418 | `e0de22cf6aca9fbb4e1ac5cd28adaa2817cb402ac0ff5e53337694d45861f173` |
| `/mnt/app/eso/hmi/lsd/jars/xEventRecorderEvo.jar` | 4005 | `3e4d0d1c936926f908cb14bebf6655d500885ba95b8095c19d6dc4b8d9b250b3` |
| `/mnt/app/eso/hmi/lsd/jars/xTcpKeyPanel.jar` | 16749 | `b66ecf519cafacd45a4f6c7e46a664fbe6643ad94174309d71a7d95850804011` |
| `/mnt/app/eso/hmi/lsd/jars/xTcpLog.jar` | 44082 | `ba0f4a75ecfc3505eed6f40d4a0ec9bc40a64321143af5e45a3646271db8c22b` |
| `/mnt/app/eso/hmi/lsd/lsd.sh` | 15079 | `a21649b32c615366fe36a193fb9a9d82637f8deb346a853c0cefd9fdcec32f2d` |
| `/mnt/app/eso/hmi/lsd/traceConfig.properties` | 19489 | `6782b9a7f7ad2ac4667ee6651f7af5794629a0ae3ed163f4ac6d5edab801c7b7` |
| `/mnt/app/eso/lib/factories/libasi_dsi_spy_carkombiproxy.so` | 102408 | `41978d0c40924b6c02a1033d7dbc7fa12418140e7a3ebc89d1f16454a9c7353e` |
| `/mnt/app/eso/lib/factories/libasimmxDVDCMostproxy.so` | 144152 | `52c74665f39f17624038537ced1d19588b00fff713e67c3acc65b6b38dda7850` |
| `/mnt/app/eso/lib/factories/libasimmxDiagDisplayManagerTypesproxy.so` | 3492 | `7c13beb9416c02ef3774c7a5cb1f7793a77ad286097ea416eba086e0e720becb` |
| `/mnt/app/eso/lib/factories/libasimmxconnectivity_bluetooth_iapproxy.so` | 48212 | `d30dab27849053e0901a580b77abf10209db9e7b8c270bb4474ef83863e9f341` |
| `/mnt/app/eso/lib/factories/libasimmxdsssclusterwarningproxy.so` | 33836 | `174dfedaea834767ed017813aafecf5bfc249a3051a3ab4fdfbeed9bce6d1595` |
| `/mnt/app/eso/lib/factories/libasimmxmapLicenceClientproxy.so` | 41524 | `6d4f777819b2986f327286748935f4e5bf40a6a74295073da157beb44853cdf4` |
| `/mnt/app/eso/lib/factories/libasimmxmapdataproxy.so` | 85720 | `854016d5ae9273e7087f35651ee325ca021f2f921941d31b12765e13ce1acc05` |
| `/mnt/app/eso/lib/factories/libasimmxmapregioninfoproxy.so` | 54416 | `175aac56e08a675359c8d9eb41a8865624a0c0f2e9b305be4867960015c5a3b4` |
| `/mnt/app/eso/lib/factories/libasimmxrdvmapviewerproxy.so` | 94400 | `0ab8eebc8864ecbee73156791f15b20c634242fa3a107e6f9e00993ce2051c14` |
| `/mnt/app/eso/lib/factories/libasimmxsmartphonemediaproxy.so` | 42780 | `8c1dc354a265c658000aa332966bc57caff81dac63e5934d7aeb35c20ad28af1` |
| `/mnt/app/eso/lib/factories/libdsicarkombiproxy.so` | 318532 | `046dc692169ddd6f428a9512281ed6f9abad14e973327e601253c7de64b3a922` |
| `/mnt/app/eso/lib/factories/libdsicarparkingsystemproxy.so` | 260512 | `53b7ad927dde5bd6e6dffe480be65d076d74de13957f06970989b708224c59f7` |
| `/mnt/app/eso/lib/factories/libdsicarplayproxy.so` | 73240 | `08315fa3c7768f68cb3cbab15197c614f307bbc67985d7bd6350891055a1a3ad` |
| `/mnt/app/eso/lib/factories/libdsikeypanelproxy.so` | 56628 | `0f2e9e0100380f4b3cd87ce01e68b67f6f421ef448c8cde8cd5a47c937127b2c` |
| `/mnt/app/eso/lib/factories/libdsikombisync2proxy.so` | 61936 | `a1924c4c66769c22e7c63c6106a96f8f106258b78ceb4ffcd2b03d928a5d9c0b` |
| `/mnt/app/eso/lib/factories/libdsikomogfxstreamsinkproxy.so` | 40080 | `63af5b81205533342da16f9deea1e1b4748ede9f36983fe71767a00f2681e72c` |
| `/mnt/app/eso/lib/factories/libdsikomonavinfoproxy.so` | 49200 | `aee60313a4c9378eb707462109c6192c0e01989d2dc5f997f79516b31a13270b` |
| `/mnt/app/eso/lib/factories/libdsikomoviewproxy.so` | 47628 | `ae0291ab4a3a797f40f4fb33fd2bfc40cae744208c6d4bf2746bba73edcf196a` |
| `/mnt/app/eso/lib/factories/libdsimapproxy.so` | 524416 | `997b9aa8cdea8b4add2b418bd5bf43a55ae08200956594f1c2a240ccccce3172` |
| `/mnt/app/eso/lib/factories/libdsimediaproxy.so` | 272012 | `8d66cfcfdd66946d7121c3aaa4809c36952a572ae6d8d2c93c544fa6b429d778` |
| `/mnt/app/eso/lib/libairplay.so` | 857735 | `34503a9f18799420005d7bd40c3cbf84634c8c9030bb0dcba79d519f0227e949` |
| `/mnt/app/eso/lib/libdmdt_core.so` | 37612 | `959b00174dc7e7679151e27b73381bdc1e0a292c167e94275197a07043327c5b` |
| `/mnt/app/speech/libDataMediaPluginAction.so` | 11568 | `8a77a48b0e5ed68c47f071aff48b0bed0e1d1c0e6ca5546b65af892387033ed6` |

### Konfiguracja z `advanced/MU1438-system` (= `/etc` MMX; kopia w `work/system/`)

| Ścieżka na jednostce | Rozmiar [B] | SHA-256 |
|---|---:|---|
| `/etc/166x76.ipod` | 25240 | `7d99a65cb5443dacb1311363f1a0c78364fc65dc499560b85a1195d3f90e7c18` |
| `/etc/boot/boot.sh` | 1146 | `2ffc5cb3099368f121d355f39d9d1693e2e2d963212b74f2d518b27fea607f7d` |
| `/etc/boot/startup.sh` | 29984 | `522945a08b36fa129d59f62af4014dafc0035d9fe8548f68e0229d374f6c79de` |
| `/etc/db/db_ipod_custom.sql` | 2892 | `9facc9278319969faa10941190db550f73684928f3298bb6955e06eee74bcd47` |
| `/etc/db/db_ipod_data.sql` | 417 | `40697ab6d56c06672928a4ffad10452d4b8a67c1a608e169ed41e6f8283ff925` |
| `/etc/db/db_ipod_schema.sql` | 10962 | `0850b78c6a200c9d834a774fe66b722f70c7862585b190e81ff1cb3e9858d1ac` |
| `/etc/db/mmsync_iap2.sql` | 7239 | `d8dacd3d9235963ccbdc67b8ad6c234ed3f2f74b2d8e77a70b23739169b2664f` |
| `/etc/db/mmsync_ipod.sql` | 3210 | `4a7856cce60095f43359ede9fd333d571693ac63899e22e5f9c2535a80f4220c` |
| `/etc/eso/media/carplay_oemicon.png` | 19255 | `c11025bb5f2a2cb09be2a8cf810122f6ba4194409327629ed687bf0ce105997e` |
| `/etc/eso/media/scripts/launch_ipod_detect_monitor.sh` | 55 | `9ab81893b2b4b5ccf52735ce5299c2e1d6caebf2d26b2ccc7b63315016cf34dd` |
| `/etc/eso/presets/cluster_hmi.json` | 3661 | `642902680f768db353e866d4aa829d214cba267e0224382ee0850bbbd8105c8c` |
| `/etc/eso/presets/displaymanager.json` | 2902 | `f97eb21c44e85184ffc05d26f068c2b298fbd1b7e15bbe6b414a9904402343b5` |
| `/etc/eso/presets/videoovermost.json` | 739 | `961264cca27ab2f707b82ff6614f3da8e893c3f1cc8496fbf5fb396d1e7229c2` |
| `/etc/eso/production/broker.json` | 67 | `dba8d3a3d222bcb522b2ad955d6f4b7da1b044a7954da17a035b813010f9df83` |
| `/etc/eso/production/comm_forwarder.json` | 3734 | `77c5004416a0b50dcbe0a9616555078b6352197bca5703478380ff8a57922794` |
| `/etc/eso/production/connectivity.json` | 8654 | `8d3f4ad033a8639d294df8477f82f693288010e1ad3712c3e25438f1504e1829` |
| `/etc/eso/production/dio_manager.json` | 9822 | `99a11a14efa4a02bc5811b5d29d0b336695089c06774ca4568bb7844f4688528` |
| `/etc/eso/production/displaymanager.json` | 6882 | `c2263eb38ff03dbd23d1c95ddf47e04762df97e40da1825f8677801cec24d644` |
| `/etc/eso/production/dsiadapter.json` | 214 | `3fe2124af3ded7cc53e8ca6cd3cd73d8b58e6b6c87b43b62621ad05ee15d14b3` |
| `/etc/eso/production/dsiagentmapping.json` | 22 | `973834a65bc82cdd3f8f313d15a8de3c2cdfc23a4658b785e81ba54b2581c7c3` |
| `/etc/eso/production/dsistartup.json` | 18717 | `3d5e8934b8f01a97edadc2f413df6ec33e52b5939eac2da2ae57214965764f98` |
| `/etc/eso/production/framework.json` | 24911 | `e7ac668f62e66f48b3df2462384431c1cbb59e373aed80dfe8401613e7fbf1d1` |
| `/etc/eso/production/framework_tcpip.json` | 19685 | `a7dc80c03bca4afe7fc4b102fde92b6999e0d8910583cbb64a2746903aade86c` |
| `/etc/eso/production/fwservices.json` | 5965 | `068fa7a0b0b110b5a2afc945b9b30ae26b12556ea74e92ec8d4b3d88a02da085` |
| `/etc/eso/production/gal.json` | 17262 | `bd1babf5395e59f1ff2dbde7b68e822f59b381ee665dc1fc53d7caa6e2a98f13` |
| `/etc/eso/production/imageserver.json` | 126 | `84ddb157d895530b0d08179866d83fe1dc54e28573ba6c2b9379352a3cc8f456` |
| `/etc/eso/production/media-versions.json` | 489 | `ab048cb0922c116da68d429879b2b63573dc9a40215d20428927312cad9e8104` |
| `/etc/eso/production/media.json` | 14886 | `50c67b33a2b41076a382824bc8add9dbc9240b1679f154f358264968fac05b5a` |
| `/etc/eso/production/medialauncher.json` | 11832 | `612261e6d453aed6879e7a04a3f51c9d00d973f14f021c5361325a125324b72e` |
| `/etc/eso/production/mediaonline.json` | 3571 | `2d741a185b6b1bba9f5c2a7140db47cfaf94eeae18f6be3bdc9c947f7f5d4d3c` |
| `/etc/eso/production/mirrorlink.json` | 4720 | `b1b9ecd261efc9fe1dc274d19a8c44b941473f7de860144c6d2c4627d6e59336` |
| `/etc/eso/production/persistence.json` | 3287 | `abe346795b2facf27cdb9b9ef7cdb25512d105b5777f04cf7d50264b76899f68` |
| `/etc/eso/production/pictureviewer.json` | 679 | `a3e6dbdf0ba33fed2ca4fb199d61b1f3555048e3f7ea9364798d21a4c7a88399` |
| `/etc/eso/production/probes.json` | 38 | `83267937dd17d792655ab77d3048eaf47cfe6abbdec46b8c81b71168309e15ca` |
| `/etc/eso/production/rdiserver.json` | 1382 | `bf8ab022228ac28d6ac1e2c838f1f9f5928639f45fe6a0d9b0d636120034d0fa` |
| `/etc/eso/production/rdiserver_exlap_plugin.json` | 879 | `6ae11075b93902b570aeac3dbfc69a5764f931f7c95fefd144fc379685cdd399` |
| `/etc/eso/production/rdiserver_sdis_plugin.json` | 285 | `8e5623e4f8ce503461c521fee988dd364194947c074e7ef871d0b5548f74abc2` |
| `/etc/eso/production/servicemgr.json` | 446 | `5ba7df3a1921cde141b2569d8354936aedf2b205a0b3f8b057ac6e3c6d05951a` |
| `/etc/eso/production/servicemgr_restart.json` | 1601 | `9181e2cb6777cfa74b55ab55d8ee27e1e67e1b9193f63dd03008021e01910293` |
| `/etc/eso/production/servicemgr_usercommands.json` | 578 | `505162ca67b2788e30f884ffd7f14e9cd9d469e79b78bf9915a542c9c790df33` |
| `/etc/eso/production/smartphone-versions.json` | 628 | `4cccdbd07e2b7bd3965e2f597288d7092ca376ab607066aebf9c5110f61c7d5c` |
| `/etc/eso/production/smartphone_integrator.json` | 6788 | `6e7968e0279a2548c50285923e5bbc66f8775ad490c2394e430318d570faeed9` |
| `/etc/eso/production/tracing.json` | 65344 | `cc0fc498f49a383aae23d30cbff8ba863dc289d273e7995c50441e4ee85cc8c4` |
| `/etc/eso/production/tracing.swdl.json` | 5909 | `2a3f217dcd55151e153763aa2e49447c5b39bf00e36efb448c6ce72e3d4905bb` |
| `/etc/eso/production/tracing.ulp.json` | 2546 | `c95a3ad932e620f11152d9edf48b1e34b56eafd356e77545e910d9aae19564d7` |
| `/etc/eso/production/videoovermost.json` | 1206 | `96b3dfb9110e13356195fb0b6f16c4a0c8e7e2bc4c09b7a249692eb32347417b` |
| `/etc/eso/scopes/combicluster.json` | 2479 | `1155b7e2f2c9d98187e6d9700beaac9a9e5148a4b9c40b3b80415f9dd25aa6bc` |
| `/etc/ipod.cfg` | 7955 | `26545b15422ce3215363f0f665f03c575d9cf0a3fbb50b0c171f0fc95d544174` |
| `/etc/mm/iap2.cfg` | 5852 | `313e1e0e9ee37a2dc4e3d6142cc4fff2c74737a2836536afabfca097c0ea7393` |
| `/etc/scripts/carplay_cleanup.sh` | 620 | `8cd371a4273da462ca83ae0670cef9565912afd7a9beaea294a498c45095c13a` |
| `/etc/scripts/start_otg_stack_device_mode.sh` | 95 | `e7c5b281e93d780b0b8462fc8b6d0da57163c8b80613fb32a4ea09a7b160c001` |
| `/etc/scripts/start_otg_stack_host_mode.sh` | 87 | `ec0d7749171bed201e6eaaef8dc96a76376c901247331c6dd66deb5a855b5f88` |
| `/etc/scripts/startncm.sh` | 222 | `8484d8b148ae2109e0fae4b567eff44caf6ad7aab583d4339ef4b0b36c0cce35` |
| `/etc/scripts/stop_otg_stack.sh` | 82 | `8267ccbbdd16e6ca420695a4a0a69452229d5cb2504826b938365592578102e2` |
| `/etc/scripts/stopncm.sh` | 185 | `3ef997ef5ce3f558d70a378907def44d5c9c7fc106ddfd13837e5c48cc9f2e76` |
| `/etc/usblauncher.lua` | 21284 | `315fc261fe7f01d7b08b78669803f181a785868844b3202fb0b429aebaacf359` |
| `/etc/usblauncher_carplay_descriptor.lua` | 5329 | `21cdf5d86c3b27324230820985ac93399cd80788099db938714edb77e825a3c4` |
| `/etc/usblauncher_otg.lua` | 19907 | `69e88b54b8eb450a659fe21fb1ec05258f0fdf7ee9e554889669faede2f1e4e2` |

