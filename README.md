# mhi2-k3346-carplay-vc

CarPlay on the Audi **Virtual Cockpit** for **MHI2 (Harman) firmware `MHI2_ER_AUG22_K3346` / MU1438**
(tested on an Audi A4 B9 with Virtual Cockpit and wired CarPlay).

A port of [edzenis/mib2high-carplay-altscreen-rgi](https://github.com/edzenis/mib2high-carplay-altscreen-rgi)
(built for K2161 / Q7 4M) to K3346, plus features ported from
[luka-dev/mib2q-carplay-rgi](https://github.com/luka-dev/mib2q-carplay-rgi) (MHI2Q).
Detailed engineering notes are in Polish in [`docs/`](docs/) — start with [`docs/00_postep.md`](docs/00_postep.md).

> **Firmware-locked.** The installer refuses to run unless `HMI_Version`, `ipod-drvr-iap2.so`,
> `lsd.jxe`, `usblauncher_otg.lua`, `iap2.cfg` and `mm-ipod` match K3346/MU1438 byte sizes.
> Do not bypass the gate on other firmware: hook offsets and replaced HMI classes are K3346-specific.

## Status (2026-10-20, all tested in the car unless noted)

| Feature | Stage | Status |
|---|---|---|
| CarPlay map on the Virtual Cockpit (both layouts, VIEW switches) | 3 | ✅ works |
| Map zoom with the left steering-wheel roller | 3 | ✅ works |
| Map keeps running while reversing (MMI shows the camera as stock) | 3 | ✅ works |
| Route guidance (RGI) data → cluster: distance, street, route flag | 4 | ✅ works |
| Maneuver arrow in the right-hand KDK panel | 4b | ❌ always straight (panel image comes from the native nav); own renderer planned |
| Siri on a **short** press of the voice button | 5.1 | ✅ works |
| MMI touchpad as a D-pad in CarPlay | 5.1 | ✅ works |
| CarPlay calls on the VC phone tab (incoming/outgoing/active, name, signal, carrier; no "iPhone is in CarPlay mode" note) | 5.2 | ✅ works |
| Answer / hang up from the VC, recents/favorites | 5.4 | 📋 planned (`docs/16`) |
| Album art on the VC | 5.3 | 🔬 probe packaged, not yet run in the car |

## How it works (short)

* **Stage 3 (AltScreen, edzenis):** hooks in `dio_manager` advertise a second CarPlay display; `vc_render`
  decodes the H.264 stream onto the cluster layer; `mib2high-lvds-control.sh` drives the cluster context.
* **Stage 4 (RGI):** a separate RGI-only library is preloaded **only into `mm-ipod`** through a small
  launcher (`usblauncher_otg.lua` one-line change) — the AltScreen library must not be loaded there because
  its constructor clears the VC session marker. `iap2.cfg` gets an `[ident]` section for 0x5200–0x5204.
  Java pushes BAP route guidance (`updateManeuverDescriptor` + `updateExitView` – the combined K3346 method is a no-op).
* **Stage 5:** `lsd.sh` puts every `lsd/jars/*.jar` on `-Xbootclasspath` **before** `lsd.jxe`; a probe proved
  that J9 then loads our classes instead of the stock ones. Replaced classes are rewritten from the decompiled
  K3346 originals with minimal changes (API and every stock call verified with `javap`):
  `TerminalModeSDSHandler` + `CarPlaySpeechRequestHandler` (Siri), `TMTouchscreenVBListener` (touchpad →
  D-pad through stock softkeys), `ServiceManagerPhone` + `K3346GatedPhoneService` (calls read from the stock
  iAP2 driver's PPS object `/ramdisk/pps/iap2/telephony`; the K3346 DSI call-state listener is an empty stub).

## Layout

| Path | What |
|---|---|
| `port_k3346/altscreen/` | sources: native hooks, renderer, Java bundle, `java/overrides/` (replaced HMI classes), installer `deployment/custom.sh` |
| `port_k3346/ratunek/` | rescue-card script (HMI boot loop recovery via M.I.B. update mode) |
| `tools/` | Docker QNX build wrapper, SD package builder, rescue-card builder, installer/rescue simulations, card checker |
| `docs/` | engineering log (Polish): analysis, decisions, car test results, build reproduction (`docs/10`) |

Nothing from the car or from Audi firmware images is included: no `lsd.jar`/`lsd.jxe`, no backups, no logs.
You need your own K3346 `lsd.jxe` → `lsd.jar` ([luka-dev/jxe2jar](https://github.com/luka-dev/jxe2jar)) to build
the Java parts, and the [QNX 6.5 toolchain image](docs/10_odtworzenie_builda.md) for native parts.

## Install (summary — read `docs/08`, `docs/12`, `docs/13`, `docs/15`, `docs/16` first)

Requires [M.I.B. – More Incredible Bash](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash) (3.6.0) on
an SD card. The installer is `mod/command.sh` (run via GEM → m.i.b. → advanced settings → *run individual script*).
Every layer has its own backup and rollback: `ROLLBACK` (everything), `ROLLBACK_ETAP4`, `ROLLBACK_ETAP5`;
marker files are in `ZNACZNIKI/`. Build and test the **rescue card** before installing Stage 5.

**Use at your own risk.** This modifies the infotainment system of a car. External power, a fresh M.I.B.
backup and the original firmware must be at hand. No warranty of any kind.

## Credits

* **edzenis** – mib2high-carplay-altscreen-rgi (base of this port)
* **LuKa (@LuKa_dev)** – mib2q-carplay-rgi (RGI parser/bus, maneuver mapping, Siri/touchpad/telephony designs, docs)
* **joeyQuery** – MHI2-altScreen (`vc_render` origin)
* **harman-f** – mhi2_altscreen_carplay research, M.I.B.
* **Mr-MIBonk** – M.I.B.

See [LICENSING.md](LICENSING.md).
