# MHI2 audio — Claude takeover checkpoint, 2026-10-15

## Owner request and scope
Continue full audio parity with `luka-dev/mib2q-carplay-rgi` pinned at
`74b0d70d7cae89061cd586f854bf2cc68c8fd50f`, independently of Ukrainian localization.
Target: Audi MHI2_ER_AUG22_K3346 / MU1438 / G22 / Tegra3 / QNX6.5,
internal amplifier, retrofit center, stock subwoofer. FULL TASK IS NOT FINISHED.
Owner requested a local commit and handoff to Claude. No push requested.

Read `HANDOFF_AUDIO_NEXT_CHAT.md` for the inherited detailed state and
`FULL_AUDIO_PARITY_20261015.md` for the complete feature matrix.
Their old pause/next-build statements are superseded by this checkpoint.

## This chat's changes and verification
- Re-ran `bash scripts/test_buffered_nvidia_pipeline.sh` after the inherited
  MHI2_PIPELINE_NATIVE conditional change: PASS, 37888 frames / 74 writes,
  actual APAP framing/auth -> APAC/AAC/APAC -> reference renderer/ring ->
  NVIDIA pump with injected native boundary. Includes unchanged interposer
  baseline (64 races, 32 stale SETUP, teardown/control scopes).
  Final evidence: `logs/buffered-nvidia-pipeline-commit.log`.
- Native compiler preflight exposed AF_INET missing from the deliberately
  minimal `prototype/qnx-compat/sys/socket.h`. Added a native-test-only
  definition in `prototype/test_buffered_nvidia_pipeline.c`. It is used only
  by included socket mocks; their baseline is skipped on native. No native
  socket is opened by this harness. Production reference source is unchanged.
- Added `scripts/build_native_buffered_pipeline.py`: builds ARMv7 softfp/NEON
  full render harness and `_start`, links APAC/AAC/FDK and NVIDIA dependencies.
  Uses real public QNX C/pthread headers and local math/socket compatibility.
  Builds with PIC and function/data section GC. No COPY relocations.
- Stock libc/libm come from checksum-validated imagefs inside the original
  base NOR KERNEL_PRIMARY (Android wrapper -> zlib -> startup imagefs).
  ui1024 kernel imagefs has diagnostic edits and fails original additive
  checksum, so the build deliberately uses `media/base/nor.bin` instead.
- Stock QNX libraries have dynamic metadata but stripped ELF section tables;
  GNU ld cannot use them directly. Builder emits LINK-ONLY views from real
  exported function/object symbols, preserving sonames; NVIDIA uses the
  previously built five-export link stub. These files MUST NEVER enter the
  guest, SD image or a vehicle package. Guest must use its real stock libs.
- Native executable build PASS; ELF audit `artifacts/native-buffered-pipeline/elf-audit.txt`.
  DT_NEEDED: libmhi2_apac.so, libmhi2_aac.so, libmhi2_fdk.so, libnvaudio.so,
  libc.so.3, libm.so.2. `_Stderr` uses GLOB_DAT, not COPY relocation.
  Build log: `logs/build-native-buffered-pipeline.log`.

## Exact reproduction on this host
From `<workspace>/analysis/audio-k3346-20261014`:

```bash
bash scripts/test_buffered_nvidia_pipeline.sh
python3 scripts/build_native_buffered_pipeline.py
```

Host runner first regenerates the TEST-only reference codec-loader seam at
`artifacts/native-nvidia-sink/reference_interposer_with_codec.c` and host APAC
library. Native builder requires existing APAC, FDK, AAC and NVIDIA link-stub
artifacts from the earlier builds, documented in the inherited handoff.
Toolchain lives at `../../runtime/toolchain/xpack-arm-none-eabi-gcc-13.3.1-1.1`.
QNX headers: `/private/tmp/mhi2-openqnx-headers/trunk`.
FDK source: `/private/tmp/mhi2-fdk-aac-20261015`; retain its NOTICE/source archive.

## Next concrete step — NOT DONE
Native executable has NOT run. Its `_start` currently expects fixture paths:
`/mnt/audio-pipeline/frames.bin` and `/mnt/audio-pipeline/raw-aac-fixture.bin`.
No native SD image, entrypoint runner flag, mount or SD payload exists yet.
The binary can compile/link and still fail at dynamic loading or runtime.
Do not report native pipeline PASS until guest exit + test PASS are captured.

IFS restore capacity is 29652992 bytes; AAC profile uses 29622784.
Do not add full pipeline libraries past this limit or alter original images.
Recommended next experiment: separate removable SD test payload, using only
an experiment-owned copy of debug_shell_local.py. Current source supports
IF_SD index 1/2; capture binary support is NOT yet proven.

Evidence already found for that experiment:
- `runtime/qemu-mhi2/hw/arm/mhi2_harman.c` attaches removable cards with
  `drive_get(IF_SD, slot + 1, 0)` and GPIO CD/WP signals.
- `runtime/qemu-mhi2/tools/mhi2/qnx6_image.py` exports
  `build(srcdir, outpath, slack_blocks=512, partition_blocks=None)`.
- Current shared debug_shell_local.py attaches only index0 eMMC. Add
  `-drive if=sd,index=1,format=raw,snapshot=on,file=...` only to its private copy.
- Previous guest log run20261015-133323 shows stock drivers start SD readers
  at 0x78000000 and 0x78000400, disk names sda and sdb; mmcsdpub watches
  `/dev/sda0` and `/dev/sdb0`. Actual card device/mount must be verified.
- Payload allowlist: native executable, real APAC/AAC/FDK libraries,
  two fixture files, FDK NOTICE. Do not copy LINK-ONLY views, libc, libm,
  libnvaudio stubs, or whole artifact directories.

## Remaining integration and acceptance limits
Production `artifacts/full-audio/port/audio_hook_k3346.c` still uses QSA;
NVIDIA pump is NOT connected to its HTTP worker/focus path. This harness uses
reference rendering and real codecs but mocked unrelated receiver boundaries.
HTTP/TCP, live phone, focus, full FLUSHBUFFERED/recovery, downstream ring purge,
whole-stream/gap-free output, Tegra CPU budget, RCC and physical speakers are
unverified. Native codec successes from earlier runs do not close these gates.
RCC routing remains the separate exact-profile blocker detailed in the inherited
handoff. Do not advertise vehicle 6ch or install old preview ZIPs as full 5.1.

No VM was started in this chat. No vehicle writes, flash, original-image edits,
subagents or push. Unrelated QEMU working-tree changes were left untouched.

## Git checkpoint boundary
This directory originally had no .git; the surrounding workspace has none.
Created an isolated local source repository here for the owner's handoff request.
The initial commit captures authored scripts/prototypes/tests and project docs
from earlier audio work plus this chat's changes, not only the two new edits.
Git excludes reference/upstream sources, artifacts (about 4.1 GB), vehicle return
bundles, build outputs and all logs except the three selected verification logs.
All excluded local data remains on disk. A Git-only copy cannot build alone;
transfer the workspace or separately restore the pinned reference/dependencies.

## Update (Claude, 2026-10-15): native pipeline executed in the QNX guest
- New: `scripts/build_pipeline_sd.py` (needs python3.13 for mmap trackfd in the shared
  `qnx6_image.py`) builds `artifacts/native-pipeline-sd/sd.img` (allowlist payload only,
  manifest with sha256) and `scripts/run_native_pipeline_sd.py` runs it with an
  experiment-owned helper copy and `-drive if=sd,index=1,snapshot=on`. Shared QEMU tree untouched.
- Run `artifacts/runs/20261015-141931`: the SD card appears as `/dev/sda0` (the guest has no
  `/dev/sdb*`; eMMC is not exposed as sda). `mount -t qnx6 -o ro /dev/sda0 /mnt/audio-pipeline` works
  (`mkdir` there is "Function not implemented" and is not needed). The binary exited 0 and printed
  `PASS (37888 frames, 74 writes)`; the log shows real `libnvaudio nvaudio_pcm_open MPL1 success`,
  `deva-nvaudio TDM1_OUT_MPL New buffer ... FadeIn`, `nvaudio_pcm_close success`.
- Limits: emulator only; the burst is not paced in real time (deva-nvaudio logged "No buffer" and
  a second "New buffer" mid-run, so gap-free output is NOT shown); no TDM capture analysis of this run yet;
  HTTP/TCP, focus, FLUSHBUFFERED, RCC and speakers still unverified (see the gap list in
  `FULL_AUDIO_PARITY_20261015.md`).

## Update 2 (Claude): NVIDIA sink instead of QSA in the K3346 hook (gap items 1-3, partly)
- `prototype/nvidia_qsa_shim.h`: QSA-shaped facade over `mhi2_nv_sink`. `prepare_full_audio_port.py`
  now also writes `artifacts/full-audio/port/audio_hook_k3346_nv.c` (anchored edits, fails if any anchor
  moves): QSA lookups -> shim, `snd_pcm_*` exports made static, no inline retry, one commit of
  accepted+uncertain frames, then `detachedNeedsPrepare` so the next cycle reopens + silence preroll.
- Tests: `bash scripts/test_nv_hook_detached.sh` (host, log `logs/nv-hook-detached.log`) and in guest
  `PIPELINE_VARIANT=nv_hook` build (`build_native_buffered_pipeline.py`) + `build_pipeline_sd.py` +
  `run_native_pipeline_sd.py`: run `artifacts/runs/20261015-143908` PASS (49 writes, 3 real open/close).
  Covers cold bootstrap, nominal writes, injected write failure (no replay, reopen, preroll), timeline flush.
- Not covered: the pump struct itself is not used by the hook (hook peek/commit + shim give the same
  prefix contract); stock type-100 path untouched (stock libairplay -> nvaudio, no interposer yet);
  real route callbacks stubbed; backpressure/timeout semantics of real nvaudio_pcm_write unproven;
  reopen != proven downstream purge; QSA-only features (type-100 upmix, status cadence) not ported;
  `MHI2_NV_SIX_CHANNEL_QUALIFIED` defaults to 0, so production build refuses 6ch until RCC gate closes.

