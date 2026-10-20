# 03 – HMI Java (lsd.jxe) K3346

## 1. Konwersja

| | |
|---|---|
| Wejście | kopia `work\lsd.jxe` (z `MU1438-lsd.jxe`), SHA-256 `e43d80e7…b99ce`, czyli ten sam plik co referencja MU1438 u harman-f |
| Narzędzie | `repos\jxe2jar` (luka-dev, commit `9eeb45b`): `python repos/jxe2jar/src/jxe2jar.py work/lsd.jxe work/lsd.jar` |
| Wynik | `work\lsd.jar`, 132 MB, **29 067 klas**, SHA-256 `f1f1df90…bad6a3`. Czas ok. 1,5 min. Bez błędów |
| Zasoby niebędące klasami | `resources/atip.properties`, `bundles.properties`, `hmi_startup.json`, `fwservices.json`, `version.properties`, `libjprof.so` |
| Ograniczenia | J9 usuwa nazwy zmiennych lokalnych i numery linii, a stałe są wklejone jako literały. Do testów kompilacji i `javap` jar wystarcza w pełni. Do dekompilacji kolejne kroki jxe2jar (`combine.sh`, un-inliner, Vineflower) wymagają JDK 17+ i bash (opcjonalnie) |

`work\lsd.jar` to dane OEM, nie wolno go publikować. Nadaje się jako **JAR zależności LSD** do budowania Javy
edzenis/Luki pod K3346 (odpowiednik `lsd-K2161-base.jar`).

## 2. Pakiety i klasy według tematów

| Kategoria | Liczba klas (bez wewnętrznych) | Najważniejsze pakiety (liczba klas) |
|---|---:|---|
| CarPlay / TerminalMode | 382 | `de.audi.app.terminalmode` (25), `…terminalmode.statemachine.commands` (22), `…statemachine` (21), `…audio` (19), `…dsi` (15), `de.audi.atip.interapp.terminalmode` (14), `org.dsi.ifc.carplay` (13) |
| Kombi / BAP / Combi | 1474 | `de.vw.mib.bap.generated.*` (serializery BAP: telephone 91, navsd 82, audiosd 76…), `org.dsi.ifc.carkombi` (49), `de.audi.app.phone.core.bap.telephone` (41), `de.audi.atip.interapp.combi.bap.navi[.data]` |
| DisplayManagement | 67 | `de.audi.atip.displaymanager` (9), `de.audi.atip.interapp.displaymanager` (5), `de.audi.tghu.fwhmi` (4), `org.dsi.ifc.displaymanagement` (4) |
| Telefon | 1081 | `de.audi.app.phone.core.*` (audio 55, dsi.cmd 48, bap.telephone 41), `org.dsi.ifc.telephone` (33), `org.dsi.ifc.telephoneng` (28) |
| Parkowanie / PDC / kamera | 184 | `org.dsi.ifc.carparkingsystem` (48), `de.audi.app.earlyfunc.core.parking[.pla|.ops|.vps]`, `de.audi.app.earlyfunc.evo.parking` (5) |
| Media / okładki | 887 | `de.audi.app.media.*`, `org.dsi.ifc.media` (33), `de.audi.tghu.picturestore.*` |

### Klasy kluczowe dla projektu (wszystkie istnieją w K3346)

