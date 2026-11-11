# RCC DSP static findings (gate 5), 2026-10-15 - NO PATCH CANDIDATE YET

Scope: static reading of the K3346 C674x image (`artifacts/rcc-dsp-sections/memory.bin`, flat, base 0x87500180,
disassembly `logs/rcc-dsp-code.txt`, tic6x objdump `-EL`). Nothing here is a flash input. The reference plan
(`reference-audio-74b0d70/docs/audio/INTERNAL_AMP_PATH.md`, sites 1-4) is for the MHI2Q MU1316 image: its addresses do not carry over.

## What is established
- `mvkh 2271150080` = 0x875F0000 (string table), `2271215616` = 0x87600000 (data). DSP debug strings are plain text at
  0x875f4b00..0x875f5300 (e.g. `arcInputDev_ReaderSettings` 0x875f4c37, `mcasp3_HW_Handle` 0x875f4e96,
  "Frontground chip is nvidia, mcasp3_rcv_cfg!" 0x875f4ea7 / "...QC, mcasp3_rcv_cfg_QC!" 0x875f4ed3).
- The setup function (0x87532d30..0x87533730 and the blocks before it) is a per-McASP sequence: create HW handle, init HW,
  `createPushmodeReaderDevice/Instance`, `ARC_registerInput`, writer device/instance. Blocks found for mcasp0 (~0x875329xx),
  mcasp1 (0x87532e..), mcasp2 (0x87533040..0x8753333c), mcasp3 (0x875333e0..0x87533730).
- `callp 0x875be9c0` (12 call sites) registers an ARC input/output: args (flag, handle, settings ptr, **index N in b6**, size 16 in a8).
  Disassembly of 0x875be9c0: `mpy32 116,N` and `cmpltu N, *(ctx+40)` => N is an INDEX into a table of 116-byte entries with a bounds check,
  not a channel count. N values per site: mcasp0 1, mcasp1 8, mcasp3 reader 2 (`mvk .L2 2,b6` at 0x875336e8), then 3,4,5,6,9.

## Corrections to earlier notes
- The "site 3" guess `mvk 6,a5` at 0x87532a2c (earlier chat) is a store in the mcasp0 registration block, not a channel count.
- The `mvk 2,b6` at 0x875336e8 (McASP3 reader) is the input table index. Changing 2 -> 6 would register the reader in slot 6 (collision
  with the slot-6 input), NOT widen it. Do NOT patch it.

## Not established (why no candidate)
- Where the McASP3 reader's real channel count/slot geometry lives: the ReaderSettings struct is filled from stack values returned by
  `callp 0x875e4780` and McASP config reads (stores at +32..+44 of the config struct, e.g. 0x875336c4..0x875336d0), not from a constant.
  The NVIDIA head is TDM8 (AFSRCTL 0x412, RTDM 0xFF), so the reader may already take 8 slots; then the limiting parts are the
  downstream PreDownMixer/MainMix maps (0x8760c108.., 0x8760c238.., 0x876225a0) and the MOST width (SPEAKERS sink width 4 vs multichannel 26).
- word9 = 2 in the McASP3 descriptors (0x8760ba94 / 0x8760b9fc): meaning unknown.
- Whether MIBRoot resource 0x78 -> eSNK_DTCP51 (internal DSI sink 6, width 26) is accepted for the DIO media source on this car.

## Next evidence needed (each can be done statically, in this order)
1. Trace the producer of the stack values stored at the McASP3 reader config (callee 0x875e4780 and the nvidia/QC branch at 0x87533544).
2. Trace consumers of the 116-byte input entry (reader callback path) to find where channel count is read.
3. Resolve PreDownMixer/MainMix maps for input 2 and confirm voice/phone routes use other inputs.
No emulator for the C674x exists here: any candidate stays unverified until a car test the owner explicitly authorizes.

## Update: trace of steps 1-3 (same day) - still NO patch candidate
Tools: `/tmp`-style helpers were used (init image = memory.bin + cinit `records.json` overlay; regenerate from `inspect_rcc_dsp.py` output).
1. **Values stored into the reader/writer config.** The stores at ctx+32..+44 (e.g. 0x875336c4..0x875336d0) take stack slots filled by two
   getter stubs: `0x875e4760` returns 0x875CFF20 and `0x875e4780` returns 0x875CCE80 (out-pointer stubs; targets decode as C6x code).
   With the strings `readerCallback`/`writerCallback` (0x875f4d12/0x875f4d21) these are callback pointers, NOT channel counts.
   Settings structs are filled with immediates per McASP block: mcasp0 S0=0x8760B1C4 {3,6,?,1,0}, mcasp2 S2=0x8760B20C {6,2,..},
   mcasp3 S3=0x8760B22C {8,16,..} (stored at 0x87533448/0x87533460, BEFORE the nvidia/QC branch at 0x87533544). Field meaning is
   unproven; S3 {8,16} is consistent with a TDM8, 16-bit geometry, i.e. the McASP3 hardware side is probably already 8 slots on this image.
2. **116-byte input entry.** `0x875be9c0`: entry = base + 116*N; entry+0 = handle, +108 = size(16), +56 = pointer to a sub-struct that receives
   settings words [0..3] (word [2] = callback pointer, invoked with (ctx+40, ctx+44)). The readers of the entry in the callback path
   (where a channel count would be consumed) were NOT traced.
