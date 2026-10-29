# Building from source and per-stage install

> **Just want to install it?** Use the ready package from Releases and the 5 steps in
> [README.md](README.md). This file is for developers: building, the release script, installing single
> stages, the rescue card.

> ## ⚠️ ONE FIRMWARE ONLY: `MHI2_ER_AUG22_K3346`, MU **1438**
>
> Everything in this repository was built, verified and tested against **one exact firmware**:
> Harman **MHI2** (not MHI2Q), train **`MHI2_ER_AUG22_K3346`**, MU **1438**,
> `HMI_Version = HMI_AU_MIB2_H_EU_G22_C6_190300`.
>
> - The native hooks patch the stock iAP2 driver **in memory at fixed offsets**
>   (`ipod-drvr-iap2.so`, 739 925 bytes). On any other build those offsets point at different code.
> - The Java part **replaces stock HMI classes** with versions rewritten from this firmware's `lsd.jxe`
>   (58 131 096 bytes). On another firmware the HMI may not boot (boot loop).
> - The installer **refuses to run** unless the HMI version and the sizes of `ipod-drvr-iap2.so`,
>   `lsd.jxe`, `usblauncher_otg.lua`, `iap2.cfg` and `mm-ipod` match exactly. It contains a developer
>   override for PC simulations; **do not use it on a car.** It skips the very checks that protect you.
> - Another MU, another K-train, MHI2Q, MIB2 Standard, or "close enough": **do not install.**
>   Porting means re-deriving offsets and re-writing the replaced classes from your own firmware
>   (see `docs/` – Polish engineering notes – and the upstream projects in the README).
>
> Tested on one car: **Audi A4 B9 with Virtual Cockpit, wired CarPlay, iPhone.** Other bodies with the
> same firmware (A5/Q5/Q7…) may need a different cluster geometry profile.

This modifies the infotainment system of a car. You need to be comfortable with M.I.B., an SD card,
a Linux shell and Docker. **No warranty. You are responsible for your car.**

## 0. How it works (short)

* **Stage 3 (AltScreen, edzenis):** hooks in `dio_manager` advertise a second CarPlay display; `vc_render`
  decodes the H.264 stream onto the cluster layer; `mib2high-lvds-control.sh` drives the cluster context.
* **Stage 4 (RGI):** a separate RGI-only library is preloaded **only into `mm-ipod`** through a small
  launcher (`usblauncher_otg.lua` one-line change). `iap2.cfg` gets an `[ident]` section for 0x5200–0x5204.
  Java pushes BAP route guidance (`updateManeuverDescriptor` + `updateExitView`).
* **Stage 5:** `lsd.sh` puts every `lsd/jars/*.jar` on `-Xbootclasspath` **before** `lsd.jxe`, so J9 loads
  our classes instead of the stock ones. Replaced classes are rewritten from the decompiled K3346 originals
  with minimal changes (API verified with `javap`): Siri, touchpad → D-pad, CarPlay calls on the VC
  (from the stock iAP2 PPS object `/ramdisk/pps/iap2/telephony`), VC answer/hang-up, album art.
* **Stage 6:** the `mm-ipod` hook extends iAP2 message group 0x41 in memory and sends `StartListUpdates
  0x4170` / `InitiateCall 0x415A`; `ListUpdate 0x4171` becomes the recents list on the VC.
* **Stage 7:** `vc_overlay`, a port of LuKa's `maneuver_render` (OpenKODE/GLES2), draws the 3D maneuver
  arrow, lanes and distance into a KD window whose displayable is declared through
  `KD_WINDOWPROPERTY_CAPTION`; the DisplayManager composites it over the map. The `mm-ipod` hook feeds
  it maneuvers over a local socket; the gauge layout (VIEW) is read from the DSITracer capture ring
  (BAP 50/54). Details: `docs/18`, `docs/19` (Polish).

---

## 1. What you get (per stage)

Each stage is installed and rolled back on its own. Install them in order; test after each one.

