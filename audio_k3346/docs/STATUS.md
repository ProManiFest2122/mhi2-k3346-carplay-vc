# Audio port: where we are (index, 2026-10-15)

Read this first (whole, ~50 lines), then `tail -60 HANDOFF_CLAUDE_20261015.md` for the newest
updates. `FULL_AUDIO_PARITY_20261015.md` is the feature matrix (`rg` it, do not read whole).
`HANDOFF_AUDIO_NEXT_CHAT.md` and `RESUME.md` are inherited history: `rg` only.
This file is an index; facts live in the handoff and in `artifacts/runs/*`. Update it on every
checkpoint (see `../../rules/session-hygiene.md`).

Car evidence digest (real device MPL5 for CarPlay media, stock stereo, AudioTable amp 120/121/122): `CAR_EVIDENCE_AUDIO_20261015.md`.
Owner's 5F sound-dataset findings (Signalflow 0x31, dataset ABI, safe-restore lessons, pc persistence path): `DATASETS_5F_DIGEST_20261015.md` (rg only; source in `reference-owner/`).

Owner menu photos and extension assessment: [MMI_MENU_PHOTOS_20261019.md](MMI_MENU_PHOTOS_20261019.md). Focus All/Front only; Surround/3D absent. UpMix belongs below Focus; qualify insertion with hidden 3D before live UI claims.

## Goal

Full audio parity with `luka-dev/mib2q-carplay-rgi` @ `74b0d70d...` as a standalone M.I.B. module
for K3346 / MU1438 / G22 (Tegra3, QNX6.5, internal amp, retrofit center, stock sub). No Ukrainian
dependency. NOT finished. No vehicle writes, flash, push, deletions or subagents without the owner.

## Done and evidenced (emulator / host only)

- Reference numeric pipeline on ARM/QNX: SCENE+mask (run 090448), type103 APAP/auth/trim (090830).
- NVIDIA sink `prototype/nvidia_pcm_sink.{h,c}`: host lifecycle/concurrency PASS; native MPL1/TDM
  run 131215 (L/R within 1 LSB, six one-hot channels exact).
- FDK AAC decoder bridge (no Cinemo on K3346): native run 133323 PASS.
- Full pipeline binary in the guest from a removable SD image: run 141931, PASS 37888 frames / 74 writes.
- K3346 hook with NVIDIA sink instead of QSA (shim `prototype/nvidia_qsa_shim.h`): run 143908,
  PASS 49 writes, 3 open/close cycles.

- Live MMI level (file poll), SoundCheck target + metadata ramps in the NV hook: host and guest PASS
  (runs 143908 base, 145949 live scenarios).

- TDM tap for hook runs (watermark + replay + ramp controls): inconclusive, see handoff Update 4; tap is not an ordered-sample observer.

- Live APAC<->AAC switching, 44.1k AAC fallback (own resampler), async mask worker on ARM: host and guest PASS (run 153224).
  Gap-free / purge on reopen are now an explicit physical-vehicle gate (owner decision).

- Fail-closed gate (QUALIFIED=0 => no 5.1 advertised, nothing spatial) + inert draft package + 9 installer simulations PASS: handoff Update 11.
- Hook built as `.so` (`scripts/build_native_hook_so.py`) and run in the guest via LD_PRELOAD against the real libairplay `CAudioDriverIF` (run 162833): claim -> NVIDIA sink 6ch, 32 writes, clean close. With a declared main stream (stub for RTLD_NEXT) the realtime upmix runs, 32/32 periods, libairplay fingerprint accepted (run 163158). TDM capture (run 163604): level 0 = surround exactly 0, levels 2/4 = RL/RR energy (2 vs 4 not distinguishable on this signal). Handoff Updates 8-10.

## Open gates (nothing below is proven)

1. Gap-free / real-time output: bursts are not paced; "No buffer" seen in the guest log.
2. Downstream ring purge on close/reopen and full FLUSHBUFFERED/recovery.
3. Real HTTP/TCP, live iPhone, focus callbacks. Stock type-100 path: nvaudio bridge host+guest (Update 7); real libairplay CAudioDriverIF + LD_PRELOAD .so claim path done in guest (Update 8); still open: real stream object/HMI ACK (stubbed), dio_manager focus, output capture.
4. Backpressure/timeout semantics of real `nvaudio_pcm_write`; Tegra CPU budget.
5. RCC internal route (6ch to speakers): reader geometry, PreDownMixer/MainMix mapping, INIC BW,
   RCC image restore. `MHI2_NV_SIX_CHANNEL_QUALIFIED` stays 0; never advertise 6ch.
6. Live level control, MMI row, settings-to-runtime on K3346.
7. ~~Cross-architecture AAC hash mismatch~~ measured: <= 1 LSB (313 of 51200 samples), see handoff Update 6.
8. M.I.B. installer/CHECK/backup/restore: INERT draft exists (`artifacts/mib-audio-hook-draft.zip`, handoff Updates 11/15, owner doc `MIB_AUDIO_HOOK_DRAFT_20261015.md`), host-simulated only (12 installer tests); no 6ch-enabled package, no on-unit CHECK yet.