## Update 3 (Claude): live level, SoundCheck, ramps in the NV hook (gap items 7-8)
- Found: our Java writes `/tmp/mhi2_audio_level`, reference hook read `/tmp/carplay_upmix_level`, and live
  changes came only via the framework bus `CMD_UPMIX` (absent here) -> no live update. NV variant now
  reads `/tmp/mhi2_audio_level` and `spa_upmix_level_poll()` (>=200 ms rate limit, bad content ignored)
  runs in the detached writer (`prepare_full_audio_port.py` anchored edits).
- `bash scripts/test_nv_hook_detached.sh` runs 3 scenarios (log `logs/nv-hook-detached.log`):
  APAC write/recover/flush; live level OFF->AGGRESSIVE->OFF on a stereo AAC stream (poll inside window not
  seen, then adopted; first surround write after 8 periods, 90% after 44, i.e. a glide not a jump; "9" ignored;
  OFF returns exact zero surround); SoundCheck (setup target -16 dB: settled ratio 0.355 = expected 0.3548
  from `spa_soundcheck_main_gain`; mid-stream tuple change ramps to 0.200 = 0.1995, max step 0.062 < half delta).
- Guest: same 3 scenarios PASS on QNX with real libnvaudio, run `artifacts/runs/20261015-145949`
  (`PIPELINE_VARIANT=nv_hook`, `PIPELINE_TIMEOUT=900`; log `logs/nv-hook-live-guest-20261015-145949.log`).
- Test pitfall: `build_extension` cookieId must stay constant per AAC stream, a new id rebuilds the decoder
  and yields silence. SoundCheck target at SETUP applies from the first period (no ramp); only metadata changes ramp.
- Not covered: real MMI row/Java write timing on the unit, real iPhone SETUP plist target, ramp audibility,
  type-100 stock path, real route callbacks. `UpmixSettingTest` Python wrapper still OK.

## Update 4 (Claude): TDM capture of the NV hook runs - NEGATIVE / inconclusive result
- Tools: `NV_WATERMARK=1 NV_DUMP=... bash scripts/test_nv_hook_detached.sh` (host: expected native write stream, C=write seq,
  LFE=frame index; records 0 audio/1 silence/2 injected failure/3 flush), guest runs with TDM marker 100/101/102 and optional
  `NV_PACE=1` (`PIPELINE_ONLY_BASE=1 PIPELINE_PACE=1`), `scripts/analyze_nv_hook_tdm.py RUN`. Control programs: `prototype/nv_replay.c`
  (`PIPELINE_VARIANT=nv_replay`: replays the recorded writes with no codec load; `PIPELINE_RAMP=1`: clean ramp+ids).
- Runs: hook unpaced 151252, hook paced 151521, replay paced 151834, clean ramp 152008 (analysis JSON in `logs/`).
- Result: the capture does NOT preserve sample order/uniqueness for time-varying content, even for the clean ramp from a minimal writer
  (ramp: 18979 of 30720 frames exact, duplicated and out-of-order 256-frame blocks, e.g. a write's tail re-read after later writes;
  replay: 2% exact). So gap-free output, replay-after-recovery and purge-on-reopen CANNOT be proven or refuted from this tap.
  Earlier one-hot plateau results (constants) are unaffected. Cause not determined (emulated DMA ring/driver block handling vs decode
  of the packed bytes for varying data); do not blame the hook: the control without the hook shows the same.
- Weak observations only: the confirmed pre-failure chunk (seq 17) is almost absent in the capture (close/reopen discards unplayed audio,
  consistent with "reopen purges" but not proof); the never-forwarded failed write (seq 18) never appears.
- Hook-level guarantees stay asserted by the host/guest tests (no replay of the failed period, one reopen, silence preroll, flush reopen).
- Next options: validate the tap with a driver-independent signal (e.g. capture before the stock mux, or fix the DMA ring read in
  the emulator copy), or accept host/guest assertions and mark gap-free/purge as physical-vehicle gates.

## Update 5 (Claude): gap item 4 - APAC<->AAC switching, 44.1k fallback, async mask worker
Gap-free/purge stay a physical-vehicle gate (owner decision after Update 4).
- `scripts/test_nv_hook_detached.sh` now runs 6 scenarios (host) and the same 6 in the QNX guest
  (`PIPELINE_VARIANT=nv_hook`, default scenarios level,soundcheck,switch,mask,rate44; run `20261015-153224`, log
  `logs/nv-hook-all-scenarios-guest-20261015-153224.log`), all PASS with real libnvaudio on ARM:
  - switch: APAC5.1 -> AAC stereo (scene upmix) -> ... x3 on ONE sink connection, no fault/reopen, frames conserved
    (in=61440 = native out + ring), surround only on stereo segments.
  - mask: SCENE with the real 32 KiB net, sync reference vs async worker (priority 17 thread): async submitted 94,
    completed 94, busy 26 (bursts are faster than real time), surround energy ratio 1.515 (the async path is late/perturbed,
    not identical), stale-generation reset exercised, 20 start/stop cycles without hang. Real-time margin on Tegra NOT measured.
  - rate44: index 22 (AAC-LC/44.1k) decoded by FDK and resampled 160/147 inside `prototype/fdk_aac_bridge.c`
    (own polyphase `prototype/resample_44k_to_48k.h`, 32 taps x 160 phases, state kept across AUs): 1114.245 frames/AU incl.
    16-frame latency, tones 1000.2/3000.0 Hz, RMS as encoded. Fixture made with ffmpeg (`artifacts/fdk-decoder-host/raw-aac-44k-fixture.bin`).
    Unit test `prototype/test_resample_44k.c` (counts 1114/1115, chunk independence bit exact, residual < -83 dB up to 8 kHz,
    -43.9 dB at 18 kHz). Hook reports index 23 after the 48k bridge output (verified-index remap), harmless.
- ARM: `scripts/build_native_aac.sh` now links libm (stock libm.so.2 at runtime; the link-only view never ships).
- Not covered: the reference does not advertise index 22 either; real iPhone selection of 22, ASC variants other than 0x1210/0x1190,
  CPU cost of the resampler on Tegra, listening quality.

## Update 6 (Claude): gate 7 - AAC host vs ARM difference measured
Tools: `prototype/aac_dump.c` (host and `PIPELINE_VARIANT=aac_dump` guest build/SD/run), `scripts/compare_aac_dump.py`.
Guest run `20261015-155423` vs host: all 800 chunks (25600 frames stereo) recovered via CRC + duplicate lines; 50887 of 51200 samples identical,
313 differ by exactly 1 LSB, max |diff| = 1, RMS diff 0.078 LSB against signal RMS 4063 (about -94 dB). So the earlier FNV mismatch is
last-bit rounding only (ARM -Os/softfp-NEON vs host arm64 -O2), not a decoding error. Result in `logs/aac-host-vs-guest-pcm.json`.
Not tested: other streams/bitrates, SBR/PS content, which exact code path rounds differently.

## Update 7 (Claude): stock type-100 path via nvaudio interposers (gate 3, partial)
- `prototype/nvaudio_stock_bridge.h` (build with `-DMHI2_NV_STOCK_BRIDGE`, appended to `audio_hook_k3346_nv.c` by `prepare_full_audio_port.py`):
  exports `nvaudio_pcm_open/set_pcm_info/create_metadata/write/close`. Claims ONLY the stock media device (`SPA_NV_STOCK_DEVICE`, default "MPL5")
  and ONLY 48k/S16/stereo; everything else (other devices, 16k mono navigation) goes to the real libnvaudio. The claimed stream is translated to
  the hook's QSA-shaped logic, so the reference type-100 upmix, handoff with the type-103 receiver, route claim and recovery run unchanged.
  Without `MHI2_NV_SIX_CHANNEL_QUALIFIED=1` NOTHING is claimed (stock audio untouched). The shim resolves the real nvaudio via RTLD_NEXT,
  falling back to dlopen("libnvaudio.so") (test dlsym mock must pass `nvaudio_*` through).
- Host `bash scripts/test_nv_stock_bridge.sh` (log `logs/nv-stock-bridge-host.log`): other device and navigation format pass through; stock music
  becomes 6ch with the stock L/R exact (surround zero at level OFF), one native connection; realtime upmix level 4 gives surround energy; format
  change 48k -> 16k mono -> 48k works; type-103 starting under a live stock stream replaces the stock fallback with ring audio on the SAME
  connection (0 stock frames in output) and after stock closes the detached writer continues without reopen. Needs a media main stream declared
  (`spa_stock_main_audio_begin`, what the hooked AudioStreamPrepare does) and >=50 ms after the stock release (route reacquire delay).
- Guest `PIPELINE_VARIANT=nv_stock` (run `20261015-161043`, log `logs/nv-stock-bridge-guest-20261015-161043.log`, claimed device MPL1): scenario returns
  0, real libnvaudio MPL1 open/close x4, deva `New buffer` events seen; counters/captures are host-mock only, the guest checks return codes.
- Not covered: the real `libairplay` `CAudioDriverIF` calling these exports (LD_PRELOAD of a built hook .so), real AudioStreamPrepare/Stop and
  dio_manager focus callbacks, the real device name used by stock on the unit (MPL5 is a candidate, unproven), 6ch qualification (RCC gate).

## Update 8 (Claude): hook as .so, real libairplay CAudioDriverIF in the guest (gate 3, partial)
- `scripts/build_native_hook_so.py` builds `artifacts/native-hook-so/libmhi2_audio_hook.so` (1.17 MB; ARM softfp/NEON, PIC; needs the link-only views of
  `PIPELINE_VARIANT=nv_hook` and `scripts/prepare_full_audio_port.py` output). Sources: generated `audio_hook_k3346_nv.c` (+stock bridge, NVIDIA sink, FDK bridge),
  reference audio/framework logging+state_trace+`airplay_seams.c`, and `prototype/hook_main_audio_only.c` (module table with only `audio_module_def`, minimal
  `hook_framework_init` = pthread_once -> `audio_init`, `hook_inject_message` returns BUSY: no iAP2/Cinemo/bus, so SCENE track-change injection is unavailable).
  `http210_seams.c`, `hook_framework.c`, `bus.c` are NOT used (wireless 210 header absent; bus needs arpa/inet.h). Built with `-DMHI2_NV_SIX_CHANNEL_QUALIFIED=1`
  and `SPA_NV_STOCK_DEVICE="MPL1"` for the emulator only; the release default stays 0. All undefined imports resolve in stock libc/libm/libnvaudio/ours except `shutdown`
  (socket symbol; libairplay itself imports it unresolved, so it comes from the process environment). Does not fit the IFS (30 KB headroom): delivered on SD.