| Stage | Feature | Car-tested |
|---|---|---|
| 3 (v8s) | CarPlay map on the Virtual Cockpit, both layouts (VIEW), map zoom with the left roller, map survives reversing | ✅ |
| 4 | Route guidance (RGI) data → cluster: distance, street, route flag | ✅ |
| 5 | Siri on a short press of the voice button, MMI touchpad as a D-pad, CarPlay calls on the VC phone tab, **Answer / Reject / Hang up** from the VC, steering-wheel phone key unchanged | ✅ |
| 5.3 | Album art (CarPlay now playing) on the VC | ✅ |
| 6 | CarPlay **recent calls** on the VC phone tab, dial from the list | ✅ (iOS 27.2; iOS 26.6–26.7.x sends an empty list – iOS bug) |
| 7 | 3D maneuver arrow + distance + flag next to the map, follows VIEW | ✅ (lanes: wired up, not yet seen in the car) |

**Not possible over CarPlay:** the full address book. iAP2 only carries recents and favorites; the MMI
phonebook comes from Bluetooth PBAP, which CarPlay switches off.

## 2. What you need

**Car / unit**
- Your unit is exactly the firmware above. Check it before anything else (M.I.B. → info, or the MMI
  version screen). The installer checks again and stops if it does not match.
- [M.I.B. – More Incredible Bash](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash) **3.6.0**
  working on your unit (GEM → m.i.b.). This guide uses its *run individual script* entry, which executes
  `/mod/command.sh` from the SD card.
- A **full M.I.B. backup** of your unit (the `lsd.jxe` from it is also a build input).
- External power or a battery charger connected while you work.
- **Two SD cards** (FAT32): the install card (your M.I.B. card) and a separate **rescue card**.
- The original firmware for your unit at hand, in case everything else fails.

**PC**
- Docker (x86-64 / `linux/amd64` images), Git, Bash (Linux, macOS, or Git Bash on Windows), Python 3
  with `pyelftools` (offset check).
- ~10 GB disk for the toolchain image.

Nothing from Audi firmware is in this repository (no `lsd.jxe`/`lsd.jar`, no binaries, no logs).
You build the package yourself from source plus your own `lsd.jxe`.

## 3. Build the package

Paths below are relative to the repository root. Build outputs go to `work/` (git-ignored).

### 3.1 QNX 6.5 ARM toolchain image (once, 20–40 min)

```bash
git clone https://github.com/luka-dev/qnx65-armv7-toolchain repos/qnx65-armv7-toolchain
(cd repos/qnx65-armv7-toolchain && \
 docker build --platform=linux/amd64 --build-arg BASE=base-4.9 -t qnx65-armv7-toolchain:4.9 .)
```

### 3.2 `lsd.jar` from **your** `lsd.jxe`

```bash
git clone https://github.com/luka-dev/jxe2jar repos/jxe2jar
mkdir -p work
cp /path/to/your/backup/lsd.jxe work/lsd.jxe          # from your M.I.B. backup (/ifs/lsd.jxe)
wc -c work/lsd.jxe                                    # must be 58131096
python3 repos/jxe2jar/src/jxe2jar.py work/lsd.jxe work/lsd.jar
```

If the size is not 58 131 096 bytes, **stop**: your firmware is not K3346/MU1438.

### 3.3 Native binaries

`tools/qnx_build.sh` copies the sources into the container before building (Docker Desktop on Windows
breaks the 32-bit QNX binutils otherwise) and copies the result back.

```bash
bash tools/qnx_build.sh build.sh                              output/libmib2high-carplay-rgi.so
bash tools/qnx_build.sh renderer/build_renderer.sh            renderer/build/vc_render
bash tools/qnx_build.sh diagnostics/build_dsitracer_qnx_v8l.sh diagnostics/v8l-dsitracer-capture
bash tools/qnx_build.sh build_rgi_ipod.sh                     output/libmib2high-carplay-rgi-ipod.so
bash tools/qnx_build.sh diagnostics/build_artwork_probe.sh    diagnostics/k3346-artwork-probe
bash tools/qnx_build.sh coverart/build_coverart_fetch.sh      coverart/k3346-coverart-fetch
bash tools/qnx_build.sh overlay/build_overlay.sh              overlay/output/vc_overlay
bash tools/qnx_build.sh overlay/build_probe.sh                overlay/output/vc_overlay_probe
```

Host unit tests of the overlay (geometry, layout parser, FBO choice) run without the car:
`cp -a port_k3346/altscreen /tmp/a && cd /tmp/a && bash overlay/tests/run_host_tests.sh` (in the container).

### 3.4 Check the hook offsets against **your** driver