## Next concrete step

2026-10-28 readiness review: **NOT READY as a functional full-audio installation**.
Latest v7 CarPlay / v5 BT ZIPs and manifests verified; both inert. v7 installer
21 PASS under sh/oksh/mksh, BT18 PASS, Java settings1 PASS. Menu insertion remains
unproven with hidden 3D; settings test stubs the UI. Next: exact-stock hidden-3D
UI qualification. See `INSTALL_READINESS_20261028.md` for contents and open gates.

UPDATE 51 (2026-10-25, static + host, from the ProManiFest2122 K3346 CarPlay base review): hook `.so` now built with `-fvisibility=hidden` + `scripts/hook_so.exports.map` (17 exports instead of 164; only AirPlay/Audio/createMetadata/nvaudio interposers stay): the CarPlay base `libmib2high-carplay-rgi.so` exported 29 same-named symbols (iap2_*, log_*, hook_inject_message + 4 AirPlay interposers) and both go into `dio_manager` via LD_PRELOAD (colon list works on QNX). Only the 4 AirPlay interposers stay shared; both chain through `dlsym(RTLD_NEXT)`, real order/behaviour untested (emulator cannot run dio_manager). Package `mib-audio-hook-draft-v6` (inert, built as v7 now, `prototype/mib-hook` + `scripts/build_mib_audio_hook.py`): no `lsd.sh` replacement (stock lsd.sh already loads `lsd/jars/*.jar`; stock lsd.sh is only a gate), `smartphone_integrator.json` edited in place by `scripts/json.awk` (merges into an existing LD_PRELOAD with `:`, restore removes only our entries, edit proven reversible before any write), unit pitfalls fixed (no `hash()` function, plain-file lock, no `mkdir` in /tmp, awk fallback to PATH gawk, `mount -uw` SD, wrapped `df`, `{ cat /dev/null > f; }`, `mod/command.sh`). 20 installer tests PASS under /bin/sh, oksh, mksh; test_mib_audio 7 and test_mib_audio_bt 18 PASS. Not rebuilt: qualified `native-hook-so` and BT `-bt-inert` hooks (old export set; BT targets mm-renderer, not dio_manager). Not verified: guest run of the hidden-symbol hook, anything on the car. UPDATE 53 (owner order is now: CarPlay VC package, then Ukrainian, then audio; host check with the real VC `smartphone.awk` on the stock JSON): VC edit then audio apply/strip round-trips byte-exactly, merged line `LD_PRELOAD=<vc>.so:<audio>.so`; VC does not touch lsd.sh, lang_data.zip, core binaries or Ukrainian files, jar packages do not overlap (`org.mib2high.carplay.rgi`, `com.luka.carplay.routeguidance|framework` vs `com.luka.carplay.tone|core`, Ukrainian `de.audi.*`). Reinstalling the VC package AFTER audio deletes the audio LD_PRELOAD (its awk drops any LD_PRELOAD line): reinstall audio then. Each package ships its own `mod/command.sh` (VC: the installer itself): copy the next package's `mod` files over before each run. UPDATE 52 (owner order: Ukrainian package first, audio afterwards, separate packages): package `mib-audio-hook-draft-v7` (zip sha256 999439d5…) accepts the exact Ukrainian preview.10 `lang_data.zip` as baseline (variant `uk`, payload = same zip + UpMix strings incl. uk_UA, 21 tests PASS under sh/oksh/mksh); RESTORE audio BEFORE RESTORE Ukrainian (its checks need its own zip). v6 is superseded. Install order with the CarPlay base: base first, audio last; its smartphone.awk deletes any LD_PRELOAD line, so reinstalling the base needs audio reinstalled.

UPDATE 50 (2026-10-22): inert libs rebuilt; packages `mib-audio-hook-draft-v4` / `mib-audio-bt-draft-v4` (handoff Update 50).

UPDATE 49 (2026-10-22): installer printf guard bug fixed (package v3), centre-feed knob added (default 0), route-down scenario found no reopen backoff (HOLD policy open); see `../NEXT_TESTS_AND_PC_PLAN_20261022.md`.

UPDATE 48: upstream isMedia fix (6b91d2c1) applied; packages rebuilt: CarPlay hook-draft-v2, BT bt-draft-v3 (handoff Update 48).

UPDATE 46 (car session TODO, owner agreed 2026-10-17): H4 test = write `datasets-h4/0x3000_v16_signalflow30_crc6539.hex` (Signalflow 0x31->0x30, CRC 6539) via CarScanner, then listen (plan + questions: `datasets-h4/H4_TEST_PLAN_UA.md`). Goal: does ch4 drive the sub. Prior ~15% (web: Jet-ZJ got a 4ch+shared centre/sub downmix at mode 30). Rollback: owner's v16 (CRC 4454) or stock (692A). Needs no adapter. Record result in field log.

UPDATE 45: BT package v2 (real copy keeps basename, fail-open fallback) + emulator spawn() of the shim OK (exit 0), medialauncher itself unproven. Handoff Update 45.