| Temat | Klasa (K3346) | Znaczenie |
|---|---|---|
| CarPlay | `org.dsi.ifc.carplay.DSICarplay` / `DSICarplayListener` | DSI do `dio_manager` |
| CarPlay | `de.audi.app.terminalmode.TMInterAppHandler` (pole `deviceManager`), `…device.IDeviceManager`, `TMDevice`, `IActiveDeviceStateListener` | cykl życia urządzenia (edzenis `CarPlayLifecycle`) |
| CarPlay | `de.audi.app.terminalmode.dsi.carplay.CarplayDSILifecycleController`, `…combi.TerminalModeBapCombi`, `…smartphone.carplay.CarPlaySpeechRequestHandler` | Luka: lifecycle, BAP kombi, Siri |
| CarPlay | `de.audi.app.terminalmode.interapp.HighPriorityResourceTracker` | **zabiera ekran CarPlay przy RVC i eCall** (sekcja 4) |
| Kombi / RGI | `de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi` (+ `…navi.data.*`) | wyjście manewrów / pasów / dystansów do VC i HUD |
| Kombi | `de.audi.tghu.navi.app.cluster.ClusterService`, `ClusterViewMode` (`favoredViewMode`, `combiService`), `BAPDistanceFormatter` | edzenis BAPBridge |
| Kombi | `de.audi.atip.interapp.combi.ddp2.CombiService` (`updateHUDDisplayContent`) | treść HUD |
| Kombi | `de.audi.tghu.combi.mapcontrol.HMIKombiMapControlActivator`, `de.esolutions.hmi.widgets.audi.evo.high.HMITerminalKombiMapControl`, `MapControllerEvoHigh`, `widgets.CombiMapController`, `LayoutMIB2HighB9`, `LayoutMIB2HighB9Sport` | mapa w VC, układy B9 |
| Kombi | `de.audi.atip.mmicombi.IViewSizeManager` | rozmiar widoku VC |
| DSI wejścia | `org.dsi.ifc.keypanel.DSIKeyPanelListener`, `org.dsi.ifc.bap.DSIBAPListener`, `org.dsi.ifc.komoview.DSIKOMOView`, `org.dsi.ifc.map.DSIMapViewerZoomEngine` | VIEW / rolka / zoom (trace edzenis) |
| Display | `de.audi.tghu.fwhmi.DisplayManagerMIB2High` | zarządzanie kontekstami (Luka nadpisuje) |
| Telefon na VC | `de.audi.app.combi.bap.app.phone.ServiceManagerPhone` | karta telefonu VC (Luka nadpisuje) |
| Okładki | `de.audi.app.combi.bap.app.audio.AppConnectorTerminalMode` | okładka CarPlay w VC (Luka nadpisuje) |
| Parkowanie | `de.audi.app.earlyfunc.core.parking.ParkingPartialPopupHandler`, `…evo.parking.ParkingSystemControllerComponentEvo`, `…ParkingSystemVPSComponentEvo`, `…core.parking.vps.AbstractParkingSystemVPSComponent`, `…pla.AbstractParkingSystemPLAComponent`, `de.esolutions.hmi.widgets.audi.evo.high.PartialPopupManagerEvoHigh` (dziedziczy po `…audi.base.PartialPopupManager`) | PDC / OPS / VPS (kamera) |
| Parkowanie DSI | `org.dsi.ifc.carparkingsystem.DSICarParkingSystem` | stałe `POPUP_OPS/VPS/VPSOPS/ARA…`, `VPSMODE_*` (identyczne nazwy jak w MHI2Q) |

## 3. Porównanie z klasami używanymi i nadpisywanymi w trzech projektach