- `scripts/run_hook_so_guest.py` (python3.13): SD image (hook, APAC/AAC/FDK, stock-driver probe) + guest run. Guest `LD_LIBRARY_PATH` must use the `/mnt/app/...` prefixes
  (`/mnt/app/root/lib-target` holds libnvparser, `/mnt/app/armle/lib` libnvss_video; without them the real libairplay does not load). `HOOK_SO_DISCOVER='cmd;cmd'` runs ad-hoc guest commands.
- Run `hookso-20261015-162833` (log `logs/hook-so-guest-20261015-162833.log`, hook sha256 dfc052b8e350...): probe `native-airplay-driver-probe` (real `CAudioDriverIF` ctor/open/configure/write/close from stock libairplay).
  A no preload: exit 0, stock path through libnvaudio MPL1. B with `LD_PRELOAD` hook, level file 0 and 4: exit 0 both, open=1, configure=1, write_errors=0, close_handle_cleared=1;
  hook log (`/tmp/carplay_hook.log`): `realtime upmix: armed (backend=scene ... period=768)`, `audio sink: writes=1 ... source=2ch target=6ch devStatus=4`, `stop periods=32`;
  deva shows `DTCP enabled!` after configure only in B. This proves: the stock bridge's format guess (48000/2/2 in the pcm_info struct) matches the real libairplay call, the claim reaches the NVIDIA sink at 6ch, close works.
- NOT proven: `upmixed=0` at level 4 too, because the probe never declares a media main stream (real `AudioStreamPrepare` needs a live stream object); so realtime upmix output, SCENE
  and the type-103 handoff are not exercised through real libairplay. Output content was not captured (no TDM tap in this run). Real AudioStreamPrepare/Stop, dio_manager focus,
  HTTP/iPhone, the real stock device name (MPL1 in the emulator, MPL5 on the unit is a candidate) remain open. Emulator only.
- Cleanup: the unconditional `fprintf` in `prototype/nvidia_qsa_shim.h` bind fallback is now under `SPA_NV_SHIM_DEBUG`; `scripts/test_all_host.sh` runs all host scenarios (PASS, `logs/all-host-20261015.log`).
  The .so was rebuilt after that edit (guest run above used the pre-cleanup build; the removed line never printed there).

## Update 9 (Claude): main stream declared through real libairplay driver calls (gate 3, partial)
- `scripts/build_stream_probe.sh` builds `native-airplay-stream-probe` (AudioStreamPrepare on a fake stream object: type 100 at +108, audioType 1 at +112, then the real
  `CAudioDriverIF` ctor/open/configure/32 writes/close, then AudioStreamStop) and `libmhi2_test_stream_stub.so` (test-only AudioStreamPrepare/Stop = no-ops). Guest order
  `LD_PRELOAD=hook.so:stub.so`: the probe's calls hit the hook, whose `dlsym(RTLD_NEXT)` finds the stub, so real libairplay never sees the fake object. Run:
  `HOOK_SO_STREAM=1 HOOK_SO_LEVELS=0,4 PIPELINE_TIMEOUT=400 /opt/homebrew/bin/python3.13 scripts/run_hook_so_guest.py`. The stub is never packaged.
- Run `hookso-20261015-163158` (log `logs/hook-so-stream-guest-20261015-163158.log`): levels 0 and 4 both exit 0 (prepare=0, open/configure=1, write_errors=0, stop=0, handle cleared). Hook log:
  `audio route: stock main stream=... phase=prepare audio_type=1`, `realtime upmix: armed (backend=scene)`, sink `source=2ch target=6ch`, `stop periods=32 upmixed=32`. That also shows the
  K3346 libairplay fingerprint (`spa103_private_profile`, dlsym of AirTunesServer_EnsureCreated + prologue bytes) is accepted on the real guest image.
- Not proven: level 0 also reports upmixed=32 (counter, not content), so the level effect is unobserved; no capture of the 6ch output. `mean_us` 12-14 ms per 768-frame (16 ms) period and the
  ms_hist top bucket are QEMU timings, NOT a Tegra CPU budget. type-103 handoff, SCENE mask, real stream objects/HMI ACK, dio_manager focus, iPhone still open. Emulator only.

## Update 10 (Claude): TDM capture of the hook .so guest run, levels 0/2/4 (gate 3/6, partial)
- The stream probe writes TDM marker `200+level` (/dev/mem 0x70080000+0xe20, PROT 0x300) around Prepare..Stop; `scripts/analyze_hook_so_levels.py RUN` groups the passive tap
  (`native-tdm0.raw/.index`) by marker and reports per-channel RMS of non-silent frames. Run `hookso-20261015-163604` (HOOK_SO_STREAM=1 HOOK_SO_LEVELS=0,2,4; result `logs/hook-so-level-rms-20261015-163604.json`).
- Result (real libairplay CAudioDriverIF -> hook .so -> NVIDIA sink -> stock mux, emulator): level 0: FL/FR only, RL/RR exactly 0; level 2: RL/RR 79/84 (FL/FR 992/1069); level 4: RL/RR 120/128
  (FL/FR 1489/1650). C and LFE are 0 at every level for this uncorrelated test signal. So the MMI level file does switch the surround path on/off on the real type-100 route.
- Limits: tap does not preserve block order (Update 4), so only aggregate RMS is used; absolute FL/FR differ between windows, rear/front ratio is 0.080 at levels 2 and 4 (identical), i.e. the 2-vs-4
  difference is NOT resolved by this signal (level 4 may differ only in parameters this content does not exercise). Not proven: SCENE mask quality, listening result, RCC/speaker path, real-time margin on Tegra.

## Update 11 (Claude): fail-closed 6ch gate and inert M.I.B. draft package (gate 8, partial)
- **Gate in the hook** (`scripts/prepare_full_audio_port.py`, anchors fail loudly): when `MHI2_NV_SIX_CHANNEL_QUALIFIED` is 0 (default if undefined) `spa_audio_config` forces atmos/buffered/realtimePCM to 0, so
  `AudioSessionGetSupportedOutputFormats` never ORs the 5.1 bit and nothing spatial starts, whatever `AUDIO_*` env says (an advertised 6ch stream with a refused sink would break CarPlay audio).
  Host `bash scripts/test_nv_inert.sh` (QUALIFIED=0 inert PASS, QUALIFIED=1 control advertises). `scripts/test_all_host.sh` now includes it; all PASS (`logs/all-host-20261015-b.log`).
- `HOOK_SO_QUALIFIED=0 python3 scripts/build_native_hook_so.py` -> `artifacts/native-hook-so-inert/libmhi2_audio_hook.so` (device default MPL5, unproven); default build stays QUALIFIED=1 for the emulator only.
- Guest, inert .so + stream probe with all `AUDIO_*` env on, levels 0/4 (`HOOK_SO_INERT=1 HOOK_SO_STREAM=1`, run `hookso-20261015-164809`, log `logs/hook-so-inert-guest-20261015-164809.log`): probe exit 0, audio goes through the
  stock libnvaudio MPL1 open, no hook `[audio]` log lines, no `DTCP enabled`. (The TDM analyzer shows 0 active frames: it decodes the 6ch layout, not the stock stereo mux; do not read it as silence.)
  Not shown: that the .so was loaded in that run (absence of logs is the expected inert behaviour; load itself is shown by the QUALIFIED=1 runs, same load path).
- **Draft package** `scripts/build_mib_audio_hook.py` -> `artifacts/mib-audio-hook-draft{,.zip}` (+`.sha256`), source `prototype/mib-hook/` (audio.sh generalised to four libs): hook (inert), libmhi2_apac/aac/fdk into `/mnt/app/eso/lib`,
  UI JAR + lsd.sh + six UpMix strings as in the standalone preview, `smartphone_integrator.json` gets `LD_PRELOAD=.../libmhi2_audio_hook.so` and `AUDIO_SPATIAL_CODEC_LIB=.../libmhi2_apac.so` for dio_manager only
  (APAC dlopen default path would otherwise be /mnt/app/root/hooks/libcarplay_apac.so). 14 manifest entries, CHECK default, INSTALL_PREVIEW/RESTORE with baseline and readback, refuses foreign libs/tamper/connected phone.
  Host simulation `python3 -m unittest tests/test_mib_audio_hook.py` 9/9 PASS (`logs/test-mib-audio-hook.log`): this is NOT the installer on a QNX unit.
- Not verified: hook loading in the real dio_manager, NEEDED libs resolved through dio_manager's LD_LIBRARY_PATH on the unit (assumes `/eso/lib` = `/mnt/app/eso/lib` as the old preview did), M.I.B. CHECK/INSTALL/RESTORE on the head unit,
  free-space margin of the app partition for ~2.9 MB lang zip + libs (script requires 40000 KiB). The old standalone preview stays historical and unrelated. No vehicle writes.

## Update 12 (Claude): in-car 6ch one-hot test prepared (gate 5), NOT run
- Owner states the door speakers, centre and subwoofer all work via the datasets (DSP side proven by the owner; input side from the NVIDIA TDM into the DSP is the open question).
- Prepared `car-tests/onehot6-audible.wav` (6 sequential one-hot tones, sha256 2dd66178...) and `CAR_TEST_6CH_ONEHOT_20261015.md` (PowerShell steps: scp -O to /tmp, stock `nvaudio_play -d MPL1`, sloginfo, remove; outcome table).
  Requires the owner's explicit go-ahead per non-read step; no hook, dataset, flash or process change. Not run in the emulator with this exact file.

## Update 13 (Claude): RCC static, mixer-config tables located (gate 5), no patch candidate
See `RCC_STATIC_FINDINGS_20261015.md` Update 2, helper `scripts/rcc_dsp_image.py`, `logs/rcc-mixer-config-tables-20261015.txt`. DSP chain objects (PreDownMixer, Zone0 MainMix/refMix/...) and the
mixer-config load site (0x8755bf88..0x8755c2a4, tables 0x8760ab58..0x8760adc8, `callp 0x875d0a60`) found by string xrefs; descriptor field layout not decoded; compact-instruction disassembly is unreliable for semantics.
Static work has diminishing returns without a C6x emulator/symbolic disassembler; the in-car one-hot test (Update 12) remains the cheapest discriminator, pending the owner's go-ahead.

