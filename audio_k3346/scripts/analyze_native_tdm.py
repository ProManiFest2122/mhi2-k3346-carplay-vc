#!/usr/bin/env python3
"""Check indexed native six-channel one-hot captures after the stock mux.

Positions recovered from K3346 deva-nvaudio's FC00/FC01 packers. This decodes
the principal S16 portion, not all 24 significant bits or an RCC speaker map.
"""
import argparse
import json
from pathlib import Path

import numpy as np

# TDM1 carries noncontiguous sample pieces, not interleaved WAV channels.
S16_BYTES = [(0, 1), (5, 6), (10, 11), (15, 8), (12, 13), (17, 18)]


def decode(raw):
    x = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 32)
    return np.column_stack([x[:, lo].astype(np.uint16) | (x[:, hi].astype(np.uint16) << 8)
                            for lo, hi in S16_BYTES]).view(np.int16)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("run", type=Path)
    ap.add_argument("--device", choices=["MPL1", "MPL5"], default="MPL1")
    args = ap.parse_args()
    raw = (args.run / "native-tdm0.raw").read_bytes()
    index = [tuple(map(int, line.split())) for line in (args.run / "native-tdm0.index").read_text().splitlines()]
    rows = []
    for channel in range(6):
        records = [r for r in index if r[3] == 16 + channel]
        y = np.concatenate([decode(raw[offset:offset + size]) for _, offset, size, _ in records]) if records else np.empty((0, 6), np.int16)
        # Allow two native 512-frame periods for queued previous content and
        # the driver's fade-in. Measure the entire remaining interval.
        steady = y[1024:]
        target = (channel + 1) * 1000
        off = np.delete(steady.astype(np.int32), channel, axis=1)
        exact = np.count_nonzero(steady[:, channel] == target) if len(steady) else 0
        target_mask = steady[:, channel] == target
        edges = np.flatnonzero(np.diff(np.r_[False, target_mask, False].astype(np.int8)))
        longest = int(np.max(edges[1::2] - edges[::2])) if len(edges) else 0
        interior_nonexact = int(np.count_nonzero(~target_mask[edges[0]:edges[-1]])) if len(edges) else 0
        row = {"input_channel": channel, "expected_sample": target,
               "captured_frames": len(y), "steady_frames": len(steady),
               "exact_target_frames": int(exact),
               "longest_exact_plateau_frames": longest,
               "nonexact_frames_between_first_and_last_target": interior_nonexact,
               "off_channel_peak": int(np.abs(off).max()) if off.size else None,
               "marker_count_by_output": (steady == target).sum(0).tolist(),
               "independent_plateau_pass": bool(longest >= 1024 and off.size > 0 and not np.any(off))}
        rows.append(row)
        print(json.dumps(row))
    result = {"scope": f"native {args.device} to TDM1 principal S16 capture, emulated; no RCC or speaker proof",
              "wire_byte_pairs": S16_BYTES, "channels": rows,
              "all_channels_independent": all(r["independent_plateau_pass"] for r in rows)}
    (args.run / "onehot-analysis.json").write_text(json.dumps(result, indent=2) + "\n")
    print("independent six-channel plateau:", result["all_channels_independent"])
    return int(not result["all_channels_independent"])


if __name__ == "__main__":
    raise SystemExit(main())