3. **PreDownMixer / MainMix maps.** The reference addresses (0x8760c108, 0x8760c238.., 0x876225a0, 0x87602648) hold zeros or unrelated data in the
   K3346 initialized image, so they do not apply. The K3346 map locations were not found: the DSP string table is packed, so
   (mvk,mvkh) pairing heuristics give garbage near 0x8755a000..0x8755bd00 (PreDownMixer/MainMix xrefs). A symbolized disassembler
   (IDA/Ghidra C6x with the cinit overlay) is needed.

Working hypothesis (unproven): the DSP reader is already TDM8 for the NVIDIA head; the stereo limit is upstream (MIBRoot resource 0x78 ->
SPEAKERS sink width 4 -> MOST BW 4), so the reference's site 1 (MIBRoot 0x78 -> 0x46f/eSNK_DTCP51, DSI sink 6 width 26) plus a zone-0 mixer
input mapping is the likely shape, with the DSP reader edit (sites 2/3) not needed. No bytes should be written on this evidence.

## Note (owner datasets, 2026-10-15)
The owner's unit already runs dataset `Signalflow=0x31` (6ch DSP topology with centre+sub active); this does not show the mixer input takes 6ch from the NVIDIA head. CarEQ channel order differs from TDM order. See `DATASETS_5F_DIGEST_20261015.md`.

## Update 2 (2026-10-15, static, same image): DSP signal chain and where the mixer configuration is loaded - STILL NO PATCH CANDIDATE
Helper: `scripts/rcc_dsp_image.py` (memory.bin + cinit overlay; `words|str|hex ADDR`). Evidence: `logs/rcc-mixer-config-tables-20261015.txt`. Static only; the heuristic decode of the compact C6x
(16-bit) instructions in `logs/rcc-dsp-code.txt` is NOT reliable enough for argument-level claims, so statements below are about string xrefs and layout, not semantics.
- **Chain objects named in the DSP** (0x875f46fc..0x875f48df): `hPreDownMixer`, `hPreAntiAlias`, `hTLAM_Speech`, `hGALA`, then per zone `hZone0_UEQ / AEQ / SourceEQ / Lim / Gain / MainMix / refMix / Delay / SinkDelay /
  DynComp / AntiAlias`, `hZone2_SinkDelay`, `hZone3_AntiAlias`, `hZone0_SigGen`. Error strings of the setup: "downmixer channel offset set failed" (0x875f46ce), "Loading mixer configuration" (0x875f4917),
  "Setting mixer output gains" (0x875f493d), "Setting mixer ChanOutGainOffset" (0x875f4962), "Setting xbar settings in Refchannel Mixer" (0x875f498c). So input-to-output channel routing is configured by data
  handed to those calls (downmixer channel offset, mixer config, xbar), i.e. a configuration, not a fixed 2-channel path in code.
- **Where**: string xrefs (`mvk` low16 + `mvkh 0x875F0000`) show the object-creation sequence at 0x8755b1e0..0x8755b7e4 (names 0x479c MainMix, 0x47ab refMix, ...) and the mixer-config block at
  0x8755bf88..0x8755c2a4 (error string 0x4917 referenced at 0x8755c0e4 / 0x8755c2a4). This corrects the earlier note that the 0x8755a000..0x8755bd00 xrefs were "garbage": the string references there are real.
- The config block builds on the stack arrays of 3 descriptors x 20 bytes from `.data` tables 0x8760ab58 / 0x8760ac88 / 0x8760ad18 / 0x8760adc8 (sploopd of 3 iterations, stride 20) and passes them to `callp 0x875d0a60`
  (the "load mixer configuration" call, preceded by `mvk 8,b4`). Table bytes (see the log): byte-sized channel/slot indices such as `04 00 06 00`, `00 05 00 07`, `04 05 06 07`, `04 05` and an all-ones entry (0x8760ab58);
  two near-identical variants (0x8760ac88 vs 0x8760ad18) differ only in two bytes (`04`/`02`, `05`/`03`), consistent with a per-chip or per-mode variant (the code selects with a predicate). Meaning of the fields is UNPROVEN.
- Implication: a 6ch widening, if the limit is here, is a data patch to these tables (small, aligned), not code; but without the field layout (and a C6x emulator) no byte may be proposed. The owner's dataset
  `Signalflow 0x31` acts downstream (CarEQ/amp topology) and does not show that this mixer receives 6 input channels.
- Not done: field layout of the 20-byte descriptors, the reader channel source (open since the first note), which zone the internal speakers belong to, the MOST/DSI sink width. The cheapest discriminator remains the
  in-car stock-player test (`CAR_TEST_6CH_ONEHOT_20261015.md`), which needs the owner's go-ahead.

## Update 3 (2026-10-15): the 20-byte descriptor tables are the reference's MainMix maps; MIBRoot site-1 data confirmed on K3346
See `RCC_K3346_SITE_CHECK_20261015.md`. Correction: the 4th table is `0x8760ada8` (not `0x8760adc8`); layout = reference minus 0x15b0 (source map `0x8760ab58`, channel maps `0x8760ac88/ad18/ada8`). K3346 MIBRoot holds the same resource-0x78 record
(file `0xC627DC`) and connection-162 record (file `0xC61860`) as the reference: internal pair `0x46e -> SPEAKERS`, external `0x46f -> DTCP51`. Conclusion: no MMX-only path; 6ch to speakers needs RCC edits (owner's risk decision).
