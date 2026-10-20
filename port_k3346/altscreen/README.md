# AltScreen v1.0 — CarPlay navigation in the instrument cluster

AltScreen displays the supplementary **Apple CarPlay navigation map** inside a compatible car's **digital instrument cluster**, alongside its familiar speedometer and tachometer. Drivers can keep the central infotainment display on another function while seeing their route ahead of them.

The release is based on **v8S**, the latest vehicle-tested implementation for the reference car. It includes the existing CarPlay Route Guidance Information (RGI) interface used for turn-by-turn arrows and distances on the **factory head-up display (HUD)**. It is **not** a replacement for the entire digital cluster or the car's navigation system.

## Features

- Real-time CarPlay navigation video on the instrument cluster, decoded using its existing video/display facilities.
- A CarPlay instruction card, with separate positioning for **small** and **large** gauge layouts.
- Steering-wheel **VIEW** layout switching: requests the correct CarPlay view and navigational safe area for each layout.
- Acknowledged, serialized CarPlay display controls: `showUI -> updateViewArea -> forceKeyFrame`. Avoids racing layout-change requests and outdated responses.
- Navigation maneuver and distance information through the factory RGI/BAP interface, used by the tested car's **HUD**.
- A reversible SD-card installer with guarded backups, diagnostic collection and rollback.
- Native C and Java source code, test harnesses, release evidence and build instructions.

## How it works

1. Your iPhone establishes an ordinary CarPlay session and a compatible app (such as Apple Maps or Google Maps) starts navigation.
2. AltScreen negotiates CarPlay's additional **instrument-cluster video stream**, carrying H.264 video of the map and app-provided instruction card.
3. Its native receiver and the dedicated renderer show this stream in a suitable layer of the digital instrument cluster.
4. When the instrument layout changes, AltScreen requests the correct CarPlay map view and adjusts the available rendering area, then asks for a fresh video keyframe.
5. Separately, the RGI component processes structured maneuver data received from CarPlay and translates it to the existing factory interface used by the HUD.

The **map video** and **HUD maneuver data** travel on separate paths. A model that supports one does not necessarily support the other.

## Navigation apps, instrument controls and behavior

Apple Maps, Google Maps and Waze were involved in development and testing. App-specific map rendering and instruction card behavior depend on app version, iOS, cable/adapter and the vehicle. The car's VIEW button retains its normal purpose; AltScreen follows the selected cluster layout. It does not make the cluster touchscreen-enabled or replace CarPlay interaction on the main MMI display.

## Compatibility and limitations

**Vehicle-tested reference:** Audi Q7 4M with Harman MIB2 High (`MHI2_ER_AUG22_K2161`, MU1421), compatible digital instrument cluster and factory HUD.

**Not claimed compatible:** all other MHI2 generations, MHI2Q, Volkswagen/Skoda vehicles or other instrument clusters. Other vehicles/firmwares must be investigated and validated separately. The reference platform uses a 1440 × 542 auxiliary video canvas.

**Known limitations:** factory VC maneuver/nav graphics may appear alongside AltScreen during active guidance. The project is still researching how to hide unwanted VC graphics **without interfering with HUD turn guidance**. That proposed change is **not included** in this stable release. Cluster-local graphics are not necessarily controlled by the same video display context.

This is independent enthusiast software, not an official Audi, Volkswagen, Škoda or Apple feature. Incorrect modification of infotainment firmware may prevent the unit from starting; read the install and recovery directions first.

## Download, install and rollback

Choose the matching assets from the **AltScreen v1.0 GitHub Release**:

- **`AltScreen-v1.0-install.zip`** — compiled QNX native library, Java RGI JAR, video renderer, configuration and M.I.B. custom installer. This is the *original car-tested v8S install ZIP*, renamed but not repacked or modified.
- **`AltScreen-v1.0-source.zip`** — editable C/Java source, scripts, test harnesses, configuration and build evidence. Excludes the platform's proprietary firmware/dependency JARs and historic intermediate `.o`/`.class` build products.

**Installation on supported platform:** back up the MMI and SD card, extract the installer ZIP to the SD card root (so the card contains `mod/custom.sh`), run *Run custom script* in M.I.B., and reboot MMI manually. Follow the included `README.txt`. Running the script again collects logs.

**Rollback:** create `mod/mib2high-carplay-rgi-v8s-simple/ROLLBACK` on the SD card, run `mod/custom.sh` again and reboot. Keep the original backup files on the SD card. **Do not use this release as an unverified universal installer.**

## For developers

The source package preserves the v8S source and configuration. Runtime code has **not** been changed for the public release. Native compilation requires a QNX 6.5 ARMv7 toolchain (GCC 4.9.4 in the validated build); Java compilation requires Java 8 and the matching firmware's proprietary compile-dependency JAR, which cannot be bundled. Detailed evidence is in `release_evidence/`. Some archived scripts need historic comparison binaries which are intentionally omitted; see `BUILDING.md`.

**Source-only rule:** change source (`.c`, `.h`, `.java`, `.sh` or configuration), rebuild affected components, and verify them. **Never patch compiled ELF binaries, Java bytecode, JARs or object files.**

## Related releases

- **RGI v1.1** — stable, car-tested HUD turn-by-turn integration.
- **MIB2 High Software Collector v1.0.2** — prerelease diagnostic collector.
- **AltScreen v1.0** — car-tested v8S map video, layout controls, HUD RGI integration and installer.

## Credits

Includes contributions or concepts derived from LuKa's [mib2q-carplay-rgi](https://github.com/luka-dev/mib2q-carplay-rgi) and community research into MHI2 AltScreen such as [Joey's MHI2-altScreen](https://github.com/joeyQuery/MHI2-altScreen). Preserve existing source copyright notices and upstream licenses. Refer to the repository `NOTICE.md` for provenance.


## Source publication / licensing

The compiled v8S installer is intentionally unchanged. The matching project-owned **K2161 renderer adaptation** was additionally recovered from the historical GEN2/AV11 checkpoint and is tracked under [`renderer/k2161/`](renderer/k2161/vc_render.c). Its exact reproducibility against the v8S ELF is not yet established. Editable installer AWK scripts and the pinned GPL-3.0 renderer upstream source are now included; see [build instructions](BUILDING.md), [renderer provenance](renderer/PROVENANCE.md) and [archived-manifest scope](ARCHIVED_MANIFEST_SCOPE.md). These additions do not constitute evidence that the exact v8S renderer executable has been reproducibly rebuilt.
