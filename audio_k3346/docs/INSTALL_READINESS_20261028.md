# Audio installation readiness — 2026-10-28

## Verdict and acceptance

NOT READY for installation as a functional full-audio upgrade. The latest CarPlay
v7 and BT/USB/SD v5 packages are deliberately inert. They do not claim stock audio
or enable spatial/APAC processing. A menu selection would not enable their DSP.
Acceptance is audible intended processing on this K3346/internal-amp car, a visible
and effective MMI control, preserved stock Focus/voice paths, and proven recovery.
Next visible milestone: qualify the actual packaged UI on the exact stock HMI with
3D hidden. This review used static inspection and host simulations, no emulator
runs, phone sessions, vehicle access, installation or firmware changes.

## Actual deliverables checked

- `artifacts/mib-audio-hook-draft-v7.zip`: SHA256
  `999439d557aaed0e9f7fa8b14863d06dbd18601bd99b1255127aadcb7e9f0b9a`.
- `artifacts/mib-audio-bt-draft-v5.zip`: SHA256
  `4bd657aae0e15d89288cd5304942c8876d3d20a22d026c987849e02d80009dd4`.
- Both match their recorded SHA256; ZIP CRC checks pass; all 15/9 respective
  SHA1SUMS entries pass. Every archived file matches its unpacked package directory.
- CarPlay hook matches `native-hook-so-inert`; package metadata declares
  `MHI2_NV_SIX_CHANNEL_QUALIFIED=0`, `claims_stock_audio=false`,
  `advertises_5_1=false`, `apac_type103=LOADED_BUT_DISABLED`, `lfe_assist=false`.
- Packaged UI matches `artifacts/java/mhi2_audio_ui-v46.jar`: six classes, all major
  46; stock replacement is `de/audi/tghu/tone/hmi/Activator.class`.
- Packaged Ukrainian baseline matches the CURRENT local localization payload at
  `../../publication/mhi2-ukrainian-localization/sd-card/mod/uk-localization/payload/lang_data.zip`,
  SHA256 `995440ea06cec135a37f53a4ac8fdff279b6e6d0d289e4d056738612fd4710b3`.
  This is not evidence of what is currently installed in the car.

## Menu answer

One intended added row: Sound / Entertainment / Sound effects / CarPlay UpMix,
with Off, Moderate, Normal, Full, Aggressive. English and Ukrainian strings are
present in the corresponding packaged language tables (slots 1000–1005).
OEM Surround, OEM 3D, separate centre/sub controls are not implemented by this UI.
BT/USB/SD is a separate inert package, not included as enabled coverage in v7.

`prototype/java/src/com/luka/carplay/tone/UpmixToneUi.java:98` obtains its parent
and styling from `findViews(THREE_D_MODE_CHOICE)[0]`. The owner's 3D row is hidden.
Whether that view still has the required parent at insertion time remains open;
an exception is swallowed by createScreen and can leave the extra row absent.
The row additionally requires successful UpmixSetting boot/storage/file publication.
There is no fresh real-HMI rendering proof. `tests/test_audio_java.py` substitutes
UpmixToneUi with a stub: its PASS covers first-boot Off, persistence and selection,
NOT row insertion or hidden-3D behavior. Thus “the menu will appear” is unproven.

## Remaining installation gates

1. Qualify hidden-3D insertion, loading of the packaged Activator, and actual
   settings-to-runtime behavior; preserve Focus All/Front and its tuned profiles.
2. Build/qualify an enabled hook only after resolving active audio gates. Existing
   host/emulator DSP evidence is not a live CarPlay/MPL5, reconnect/purge, focus,
   voice/Siri/phone or sustained realtime proof for the final package.
3. Real centre rendering needs listening validation. Owner chose upstream
   AUDIO_SCENE_CENTRE=1.0; independent LFE/sub remains unproven and assist is off.
   Historical MPL1 speaker evidence does not prove full live MPL5 behavior, nor
   justify claiming an RCC flash is definitely required.
4. Current CarPlay-base/audio AirPlay interposition order remains unverified.
   STATUS records four shared AirPlay interposers despite export pruning.
   Whole-combination G1–G7 and independent recovery remain open.
5. BT v5 additionally lacks actual medialauncher/watchdog qualification and the
   newer export-set rebuild recorded as open in STATUS.

Install-order evidence remains base → Ukrainian → audio; restore audio before
Ukrainian. Reinstalling the CarPlay base can remove the audio preload entry.
No current vehicle CHECK or recovery-channel verification was performed.

## Fresh verification

- `python3 tests/test_mib_audio_hook.py`: 21 PASS, 80.308 s, /bin/sh.
- `python3 tests/test_mib_audio_bt.py`: 18 PASS, 47.442 s.
- `python3 tests/test_audio_java.py`: 1 PASS, 0.588 s (UI stub limitation above).
- `MHI2_AUDIO_TEST_SHELL=/opt/homebrew/bin/oksh python3 tests/test_mib_audio_hook.py`:
  21 PASS, 85.908 s.
- `MHI2_AUDIO_TEST_SHELL=/opt/homebrew/bin/mksh python3 tests/test_mib_audio_hook.py`:
  21 PASS, 85.022 s.
- Broad DSP rebuild/regression and emulator runs were not needed to establish
  the inert-package and missing-UI-proof blockers and were not run.

Earlier STATUS/FULL_AUDIO_PARITY entries are historical and partly superseded;
this report separates actual current package contents from prior pipeline proof.
