# K2161 v8S — v8N left card + completion-driven CarPlay control

> **Public-release note (October 2026):** This is the historical v8S *offline* build/test report. AltScreen v1.0 was subsequently tested on the Audi Q7 4M; see [AltScreen v1.0 release notes](RELEASE_NOTES.md). Do not read the old statement "no real iPhone session" as the current release validation status.


Baseline: v8R, **offline-validated only**, never substitute for vehicle results.
All changed runtime code is rebuilt from C source using saved QNX 6.5 ARMv7 GCC 4.9.4.
No ELF, object-file, Java JAR, bytecode, or other compiled binary was patched.

## Exact requested changes

- SMALL GAUGES, `maneuverLayout=leftAligned`: restore original v8N safe areas,
  view 0 idle `(360,65,720,367)` and view 1 active route `(132,85,840,332)`.
  v8N's active-route card was farther left.
- BIG GAUGES, `maneuverLayout=topAligned`: retain v8R safe areas,
  view 2 idle and view 3 route `(490,72,460,378)`.
- Preserve all four view indices, existing Apple cluster URLs, 1440×542
  video, physical profile, RGI native/Java behavior, zoom and NavIgnore.
- Gauge changes replace the latest pending ViewArea (150ms settling interval);
  a newer state supersedes obsolete pending operations after their ack.
- `showUI` sent once on new stream/session, or on a REAL gauge-slot change
  that changes the alignment URL; do not resend on unchanged URL/route events.
- Serialize `showUI → updateViewArea → forceKeyFrame`. Each advance requires
  the corresponding **AirPlay completion callback with status=0** (not simply
  the successful SendCommand queue return value).
- Every async completion is protected by session generation and command token.
  Stale completions after VIEW changes, timeouts, and reconnects cannot advance
  an outdated step. A command without completion times out after 2500ms and
  fails closed, until a subsequent genuine state change or fresh session.
- The original first-real-frame `request_cluster` activation remains, now via
  the same serialized worker; first activation is allowed before the VC
  active marker exists. Reconnect resets the successful-URL state.
- The original M.I.B. SD installer, collector and exact rollback are kept,
  with only the release namespace and version text changed.

## Source/build/test evidence

- `native/altscreen_session.c`: all behavior changes in editable C.
- `native/gauge_layout_urls.h`: comment update; URL values unchanged.
- `config/mib2high-lvds-viewareas.conf`: v8N left + v8R big-gauge region.
- `deployment/custom.sh`: v8S names only, no changed on-car install actions.
- `tests/test_v8s_control_mock.c`: host runs the exact session C code with
  fake AirPlay callbacks and verifies correct serialized steps, coalescing,
  refusal, negative completion, timeout, late callbacks and fresh reconnects.
- `diagnostics/test_v8s_release.py`: clean install, v8R upgrade, collection,
  failure rollback and exact restoration mock tests.
- `release_evidence/`: QNX direct rebuild log, full source diffs, hashes,
  host tests and installer JSON.

## SD install and diagnostic collection

Unzip ZIP directly at SD root; run M.I.B. `mod/custom.sh` once for installation.
Reboot manually. Test Apple Maps, Google Maps and Waze routes in both gauge
layouts, including rapid VIEW switching and provider handovers. Run again to
collect numbered logs under `mod/mib2high-carplay-rgi-v8s-simple/collected/`.

For exact rollback, create
`mod/mib2high-carplay-rgi-v8s-simple/ROLLBACK`, rerun `mod/custom.sh`, reboot.
Do not discard the pre-v8S backup on the SD card.

## Risk/validation status

No real iPhone/K2161 session was available for the test. Successful callback
serialization does NOT prove that Google Maps/Waze will choose to draw the
instruction card or that their layout responds identically. The 2500ms timeout
and full callback API integration need the real car test. No undocumented
extra layout, renderer or RGI behavior has been changed.
