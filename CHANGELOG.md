# Changelog

All versions are for MHI2 (Harman) firmware `MHI2_ER_AUG22_K3346`, MU 1438 only.
Downloads: [Releases](../../releases). Where it has been reported to work: [COMPATIBILITY.md](COMPATIBILITY.md).

## 2.0.0 (2026-11-14)

### New

- **Favourites** next to recent calls on the cluster (checked on iOS 27; FaceTime entries and
  e-mail addresses are not shown).
- **Arrival time where you want it:** as a frame on the cluster map, as a capsule on the cluster
  bar, or both. Arrival hour, time left and distance, with Apple Maps and Google Maps, on both
  cluster layouts.
- **The phone's maneuver card can be switched off:** a clean map, on both cluster layouts.
- **Own call card** on the CarPlay map: name, state dot and call time.
- **Hold, resume and mute** a CarPlay call from the cluster phone tab.
- **Look of the call card and the arrival-time frame:** smoked or light glass; the call card can
  also be switched off.
- **Switches** for the maneuver arrow, for the CarPlay map on the cluster (the factory view comes
  back) and for the HUD arrows. I could not check the HUD switch: my car has no HUD.
- **My options in the MMI and the call card in 8 languages:** English, Polish, German, Russian,
  Czech, French, Italian and Spanish, following the MMI language (other languages: English).
- **Everything is set in the MMI**, with no SD card and no scripts: Settings → Connection
  manager → knob to the right → bottom of the list.
- The installer is on the card as `mod/custom.sh` too, the file newer M.I.B. versions run
  (**Run Custom Script**; not checked in my car), and it stops without changing anything when
  another CarPlay mod is on the unit.

### Fixed

- The map no longer flickers after **VIEW**.
- The arrival-time frame is no longer cut off or skipped by the phone.
- The car marker and the street name on the map sit above the arrival-time frame, on both
  layouts, with or without the phone's maneuver card; the frame and the call card keep the same
  gap from the gauges.
- Without the phone's maneuver card the car marker stays near the middle of the map on the small
  gauges.
- The arrival-time frame appears without pressing VIEW; the arrow comes back after the cable is
  reconnected.
- The maneuver arrow fills once as you approach the turn instead of refilling in a loop. Drawing
  it takes almost no processor time now (measured in my car: 13% of one core before, 0.3% now).
- Installation guide: after the MMI restart, switch the engine off for a minute. An MMI restart
  does not restart the cluster.
- Lane arrows under the maneuver go away once the turn is taken ([#16](../../issues/16)).
  Confirmed by the reporter.

### Known limitations

- S4 Sport cluster layout and wireless adapters are not supported yet.

## 1.0.1 (2026-11-02)

### New

- The installer reports whether NavIgnore is installed.
- Touchpad modes: arrow keys (default) or factory mode for Android-over-CarPlay boxes.
- One-file update (`…-update.zip`).

### Fixed

- "Ghost" arrow left on the cluster after the end of a route: the arrow now disappears together
  with the tile.
- First route after an MMI start: the tile no longer shows in the wrong place; it appears
  2–3 s later, in the right place.

## 1.0.0 (2026-10-29)

First release: CarPlay map on the cluster (both layouts, VIEW), map zoom with the steering-wheel
roller, 3D maneuver arrow with distance and destination flag, CarPlay calls, recent calls and
album art on the cluster, Siri on a short press, MMI touchpad as arrow keys.
