#!/usr/bin/env python3
"""Compare host vs guest AAC PCM dumps (prototype/aac_dump.c output, CRC-checked duplicated hex chunks)."""
import json, re, sys, zlib
from pathlib import Path
import numpy as np


def parse(text):
    got = {}
    for line in text.splitlines():
        m = re.search(r'(S \d+ \d+ [0-9a-f]+) ([0-9a-f]{8})\s*$', line.replace('\r', ''))
        if not m or zlib.crc32(m.group(1).encode()) & 0xffffffff != int(m.group(2), 16):
            continue
        _, au, off, hx = m.group(1).split()
        got[(int(au), int(off))] = np.frombuffer(bytes.fromhex(hx), dtype='>u2').astype(np.uint16).view(np.int16)
    return got


def main():
    host, guest = (parse(Path(p).read_text(errors='replace')) for p in sys.argv[1:3])
    common = sorted(set(host) & set(guest))
    diff = np.concatenate([host[k].astype(int) - guest[k].astype(int) for k in common])
    sig = np.concatenate([host[k].astype(float) for k in common])
    out = {"host_chunks": len(host), "guest_chunks": len(guest), "compared_samples": int(len(diff)),
           "identical": int((diff == 0).sum()), "max_abs_diff": int(np.abs(diff).max()),
           "rms_diff": float(np.sqrt((diff ** 2).mean())), "signal_rms": float(np.sqrt((sig ** 2).mean())),
           "histogram_abs_diff": {int(v): int(c) for v, c in zip(*np.unique(np.abs(diff), return_counts=True))}}
    print(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
