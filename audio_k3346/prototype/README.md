# K3346 audio prototype

Локальні експерименти для MHI2_ER_AUG22_K3346/MU1438. Це не SD/update пакет.

- `mhi2_scene_adapter`: S16/48k stereo→six-channel buffer; L/R/Ls/Rs active, C/LFEzero. Exact OFF, reset/mute, bounded1024frame inputs,1024frame buffering latency; no allocation after init.
- `nvaudio_upmix_fixture`: single serialized synthetic-client ABI experiment (compiled MPL1). Not thread-safe across audio owners; no focus/route integration, backpressure transaction/retry, large-packet support or automatic production activation.
- Native APAC probe: six real fixture packets; QNX numeric gate passed. Live type103 integration pending.

Host validation: `bash scripts/check_prototype.sh` from audit directory.
ARM builds: `bash scripts/build_native_probe.sh`, `bash scripts/build_native_scene.sh`, `bash scripts/build_native_apac.sh`.
Emulator: prepare separate media then run `AUDIO_PROBE_APAC=1 python3 scripts/run_native_probe.py` or `AUDIO_PROBE_ADAPTER=1 ...` or `AUDIO_PROBE_INTERPOSE=1 ...`. Original media unchanged; snapshot disk and UI lock. Do not prepare while that VM runs.
Latest IFS omits old standalone native-scene-probe and failed route5 preload fixture to fit cloned restore-file capacity; their artifacts are retained. APAC image has only512bytes restore-size margin, so review capacity before adding anything.

Results, failed comparisons and remaining gates: ../REPORT.md. Pinned sources/licenses: ../upstream/PROVENANCE.md.


## Live-baseline audio continuation (2026-10-15)

Live libairplay/libnvaudio/driver binaries match the tested stock. Music48k/S16/stereo and nav16k/S16/mono informed new format-transition/error-latch tests. See ../LIVE_AUDIO_IMPLEMENTATION_20261015.md.

Experimental native AirPlay configure seam is built separately in nvaudio-upmix-airplay-experimental.so (defaultMPL5) and nvaudio-upmix-airplay-test.so (syntheticMPL1). Same-rate activation is host-tested; no real-phone/focus/thread-safety claim. Current testing remains single-owner. Exact private handle offset+4 belongs only to the matchedK3346 binary. Native write errors latch until reconfigure/close, so automatic seamless recovery is not implemented.

For the native AirPlay-driver harness: prepare with `python3 scripts/prepare_native_media.py --audio-seam`, then `AUDIO_PROBE_AIRPLAY=1 python3 scripts/run_native_probe.py`. This profile omits APAC/adapter probe executables to fit restoreIFS; originals retained on host. Reprepare default profile before APAC/adapter tests. Runner now checks requested probe presence before starting QEMU. Never prepare while a VM is active.
