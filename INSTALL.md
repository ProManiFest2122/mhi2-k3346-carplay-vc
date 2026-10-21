# Installation guide

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

---

## 1. What you get (per stage)

Each stage is installed and rolled back on its own. Install them in order; test after each one.

| Stage | Feature | Car-tested |
|---|---|---|
| 3 (v8s) | CarPlay map on the Virtual Cockpit, both layouts (VIEW), map zoom with the left roller, map survives reversing | ✅ |
| 4 | Route guidance (RGI) data → cluster: distance, street, route flag | ✅ (maneuver arrow in the KDK panel is always "straight" – known) |
| 5 | Siri on a short press of the voice button, MMI touchpad as a D-pad, CarPlay calls on the VC phone tab, **Answer / Reject / Hang up** from the VC, steering-wheel phone key unchanged | ✅ |
| 5.3 | Album art (CarPlay now playing) on the VC | ❌ packaged, not yet car-tested |
| 6 | CarPlay **recent calls** on the VC phone tab, dial from the list | ❌ packaged, not yet car-tested |

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
```

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

### 3.6 Assemble the SD package and run the simulations

```bash
bash tools/make_sd_package.sh                       # -> work/sd_etap5/  (+ SHA256SUMS.txt)
docker run --rm --platform=linux/amd64 -v "$PWD/work:/work" -v "$PWD/tools:/tools:ro" \
  qnx65-armv7-toolchain:4.9 bash /tools/sim_installer.sh      # must end with "0 FAIL"
```

The simulation runs the real installer against a fake unit tree: install, update, every rollback,
refusals, injected failures. It needs a copy of your stock files in `work/` (see `docs/10`); without
them, skip it and rely on the installer's own checks.

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

### 4.0 Prepare the card

1. Copy the **contents** of `work/sd_etap5/` (not the folder itself) to the root of your M.I.B. card.
   It adds `mod/command.sh`, `mod/mib2high-carplay-rgi-v8s-simple/` and `ZNACZNIKI/` (marker files).
2. Make sure M.I.B. autorun is **disarmed** (`_Swdlautorun.txt`, with the underscore).
3. Optional check on Windows: `powershell -ExecutionPolicy Bypass -File tools/sprawdz_karte.ps1 -Karta E: -Typ instalacyjna`.

**The card keeps the backup of your original files** (`mod/mib2high-carplay-rgi-v8s-simple/backup/`).
Rollback works only with that card. After every install copy the whole `mod/` folder to your PC.
Do not format the card or overwrite `mod/` with a fresh package while the mod is installed.

How the script decides what to do (one action per run):
`ROLLBACK` → `ROLLBACK_ETAP5` → `ROLLBACK_ETAP4` → `COLLECT_LOGS` → `INSTALL_ETAP4` → `INSTALL_ETAP5`
→ (installed: collect logs) → (nothing installed: install Stage 3).
A marker is an empty file, without extension, copied from `ZNACZNIKI/` into
`mod/mib2high-carplay-rgi-v8s-simple/`. Each marker is consumed when its step succeeds.

Run the script: **GEM → m.i.b. → advanced settings → run individual script**. Wait for `SAFE TO REMOVE SD`.

### 4.1 Stage 3 – map on the VC

1. No markers. Run the script once → `INSTALL_RESULT=PASS`. If you see `VERSION_GATE … PRZERWANA`,
   nothing was written: your firmware does not match. Stop.
2. Restart the MMI (hold the power button until it reboots).
3. Connect the iPhone with a cable, start CarPlay, open Google/Apple Maps, switch the VC to the
   navigation view. The CarPlay map should appear on the cluster; VIEW switches layouts.

### 4.2 Stage 4 – route guidance data

1. Copy `ZNACZNIKI/INSTALL_ETAP4` into `mod/mib2high-carplay-rgi-v8s-simple/`, run the script →
   `ETAP4_INSTALL_RESULT=PASS`, restart the MMI.
2. Start guidance on the iPhone: distance and street appear on the VC.
3. If CarPlay no longer connects over the cable: `ROLLBACK_ETAP4` marker → run → restart.

### 4.3 Stage 5 – phone, Siri, touchpad, album art, recents (Stages 5.x and 6)

Build and test the rescue card first (3.7).

1. Copy `ZNACZNIKI/INSTALL_ETAP5`, run → `ETAP5_INSTALL_RESULT=PASS`, restart the MMI.
2. The HMI must boot normally. If it boot-loops: insert the **rescue card**, wait for the update mode to
   finish (`RATUNEK_LOG.txt` with `WYNIK=OK`), restart, then use your install card with
   `ROLLBACK_ETAP5`.
3. Test: short press of the voice button → Siri; touchpad → selection moves in CarPlay; incoming call →
   VC shows the caller with Answer/Reject; music → album art on the VC; VC phone tab → recent calls.

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
| `ROLLBACK_ETAP5` | removes Stage 5 (JAR, cover-art tool) – map and Stage 4 stay |
| `ROLLBACK_ETAP4` | restores `usblauncher_otg.lua`, `iap2.cfg`, removes the `mm-ipod` hook – map stays |
| `ROLLBACK` | everything, in order 5 → 4 → 3, back to the stock files (byte for byte) |

Restart the MMI after a rollback.

## 6. Exactly what changes on the unit

| Stage | Files |
|---|---|
| 3 | new: `/mnt/app/root/hooks/libmib2high-carplay-rgi.so`, `/mnt/app/eso/hmi/lsd/jars/mib2high-carplay-rgi.jar`, `/mnt/app/root/{vc_render, v8l-dsitracer-capture, mib2high-lvds-*}`, two marker files; modified (3 small edits, backed up): `bundles_dsitracer_enabled.properties`, `traceConfig.properties`, `smartphone_integrator.json` |
| 4 | new: `/mnt/app/root/hooks/libmib2high-carplay-rgi-ipod.so`, `/mnt/app/root/mm-ipod-rgi.sh`; modified: `/mnt/system/etc/usblauncher_otg.lua` (1 line: start `mm-ipod` through the launcher), `/mnt/system/etc/mm/iap2.cfg` (`[ident]` section appended: 0x5200–0x5204, 0x415A, 0x4170, 0x4171) |
| 5 | new: `/mnt/app/eso/hmi/lsd/jars/mib2high-k3346-overrides.jar` (replaced HMI classes, loaded before `lsd.jxe` by the stock `lsd.sh`), `/mnt/app/root/k3346-coverart-fetch` |

The stock `lsd.jxe`, the iAP2 driver file and all other files are never modified.

## 7. Credits and licensing

See [README.md](README.md) and [LICENSING.md](LICENSING.md).
