#!/usr/bin/env python3
"""Per-channel RMS of the emulated TDM1 capture, grouped by the probe's marker (200+level).
Coarse: the tap does not preserve block order (handoff Update 4), so only aggregate energy is meaningful."""
import json, sys
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).parent))
from analyze_nv_hook_tdm import decode
run = Path(sys.argv[1])
raw = (run / "native-tdm0.raw").read_bytes()
rows = [tuple(map(int, l.split())) for l in (run / "native-tdm0.index").read_text().splitlines()]
out = {}
for _, off, n, marker in rows:
    if 200 <= marker < 210:
        out.setdefault(marker - 200, []).append(raw[off:off + n])
names = ["FL", "FR", "C", "LFE", "RL", "RR"]
res = {}
for lv, parts in sorted(out.items()):
    pcm = decode(b"".join(parts)[:len(b"".join(parts)) // 32 * 32]).astype(np.float64)
    act = np.abs(pcm).max(1) > 8   # drop silent frames (pre/post roll)
    p = pcm[act]
    rms = np.sqrt((p ** 2).mean(0)) if len(p) else np.zeros(6)
    res[lv] = {"frames_total": int(len(pcm)), "frames_active": int(act.sum()), "rms": dict(zip(names, map(lambda v: round(float(v), 1), rms)))}
print(json.dumps(res, indent=1))
(run / "level-rms.json").write_text(json.dumps(res, indent=1) + "\n")
