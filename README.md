# mhi2-k3346-carplay-vc

CarPlay on the Audi **Virtual Cockpit** for **MHI2 (Harman) firmware `MHI2_ER_AUG22_K3346` / MU1438**
(tested on an Audi A4 B9 with Virtual Cockpit and wired CarPlay).

A port of [edzenis/mib2high-carplay-altscreen-rgi](https://github.com/edzenis/mib2high-carplay-altscreen-rgi)
(built for K2161 / Q7 4M) to K3346, plus features ported from
[luka-dev/mib2q-carplay-rgi](https://github.com/luka-dev/mib2q-carplay-rgi) (MHI2Q).
Detailed engineering notes are in Polish in [`docs/`](docs/) — start with [`docs/00_postep.md`](docs/00_postep.md).

> ## ⚠️ For ONE firmware only: `MHI2_ER_AUG22_K3346`, MU 1438
>
> `HMI_Version = HMI_AU_MIB2_H_EU_G22_C6_190300`, Harman MHI2 (not MHI2Q). Hook offsets and the
> replaced HMI classes are specific to this exact build. The installer refuses to run unless
> `HMI_Version`, `ipod-drvr-iap2.so`, `lsd.jxe`, `usblauncher_otg.lua`, `iap2.cfg` and `mm-ipod` match
> K3346/MU1438 byte for byte in size. **Any other MU / K-train: do not install, do not bypass the gate.**
>
> **→ Read [INSTALL.md](INSTALL.md) first** (build, rescue card, step-by-step install, rollback).

## Status (2026-10-21)

| Feature | Stage | Status |
|---|---|---|
| CarPlay map on the Virtual Cockpit (both layouts, VIEW switches) | 3 | ✅ works in the car |
| Map zoom with the left steering-wheel roller | 3 | ✅ works in the car |
| Map keeps running while reversing (MMI shows the camera as stock) | 3 | ✅ works in the car |
| Route guidance (RGI) data → cluster: distance, street, route flag | 4 | ✅ works in the car |
| Maneuver arrow in the right-hand KDK panel | 4b | ❌ always straight (panel image comes from the native nav); own renderer planned |
| Siri on a **short** press of the voice button | 5.1 | ✅ works in the car |
| MMI touchpad as a D-pad in CarPlay | 5.1 | ✅ works in the car |
| CarPlay calls on the VC phone tab (incoming/outgoing/active, name, signal, carrier; no "iPhone is in CarPlay mode" note) | 5.2 | ✅ works in the car |
| Answer / Reject / Hang up from the VC; steering-wheel phone key keeps working (no double action) | 5.4 | ✅ works in the car |
| Album art (CarPlay now playing) on the VC | 5.3 | ✅ works in the car |
| Recent calls on the VC phone tab during CarPlay, dial from the list (iPhone dials over iAP2) | 6 | ✅ works in the car – the list is the one the MMI fetched over Bluetooth (kept when CarPlay turns BT off); calls made during the session are added on top (without date) |
| Recent calls sent by the iPhone over the cable (iAP2 ListUpdate, no Bluetooth needed) | 6b/6c | ⛔ not possible on this MMI/iPhone: the iPhone always answers "available, 0 entries" – tested with 5 request variants (one byte-identical to LuKa's MHI2Q request) and with a Bluetooth transport component (optional `BT_MAC`) |
| Full address book on the VC | – | ⛔ not possible over CarPlay (iAP2 has only recents/favorites; the phonebook needs Bluetooth PBAP) |
| Own call card drawn on the CarPlay map (the stock VC popup disappears after a tab change) | – | 📋 idea, together with the 4b renderer |

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
  iAP2 driver's PPS object `/ramdisk/pps/iap2/telephony`; the K3346 DSI call-state listener is an empty stub),
  `MFLPhoneButtonsListener` (VC answer/hang-up go the stock steering-wheel key path, CarPlay HID 13/14),
  `TerminalModeBapCombi` + `AppConnectorTerminalMode` (album art pushed through the stock kombi picture
  server; the art is fetched from the stock iAP2 daemon by `k3346-coverart-fetch`).
* **Stage 6:** the `mm-ipod` hook extends the driver's message group 0x41 in memory and sends iAP2
  `StartListUpdates 0x4170` (byte-identical to a request verified in a car by xPaiiN) and `InitiateCall
  0x415A`; `ListUpdate 0x4171` is parsed into a file that the Java phone wrapper shows as recents. Because the
  iPhone returns an empty list here, the wrapper also keeps the last Bluetooth call list of the MMI (the MMI clears
  it when CarPlay starts) and adds calls made during the CarPlay session.

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

## Install

**Step-by-step guide: [INSTALL.md](INSTALL.md).** In short: build the package from source with your own
`lsd.jxe` (nothing from Audi firmware is distributed here), make the rescue card, then install stage by
stage with [M.I.B.](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash) 3.6.0 (*run individual
script* runs `mod/command.sh`). Every stage has its own backup and rollback (`ROLLBACK`, `ROLLBACK_ETAP4`,
`ROLLBACK_ETAP5`; marker files in `ZNACZNIKI/`).

**Use at your own risk.** This modifies the infotainment system of a car. External power, a fresh M.I.B.
backup and the original firmware must be at hand. No warranty of any kind.

## Credits

* **edzenis** – mib2high-carplay-altscreen-rgi (base of this port)
* **LuKa (@LuKa_dev)** – mib2q-carplay-rgi (RGI parser/bus, maneuver mapping, Siri/touchpad/telephony designs, docs)
* **joeyQuery** – MHI2-altScreen (`vc_render` origin)
* **harman-f** – mhi2_altscreen_carplay research, M.I.B.
* **Mr-MIBonk** – M.I.B.

See [LICENSING.md](LICENSING.md).