## Update 14 (Claude): audio evidence read from the Windows return (car logs)
`CAR_EVIDENCE_AUDIO_20261015.md`: libairplay/libnvaudio hashes equal the emulator copies; the car routes CarPlay media to MPL5 (sloginfo, 10 events), start-up default MPL1, navigation ANN3, stock format PCM/48000/16/2;
AudioTable amps 120/121/122 = 2/4/6 ch, selected amp unknown (SETTINGS blob unparsed). The emulator probes use MPL1; the inert package default is MPL5. Reading logs only, no vehicle access.
- dio_manager load check (inert .so) in this emulator profile: NOT possible. Running the stock `/mnt/app/eso/bin/apps/dio_manager` directly in the guest, with and without LD_PRELOAD, fails identically at the loader
  (`ldd:FATAL: Could not load library libcomm.so`, runs `hookso-20261015-165822` and the find run); `find /mnt/app /ifs /lib /usr -name 'libcomm*'` finds nothing, so the emulator image lacks it. The preload-into-dio_manager question stays open until the car.

## Update 15 (Claude): package tests extended, owner doc for the inert draft
`tests/test_mib_audio_hook.py` now 12 tests PASS (fault injection at each of the 8 activation moves + RESTORE; reinstall after RESTORE refused; low space refused), `tests/test_analyze_levels.py` (synthetic TDM capture) PASS.
Owner-facing summary of the draft package: `MIB_AUDIO_HOOK_DRAFT_20261015.md`. Still host simulation only.

## Update 16 (Claude): Windows kit for the in-car one-hot test
`car-tests/onehot6-windows-kit.zip` (+`.sha256`; sources in `car-tests/onehot6-windows-kit/`): README-UA.md (preconditions, run, manual fallback, what to return), `run-test.ps1` (guided, `yes` before every non-read step,
logs to `result-<stamp>/`), `RESULTS-TEMPLATE.md`, WAV, SHA256SUMS. The PowerShell script was written WITHOUT a PowerShell on the author's machine (not executed); README has the same commands as a manual path.
Reading the returned logs/notes will be recorded in `analysis/05-FIELD-LOG.md` as physical-vehicle, by ear. Nothing was run on the vehicle.

## Update 17 (Claude): the one-hot playback test was run on the car (by a Windows agent session) - INCONCLUSIVE
Read `car-tests/onehot6-session-20261015/` (README-TRANSFER, 05-FIELD-LOG, result-agent-*, route-evidence-*) and merged the four relevant Windows field-log entries into `analysis/05-FIELD-LOG.md` (verbatim, 218 lines now).
Facts: Wi-Fi AP SSH (root@<unit-ip>) works, USB-Ethernet does not (AX88179, Opkts=0); stock `nvaudio_play` MPL1 accepts 6ch (config num channels=8, frag 6144) and plays all bytes, deva logs FadeIn/FadeOut and `DTCP enabled!`
(same 6ch signature as our emulator runs); nothing audible; MPL5 write status 4. Reading and next options in `CAR_TEST_6CH_ONEHOT_20261015.md` (RESULT section). No 6ch capability conclusion. The unit was left unchanged (temp file removed).

## Update 18 (Claude): static MPL1 routing/mute analysis and a one-visit car kit
`MPL_ROUTING_MUTE_STATIC_20261015.md`: mm-renderer writes MPL1; mediarouter sets routes via nvaudio_control_set_route/NvMS_ServerSetRoute (SET_ROUTE_CONFIG); deva has no mute logic; source gating/mute lives in RCC audio management (AudioTable LS_FCT_AMP_MUTE_*, flags) - exact condition NOT found statically.
Owner asked for one complete instruction instead of repeated trips: `car-tests/session2-kit.zip` (sha256 in `.zip.sha256`): README-UA.md (owner: one approval + listening), AGENT-BRIEF.md (Windows agent: exact commands, 3 conditions S0/S1/S2, 9 runs, cleanup, deliverables),
RESULTS-SHEET.md, WAVs (onehot6-v2 -30 dBFS, stereo L/R 48k, mono 16k). Interpretation table is in README-UA.md. Not run.