Take `ipod-drvr-iap2.so` from your backup (`/mnt/app/armle/lib/dll/ipod-drvr-iap2.so`, 739 925 bytes):

```bash
pip install pyelftools
python3 tools/verify_port_offsets.py port_k3346/altscreen/native /path/to/ipod-drvr-iap2.so
```

Every line must be `PASS` and the last line `RESULT: ALL PASS`. Any `FAIL` = do not continue.

### 3.5 Java (JDK 8 from jxe2jar, inside the container)

```bash
docker run --rm --platform=linux/amd64 \
  -v "$PWD/port_k3346/altscreen:/src" -v "$PWD/work:/work" -v "$PWD/repos/jxe2jar/jvms:/jvms:ro" \
  -e JAVA_HOME=/jvms/zulu8.78.0.19-ca-jdk8.0.412-linux_x64 -e K2161_BASE_JAR=/work/lsd.jar \
  qnx65-armv7-toolchain:4.9 sh -c 'sh /src/java/build_java_release.sh && sh /src/java/build_overrides.sh'
```

(On Git Bash prefix the command with `MSYS_NO_PATHCONV=1` and use `$(pwd -W)` instead of `$PWD`.)
`build_overrides.sh` must end with `OVERRIDES_VERIFY=PASS`: every replaced HMI class keeps exactly the
stock public API and class format (Java 1.4).

### 3.6 Release package (what the Releases page contains)

```bash
bash tools/make_release.sh 1.0.0            # builds 3.3 + 3.5 itself, then packages
bash tools/make_release.sh 1.0.0 --no-build # package already built artifacts
```

Output in `work/release/<version>/`: `mhi2-k3346-carplay-vc-<version>-install.zip` (all stages, the
`INSTALL_ALL` marker armed, `RELEASE` file with the version), `…-uninstall.zip` (only the `ROLLBACK`
marker) and `SHA256SUMS.txt`. The script refuses CRLF in files for the car, non-ARM binaries, missing
payload, and any `lsd.*`, `.properties`, `.lua`, `.json`, `iap2.cfg`, log or backup in the tree: car
files are changed by `awk` on the unit itself. The one Audi-derived file shipped is
`mib2high-k3346-overrides.jar` (Stage 5 classes).

Simulations (each in its own container; they need a copy of your stock files in `work/`, see `docs/10`):

```bash
docker run --rm --platform=linux/amd64 -v "$PWD/port_k3346/altscreen:/src" -v "$PWD/tools:/tools" \
  -v "$PWD/work:/work" -e RELEASE_DIR=/work/release/1.0.0 qnx65-armv7-toolchain:4.9 \
  bash /tools/sim_installer.sh          # must end with "0 FAIL"
```

The installer simulation runs the real installer against a fake unit tree: install, every rollback,
refusals, injected failures, `INSTALL_ALL` (one run, resume after a failure, update = uninstall +
install) and, with `RELEASE_DIR`, the release zips exactly as a user unpacks them.

<a id="rescue-card"></a>
### 3.7 Rescue card (strongly recommended before Stage 5)

```bash
mkdir -p work/downloads/mib_3.6.0
curl -sL -o work/downloads/mib_3.6.0/mib_V3.6.0.zip \
  https://api.github.com/repos/Mr-MIBonk/M.I.B._More-Incredible-Bash/zipball/V3.6.0
python3 -I -c "import zipfile; zipfile.ZipFile('work/downloads/mib_3.6.0/mib_V3.6.0.zip').extractall('work/downloads/mib_3.6.0/x')"
bash tools/make_rescue_card.sh                      # -> work/sd_ratunek/
```

Extract with Python, not `git clone` (Windows `core.autocrlf` would break the M.I.B. scripts; the script
checks it). The script expects your car's stock `traceConfig.properties` (see `CAR_TRACE` in the
script). Copy `work/sd_ratunek/` to the **second** SD card. That card is *armed*: with it inserted the
unit enters M.I.B. update mode and removes our two JARs (map bundle and Stage 5 classes) plus the
DSITracer switch, and restores the stock `traceConfig.properties` – enough to bring back a
boot-looping HMI. **Do not insert it into a healthy, modded car** (the map would be gone until you roll
back and reinstall with the install card).

## 4. Install in the car

### 4.0 Release package (recommended)