Metoda: kompilacja źródeł każdego projektu z `work\lsd.jar` (`javac --release 8`). Daje twardą listę różnic API.
Wyniki i logi: `work\javacheck\`.

| Projekt | Pliki Java | Błędy kompilacji z K3346 | Wnioski |
|---|---:|---|---|
| **edzenis** (`java\src`, `altscreen\java\build\src`) | 10 / 12 | 58, po zmianie pakietu **9** | Interfejs `CombiBAPServiceNavi`: pakiet `bap` → `bap.navi`, brak `updateRGStatusAndActiveRGType`, `updateActiveRGTypeAndRGStatus`, `responseLast/FavoriteDestinationsList`, nowe `updateEtcStatus(EtcStatus)`, `CombiBAPTMCInfoMessage` w `…bap.audio.data`. Wszystkie cele refleksji istnieją |
| **Luka** (`java_patch`) | 60 | **129 w 12 plikach** (48 kompiluje się bez zmian) | 20 z 23 nadpisywanych klas OEM istnieje w K3346 pod tą samą nazwą (pozostałe 3 to jego nowe klasy). `PdcSmallStageGuard` i `OpsAudioDrawerPolicy` kompilują się **bez błędów**. Różnice: `PartialPopupManagerEvoHigh` (w K3346 inna klasa bazowa `PartialPopupManager`, brak pól `currentConnectedScreen`, `terminal`), `ParkingPartialPopupHandler` (brak `getParkingMutex()`), `CarplayDSILifecycleController` (inne sygnatury `IDSIResource`), `HighPriorityResourceTracker`/`ExternalEventsListener` (brak pakietów `atip.utils.reactive.properties` i `utils.dispatching`), `CombiMapController`, `KombiListenerBase` (brak nowszych typów `org.dsi.ifc.carkombi`: SIA*, HUDCollectiveTopics, DC*), `ClusterService` (inna sygnatura `formatDistance`), `GatedPhoneService` |
| **yuedizhibo** (`hmi\src`, `hmi\vendor\src`) | 48 | **102 w 10 plikach** | Stuby yuedizhibo (`hmi\stubs`) mają już pakiet `bap.navi` i `EtcStatus`, tak samo jak K3346. Różnice głównie w `NativeRouteStateJarPatch`, `RgiRefreshJarPatch` i telefonii |

Wniosek: **HMI K3346 (CLU6 KD4) to ta sama linia kodu „MIB2 High Evo” co MHI2Q Luki.** API kombi/BAP jest bliższe
MHI2Q niż K2161 edzenis. Klasy nadpisywane przez Lukę (podmiana klasy OEM w bootclasspath) **nie mogą być wgrane 1:1**,
bo każda zastępuje całą klasę OEM. Trzeba je odtworzyć z dekompilacji klasy K3346 i nanieść na nią zmiany Luki.
Zmiany niezależne od klas OEM (`com.luka.carplay.*`) w większości kompilują się bez poprawek.

## 4. Zachowanie stockowego HMI przy kamerze cofania (RVC)

Z bajtkodu `HighPriorityResourceTracker.processMsg(int)` (K3346):

```text
msg 108 (start parkowania/kamery), gdy aktywne urządzenie to CarPlay:
    model 3200046 := przełączony; model 3200049 := 3 (SCREEN_BLOCKED_CARPLAY_BY_RVC)
    rvcLock.acquire() → LockStateChangedListener → acquireHighPriorityResources(Resource.SCREEN)
msg 107 (koniec):
    rvcLock.release() → releaseHighPriorityResources(Resource.SCREEN); model 3200049 := 0
stałe: SCREEN_NOT_BLOCKED=0, …_CARPLAY_BY_ECALL=1, …_ANDROIDAUTO_BY_ECALL=2,
       …_CARPLAY_BY_RVC=3, …_ANDROIDAUTO_BY_RVC=4
```

- Stock **nie zrywa sesji CarPlay** przy wstecznym. Zabiera tylko zasób `SCREEN` (ekran główny) i oddaje go po
  komunikacie 107. Dzięki temu CarPlay wraca bez ponownego łączenia.
- Komunikat 108 wysyła też aplikacja parkowania dla zwykłego popupu PDC/OPS. Dlatego Luka przechwytuje go wcześniej
  (`ParkingSystemControllerComponentEvo` + `PdcSmallStageGuard`) i dla „czystego OPS” zostawia CarPlay na ekranie.
- Obraz RVC pochodzi z DisplayManagera (`displaymanager.json`, blok `rvc`, target 720×384 na ekranie **głównym**).
  Konfiguracja nie dotyka wyświetlacza kombi (id 4).
- Co to oznacza dla mapy na VC: ekran główny jest w tym czasie „pożyczony” przez auto, tak jak przy przełączeniu MMI
  na radio. Według edzenis AltScreen działa, gdy ekran główny pokazuje coś innego. **Hipoteza:** strumień 111 i mapa VC
  przetrwają wsteczny bez zmian. Wymaga to potwierdzenia w aucie (plan w 05).

## 5. Aktywacja mapy w kombi (HMIKombiMapControlActivator)

Bajtkod: decyzja zależy od `IFrameworkAccess.getKombiType()` i `getSysConst(541)`. Zgadza się to z harman-f:
KombiType==4 → terminal 0 / LVDS, SysConst541==2 → terminal 1 / LVDS, w przeciwnym razie H264/MOST.
Pliki-flagi `/mnt/app/eso/hmi/enableKombiMapFPK|MOST` (skrypty engdefs) **nie są czytane przez HMI**, a w K3346 ich nie ma.
Tryb kombi ustawia się przez persistence `3221356656` (skrypty `clusterCoding*.sh`: 00 OFF, 02 MOST, 03 LVDS, 04 MMI).
Wartości runtime trzeba odczytać w aucie (tylko do odczytu, `dumb_persistence_reader`; plan w 05).