UPDATE 44: inert BT/USB/SD package built (`artifacts/mib-audio-bt-draft.zip`, builder `scripts/build_mib_audio_bt.py`, 18 host installer tests PASS, independent of the CarPlay package); shim spawn by medialauncher still unproven; no install without the AX88772B adapter. Handoff Update 44.

UPDATE 41 (session 4, car): CPU measured on Tegra3 (no mask 35 %, inline mask 88-91 %: mask only async), no discrete sub path (LFE must stay 0, rear bass reaches the sub), 8ch format folds onto fronts (always 6ch), mm-renderer == firmware, telnet closed on Wi-Fi. Handoff Update 41.

UPDATE 38: voice: centred speech rears -35 dB, hard-left speech goes to rears (host); BT bridge stress PASS; MMI jar statically links against K3346 HMI (17/17 classes), jar now major 46. Handoff Update 38.

UPDATE 37: mm-renderer is spawned by stock medialauncher (hard-coded env); injection = mm-ipod-style exec shim, unverified; SCENE bench built (host 0.04-0.2 ms/period, emulator unusable, Tegra unknown); session-4 kit ready (car-tests/session4-kit). Handoff Update 37.

UPDATE 36: Bluetooth/USB/SD bridge variant (MPL1 44.1k, own media route) host + emulator guest PASS with stock nvaudio_play (level 0 fronts only, level 2 rears ~75); CPU guard trips in the emulator; mm-renderer injection unproven. Handoff Update 36.

UPDATE 28 (car A/B): centre sounds the same with 6ch C=0 as with stereo, so the amp/DSP derives it; no centre feed (handoff Update 28). Sub: no feed either (Update 21).

UPDATE 27 (session 3, car): stock CarPlay confirmed: MPL5 = 8-slot TDM1_OUT_MPL, route set on open, source 10 same as BT, Sound_System=01 internal amp. Handoff Update 27.

UPDATE 23: stock CarPlay opens MPL5 with the same 8-slot geometry as MPL1 and the route follows that open; discrete 6ch likely works via MPL5 in a CarPlay session without RCC edits (inference, needs live test). See handoff Updates 22-23.

UPDATE 21 (2026-10-16, car session 2): discrete ch1,2,3,5,6 reached the labelled speakers on MPL1 with a selected-but-paused source (sub is amp bass-managed, ch4 silent); the static 'RCC edits required' conclusion below is contested for MPL1, see handoff Update 21. CarPlay MPL5 path and MPL1 ownership by mm-renderer unresolved.


Static conclusion (Update 19, `RCC_K3346_SITE_CHECK_20261015.md`): discrete 6ch to the speakers on this internal-amp car needs RCC edits (MIBRoot resource 0x78 + DSP MainMix maps) and an RCC flash, same as the reference's plan; MMX-only is not enough. Keep `MHI2_NV_SIX_CHANNEL_QUALIFIED=0`. Any RCC flash is a separate owner risk decision.

Gate 5 static (Update 2 in RCC_STATIC_FINDINGS): mixer/xbar/downmixer config tables located in the DSP image (0x8760ab58..0x8760adc8, loaded at 0x8755bf88..0x8755c2a4), fields unproven, no patch candidate.
Note: stock dio_manager cannot start in the emulator profile (libcomm.so absent), so loading the .so into the real dio_manager is untestable there (handoff Update 14).
In-car one-hot test RUN (Update 17): MPL1 accepted 6ch but nothing audible, MPL5 write status 4 => INCONCLUSIVE (test bypasses audio management). One-visit kit for the next car session: `car-tests/session2-kit.zip` (playback under a live source, ANN devices, pause state; interpretation table inside). Static routing/mute analysis: `MPL_ROUTING_MUTE_STATIC_20261015.md`. Full CarPlay path with the QUALIFIED hook needs a separate approval.

Gate 5 (RCC): see `RCC_STATIC_FINDINGS_20261015.md` (incl. the steps 1-3 update). No patch candidate yet; leading hypothesis: upstream width (MIBRoot 0x78/MOST BW 4), DSP reader already TDM8; mixer maps unlocated, needs a symbolized C6x disassembler; the earlier MVK 2->6 idea was wrong
(that constant is an input table index). Continue the static trace of the McASP3 reader channel source, or stop and wait for a car test.
Gap-free/purge are a physical-vehicle gate. Items 4 and a-c of the audio parity list are done in host/guest (see handoff Updates 2-5).

## Constraints that bite

- IFS restore capacity 29,652,992 B; AAC profile uses 29,622,784. Do not add libraries past it.
- Link-only library views, libc/libm and libnvaudio stubs NEVER go into a guest image or package.
- `build_pipeline_sd.py` needs python3.13 (mmap trackfd in the shared `qnx6_image.py`).
- Do not edit `../../runtime/qemu-mhi2` or the original emulator images; use snapshot-on and
  experiment-owned helper copies. Old `mib-audio-standalone-preview.zip` is NOT for 5.1 testing.
- Git here is a local source checkpoint only (artifacts, logs, upstream are excluded).
