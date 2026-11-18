# Owner audio-menu baseline and extension assessment — 2026-10-19

## Evidence and exact target
K3346 / MU1438 / G22, internal amplifier, retrofit centre, stock sub. Owner supplied three photos dated 2026-10-15; inspected decoded pixels on 2026-10-19. Originals preserved in `reference-owner/menu-20261015/`.
- IMG_3670: Sound: Entertainment -> Treble, Bass, Balance/fader, Sound effects, Speed dependent volume control, in that order. No visible Middle/Subwoofer row.
- IMG_3671: Balance/fader -> graphical cabin with two-axis position control.
- IMG_3672: Entertainment -> Sound effects -> one visible Focus row, selected `front`; right-hand sound-effects illustration. No Surround level or 3D effect.
- Owner confirms two Focus choices. The dataset findings identify them as All and Front; this photo does not show the expanded dropdown.

## Parameterisation match
`reference-owner/MIB2_5F_sound_datasets_technical_findings_2.md` sections 5–6: 0x3B00[30:32] = 1,2; slot 1 selects 0x3000 Profile1, slot 2 Profile2. Audi displays enum 2 as Front, although the VW template calls it Driver. Preserve All/Front labels and both existing CarEQ profiles. Do not infer acoustic focus from labels alone: these are owner-tuned EQ/delay profiles. Two dataset slots do not establish a third independent profile.

## Exact-firmware static comparison
`runtime/k3346-decomp/dec_all/de/audi/tghu/tone/hmi/evohigh/ToneScreenFactory.java:138` maps screen 1000006 to ToneScreenBag1.tONESOUNDEFFECTMAIN. In `ToneScreenBag1.java:568` onward that screen constructs three menu items (model IDs 1000014, 1000040, 1000034); the firmware model interface contains THREE_D_MODE_CHOICE, SHOW_THREE_D_MODE_CHOICE and TONE_SURROUND_ON_OFF_CHOICE. Runtime conditions control availability; presence of widgets/models does not prove processing in the internal amplifier. No condition bypass or coding/dataset change was made.

## UpMix placement and integration gate
Desired layout: Sound: Entertainment -> Sound effects -> Focus [All/Front], then CarPlay UpMix [Off/Moderate/Normal/Full/Aggressive]. Preserve the stock parent menu, Balance/fader, Focus profiles and illustration. BT/USB/SD remain a separate module; do not duplicate a row or imply CarPlay-only control covers every source.
Current prototype UpmixToneUi.addRow obtains styling and the parent via findViews(THREE_D_MODE_CHOICE)[0]. Photos establish that 3D is invisible on this car. The static factory creates the item, but whether it remains addressable with the required parent when hidden must be tested on the exact configuration. Add an explicit hidden-3D insertion test before claiming the menu is ready; do not require enabling 3D to expose UpMix.

## Surround and 3D assessment (not implemented)
- Surround level: a custom DSP control of rear ambience/upmix strength is plausible using the existing SCENE/NVIDIA path, initially for sources actually intercepted by the hook. Independent control needs a defined DSP parameter and headroom/Focus/level-0 tests; a second slider that merely duplicates UpMix is not yet justified. Enabling the OEM row alone does not establish OEM surround support.
- OEM 3D: unqualified. Owner stock dataset findings section 11.2 identify GADK as Bypass with a nonzero table, but do not establish equivalence to the displayed B&O 3D effect or executable DSP support on this car. Trace HMI -> tone service -> DSP before proposing activation.
- Custom spatial widening is a possible separate research direction, not factory B&O 3D or physical height channels. Tegra measurements already show synchronous mask work is too expensive (88–91% CPU); any extra algorithm requires a measured budget and listening validation. No new effect is promised.

Next: qualify insertion below Focus while OEM Surround/3D stay hidden; then trace OEM capability conditions and DSP setters read-only if pursuing native controls. No code or vehicle changes in this session.
