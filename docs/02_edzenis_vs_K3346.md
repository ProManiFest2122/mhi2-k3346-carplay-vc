# 02 – edzenis (K2161 / MU1421) a moje K3346 / MU1438

Repozytorium: `repos\mib2high-carplay-altscreen-rgi` (commit `bfea868`, 2026-10-14).
Przeczytane: README, NOTICE, CHANGELOG, `altscreen\README*.md`, `BUILDING.md`, `RELEASE_NOTES.md`, `docs\*`
(architecture, implementation, installation, build, validation, compatibility, ALTSCREEN_V1_0), `collector\*`,
całe źródła natywne (`native\`, `altscreen\native\`), Java (`java\src`, `altscreen\java\build\src`), instalator
(`altscreen\deployment\*`, `altscreen\config\*`) i renderer.

## Podsumowanie

| Obszar | Ocena dla K3346 |
|---|---|
| AltScreen: hooki w `libairplay.so` / NvSS (`dio_manager`) | **Prawdopodobnie zadziała bez zmian w kodzie.** Wszystkie symbole istnieją, wywołania idą przez PLT, a obie bramki (`cfg[10]==59`, adres powrotu AES) są spełnione statycznie |
| AltScreen: renderer `vc_render`, `dmdt`, konteksty 72/77, warstwa 58 | **Zgodne z topologią B9 (LVDS).** Wymaga testu w aucie: czy `dmdt sc/dc` działa i jaki jest aktywny KombiType/SysConst 541 |
| AltScreen: geometria (1440×542, viewareas Q7, „physical 200 75”) | **Wymaga adaptacji**: VC B9 ma 1440×540, układy B9 (classic, infotainment, ew. Sport) trzeba zmierzyć |
| AltScreen: wykrywanie VIEW (DSIBAP 50/54 przez DSITracer) i zoom (`updateEncoder2`) | **Niewiadoma.** Mechanizm (porty 1798/20991) istnieje, ale wartości BAP zależą od kombi AU491, trzeba zrobić trace w aucie |
| RGI natywne (`ipod-drvr-iap2.so`, 20 twardych offsetów) | **Wymaga adaptacji.** Inny build sterownika. Wszystkie offsety udało się jednoznacznie przemapować statycznie (tabela niżej). Kod sam się wyłączy („fail closed”), więc nie grozi crashem |
| RGI: preload do `mm-ipod` | **Brakujący element.** Instalator AltScreen v1.0 preloaduje tylko `dio_manager`, a launchera `mm-ipod` nie ma w publicznym repo |
| Java (`CombiBAPServiceNavi` / BAPBridge) | **Mała adaptacja**: zmiana pakietu `bap` na `bap.navi`, 4 usunięte metody, 1 nowa. Pozostałe 9 błędów kompilacji opisane niżej |
| Instalator AWK | Działa na K3346 (rc=0). Sprostowanie (2026-10-20): plik ma CRLF, ale akurat linia `Bundle.Startup.Autostart` kończy się samym LF, więc `bundles.awk` dopisuje bundle poprawnie (sprawdzone na bajtach i awk w WSL) |
| Collector v1.2 | **Tylko do odczytu firmware'u** (przegląd linia po linii niżej). Uwzględnia braki `rmdir`/`wc` zgłoszone przez użytkownika z A4 B9 K3346 |

**Werdykt: port edzenis na K3346 jest realny.** Najtrudniejsza część, czyli natywny RGI z offsetami, okazała się
mechaniczna, bo sterownik K3346 ma pełną tablicę symboli. Najwięcej niewiadomych dotyczy runtime w aucie
(VIEW/BAP, kontekst kombi, geometria), a nie kodu.

---

## A. Natywny RGI: `ipod-drvr-iap2.so`

K3346: `/mnt/app/armle/lib/dll/ipod-drvr-iap2.so`, 739 925 B, SHA-256
`0ac13f43d3fb259eee328c272fa1211bb558b6e2cd97c6e38ef38e4e71469d51`. Plik ma `.symtab` z symbolami lokalnymi
(530 symboli), build-id i sekcję debuglink.

Kod edzenis (`observer.c`) wylicza bazę sterownika z dwóch eksportów (`ipod_module − 0x2614c` oraz
`packet_addparam_blob − 0x1d0d8`) i porównuje oba wyniki. Na K3346 wyniki się rozjadą, kod zapisze
`base_crosscheck_failed` i **żaden hook się nie zainstaluje** (zachowanie fail-closed, potwierdzone czytaniem kodu).

### Przemapowanie offsetów (wynik `tools\derive_k3346_iap2_offsets.py`, pełny wydruk: `work\k3346_iap2_offsets.txt`)

| Stała w kodzie edzenis | K2161 | **K3346** | Dowód / walidacja |
|---|---|---|---|
| `ipod_module` (observer.c) | 0x2614c | **0x27e7c** | eksport `.dynsym` OBJECT |
| `packet_addparam_blob` (observer.c, ident_hook.c) | 0x1d0d8 | **0x1e2c4** | eksport FUNC |
| `IAP2_MSG_OFFSET` | 0x0e484 | **0x0f4c8** | symbol `iap2_msg` |
| `IAP2_CLOSE_OFFSET` | 0x0f760 | **0x107b4** | symbol `iap2_ocb_close` |
| `LINK_SEND_MESSAGE_OFFSET` | 0x10530 | **0x1165c** | symbol `link_send_message` |
| `ADDPARAM_NODATA_OFFSET` | 0x1cf50 | **0x1e13c** | symbol `packet_addparam_nodata` |
| `ADDPARAM_NUM_OFFSET` | 0x1d210 | **0x1e3fc** | symbol `packet_addparam_num` |
| `SENDLIST_PUSH_OFFSET` | 0x1dc14 | **0x1ee00** | symbol `packet_sendlist_push` |
| `SENDLIST_POP_OFFSET` | 0x1dfac | **0x1f198** | symbol `packet_sendlist_pop` |
| `IAP2_MSG_SLOT_OFFSET` | 0xa7dac | **0xa9bd4** | jedyne słowo w `.data` równe `iap2_msg` (R_ARM_RELATIVE), w obiekcie `iap2_drvr` |
| `IAP2_CLOSE_SLOT_OFFSET` | 0xa7dc0 | **0xa9be8** | jak wyżej dla `iap2_ocb_close`. Odstęp 0x14 taki sam jak w K2161 |
| `K2161_IDENT_ID_OFFSET` (wartość 0x0016) | 0xa7d80 | **0xa9ba0** | `ident_info_funcs[22]`, id = 0x16 (LocationInformationComponent) |
| `K2161_IDENT_BUILDER_OFFSET` | 0xa7d84 | **0xa9ba4** | slot → `ident_info_gpscomp` |
| `K2161_GPS_BUILDER_OFFSET` | 0x0a11c | **0x0cb70** | `ident_info_gpscomp` |
| `K2161_CTRL_MSG_TABLE_OFFSET` | 0xab050 | **0xace80** | symbol `iap2_ctrl_msg_table`, 3072 B = 256 grup × 12 B (ten sam układ) |
| `GROUP50_NAME` / `MESSAGES` | 0x237dc / 0xac4dc | **0x2518c / 0xae30c** | `'Now Playing'`, count=4 (tak jak oczekuje kod) |
| `GROUP68_NAME` / `MESSAGES` | 0x237e8 / 0xac50c | **0x251bc / 0xae36c** | `'Human Interface Device'`, count=4 |
| `NP_START_NAME` / `PARAMS` | 0x23b80 / 0x26bd8 | **0x25554 / 0x28918** | `'StartNowPlayingUpdates'`, flags=1, params = `params_nowplay_start` |
| `NP_UPDATE_NAME` / `PARAMS` | 0x23b98 / 0x26bf8 | **0x2556c / 0x28938** | `'NowPlayingUpdate'`, flags=2, params = `params_nowplay_upd` |
| Grupa 0x52 (RGI) w stocku | 0,0,0 | **0,0,0** | pusta, tak jak oczekuje kod (aktywacja w pamięci jest możliwa) |
| Adres powrotu `memcpy` RX (observer.c) | 0x11428 | **0x125e8** (wysoka pewność) | patrz niżej |

Różnice strukturalne między buildami:
- K3346 ma dodatkowy wpis `ident_info_funcs` `0x18 → ident_info_tspwifi` (komponent transportu Wireless CarPlay).
  Dlatego tablica i sloty za nią są przesunięte o +8 B. To nowszy sterownik iAP2 niż w K2161.
- Adres powrotu `memcpy` w ścieżce RX: w `link_get_transport_data` K3346 są dwa wywołania kopiujące cały pakiet łącza
  z bufora transportu (`[r5+0x4c]`) do bufora pakietu (`[pkt+0x18]`), z długością zapisywaną do `[pkt+0x1c]`.
  Znane przesunięcia K2161→K3346 rosną monotonicznie: +0x1044 (`iap2_msg`) → +0x112c (`link_send_message`) →
  +0x11ec (`packet_*`). Dla 0x11428 delta musi leżeć w [+0x112c, +0x11ec]. Mieści się w tym tylko kandydat
  **0x125e8** (+0x11c0), a 0x123a8 (+0xf80) nie. Do potwierdzenia w aucie licznikiem `callsite_ready` w logu
  natywnym. Alternatywa: obserwować oba adresy (oba kopiują pełne pakiety).
- Niezweryfikowane statycznie (do sprawdzenia w aucie, kod fail-closed): sygnatury `iap2_msg(ctx, ctp, msg, …)`
  i `msgid` pod `msg+6`. Linia sterownika jest ta sama (te same nazwy funkcji i układ tablic), więc ryzyko jest niskie.

### Proces docelowy dla RGI: `mm-ipod`, nie `dio_manager`

- `mm-ipod` ładuje sterownik przez `dlopen("ipod-drvr-%s.so")`. W binarce są stringi `ipod-drvr-%s.so`,
  `device_drvr_init`, `dlopen`.
- `mm-ipod` uruchamia **`/etc/usblauncher_otg.lua`**:
  `mm-ipod -d iap2,config=/etc/mm/iap2.cfg,probe,ppsdir=/ramdisk/pps/iap2 … -l /dev/shmem/iap2.log -vvvvvv`.
- `dio_manager` (proces CarPlay) korzysta tylko z `/dev/ipod0` i PPS `/ramdisk/pps/iap2` przez `libiap2client.so.1`.
  Nie ładuje `ipod-drvr-iap2.so`.
- **Wniosek:** instalator AltScreen v1.0 dodaje `LD_PRELOAD` wyłącznie do `"carplay"` (`dio_manager`)
  w `smartphone_integrator.json`. Hooki AltScreen tam zadziałają, ale natywna część RGI (`observer.c`)
  w `dio_manager` nie znajdzie `ipod_module`. Na aucie edzenis RGI działało, bo był tam wcześniej zainstalowany
  launcher dla `mm-ipod` (kolektor logów kopiuje `/tmp/k2161-rgi-launcher.log`). Tego launchera **nie ma w publicznym repo**
  (`docs/installation.md`: „A launcher and automated installer are not included”). Na K3346 trzeba go napisać
  (wrapper `execv` ustawiający `LD_PRELOAD` tylko dla `mm-ipod`) i podpiąć w `usblauncher_otg.lua`.
  Precedens: Luka w swoim instalatorze modyfikuje `usblauncher_otg.lua`.
- `iap2.cfg` K3346 jest stockowy, bez sekcji `[ident] recvmsgs/sendmsgs` (tak jak zakłada edzenis). RGI v1.1 opisuje
  dopisanie tych wpisów. AltScreen v1.0 polega na aktywacji grupy 0x52 w pamięci. Na K3346 trzeba sprawdzić,
  czy sterownik bez wpisów w `iap2.cfg` deklaruje 0x5201/0x5202/0x5204 w Identification (`ident_info_recvmsgs`).

## B. AltScreen: hooki w `dio_manager` / `libairplay.so`

K3346 `libairplay.so` (857 735 B, `34503a9f…`) jest identyczny z referencją MU1438 z harman-f.

| Zależność edzenis | K3346 | Status |
|---|---|---|
| 10 funkcji przechwytywanych przez LD_PRELOAD: `AirPlayReceiverServerPlatformCopyProperty`, `…SessionPlatformCopyProperty`, `AirPlayCopyServerInfo`, `…Screen_StartSession`, `…Screen_SetTimeSynchronizer`, `…Screen_Delete`, `AirPlay_DeriveAESKeySHA512ForScreen`, `AES_CBCFrame_Init`, `…SessionTearDown`, `…SessionSetup` | wszystkie GLOBAL/DEFAULT, **wszystkie wywoływane wewnątrz biblioteki przez PLT** (`R_ARM_JUMP_SLOT`), brak `DT_SYMBOLIC` | OK, interpozycja możliwa |
| 6 funkcji NvSS (`NvSSVideoOpen/StreamConfigure/Get/SetAttribs/Decode/Close`) | w `libnvss_video.so`, importowane przez libairplay przez PLT | OK |
| 39 symboli przez `dlsym(RTLD_NEXT)` | 35/39 obecnych. Brak `kCFType*CallBacks` (3) i `CFDictionaryCreateMutableCopy`, ale edzenis ma na to fallbacki (`kCFL*CFLTypes` są obecne, `dict_mutable_copy_compat`) | OK, kod przewiduje ten przypadek |
| `NVSS_OUTPUT_CONFIG_OFFSET 10`, `DISPLAY_TARGET_MAIN 59` | `NvssVideoImpl::start` (0x7fd34): konfiguracja na stosie od `sp+16`, `strb 59 → sp+26` (= cfg[10]), uchwyt `this+0x50` | OK |
| Bramka AES: adres powrotu < `SetSecurityInfo + 0x80` | według harman-f powrót na +0x38 (MU1438) | OK |
| `/mnt/app/carplay_altscreen`, `/mnt/app/root/*`, `/tmp/mib2high-lvds-*` | `/mnt/app/root/` istnieje (`bin-target`, `lib-target`). Pliki tworzy instalator | OK |
| Port 19830 (hook → renderer) | brak konfliktu w configach | prawdopodobnie OK |

## C. Instalator AltScreen (`altscreen\deployment\custom.sh`, AWK)

| Element | K3346 | Status |
|---|---|---|
| Preflight: `/mnt/app/eso/hmi/lsd`, `/mnt/system/etc/eso/production`, `/mnt/app/root`, `DSITracer.jar`, `bundles_dsitracer_enabled.properties`, `traceConfig.properties`, `smartphone_integrator.json` | wszystkie istnieją | OK |
| `bundles.awk`: dokładnie 1 linia `Bundle.Startup.Autostart` | 1 linia (`DSITracer,FwUtilServices,Profiling,Tracing,ATIPImpl`), rc=0 | OK |
| CRLF w `bundles_dsitracer_enabled.properties` (263 linie CRLF) | **Sprostowanie:** linia Autostart ma samo LF (zapewne zmieniona przez skrypt budujący Audi), więc dopisek `,MIB2HighCarPlayRGI` jest poprawny. Wcześniejsza hipoteza błędu była fałszywa: narzędzia Git Bash (grep/gawk) po cichu usuwały `` | OK, bez zmian w `bundles.awk` |
| `trace.awk` | rc=0. Zmienia `use.appender=client` na `telnet,bluepirat` i odkomentowuje DSIBAP, DSICarKombi, DSIKeyPanel, DSIKombiSync, DSIKOMOView, DSIMapViewerControl, DSIMapViewerZoomEngine (=full) | OK |
| `smartphone.awk`: jedno `"carplay":` i `"envs":[` | dokładnie jedno, wstawia LD_PRELOAD do `dio_manager` | OK |
| `dsitracer_activated` | w stocku brak, instalator go tworzy, a `lsd.sh` wtedy dokłada `DSITracer.jar` do bootclasspath | OK |
| `NavActiveIgnore.jar` | **już jest** w `/mnt/app/eso/hmi/lsd/jars/` (10.07.2023), tak jak na aucie testowym edzenis („NAVIGNORE=UNTOUCHED”) | OK |
| Backup/rollback (`backup/pre-v8s`, `.mode`, `.absent`, `ROLLBACK`) | uniwersalny, bez zależności od K2161 | OK |
| Nazwy `mib2high-carplay-rgi-v8s-simple`, `K2161_*` | tylko nazewnictwo | zmienić na K3346, by nie mylić instalacji |
| Brak bramki wersji firmware'u w instalatorze | instalator sprawdza tylko obecność plików | **Do dodania** w naszej wersji (sprawdzenie `MU1438` / hashy), zgodnie z zasadą „nie uruchamiać na niezweryfikowanym” |

## D. Wyświetlacz, kombi, sterowanie (`mib2high-lvds-control.sh`, `altscreen_session.c`)

| Założenie K2161 | K3346 | Status |
|---|---|---|
| `/eso/bin/apps/dmdt` z komendami `dc` (declare context) i `sc` (switch context) | jest, identyczny z referencją. Usage: `dc cid did1…`, `sc display cid` | OK statycznie. harman-f: `gs/gc/gd` nie dawały wyniku na tym firmware → **test w aucie** (z `LD_LIBRARY_PATH=/eso/lib`) |
| Kontekst 72 = natywny, 77 = z displayable 58, display 4 = kombi, `dc 900 58` | statyczna analiza `lsd.jxe` MU1438 (harman-f) dla SysConst541==2: 72={33}, 77={20,102,101,58}, `DISPLAYID_CLUSTER=4` | **zgodne co do joty**. Przy KombiType==4 HMI używa kontekstów i+79, więc trzeba odczytać wartości w aucie |
| Renderer `--out 1 --layer 58` (wyjście 2. LVDS/HDMI0, warstwa 58 = `GOOGLE_EARTH_KOMBI_MAP_VIEW`) | `displaymanager.json`: tryb `2_lvds`, terminal `Tegra:HDMI0` → display 4 `2nd_lvds`, annotation. B9: mapa VC po LVDS J794→J285 (materiał szkoleniowy Audi wg harman-f) | OK koncepcyjnie |
| Canvas 1440×542, profil `1440 542 30 0 0 0 1440 542` | VC B9 Gen1 12.3" = **1440×540** (potwierdził Zykki, A4 B9 K3346, w issue #2 edzenis) | wymaga decyzji: zostawić 542 (jak edzenis) czy 540 → test |
| `viewareas`: `physical 200 75`, views 0–3 (Q7: small gauges leftAligned, big gauges topAligned) | B9: inne układy (`LayoutMIB2HighB9`, `LayoutMIB2HighB9Sport`), geometria KDK **zmierzona w aucie 2026-10-26**: **210×153 w obu kotwiczeniach**, pozycje (1091,110) i (1055,207) | **ZMIERZONE** — zrzuty `dmdt ts 4`, `docs\18`. Wcześniejsze „large … 328×180" było szacunkiem z innego auta i jest **nieprawdziwe dla K3346**: oba kotwiczenia mają ten sam rozmiar |
| VIEW: `2#org.dsi.ifc.bap.DSIBAPListener#indication#…` service 50, FctID 54, bit0 | DSITracer: telnet **1798**, bluepirat **20991** (identyczne porty). Wartości BAP zależą od kombi AU491 | **trace w aucie** |
| Zoom: `DSIKeyPanelListener#updateEncoder2#…|int:4|int:40|` | `DSIKeyPanel` istnieje. Kod enkodera/rolki może być inny | **trace w aucie** |
| `sloginfo` szuka `Change NavStateOfOperation to 5` | string jest w `/mnt/app/navigation/libPresentationController.so` | OK |
| Polecenia: `on -p`, `slay`, `pidin`, `sloginfo`, `sleep`, `grep`, `date`, `kill` (wbudowane w ksh) | obecne w `/bin` i `/usr/bin` MMX (`folders.txt`). `awk` = gawk z `/mnt/app/armle/usr/bin`. **Brak `rmdir`, `sha256sum`** | OK, skrypty nie mogą używać `rmdir` |

## E. Java (`java\src`, `altscreen\java\build\src`)

Test: kompilacja źródeł edzenis z `work\lsd.jar` (JDK 25, `--release 8`, test API, nie budowanie jara do auta).

1. Bez zmian: 58 błędów, bo w K3346 klasy BAP leżą w innym pakiecie:
   `de.audi.atip.interapp.combi.bap.CombiBAPServiceNavi` → **`…combi.bap.navi.CombiBAPServiceNavi`**,
   `…bap.data.*` → **`…bap.navi.data.*`** (tak samo jak w MHI2Q, czyli u Luki i yuedizhibo).
2. Po zmianie importów zostaje 9 błędów, wszystkie w interfejsie `CombiBAPServiceNavi`:
   - brak `updateRGStatusAndActiveRGType(int,int)` i `updateActiveRGTypeAndRGStatus(int,int)`: trzeba je zastąpić
     sekwencją `updateRGStatus` + `updateActiveRGType` (do sprawdzenia, w jakiej kolejności woła je stock K3346);
   - brak `responseLastDestinationsList` i `responseFavoriteDestinationsList` (usunąć z bramki);
   - nowa metoda `updateEtcStatus(EtcStatus)` (dodać przekazanie do oryginału);
   - `CombiBAPTMCInfoMessage` przeniesione do `…combi.bap.audio.data`.
3. Cele refleksji (kompilator ich nie sprawdza) istnieją w K3346: `ClusterViewMode.favoredViewMode`,
   `.favoredViewModeReceived`, `.combiService`, `.setFavoredViewMode(int)`; `TMInterAppHandler.deviceManager`;
   `ClusterService.getClusterViewMode()` / `setCombiBAPService(CombiBAPServiceNavi)`;
   `CombiService.updateHUDDisplayContent(boolean)`; `Navigation`, `BAPDistanceFormatter`, `DispatcherBase`.
4. Bundle: `org.mib2high.carplay.rgi.Activator`, rejestracja przez `bundles_dsitracer_enabled.properties`
   (mechanizm `lsd.sh` jest taki sam).

## F. Wymagania budowania (dla późniejszego etapu)

- Natywne: QNX SDP 6.5 ARMv7, GCC 4.9.4 (`qcc -Vgcc_ntoarmv7le`). Luka publikuje obraz Docker:
  `luka-dev/qnx65-armv7-toolchain`. **Nie mam toolchaina na tym PC**, to temat na osobną decyzję.
- Java: JDK 8 (javac `-source/-target 1.4`). Obecnie jest tylko JDK 25, który nie obsługuje targetu 1.4.
  jxe2jar zawiera Zulu JDK 8 w `repos\jxe2jar\jvms\`.
- JAR zależności LSD: **mam go już** (`work\lsd.jar`, 29 067 klas z mojego `lsd.jxe`). Nie publikować.

## G. Software Collector v1.2 (`collector\custom.sh`, 448 linii): przegląd linia po linii

Co robi (8 etapów):
1. **Test SD (l. 95–111):** szuka SD wyłącznie pod prefiksami z listy `removable()` (`/net/mmx/fs/sd*`, `/fs/sd*`,
   `/fs/usb*`, `/media/*`, `/mnt/sd*|usb*`). Plik testowy: zapis, odczyt, `mv`, `rm`, `sync`. Jeśli zapis się nie uda,
   robi `mount -uw` **tylko na tej karcie SD** (l. 85).
2. **Testy poleceń (l. 126–215):** sprawdza `cp awk find ls` (obowiązkowe) oraz `wc df mount uname`. Kalibruje
   `ls -l` na plikach 10 B i 31 B (dla jednostek bez `wc`, jak K3346 u Zykkiego), testuje `find -name -print`,
   `awk tolower/split`, `mkdir -p`, `cp`, `df -k`. Po testach usuwa wyłącznie pliki próbne z katalogu wyjściowego na SD.
3. **Informacje o systemie (l. 217–263):** `uname -a`, `mount`, parsowanie `df -k` tylko dla wybranej karty SD.
4. **Indeks ścieżek (l. 265–335):** `find <root> -print` po `/net/mmx/mnt/app`, `/net/mmx/mnt/system/etc`,
   `/net/mmx/eso`, `/net/mmx/ifs`, `/net/mmx/etc`, `/net/mmx/usr`, `/net/mmx/lib`, `/net/mmx/bin`, `/net/mmx/opt`.
   Korzenie SD/USB są wykluczone. Wybór plików odbywa się **po nazwach** (AWK), bez czytania zawartości.
5–6. **Kopiowanie (l. 337–389):** `cp źródło → SD/…/files/<ścieżka>.part`, weryfikacja długości, `mv` na SD.
   Rezerwa 512 MiB na SD, jeśli `df` jest rozpoznany.
7. **Weryfikacja (l. 391–417):** ponowny pomiar rozmiarów, podsumowanie na SD.
8. **Koniec (l. 419–448):** znacznik `COLLECTION_COMPLETE.txt`, dwa `sync`, komunikat „SAFE TO REMOVE”.

**Werdykt: względem firmware'u collector jest w 100% tylko do odczytu.** Jedyne operacje zapisu, `rm`, `mv`
i `mount -uw` dotyczą karty SD (katalog wyjściowy i pliki próbne w jej katalogu głównym). Collector nie restartuje,
nie zmienia kodowania i nie uruchamia usług. Uwagi:
- `MIB2_OUTPUT_ROOT` / `MIB2_SCAN_ROOTS` to zmienne nadpisujące. Domyślnie są puste, a wyjście i tak przechodzi
  przez `removable()`.
- `find` po `/net/mmx/mnt/app` to chwilowe obciążenie CPU/IO. Najlepiej uruchamiać na postoju, z zasilaniem zewnętrznym.
- Collector zbiera to, czego brakuje w moim backupie: pliki z **IFS MMX** (`/net/mmx/ifs`, `/net/mmx/etc`, w tym
  `displaymanager`) oraz `mount`/`uname`. **Nie zbiera** `pidin`, `sloginfo`, wartości persistence ani `dmdt`.
  Do tego potrzebny będzie osobny, również tylko-do-odczytu skrypt diagnostyczny (plan w 05).
- Zebrane pliki mogą zawierać dane jednostki. Nie publikować bez przejrzenia.
- Status w repo: „tested offline only; not yet tested on a physical MMI”.
- Pliki collectora w repo to `collector\custom.sh` (sha256 w `collector\SHA256SUMS.txt`: `b2f32076…`), a ZIP jest
  w GitHub Release `collector-v1.2`. Przed użyciem sprawdzić zgodność hasha pobranego pliku.

## H. Inne ustalenia z repo i issue

- Issue #2 w edzenis: **Zykki, Audi A4 B9 Avant 2016, MU1438 / K3346, HUD, VC Gen1 1440×540, dostęp SSH.**
  Ma ten sam firmware co ja. Jego jednostka nie ma `rmdir`, `wc`, `sha256sum`, `openssl`. Wspomina też emulator
  **iscle/qemu-mhi2** z roboczym profilem dla tego firmware'u (PR #4), który dochodzi do menu głównego. Może to
  umożliwić testy HMI/Java bez auta.
- Edzenis deklaruje, że wspiera tylko swój system. Port robimy sami, a jego repo traktujemy jako bazę źródłową.
- Licencje: renderer (Joey) i harman-f mają GPL-3.0. Repo Luki jest **dziś na GPL-3.0** (klasy OEM w `java_patch/de/`
  pozostają własnością Audi/Harman). Edzenis w NOTICE.md zastrzega, że nie nadaje licencji na pliki pochodzące od Luki
  w swoim repo. yuedizhibo: PolyForm Noncommercial + GPL-3.0. Dla prywatnego użytku nie ma problemu. Przy ewentualnej
  publikacji portu trzeba zachować nagłówki i GPL-3.0.
