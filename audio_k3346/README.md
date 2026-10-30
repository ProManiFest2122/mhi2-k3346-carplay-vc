# audio_k3346: stereo to 5-speaker upmix research for MHI2 K3346 / MU1438

Independent audio work for the same unit this repository targets
(`MHI2_ER_AUG22_K3346`, MU1438, G22/MLB Evo, Tegra3, QNX 6.5, internal amp,
retrofit centre speaker, stock sub). Goal: upmix CarPlay stereo, and separately
Bluetooth/USB/SD, to the five speakers. Offered as source and evidence for
reuse, not as an installable package.

**Status: NOT READY for installation. Nothing here has been run as an enabled
mod on a car.** The hook stays inert (`MHI2_NV_SIX_CHANNEL_QUALIFIED=0`).

## What is tested where

| Evidence | Covered |
|---|---|
| Car (read-only + RAM tests, by ear and logs) | MPL1/MPL5 geometry, which channels reach which speaker, centre/sub behaviour, CPU cost, `mm-renderer` identity, ANN3 nav-prompt stream. See `docs/CAR_FACTS_K3346_20261016.md` |
| Host / ARM-QNX emulator | NVIDIA PCM sink, FDK-AAC bridge, realtime upmix, live level control, `LD_PRELOAD` claim of the real libairplay `CAudioDriverIF`, installer simulations |
| Not tested | Hook inside the real `dio_manager` in a live CarPlay session; Bluetooth shim via `medialauncher`; the MMI "CarPlay UpMix" row with the OEM 3D row hidden; independent sub/LFE; on-unit CHECK, backup, restore |

Full gate list: `docs/STATUS.md`, `docs/INSTALL_READINESS_20261028.md`. Chronological evidence log: `docs/HANDOFF_CLAUDE_20261015.md` (Updates 1-51).

**To rebuild and re-run the host tests, read [`REPRODUCE.md`](REPRODUCE.md)** (pins, one-command dependency fetch, the two stock libraries you supply, and what is not yet reproducible).

## Layout

- `prototype/` C sources (SCENE adapter, NVIDIA sink, hook, resampler, tests), Java UI (`UpmixToneUi`), installer scripts
- `scripts/` build, host test and emulator runners (`scripts/test_all_host.sh`)
- `tests/` Python installer and Java tests
- `docs/` findings and status

## Not included, on purpose

- Prebuilt binaries and installer ZIPs (they would carry firmware-derived files and an FDK-AAC build). Build from source; hashes of the reviewed packages are in `docs/INSTALL_READINESS_20261028.md`. Ask if you want the ZIPs.
- Firmware images, `mm-renderer`, stock HMI jars, the emulator, car logs and the owner's sound datasets.
- The reference sources the hook is generated from (`scripts/prepare_full_audio_port.py` expects luka-dev/mib2q-carplay-rgi at a63d547 / 74b0d70). Fetch them yourself; their licence applies.

## Integration notes for the CarPlay base

1. `smartphone.awk` removes any `LD_PRELOAD` line from `smartphone_integrator.json`; audio needs a colon-merge (`<base>.so:<audio>.so`). `prototype/mib-hook/mhi2-audio/scripts/json.awk` shows a merge that removes only its own entry.
2. Preloads into one `dio_manager` must not export generic names. Build with `-fvisibility=hidden` and `scripts/hook_so.exports.map` (164 -> 17 exports).
3. Package install order that has been host-checked: base, then Ukrainian, then audio. Restore in reverse. Reinstalling the base drops the audio preload entry.

Some documents are in Ukrainian; translations on request.

## Licence

Our code: GPL-3.0-or-later, like this repository. Code adapted from LuKa
(`mib2q-carplay-rgi`) keeps its notices where present (e.g. UpmixToneUi.java, upmix_lang.py). See `NOTICE.md`.
No file from chefranov/mhi2-au37x-carplay is included.