Unzip `…-install.zip` to the root of the M.I.B. card and run **GEM → m.i.b. → advanced settings →
run individual script** once. The `INSTALL_ALL` marker installs every stage not yet installed, in order
3 → 4 → 5 (with 6) → 7. Every stage stays a separate transaction with its own backup and automatic
rollback; the first error stops the run with `INSTALL_ALL_RESULT=FAIL`, the failed stage is undone,
earlier ones stay, and running again resumes. `SAFE TO REMOVE SD` is printed once, at the very end.
Uninstall: `…-uninstall.zip` → run → `ROLLBACK_RESULT=PASS`. Update: uninstall, then install (an
install over a complete installation reports `ALREADY_INSTALLED` and changes nothing).

### 4.0a Per-stage install (developers)

1. Copy the **contents** of a stage package (`work/sd_etap5/`, `work/sd_etap7/`) to the root of your
   M.I.B. card. It adds `mod/command.sh`, `mod/mib2high-carplay-rgi-v8s-simple/` and `ZNACZNIKI/` (marker files).
2. Make sure M.I.B. autorun is **disarmed** (`_Swdlautorun.txt`, with the underscore).
3. Optional check on Windows: `powershell -ExecutionPolicy Bypass -File tools/sprawdz_karte.ps1 -Karta E: -Typ instalacyjna`.

**The card keeps the backup of your original files** (`mod/mib2high-carplay-rgi-v8s-simple/backup/`).
Rollback works only with that card. After every install copy the whole `mod/` folder to your PC.
Do not format the card or overwrite `mod/` with a fresh package while the mod is installed.

How the script decides what to do (one action per run):
`ROLLBACK` → `ROLLBACK_ETAP7` → `ROLLBACK_ETAP5` → `ROLLBACK_ETAP4` → `COLLECT_LOGS` → `INSTALL_ALL`
→ `INSTALL_ETAP4` → `INSTALL_ETAP5` → `INSTALL_ETAP7` → `PROBE_ETAP7`
→ (installed: collect logs) → (nothing installed: install Stage 3).
A marker is an empty file, without extension, copied from `ZNACZNIKI/` into
`mod/mib2high-carplay-rgi-v8s-simple/`. Each marker is consumed when its step succeeds.

Run the script: **GEM → m.i.b. → advanced settings → run individual script**. Wait for `SAFE TO REMOVE SD`.

### 4.1 Stage 3 – map on the VC

1. No markers. Run the script once → `INSTALL_RESULT=PASS`. If you see `VERSION_GATE … PRZERWANA`,
   nothing was written: your firmware does not match. Stop.
