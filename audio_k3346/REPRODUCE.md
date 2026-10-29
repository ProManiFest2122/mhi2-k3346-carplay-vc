# Reproducing the audio build

What you need, what is pinned, and what cannot be reproduced from this repository.
Nothing here touches a car. Read `README.md` first for scope and status.

## Pinned inputs (all public)

| Input | Pin | Used for |
|---|---|---|
| luka-dev/mib2q-carplay-rgi | `a63d54741839c7d30d32dd4e149f693856bcb0db` | reference audio code (`reference-audio-a63d547/`) |
| luka-dev/mib2q-carplay-rgi | `10fec32079477b31e03683aee453eb4faf21442b` | older pin, still used by some scripts (`upstream/`) |
| mstorsjo/fdk-aac | `c41c3c7516ea8d0d78a53b96b30bed3a29745cc6` | AAC decoder (built from source, never shipped) |
| vocho/openqnx | `cc95df35` (sparse: `trunk/lib/c/public`, `trunk/services/system/public`) | QNX headers |
| xpack arm-none-eabi-gcc | `13.3.1-1.1` (darwin-arm64 tarball sha256 `e37c379cbc423353902aada84b2b4d5a9316c2c8746c9608e69c8ac23e654aa3`) | ARM cross compiler, install yourself |

`reference-audio-a63d547/` is an unchanged export of luka a63d547. Our local changes live only in `prototype/` and
`scripts/prepare_full_audio_port.py`; the old `upstream-patches` of the 74b0d70 pin are superseded and not needed.

## Verified in a clean checkout (2026-10-29)

On a fresh copy of this folder (macOS arm64, no other files from our workspace): `scripts/fetch_deps.sh`, the two stock
libraries below, then `bash scripts/test_all_host.sh` printed `ALL HOST SCENARIOS PASS` (about 22 s). The fetched
`reference-audio-a63d547/` and `upstream/` were byte-identical to our working copies apart from our `PROVENANCE.md` files.
Needs `ffmpeg`, `python3` with `numpy`, and a C/C++ compiler. `MHI2_TOOLCHAIN` was exported in that run, but the host scenarios do not use it.

**Not verified from a clean checkout:** the ARM/QNX build chain and the installer ZIP builders (see below).

## Steps

```bash
cd audio_k3346
bash scripts/fetch_deps.sh                      # luka exports, fdk-aac, openqnx into ./deps
# put the two stock libraries from your own firmware at the paths below
bash scripts/test_all_host.sh                   # host scenarios (bootstraps the FDK host decoder and AAC fixtures)
```

Overrides: `MHI2_TOOLCHAIN` (xpack gcc directory), `MHI2_QNX_HEADERS` (default `./deps/openqnx/trunk`),
`MHI2_FDK_SRC` (default `./deps/fdk-aac`). Output goes to `./artifacts/`, created on demand.

## ARM build chain (not verified here)

Intended order: `build_fdk_decoder.py qnx` and `build_native_type103.sh` (codec libraries), `build_native_nvidia_sink.sh`
(this one passes in a clean copy), `build_native_buffered_pipeline.py` with `PIPELINE_VARIANT=nv_hook` (makes link-only views of
stock `libc`/`libm`), then `build_native_hook_so.py` (`HOOK_SO_QUALIFIED=0` is the inert variant, `HOOK_SO_LOCAL=1` the
Bluetooth/USB/SD one). Installer ZIPs: `build_mib_audio_hook.py` (CarPlay), `build_mib_audio_bt.py` (BT).

Known blockers on a clean machine, found while checking: `build_native_buffered_pipeline.py` and `prepare_native_media.py` import
`mhi2_ifs` (IFS reader from the qemu-mhi2 tools, not included here, put it on `PYTHONPATH`) and read libraries out of your
firmware; `build_fdk_decoder.py qnx` fails in a clean copy for a reason not yet diagnosed; `tests/` (installer tests) expect
the built package directories under `artifacts/`. Treat this section as a map, not a working recipe.

## What you must supply from your own unit

| File (from firmware) | Expected path | sha256 on the K3346 we used |
|---|---|---|
| `/eso/lib/libairplay.so` | `artifacts/stock/eso/lib/libairplay.so` | `34503a9f18799420005d7bd40c3cbf84634c8c9030bb0dcba79d519f0227e949` |
| `/armle/usr/lib/libnvaudio.so` | `artifacts/stock/armle/usr/lib/libnvaudio.so` | `aeb9153420c07027f511939c6bf5b26708f71adf13ecf69b661469c483f7b2de` |

Both come from the K3346 `app70` image. If your hashes differ, the hook's ABI assumptions (private handle offsets, codec tables)
may not hold; compare before trusting any result. The ARM chain also reads stock `libc`/`libm`, and the installers verify
hashes of the HMI jar, `lsd.sh`, `smartphone_integrator.json` and `mm-renderer`; see `docs/INSTALL_READINESS_20261028.md`
and `docs/CAR_FACTS_K3346_20261016.md`. These files are never redistributed.

## Not reproducible from this repository

- Emulator runs (`run_*`, `build_pipeline_sd.py`, `build_capture_qemu.py`, `check_hw_handshakes_isolated.py`) need a private K3346 qemu setup that is not published.
- RCC/DSP analysis scripts (`inspect_rcc_*`, `rcc_dsp_image.py`, `decode_mibroot_routes.py`) need extracted RCC firmware.
- The car measurements in `docs/` were taken on one car; they are evidence, not a test you can rerun.