## Update 19 (Claude, autonomous hour): RCC site check on K3346 - MainMix maps identified, MIBRoot site-1 data confirmed
`RCC_K3346_SITE_CHECK_20261015.md`, `scripts/decode_mibroot_routes.py`, corrected `logs/rcc-mixer-config-tables-20261015.txt`. K3346 MIBRoot carries the reference's resource-0x78 record (file 0xC627DC, internal 0x46e->SPEAKERS, external 0x46f->DTCP51) and
conn-162 record (file 0xC61860); the 20-byte DSP tables are the reference's MainMix source/channel maps (K3346 = ref - 0x15b0; correction: 4th table 0x8760ada8). AudioProcess strings show the same sinks (ENT1_DTCP51 "Entertainment Multichannel").
Conclusion: no MMX-only path to discrete 6ch on an internal-amp car; needs RCC edits + RCC flash (owner's risk decision, not done); owner's centre/sub come from the DSP's own stereo->6 rendering (AU_6CH, Signalflow 0x31). Candidate site-1 bytes are documented, NOT to be applied.
Session 2 kit README now states the expected B2 result (front L/R only). Nothing written to the vehicle; no commits.

## Update 20 (Claude): session 2 kit v2 - objective (non-auditory) evidence added
Self-review: v1 depended on hearing and on MMX-only logs (deva has no mute logic). v2 adds read-only RCC-side evidence (RCC sloginfo path discovery + key-line grep around every run; AmplifierCoding/useExternalAmplifier; `modifyE2P r fc 1` Sound_System byte, background+kill),
an audible-state baseline (S1) for log comparison, optional TDM shm snapshot during B2, and S3 (stock CarPlay, no hook) to read the real sink/bandwidth. Silent outcome is explicitly handled (deliver the evidence set). Not run.

## Update 21 (Claude): session 2 results merged (physical car, by ear) - static conclusion for gate 5 is now CONTESTED
Source: `FROM WINDOWS/car-sessions-20261016/` (README-HANDOFF, answers.txt, rcc-sloginfo-full.txt, analysis/16). Field-log entry merged into `analysis/05-FIELD-LOG.md`.
Facts: with a source selected but PAUSED (Bluetooth), `nvaudio_play` on MPL1 is audible and single-channel files land on the labelled speakers: ch1 FL, ch2 FR, ch3 centre, ch5 RL, ch6 RR. ch4 (sub) silent at 60/90 Hz and 40-200 Hz sweep; the same sweep on front L+R drives the sub => sub is bass-managed by the amp, ch4 is not LFE on this route.
No source (S0): mpl1/ann1 silent, ann3 write status 4. Source playing: mpl1 busy (open status 1), ann1 plays but only music heard. RCC log: audio opens via DSP `AC:connect,source N` (s1 boot, s10 BT, s7); nvaudio_play triggers none. Amp id still unread. First S1/S2 pass at min volume is invalid.
Impact: Update 19's "no MMX-only path to discrete 6ch" is contradicted for MPL1 under a selected source: discrete 5 speaker channels reach the cabin without RCC edits. NOT yet shown for the CarPlay path: CarPlay media uses MPL5 (write status 4 for us), mm-renderer owns MPL1 while a source plays, and gap-free/focus are unproven. Re-examine the 0x78/MainMix premise (maybe this route is already AU_6CH multichannel). `QUALIFIED` stays 0; nothing written to the car.
Next (no car): (1) map static why MPL1 6ch is routed (source-10 connect, route table) vs MPL5; (2) decide hook design: take over MPL1 only while CarPlay is the selected source (mm-renderer conflict); (3) car, read-only: map source ids while switching FM/USB/BT, read amp id. Decisions on any install need owner approval.

## Update 22 (Claude): static MPL1 vs MPL5 reading from the car logs (host analysis, no car access)
Evidence: `route-evidence-20261015/sloginfo-route.txt`, session-2 sloginfo. Boot default: `SET_ROUTE_CONFIG: TDM1_OUT_MPL already routed to input MPL1` (connections table entry MPL1 -> TDM1_OUT_MPL). CarPlay later routes `TDM1_OUT_MPL <- MPL5` (mediarouter, on AudioStreamPrepare).
Reading (inference): the one TDM1_OUT_MPL output carries whichever MPL input is routed. 6ch on MPL1 is audible because MPL1 is the routed input while a source is selected/paused; `nvaudio_play` on MPL5 opens fine but write fails (status 4) because MPL5 is not the routed input outside a CarPlay session (open ok at 00:13:16, no `New buffer from MPL5`). So "6ch needs RCC edits" is not supported for the speakers path; the 5-channel discrete route to the cabin is the existing MPL1/TDM1_OUT_MPL path. Our NV sink already targets MPL1 6ch (matches car-proven path).
Open (needs CarPlay session, not testable in emulator): during CarPlay the route is MPL5, so MPL1 data would be inaudible unless the hook (a) writes 6ch to MPL5 while routed (does MPL5 accept 8-slot config?) or (b) re-routes TDM1_OUT_MPL <- MPL1 via nvaudio_control_set_route and prevents mediarouter/mm-renderer from fighting it. Also ch4 is not LFE (amp bass-manages from fronts): 5.1 LFE content must be folded into front L/R or dropped.
Next car, read-only/by-ear with owner approval: with a live CarPlay session read deva log for MPL5 device config (num channels) via `nvaudio_play`-free means (sloginfo), and check whether stock MPL5 config is 8 slots.

## Update 23 (Claude): host regression + MPL5 geometry from the 2026-10-15 return logs (static, no car)
Regression: `scripts/test_all_host.sh` ALL HOST SCENARIOS PASS, NV INERT GATE PASS; `unittest tests.test_mib_audio_hook tests.test_analyze_levels` 13 tests OK (58 s). Host only.
Evidence (`handoff-windows/MHI2_RETURN_20261015-20261015-073857/evidence`, stock CarPlay, physical car): dio_manager `CAudioDriverIF` opens MPL5 with `Allocated size = 16384 B (sample size 4 B, channels = 8)`, 512 frames, i.e. the SAME 8-slot TDM geometry as MPL1 (`num channels = 8`, 6ch accepted by nvaudio_play). On each open: `SET_ROUTE_CONFIG TDM1_OUT_MPL -> MPL5`, `NvAudioStreamUpdateConnection: TDM1_OUT_MPL connected to MPL5`, `DTCP enabled!`; on close `disconnected from MPL5`.
Reading (inference): the route to MPL5 is made by mediarouter around the stock open inside a CarPlay session; the same TDM1_OUT_MPL output carried our discrete 5 channels from MPL1 in session 2. So a hook inside dio_manager writing 6 slots to the MPL5 it opens should reach the same speaker path without RCC changes; our `nvaudio_play` MPL5 write 4 is explained by running outside that route. NOT proven: needs a live CarPlay session with the hook (or an audibility probe during CarPlay). ch4 stays bass-managed (fold LFE to fronts).
Design implication: no re-routing needed (option b of Update 22 not required); open question narrowed to "6 audible slots on MPL5 during CarPlay" plus gap-free/CPU gates. QUALIFIED stays 0.

## Update 24 (Claude): LFE fold not needed; session 3 read-only kit
LFE: our upmix already leaves C/LFE... correction: `prototype/mhi2_scene_adapter.h` and tests state "C and LFE remain zero" for the current SCENE path, so there is no LFE content to fold into fronts; ch4 bass management by the amp is therefore not a defect of our output. No code change. (Session 2 showed ch3 centre is a real discrete output, so a future C-feed is possible; separate decision.)
Kit: `car-tests/session3-kit.zip` (+.sha256; README-UA, AGENT-BRIEF, RESULTS-SHEET). READ-ONLY with live stock CarPlay, no hook, no files copied: confirms MPL5 channels=8 and route-on-open, maps RCC source ids (FM/USB/BT/CarPlay/nav), reads amp coding (Sound_System byte). Not run.

## Update 25 (Claude): centre (ch3) feed - analysis only, no code changed
Fact (static): the SCENE engine writes `o[2]=0, o[3]=0` (C, LFE) by design (`scene_engine.c` ~798/854; host check `upmix_center_zero`, `lfe_disabled_zero`); the OFF/bypass branch of `mhi2_scene_adapter_process` also zeroes C. Session 2 (by ear): in discrete 6ch mode ch3 drives the owner's centre speaker, a front-only file did not light the centre, and the sub follows the fronts (amp bass management).
Risk: the stock stereo path gets centre (and sub) from the DSP (AU_6CH, Signalflow 0x31). If a 6ch stream carries C=0 the centre is likely SILENT = regression vs stock for this car (the reference targets cars whose amp/rendering differs). Sub needs no feed (S-4e).
Proposal (not implemented, needs owner OK): post-render centre feed `C = g*(L+R)/2`, `g` default 0 (inert, bit-exact with today) and one setter; `|C| <= max(|L|,|R|)` so no new clipping; apply in both upmix and bypass paths of the hook that actually runs (check whether `audio_hook_k3346_nv.c` uses the adapter or `stereo_upmix.c`/`spatial_downmix.c` first). Host test: C equals g*(L+R)/2 within 1 LSB, g=0 leaves current hashes unchanged, no clipping on full-scale input. Final g only by ear in the car (start 0.5-0.7, whether fronts must be attenuated to keep vocal level). Spatial 5.1 content (spatial_downmix) already carries a real C (`dst[2]=center`), unaffected.

## Update 26 (Claude): centre feed - implementation deferred, reference premise found
`reference-audio-74b0d70/hook/audio/stereo_upmix.h` header: output L,R,C,LFE,Ls,Rs with "C deliberately zero because the Audi amplifier already derives its centre" (reference targets MHI2Q PCM6 rail; MHI2-only rule: premise unverified for K3346). So C=0 may be fine (amp derives centre) and a forced C feed could DOUBLE the centre. Update 25's regression risk is therefore unproven, either way.
Decision: no code change until an in-car A/B (owner OK'd by Mac over Wi-Fi, `nvaudio_play` WAVs in /tmp, mpl1, source selected+paused): (A) centred L=R 6ch with C=0, (B) same with C=0.5*(L+R)/2, (C) stock stereo for reference level. Question: does the centre speaker sound in A; is B louder/closer than C. Then choose default g (0 if A matches stock). Implement the knob after the result (patch point: generated `audio_hook_k3346_nv.c` from `prepare_full_audio_port.py`, not the pinned reference).

## Update 27 (Claude): session 3 done from the Mac (physical car, read-only)
Evidence in `FROM WINDOWS/car-sessions-20261016/session3-results/` and field log. Amp: Sound_System=01 internal. RCC sources: FM 1, BT 10, CarPlay media 10 (all sink 0); 15/sink1 prompts. Stock CarPlay: dio_manager opens MPL5 (configure 2ch/48k, device buffer 8 slots x 512 frames), mediarouter sets TDM1_OUT_MPL <- MPL5 on open (MPL1 disconnected), closes on pause.
Confirms Update 23 inference: MPL5 is the same 8-slot TDM1_OUT_MPL path as MPL1 (which carried 5 discrete channels in session 2), route follows the open, source 10 gates audibility. A hook writing 6 slots to that MPL5 should reach the same speakers; still NOT proven (needs the hook in dio_manager). Note MPL5 is closed/opened on every pause/play: reopen/purge gate is real. Mac access recipe: key auth + per-command legacy opts (see field log). Next: centre A/B (WAV to /tmp, owner OK) then decide centre feed.

## Update 28 (Claude): centre A/B result (physical car, by ear) - no centre feed needed
Stereo 2ch (S) and 6ch with C=0 (A) on MPL1 gave an equally sounding centre speaker: the centre is derived downstream (amp/DSP) from the front pair even in the 6ch format, as the reference header says. Update 25's silent-centre risk is refuted for K3346; centre feed from the hook is NOT needed and would likely double the centre. Decision: no code change, C stays 0. Also: MPL1 write after a CarPlay session fails with status 4 until the source is reselected (route stays on MPL5) - consistent with Update 22/23 (write works only on the routed input). Test WAVs removed from /tmp. Session artifacts: `FROM WINDOWS/car-sessions-20261016/session3-results/AB-*.txt`.

## Update 29 (Claude): ANN3 / nav prompt and feature survey (physical car, read-only)
CarPlay navigation prompt: no ANN3 open or reroute and no new RCC connect in the logs; AirPlay shows `speech n/a`, `mainAudio accessory`, and `turns controller` toggling (turn-by-turn available to the head unit). So prompts most likely ride the media stream on MPL5, i.e. our 2->6 upmix would also process them (not proven: only "prompt sounded" by owner). Stock unit has none of the D-pad/cover-art/RGI patch files. Survey of cluster scripts and persistence key 3221356656 recorded in the field log. No writes; WAV test files removed earlier.
Update 29 addendum: by ear (uncontrolled) the nav prompt sounded from the front doors + centre only, no rear noted; level vs music and ducking not observed.

## Update 30 (Claude): non-audio feature evidence gathered while connected (read-only)
D-pad/cover art/RGI: stock CarPlay HID set is Knob/Playback/Telephony/TrackPad (a D-pad patch would add a device or events); AirPlay `turns` mode is controller-capable; no patch artifacts on the unit; cluster mode persistence key 3221356656 (clusterCoding*.sh write 00/02/03/04, not run); `displaymanager 2_lvds`; cluster path details still need the dedicated VC/RGI analysis. Details and file names: field log 2026-10-16 entries; raw: `FROM WINDOWS/car-sessions-20261016/session3-results/`.

## Update 31 (Claude): VC/RGI baseline from the live unit (read-only)
Stock iAP2 identification has no route guidance (0x52xx) and no HID component, so RGI is purely additive; HMI already starts DSIKOMOView / DSIMapViewerManeuverView / DSICarKombi / DSIKombiPictureServer (cluster services present). Cluster-terminal propagation shows a one-off "no valid cluster terminal found" at boot, later layout propagated. Raw: `session3-results/K*.txt`, field log. Not done: reading PPS contents, cluster display mode persistence value, any write. Offline next: compare iap2 message list with the RGI reference (needs 0x5200-0x5203 etc.).

## Update 32 (Claude): cluster image routing facts (read-only)
Cluster mode persistence first byte = 03 (LVDS, by writer-script semantics); displaymanager `2_lvds`: display 0 = MMI (TFTLCD0), display 4 = cluster on Tegra:HDMI0; the CarPlay screen is display 0 layer 59 (1024x480). This is the target surface family for cover art / maneuver tiles (KOMO/BAP path not yet read). Writer was invoked twice harmlessly (no args / -h), nothing written. Raw: `session3-results/P*.txt`.

Update 32 addendum: Mac access recipe kept for later visits: key copy `<ssh-key>` (owner-supplied, outside the repo), helper `scripts/mib_ssh_mac.sh` (car Wi-Fi AP <car-wifi-ap>, iPhone USB above Wi-Fi in `networksetup` service order). Read-only unless the owner authorizes a specific write.

## Update 33 (Claude): owner's sound datasets analysed against the port
See `OWNER_DATASETS_ANALYSIS_20261016.md`. Main points: per-channel CarEQ (gain/delay/phase/7 biquads/DRC, 2 profiles) already exists in the DSP on the output channels, so the hook must not duplicate it; sub is fed from the fronts (ch4 input dead), bass shaping is data; menu styles are data-declared (`0x3B00`, 2 slots); non-CarPlay (Bluetooth/USB/SD) upmix would need an mm-renderer interposer (unstudied); SCENE keeps vocals front by design (prompt behaviour untested). No URLs for the owner's cited articles were found.

## Update 34 (Claude): owner history file read - Bluetooth is the main path
`reference-owner/A4B9_sound_project_history_and_findings.md`: base acoustics, no external amp; owner listens ~99% via the Bluetooth source (mm-renderer -> MPL1). Priority shift proposal: study the mm-renderer/MPL1 interposer (launch mechanism, LD_PRELOAD feasibility, CPU budget, MPL1 ownership) before polishing the CarPlay/dio_manager hook; both end in TDM1_OUT_MPL with the same 8-slot geometry. Details in `OWNER_DATASETS_ANALYSIS_20261016.md`. Pending: GitHub repo NumberOneBot/mqb-mib-sound-datasets (fetch agent running).
Update 34 addendum: repo NumberOneBot/mqb-mib-sound-datasets read via fetch agent (secondary source): template v0.6 uses channel gain /20, delay /10 ms, DRC scales as in `OWNER_DATASETS_ANALYSIS_20261016.md`; id map and `pc` write recipe recorded there. /20 vs /10 still unmeasured.
Update 34 addendum 2: three owner-supplied articles (lprot, vvick3d, Strelets) summarised in `OWNER_DATASETS_ANALYSIS_20261016.md`. Gain scale /10 vs /20 still open (vvick3d says /10); Signalflow 5.1/4/2-compatible suggests the DSP accepts 2/4/6 ch input; open anomaly: ch4 (LFE) silent in discrete mode on this car.

## Update 35 (Claude): BT path, LFE, native surround research (static/web, no car)
Owner: Bluetooth and CarPlay equal priority. `RESEARCH_BT_LFE_20261016.md`: mm-renderer reaches MPL1 via libnvaudio (`nvaudio_pcm_open`), so the existing nvaudio stock bridge seam applies (device MPL1, 44.1k); emulator test with nvaudio_play is possible now. LFE: hypotheses H1 slot 7/8, H2 MainMix LFE gain 0, H3 AM gating; decisive 8-slot one-hot car test designed. AudioTable AU_6CH carries MODE3D/SURLEV/SURROUND_ACTIVITY and HMI has SurroundRange, likely for external amps only (lead, low prior). Web: forum pinout (9VD sub pins 9/13, centre 10/14), no datasheet.

## Update 36 (Claude): Bluetooth/USB/SD bridge variant (MPL1, 44.1 kHz) - host PASS, emulator guest PASS
Owner: BT and CarPlay equal priority, kept independent. Separate build flag `MHI2_NV_BRIDGE_LOCAL_MEDIA` (CarPlay build unchanged; all host scenarios incl. the 48k stock bridge still PASS):
- `prototype/nvaudio_stock_bridge.h`: claims 44.1k/S16/2ch (and 48k) on MPL1; declares its OWN media main stream (no libairplay/AudioStreamPrepare in mm-renderer) and ends it on close/format change; 44.1->48k via `resample_44k_to_48k.h`; feeds the hook in whole 768-frame periods (realtime upmix only runs on full periods); blocks up to the caller's write timeout when the sink is full and, if still stalled, drops the oldest pending audio instead of returning an error (mm-renderer stops after 1 error: `max-errors=1` in mm-renderer.conf); interposes `nvaudio_pcm_info` (54-byte struct from the real device, served from a copy after the claim; without it nvaudio_play read garbage `path = BTVN` and exited).
- Host: `scripts/test_nv_local_bridge.sh` (in test_all_host.sh): 40960 in -> 44544 native frames, L/R exact, own route set/released, level 4 rears > 0.
- Build: `HOOK_SO_LOCAL=1 python3 scripts/build_native_hook_so.py` -> `artifacts/native-hook-so-bt/`. Guest: `HOOK_SO_LOCAL=1 HOOK_SO_PLAYER=1 HOOK_SO_LEVELS=0,2,4 python3.13 scripts/run_hook_so_guest.py` (stock `/armle/sbin/nvaudio_play -d MPL1` + 44.1k stereo WAV, `native-tdm-mark` labels capture blocks). Run `hookso-20261016-151444`: all three runs exit 0, 500 periods each; TDM RMS level 0 = fronts only (FL/FR ~2611, rest <3), level 2 = RL/RR ~75; earlier run `hookso-20261016-150626/151035` level 4 RL/RR ~163 (capture of level 4 is not always present: tap limits, Update 4).
- Gate seen in the guest: realtime upmix guard disables after 68-147 periods (emulator 10-22 ms per 16 ms period vs 12 ms budget). Emulator CPU is not Tegra; CPU budget stays an open physical gate for BOTH paths.
- Not proven: mm-renderer itself (launcher/LD_PRELOAD injection, its set_pcm_info values, pause/seek behaviour), real CPU, audibility. Emulator only; nothing written to the car.

## Update 37 (Claude): no-car work after Update 36 - mm-renderer launch path, CPU bench, session-4 kit
1. **How mm-renderer starts [static]**: `/eso/bin/apps/medialauncher` (stock binary, `strings`) spawns `mm-renderer -r /ramdisk/pps/multimedia -k` and `mm-sync`, with a built environment (`LD_LIBRARY_PATH=/lib:/mnt/app/armle/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/lib/dll:/mnt/app/root/lib-target:/eso/lib`, `MM_RENDERER_CONFIG=/etc/mm/mm-renderer.conf`, `NV_AUDIORENDERER=8`); hard-coded constants, no LD_PRELOAD key seen. The CarPlay/dio_manager LD_PRELOAD goes into the `IPL_CONFIG_DIR_DIO_MANAGER` env line of `smartphone_integrator.json` (the reference installer, `mhi2-au37x-carplay-main/install.sh`). For mm-renderer the proven pattern in the same installer is the mm-ipod shim: keep the original as baseline (`.orig`), move it to `rgd_real/`, and put a `#!/bin/sh` shim at `/mnt/app/armle/usr/sbin/mm-ipod` that sets `LD_PRELOAD` and `exec`s the original. Same pattern is feasible for `/mnt/app/armle/usr/sbin/mm-renderer` (spawned by absolute path; `exec` keeps the PID), but NOT yet verified for medialauncher's spawn of a script, and the BT package therefore stays unbuilt until a car read-only check (binary hash vs firmware copy, mount flags) confirms. CarPlay and BT hooks remain independent: different .so builds, different processes, separate shims/env lines.
2. **CPU [measured on host, emulator, reference docs]**: new `scripts/native_scene_bench.c` + `scripts/build_scene_bench.sh` (host + ARM/QNX build, per-period time at 768 frames, levels 2/4 with/without mask net). Host (Apple Silicon): 0.04-0.2 ms per 16 ms period. Emulator (QEMU): 10-46 ms, i.e. NOT usable (it explains the guard tripping in guest runs). Reference measurements were on Krait (APQ8064) and exclude other load; Tegra3 Cortex-A9 NEON is slower than Krait, so the real cost is unknown: probe binary `car-tests/session4-kit/native-scene-bench` runs from /tmp on the unit (RAM only). Mask net costs about 2-4x the no-mask path; in the real hook the mask runs on an async worker.
3. **Session-4 kit** `car-tests/session4-kit/` (README-UA with listening checklist, AGENT-BRIEF for the Mac, RESULTS-SHEET, `gen_wavs.py`, bench binary + sha256): read-only mm-renderer facts, CPU bench idle/loaded, 8-slot one-hot tones (slots 1-8, 70 Hz/1 kHz; stock nvaudio_play accepts 8-channel WAV on MPL1: emulator check `HOOK_SO_PLAY8`, exit 0) and sub sweeps (C only, RL+RR only, fronts) to decide the LFE question. Wi-Fi only, RAM writes only, no adapter needed. Not run.
4. Reference doc note: "C/LFE = 0 in PCM does not mute the physical centre or sub" (`UPMIX_RELEASE_AND_ACCEPTANCE.md`), consistent with our centre A/B.

## Update 38 (Claude): more no-car work - voice leakage, bridge stress, UI jar static check
1. **Voice stays front [host numerical, synthetic bed + macOS `say` TTS voice]** (`scripts/voice_leak_test.py`, tool `scripts/scene_render_wav.c`, mask net on, 768-frame periods). Rear/front energy ratio: centred speech (L=R, how an iPhone nav prompt arrives) -35.8..-35.5 dB at levels 1-4 (rears practically silent, C/LFE 0); synthetic wide music bed -10.6 (L1) .. -6.8 dB (L4); voice over bed -12.7 .. -9.0 dB; speech panned HARD LEFT is treated as an instrument and goes to the rears (-0.7 dB at L1, +5.1 dB at L4). So centred prompts/voice stay front; side-panned voices (rare: backing vocals, stereo spoken-word) do not. Not an ear test.
2. **BT bridge stress [host]** (`MHI2_NV_STRESS=1` in `scripts/test_nv_local_bridge.sh`): 4000 random operations, 428 open/close cycles, 2.56 M frames fed, odd write sizes, 44.1k/48k/16k flips on a live handle, level changes and injected sink failures: no crash, route balanced, opens == closes. (A first failure was only the mock asserting 6ch alignment on the 16 kHz mono passthrough; fixed in the mock.)
3. **Static check of the MMI row jar** `mhi2_audio_ui.jar` against the decompiled K3346 HMI `lsd-k3346.jar` (javap): all 17 referenced external classes exist and every referenced method/field matches by name+descriptor; the one miss (`DefaultChoiceListener.<init>()V`) is a javap parse limit (protected default constructor of a class with flag 0x2000). Not proven: runtime behaviour, layout, navigation.
4. **Class-file major**: our UI jar was major 48; every stock/known-working patch jar (chefranov dpad/coverart/rgd) is 46. `scripts/downgrade_jar_major.py` rewrites to 46 only after checking (javap) there is no ldc of a Class constant; `artifacts/java/mhi2_audio_ui-v46.jar` is now what `build_mib_audio_hook.py` packages (13 tests still pass). The car's J9 is "J9_Version 1.15", language level unverified; 46 removes the question.
5. **Decisions not to build yet**: inert BT package (it would replace the mm-renderer executable with an exec shim; needs the session-4 read-only facts first and its own baseline/restore); bass-to-fronts option in the hook (the owner's datasets already shape bass; wait for the LFE test).

## Update 39 (Claude): RGI baseline vs the K3346 iAP2 identification (static/live-log comparison, no car)
Live Identify (mm-ipod HARMAN stack, `iap2.log`, 2026-10-16): components = USB host transport (CarPlay interface 1) + Location only; MessagesSentByAccessory `5000 5002 5003 AE00 AE02 AE03 FFFB 4154 4156 4157 4159`, MessagesReceivedFromDevice `5001 AE01 FFFA FFFC 4155 4158 4E09 4E0A`. The reference RGI hook (`hook/routeguidance/rgd_hook.c` `rgd_identify_patcher`) finds the route-guidance component TLV 0x001E, and because stock has none it APPENDS it (growing the Identify buffer if needed). The reference README additionally lists `0x5200,0x5203` (sent) and `0x5201,0x5202,0x5204` (received) for the Cinemo iAP2 SDK in `dio_manager.json` on MU1316; on this K3346 `dio_manager.json` carries no such lists (CarPlay iAP2 identification is generated inside mm-ipod/HARMAN), so how the 0x52xx IDs reach the lists here is exactly what the existing librgd_hook.so port (14 symbols, `ident_info_funcs` layout check, Update 12-era notes) must be re-verified on the unit. Nothing is installed; RGI/cover art/D-pad remain separate cycles.

## Update 40 (Claude): static search for the K3346 C/LFE output-gain table (negative result)
The reference lists C/LFE output gains at `0x87602648` ([4]=[5]=0.5). On K3346: the address shifted by the MainMix offset (-0x15b0, `0x87601098`) and neighbours hold zeros/NaN/unrelated data; a whole-image scan (`rcc_dsp_image.py` memory + cinit overlay) for six-float windows with [4]==[5]==0.5 and the rest in {0, 0.5, 1} found no hit; a scan of `0x8760a000..0x8760c400` for runs of clean floats found none. So the table is not at a recoverable offset-derived location and the semantics of the MainMix rows (`04 05 06 07 01 01`) stay undecoded: LFE hypotheses H1-H3 are NOT narrowed by statics. The session-4 8-slot one-hot and sweep tests remain the decisive way (no further blind RE planned).

Update 40 addendum: session-4 kit test frequencies now follow the owner's installed sub filters (0x3000 v16: sub LPF2 120 Hz, HPF2 50 Hz x2, peak 67 Hz +3 dB, ported box Fb ~67 Hz): slot tones 70 / 100 / 1000 Hz (70 and 100 inside the sub passband), sweeps 45-130 Hz. Note: session 2's silent ch4 was tested at 60 Hz, 90 Hz and a 40-200 Hz sweep, i.e. inside that passband, so the sub's own filters cannot explain the silence (consistent with the DSP not feeding the sub output from input ch4, H1/H2/H3 still open).

## Update 41 (Claude): session 4 results (physical car, Mac over Wi-Fi) - decisions
Data: field log 2026-10-16 "Session 4"; raw `FROM WINDOWS/car-sessions-20261016/session4-results/` (answers.txt, bench, mm-renderer copies).
1. **CPU on Tegra3 [measured]**: SCENE without mask 5.6-5.8 ms mean (35 %), max <=13 ms of 16 ms; with the mask net inline 14.1-14.7 ms (88-91 %), max 24 ms. Decision: no mask inline; the mask net only on the async worker (reference design) and only if a worker core keeps up, otherwise run mask-less (levels without mask still upmix). Both hooks (CarPlay, BT) need this as a physical gate with the real hook (decode + I/O on top); the realtime guard budget (12 ms) fits the mask-less path.
2. **LFE/sub [ear, control-validated]**: no input slot or channel drives the sub on its own (6ch ch4 silent in sessions 2 and 4; 8ch slot 4 silent). The sub gets bass from the front pair AND from the rears (rear-only sweep still hums the sub) and not from the centre (centre-only sweep hums from the front). So (a) our hook must keep LFE = 0 (already so) and need NOT fold or assist LFE; (b) energy moved to the rears by SCENE does not lose sub bass; (c) the reference's LFE V2 assist is not applicable on this car. H1 (slot 7/8) refuted; H2/H3 (LFE not mapped / gated) remain the explanation but are no longer decision-relevant.
3. **Channel format [ear]**: the 6ch format maps as session 2 (ch1 FL, ch2 FR, ch3 C, ch5 RL, ch6 RR); an 8-channel stream folds slots 5-8 onto the fronts and slot 3 is centre+front. The hook must keep writing exactly 6 channels (`voices 6`), never 8.
4. **mm-renderer [read-only]**: unit binary == firmware copy (sha256 6f8e3613...), /mnt/app is not ro-flagged; BT-package expected hash is therefore known. Telnet 23 is closed over the Wi-Fi AP: no recovery channel without the USB-Ethernet adapter (AX88772B).
5. Not done/unknown: spawn of a shell-script mm-renderer by medialauncher (needs an install-time test), audibility of the actual hook, live CarPlay hook test.

## Update 42 (Claude): corrections and network facts from session 4
1. **Correction to Updates 25/29/35 (nav prompt)**: CarPlay navigation prompts DO open their own stream: three `Open the path: ANN3` with `CONFIGURE 16000 Hz, 1 channel` in the 2026-10-16 CarPlay session (earlier "no ANN3 open" came from a log window that missed them). The hook bridge claims only the 48 kHz stereo main stream (MPL5) and passes everything else (16 kHz mono ANN3, phone) through untouched, so spoken CarPlay prompts are NOT upmixed and need no voice rule; the owner heard them front + centre (stock mono stream through the DSP). The voice-leak host test remains relevant only for voice inside music (Bluetooth, podcasts, media).
2. **Telnet/recovery**: the unit listens on 22 and 23 (inetd), but `/etc/pf.conf` on the Wi-Fi AP interface uap0 allows only tcp 22 (+ service ports), so telnet is unreachable over Wi-Fi by firewall design; the project's recovery channel remains telnet on en0 via the USB-Ethernet adapter (AX88772B). SSH over Wi-Fi is unaffected by audio payloads and survives mm-renderer/dio_manager problems, but not a non-booting unit.
3. `/ramdisk/pps/multimedia/context/context1` is unreadable (mode d---------): mm-renderer's per-track stream format cannot be read from outside; the BT bridge must handle arbitrary `nvaudio_set_pcm_info` values (rates 44100/48000 claimed, others passthrough), which the stress test covers.

## Update 43 (Claude): the owner's tuning test tones run through the reference SCENE (host, no car)
Files (owner, `reference-owner/test-tones/`, 48 kHz stereo S16): 01 sweep 20-200 Hz, 02 sweep 200-8000 Hz, 03 sub steps 20-125 Hz, 04 centre steps 200-2000 Hz, 05 channel ID (L, pause, R, both), 06 polarity (in-phase then inverted), 07 pink mono, 08 kick transients, 09 sub steps 100-500 Hz. Tool: `scripts/owner_testset_render.py <dir>` (+ `scripts/scene_render_wav.c`), mask net on, levels 1-4. Rear/front energy ratio (dB) L1..L4 / front <130 Hz retention vs input:
- 01 sweep 20-200 Hz: -69..-68 dB, bass retention 0.0 dB; 03 sub steps: -72..-71, 0.0; 09 sub steps 100-500: -46..-45, 0.1 => bass/sub material stays entirely in the fronts, so the sub (fed by bass management) is unchanged by the upmix.
- 04 centre steps 200-2000: -43..-42 dB => centre-type content stays front (consistent with the centred-speech result).
- 07 pink mono: -22 dB; 08 kick: -25..-24 dB (centred broadband leaks a little more than speech).
- 05 channel ID (hard L / hard R): -6.8..-6.5 dB and 06 polarity (inverted part): -8..-7.4 dB => hard-panned and out-of-phase material is treated as instrument/ambience and moved to the rears BY DESIGN. With the upmix on, files 05/06 therefore no longer identify the L/R speakers; use upmix level 0 (bit-exact stock, rears silent) for identification and level >=2 for A/B listening.
In-car A/B plan with these files (one hook-installed visit, owner told in advance what to hear): 05 and 07 at level 0 vs 2 vs 4 (rears appear/disappear, fronts keep their image), 03/09 at level 0 vs 4 (sub must sound identical), 08 kick (no rear smear), 06 (inverted part widens into the rears). Not yet played on the unit; emulator/Tegra playback of the originals through nvaudio_play (2ch, stock path) is possible in a Wi-Fi visit as the stock reference.
Update 43 addendum (car, by ear): stock stereo reference for the owner's file 05: L -> front left, R -> front right, both -> front pair (+ probably the amp-derived centre), rears silent. This is the behaviour upmix level 0 / the bridge OFF path must keep bit-exact (verified on host: `off_front_bit_exact`).

## Update 44 (Claude): inert BT/USB/SD M.I.B. package built (host-simulated only, nothing on the car)
Decision (autonomous, owner asked to proceed without questions): build the BT package as a fully independent unit.
- Hook: `HOOK_SO_LOCAL=1 HOOK_SO_QUALIFIED=0 python3 scripts/build_native_hook_so.py` -> `artifacts/native-hook-so-bt-inert/` (claims no stock audio).
- Builder `scripts/build_mib_audio_bt.py` (template `prototype/mib-hook-bt/`) -> `artifacts/mib-audio-bt-draft{,.zip,.zip.sha256}`. mm-renderer stock copy extracted from `app.img` and checked against the unit sha256 6f8e3613... (sha1 e958eded...). Core fingerprints: mm-renderer, libnvaudio.so, deva-nvaudio; HMI.sha1 as in the CarPlay package.
- Mechanism: libs in a private dir `/mnt/app/eso/lib/mhi2bt/` (no shared files with the CarPlay package); original saved as `mm-renderer.mhi2real` + `/mnt/app/root/mhi2-audio-bt/baseline/`; libs and real copy are placed first, the shell shim replaces `mm-renderer` LAST via atomic mv, so the stock path always holds a runnable file. Shim is fail-open (missing libs or `DISABLE` file in the state dir -> stock exec, no LD_PRELOAD). Effect at next mm-renderer start.
- Tests `tests/test_mib_audio_bt.py`: 18 PASS (check/install/restore, reinstall guard, tamper, wrong renderer/core, foreign files, low space, symlink, no baseline, failure injected at every mv and cp step then clean RESTORE, interrupted RESTORE resumes, independence from CarPlay files, shim fail-open). CarPlay installer tests (12) and `test_all_host.sh` still PASS.
- NOT proven: that stock `medialauncher` spawns a shell script in place of the binary (arguments `-r /ramdisk/pps/multimedia -k`, env), M.I.B. run on QNX (`bt.sh` uses the same primitives as the proven audio.sh), hook inside a real mm-renderer. Needs AX88772B (telnet + backup) first; first car step = CHECK, then INSTALL with the inert hook only to prove the shim spawn (mm-renderer pid alive, BT audio unchanged), then RESTORE.

## Update 45 (Claude): BT package v2 + emulator spawn test of the mm-renderer shim
- v2 (`artifacts/mib-audio-bt-draft-v2.zip`, sha256 008115ed...; v1 superseded, kept): stock mm-renderer copy now lives at `/mnt/app/eso/lib/mhi2bt/real/mm-renderer` (same basename as stock, in case medialauncher matches the process by name; `ProcessTNG` Exec/registeredPath/usePid* strings seen in its binary); shim falls back to `baseline/mm-renderer` without preload if the real copy is missing. 18 host tests PASS (fallback case added).
- Emulator (QNX guest, `HOOK_SO_LOCAL=1 HOOK_SO_INERT=1 HOOK_SO_SPAWN=1 python3.13 scripts/run_hook_so_guest.py`, probe `scripts/native_spawn_probe.c`, run `artifacts/runs/hookso-20261016-17*`): QNX `spawn()` of the `#!/bin/sh` shim by absolute path with `-r /ramdisk/pps/multimedia -k` returns a pid and exit 0; with DISABLE also 0; with no real copy and no baseline exit 126 (expected). Limits: the guest has no `printf`, so the fake real was an empty script (child output not observed); medialauncher itself was not the spawner. Still open: the real medialauncher spawn and pid/watchdog behaviour on the car.

## Update 46 (Claude): web/GitHub research on sub/LFE addressing + H4 test prepared (not run)
Details: `RESEARCH_BT_LFE_20261016.md` sections 6 and 6b. No public source documents how the DSP derives sub/centre or maps LFE. Best evidence: NumberOneBot issue 11 / M.I.B. discussion 719 (Jet-ZJ): at 6ch mode the unit downmixes to 4ch + shared centre/sub, sub-channel content silent, same as our ch4 result. H4 = Signalflow 0x31 -> 0x30 on the owner's v16: file `datasets-h4/0x3000_v16_signalflow30_crc6539.{bin,hex}` (only bytes 0 and CRC differ, CRC-16/CCITT init FFFF verified on v16), plan `datasets-h4/H4_TEST_PLAN_UA.md`. Owner agreed to try, has backups, writes via CarScanner (no adapter needed). PENDING in car; result goes to field log. Also owner is preparing a Telegram chat export "Vag - MIB2" to share (read it with rg, look for Signalflow/sub/LFE/6ch).

## Update 47 (Claude): upstream luka-dev/mib2q-carplay-rgi check (2026-10-17, read-only via gh)
Our pin 74b0d70d is 18 commits behind main e8788bf8 (2026-10-14..16; one is a bulk 'Snapshot of the current CarPlay stack', 300 files). Audio-relevant deltas NOT in our `reference-audio-74b0d70` copy:
- 6b91d2c1 (#42): type-103 buffered SETUP without isMedia (iOS 26.6 Apple Music sends none; only explicit isMedia=false is rejected). Our copy (`hook/audio/audio_hook.c:895`) still rejects it, so a buffered-audio session from iOS 26.6 would fail with -6714 and CarPlay restarts. Relevant to the CarPlay build whenever the hook is enabled (the inert build never reaches it). Small patch (replace the check as upstream).
- 7bf9cfe0: SCENE front anchor `AUDIO_SCENE_FRONT_KEEP_DB` (1 dB default in the deploy script): sparse wide mix no longer moves entirely to the rears. Our copy already has the env knob (default 0); upstream changed scene_engine.c (+38 lines) and the startup script default.
- 9ff095e1: installer patches the unit's own SI via binpatch and always installs a six-channel deva-ctrl-qc.so; 5.1/UpMix gated on the amplifier coding. (MHI2Q packaging, not applicable to K3346 NVIDIA/deva-nvaudio; MHI2-only rule.)
- Other commits: lane guidance, cluster night mode, logging card, USB installer: not audio, not ours.
Decision pending (not applied, pin untouched): port 6b91d2c1 + 7bf9cfe0 into the generated port via `prepare_full_audio_port.py`, with tests. Do it before any live CarPlay hook cycle.

## Update 48 (Claude): upstream 6b91d2c1 ported; packages rebuilt
- 7bf9cfe0 (SCENE front anchor) was already in our pin (patch detected as applied). 6b91d2c1 (#42, type-103 SETUP isMedia optional) applied in place to `reference-audio-74b0d70` (hook/audio/audio_hook.c, tests/audio_interposer_test.c, docs): patch + pristine copies in `upstream-patches/`, deviation recorded in the pin's PROVENANCE.md (it earlier said adaptations belong outside; 22 scripts hardcode the path, so in-place + documented).
- Checks: reference `scripts/test_audio_interposers.sh` PASS (incl. 'isMedia true/absent accepted, false rejected'); `test_all_host.sh` PASS; inert hooks rebuilt (CarPlay + BT).
- Packages rebuilt (older ones superseded, kept): CarPlay `artifacts/mib-audio-hook-draft-v2.zip`, BT `artifacts/mib-audio-bt-draft-v3.zip` (+ .sha256). Tests: 12 CarPlay installer + 18 BT installer PASS against them (defaults now point to v2/v3).
- Not done: other upstream deltas (lane guidance etc.) are out of scope; no live iOS 26.6 test.

## Update 49 (Claude): PC-only package 1 (2026-10-22)
Details: `analysis/NEXT_TESTS_AND_PC_PLAN_20261022.md` (section Package 1). Installer guard used the missing `printf` and failed open: fixed
in `prototype/mib-hook` and `prototype/mib`, test added, package `mib-audio-hook-draft-v3` (installer fix only). Optional centre feed
`AUDIO_CENTER_FEED` (default 0, bit-exact) added to the pin (PROVENANCE) with host test. Route-down scenario shows a reopen per failed period
(no backoff): HOLD policy is open. ARM `.so` not rebuilt: OpenQNX headers missing in /tmp. `test_all_host.sh` ALL PASS. Nothing on the car.

## Update 50 (Claude): inert libs and v4 packages rebuilt (2026-10-22)
OpenQNX public headers re-fetched (sparse clone of vocho/openqnx cc95df35; original source unrecorded). Rebuilt `native-hook-so-inert` and `native-hook-so-bt-inert`
(old kept as `*.pre-20261022`), packages `mib-audio-hook-draft-v4` and `mib-audio-bt-draft-v4` (+ .sha256), installer tests 31 PASS, guest smoke exit 0
(`artifacts/runs/hookso-20261022-152435`). The centre knob is in the libs but default 0 and never exercised in the guest. Nothing on the car.

## Update 51 (Claude): rebased on luka-dev a63d547, centre knob dropped, v5 packages (2026-10-22, host + guest smoke only)
- New reference copy `reference-audio-a63d547/` (see its PROVENANCE.md; hook/audio, hook/framework, hook/coverart, apac_decoder, models, docs/audio, tests, 2 test scripts; old `reference-audio-74b0d70/` kept, superseded). Upstream's own `scripts/test_hook_audio.sh` PASS (11 tests incl. soundcheck, apap lifecycle, type103) plus `scene_centre_test` PASS.
- `AUDIO_CENTER_FEED` (our A2) is gone with the old pin; upstream `AUDIO_SCENE_CENTRE` (centred vocal moves L/R -> C with power kept, default **1.0** in luka, 0 = C silent) replaces it. The mask blob already outputs the vocal row (`models/` unchanged), only a one-line `memcpy` in `scene_mask_worker.c` was new. The default for K3346 is an ear decision (our old default kept C silent); hook stays inert (`QUALIFIED=0`).
- Upstream removed the synchronous mask path (`AUDIO_SCENE_MASK_SYNC`, `sceneMaskSync`). Only our test `prototype/test_nv_hook_detached.c` used it: the mask scenario now compares two worker runs. All of our `prepare_full_audio_port.py` source anchors still matched unchanged.
- Scripts/prototype/tests repointed from `reference-audio-74b0d70` to `reference-audio-a63d547` (originals saved in the session scratchpad only). Host tests PASS in a path without spaces: `test_nv_hook_detached` (detached/level/soundcheck/switch/mask/rate44), `test_nv_inert`, `test_buffered_nvidia_pipeline`, `test_reference_nvidia_receiver`, `test_nv_local_bridge`, `test_nv_stock_bridge`.
- Inert libs rebuilt (`native-hook-so-inert` 993740 B, `native-hook-so-bt-inert` 996900 B; previous kept as `*.so.pre-a63d547`), packages `mib-audio-hook-draft-v5` (sha256 c52f3433...) and `mib-audio-bt-draft-v5` (4bd657aa...) with .zip/.sha256; installer tests default to v5: 13 + 18 PASS. Guest smoke (`HOOK_SO_INERT=1 python3.13 scripts/run_hook_so_guest.py`, python 3.13 needed): exit 0, run `artifacts/runs/hookso-20261022-170418`. v4/v3/v2 kept, superseded.
- Not taken from luka: `decoder_optimize` (Qualcomm OMX, libairplay 210.81 vaddrs). Not examined: luka's later internal-amp-path commits (1e087b59, aa3621d5, 1bf8b81f) and iAP2 installer commits; their docs are in `reference-audio-a63d547/docs/audio` (rg `INTERNAL_AMP_PATH.md`) but our car routing evidence (sessions 2-3) is unchanged.
- Nothing on the car, no install.
- **Owner decision (2026-10-22): centred vocal goes to the centre channel.** Keep upstream default `AUDIO_SCENE_CENTRE=1.0` (no override in our packages/installer). Active only when the hook is qualified and the SCENE mask is loaded; the inert hook still changes nothing. Final level only by ear in the car.

## 2026-10-28 — installation-readiness audit (static + host)

- Done: inspected current CarPlay v7 / BT v5 ZIPs, payloads, menu source and test scope;
  both packages are inert; full-audio installation readiness is NOT established.
- Verified: ZIP SHA256/CRC and 15/9 manifest entries, unpacked/archive equality,
  current Ukrainian baseline equality; test_mib_audio_hook.py 21 PASS each under
  sh/oksh/mksh; test_mib_audio_bt.py 18 PASS; test_audio_java.py 1 PASS.
- Not verified: hidden-3D row insertion, actual HMI, active audio in dio_manager,
  current vehicle/recovery or combined preload operation. No emulator/device runs.
- Open: packaged row is only CarPlay UpMix; no OEM Surround/3D/centre/sub controls.
  Java settings test stubs UI and cannot establish visibility.
- Next: exact-stock hidden-3D UI qualification.
- Files: INSTALL_READINESS_20261028.md, STATUS.md, this handoff, root PROJECT_STATE.md.