2. Restart the MMI (A4/A5/Q5 B9: hold the big MMI knob + NAV/MAP + RADIO for ~10 s; other models:
   [M.I.B. wiki](https://wiki.mibsolution.one/en/MHI2)).
3. Connect the iPhone with a cable, start CarPlay, open Google/Apple Maps, switch the VC to the
   navigation view. The CarPlay map should appear on the cluster; VIEW switches layouts.

### 4.2 Stage 4 – route guidance data

1. Copy `ZNACZNIKI/INSTALL_ETAP4` into `mod/mib2high-carplay-rgi-v8s-simple/`, run the script →
   `ETAP4_INSTALL_RESULT=PASS`, restart the MMI.
2. Start guidance on the iPhone: distance and street appear on the VC.
3. If CarPlay no longer connects over the cable: `ROLLBACK_ETAP4` marker → run → restart.
4. **Experimental (Stage 6b):** to let the installer also declare the MMI's own Bluetooth address to the
   iPhone (`[bluetooth]` section in `iap2.cfg` → `BluetoothTransportComponent`), put a file `BT_MAC` with one
   line `XX:XX:XX:XX:XX:XX` (the address of **your MMI**, e.g. read from a laptop paired with it) into
   `mod/mib2high-carplay-rgi-v8s-simple/` before `INSTALL_ETAP4`. Without the file nothing changes. A wrong
   format stops the install before any change. Goal was that the iPhone sends recents over the cable without
   BT. **Tested in the car: the component is accepted, but the recents list stays empty – not recommended.**

### 4.3 Stage 5 – phone, Siri, touchpad, album art, recents (Stages 5.x and 6)

Build and test the rescue card first (3.7).

1. Copy `ZNACZNIKI/INSTALL_ETAP5`, run → `ETAP5_INSTALL_RESULT=PASS`, restart the MMI.
2. The HMI must boot normally. If it boot-loops: insert the **rescue card**, wait for the update mode to
   finish (`RATUNEK_LOG.txt` with `WYNIK=OK`), restart, then use your install card with
   `ROLLBACK_ETAP5`.
3. Test: short press of the voice button → Siri; touchpad → selection moves in CarPlay; incoming call →
   VC shows the caller with Answer/Reject; music → album art on the VC; VC phone tab → recent calls.

### 4.3a Stage 7 – maneuver arrow on the VC

Needs Stage 4. Copy `ZNACZNIKI/INSTALL_ETAP7`, run → `ETAP7_INSTALL_RESULT=PASS`, restart the MMI, start
a CarPlay route: the arrow tile appears next to the map in both layouts. `ROLLBACK_ETAP7` restores exactly
the files from before Stage 7 (including the control script and the `mm-ipod` library it updates).
`PROBE_ETAP7` runs a read-only probe of the DisplayManager (`collected/probe-*`).

### 4.4 Updating to a newer package version

Put **both** rollback and install markers of the changed stages at once; the script processes them one
per run in the right order. Example for Stages 4 + 5: `ROLLBACK_ETAP5`, `ROLLBACK_ETAP4`,
`INSTALL_ETAP4`, `INSTALL_ETAP5` → run the script **four times** → restart the MMI.
Copy only the new files from the package; **keep** `backup/`, `*.installed.ok` and `collected/` on the card.

### 4.5 Logs

Run the script with nothing pending (or with `COLLECT_LOGS`): it writes
`mod/mib2high-carplay-rgi-v8s-simple/collected/run-NN/` (system log, iAP2 log, our hooks' logs).
**These logs contain personal data** (VIN, phone numbers, contact names, GPS). Do not post them publicly.

## 5. Uninstall

| Marker | Effect |
|---|---|
| `ROLLBACK_ETAP7` | removes Stage 7 (overlay, probe, config) and restores the two files it updated |
| `ROLLBACK_ETAP5` | removes Stage 5 (JAR, cover-art tool) – map and Stage 4 stay |
| `ROLLBACK_ETAP4` | restores `usblauncher_otg.lua`, `iap2.cfg`, removes the `mm-ipod` hook – map stays |
| `ROLLBACK` | everything, in order 7 → 5 → 4 → 3, back to the stock files (byte for byte) |

Restart the MMI after a rollback.

## 6. Exactly what changes on the unit

| Stage | Files |
|---|---|
| 3 | new: `/mnt/app/root/hooks/libmib2high-carplay-rgi.so`, `/mnt/app/eso/hmi/lsd/jars/mib2high-carplay-rgi.jar`, `/mnt/app/root/{vc_render, v8l-dsitracer-capture, mib2high-lvds-*}`, two marker files; modified (3 small edits, backed up): `bundles_dsitracer_enabled.properties`, `traceConfig.properties`, `smartphone_integrator.json` |
| 4 | new: `/mnt/app/root/hooks/libmib2high-carplay-rgi-ipod.so`, `/mnt/app/root/mm-ipod-rgi.sh`; modified: `/mnt/system/etc/usblauncher_otg.lua` (1 line: start `mm-ipod` through the launcher), `/mnt/system/etc/mm/iap2.cfg` (`[ident]` section appended: 0x5200–0x5204, 0x415A, 0x4170, 0x4171; with `BT_MAC` on the card also a `[bluetooth]` section with that address) |
| 5 | new: `/mnt/app/eso/hmi/lsd/jars/mib2high-k3346-overrides.jar` (replaced HMI classes, loaded before `lsd.jxe` by the stock `lsd.sh`), `/mnt/app/root/k3346-coverart-fetch` |
| 7 | new: `/mnt/app/root/{vc_overlay, vc_overlay_probe, mib2high-overlay.conf, flag_atlas.rgba, k3346-overlay-probe.sh}`; updated (backed up by Stage 7): `/mnt/app/root/mib2high-lvds-control.sh`, `/mnt/app/root/hooks/libmib2high-carplay-rgi-ipod.so` |

The stock `lsd.jxe`, the iAP2 driver file and all other files are never modified.

## 7. Credits and licensing

See [README.md](README.md) and [LICENSING.md](LICENSING.md).
