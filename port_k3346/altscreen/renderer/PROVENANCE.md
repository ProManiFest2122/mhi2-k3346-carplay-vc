# Renderer source provenance and build status

**Upstream source:** [joeyQuery/MHI2-altScreen](https://github.com/joeyQuery/MHI2-altScreen) commit `59b7fa6d1b81e0da266cf4f70b4cf54c34bc03dc`, file `src/vc_render/vc_render.c`, Git blob SHA `0191a785eb2040eec1cb3926b5c0a2454fa34242`.

**License:** upstream GPL-3.0 (full text `../LICENSE-GPL-3.0`). This repository now includes the upstream editable C source and rebuild instructions, rather than linking to it as the sole source record.

**Critical qualification:** the AltScreen v1.0 archive contains the *unchanged vehicle-tested v8S* compiled renderer, SHA-256 `49b2d5c38899dd4aab02015962b5df2cb39162931b1e973b17216ce076501186`. Exact bit-for-bit correspondence between the public upstream C file here and that historical compiled executable has **not** been established. No binary editing is permitted. Do **not** assert this source is an exact match, nor claim fully reproducible release binaries before the missing historical adaptations are recovered and rebuilt.

The renderer accepts `--out 1 --layer 58 --w 1440 --h 542 --fps 30 --ts wall`. These parameters are supplied at runtime by the car-tested `deployment/mib2high-lvds-control.sh`. They do not have to be the C default constants.

**Future changes:** edit source and build a new executable with your licensed QNX SDK; test independently. Never substitute a newly built but unverified renderer into the immutable v8S installer.

## Recovered K2161-specific source checkpoint

The actual project's **K2161-adapted renderer C source** is now also preserved in [`k2161/vc_render.c`](k2161/vc_render.c), unchanged from historical Git commit [`f843f2027bca419903941dd4fc7bbc63bf3b2a44`](https://github.com/edzenis/mib2high-carplay-altscreen-rgi/commit/f843f2027bca419903941dd4fc7bbc63bf3b2a44), where it lived at `native/vc_render-k2161/vc_render.c`. The matching copy also exists in Library archive `k2161-gen2-av11-sink-checkpoint.zip`.

- Archived C file Git blob: `9c2185f6a3ea099177f6c4fb9f88d45dc7da595e` (10,999 bytes).
- This later checkpoint's defaults are **800×480 at 30 fps**, whereas the v8S controller supplies **1440×542** at runtime through command-line arguments. The source retains CLI dimensions support.
- The v8S renderer's archived ELF has SHA-256 `49b2d5c38899dd4aab02015962b5df2cb39162931b1e973b17216ce076501186` and size 23,822 bytes.
- A rebuild of the archived K2161 source with the saved GCC 4.9.4 QNX toolchain produced a different 29,441-byte executable. This establishes the source is **available and buildable**, but **not** bit-for-bit parity with the compiled v8S renderer; different compiler settings or source revisions remain possible. Do not replace the verified v8S binary with that rebuild.

The K2161 file originated as an adaptation of Joey's GPL-3.0 renderer source. Its provenance is documented; original upstream notices and GPL terms apply to the adapted renderer.
