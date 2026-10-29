#!/usr/bin/env python3
"""Compare a guest TDM1 capture of the NV hook run with the host-expected native write stream.

Expected stream: `NV_DUMP=... bash scripts/test_nv_hook_detached.sh` (host, deterministic APAC).
Capture: guest run with MHI2_TDM_CAPTURE_BASE (marker 100/101/102 = nominal/recovery/flush).
Scope: principal S16 portion of the emulated TDM1 stream after the stock mux. Not RCC, not speakers,
not real-time pacing proof (emulator bursts).
"""
import argparse
import json
import struct
from pathlib import Path

import numpy as np

S16_BYTES = [(0, 1), (5, 6), (10, 11), (15, 8), (12, 13), (17, 18)]
TOL = 2
CHUNK = 128


def decode(raw):
    x = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 32)
    return np.column_stack([x[:, lo].astype(np.uint16) | (x[:, hi].astype(np.uint16) << 8)
                            for lo, hi in S16_BYTES]).view(np.int16)


def load_expected(path):
    data = path.read_bytes()
    at, writes, seq = 0, [], 0
    while at < len(data):
        kind, frames = struct.unpack_from("<II", data, at)
        at += 8
        pcm = None
        if kind in (0, 1):
            pcm = np.frombuffer(data[at:at + frames * 12], dtype=np.int16).reshape(-1, 6)
            at += frames * 12
        if kind != 3:
            seq += 1
        writes.append({"kind": kind, "frames": frames, "pcm": pcm, "seq": seq if kind != 3 else None})
    return writes


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("run", type=Path)
    ap.add_argument("--expected", type=Path, default=Path(__file__).resolve().parents[1] / "artifacts/native-nv-hook/expected-writes.bin")
    args = ap.parse_args()
    index = [tuple(map(int, line.split())) for line in (args.run / "native-tdm0.index").read_text().splitlines()]
    raw = (args.run / "native-tdm0.raw").read_bytes()
    parts, marks, at = [], [], 0
    frame_ns = []
    for ns, offset, size, marker in index:
        if marker < 100:
            continue
        parts.append(decode(raw[offset:offset + size]))
        marks.append((at, marker, ns))
        at += size // 32
        frame_ns.append((at, ns))
    capture = np.concatenate(parts).astype(np.int32)
    writes = load_expected(args.expected)
    # seq of record i is i+1 (the test numbers every native call, failed ones too)
    base, expected, cursor = {}, [], 0
    for w in writes:
        if w["kind"] in (0, 1):
            base[w["seq"]] = cursor
            expected.append(w["pcm"].astype(np.int32))
            cursor += w["frames"]
    flat = np.concatenate(expected)
    failed_seqs = [w["seq"] for w in writes if w["kind"] == 2]
    frames_of = {w["seq"]: w["frames"] for w in writes if w["kind"] in (0, 1)}
    silent_seqs = {w["seq"] for w in writes if w["kind"] == 1}
    seq, off = capture[:, 2], capture[:, 3]
    cls = np.zeros(len(capture), dtype=np.int8)  # 0 driver silence, 1 exact, 2 altered (fade/other)
    expected_index = np.full(len(capture), -1, dtype=np.int64)
    nonzero = np.any(capture != 0, axis=1)
    for n in np.flatnonzero(nonzero):
        sq, of = int(seq[n]), int(off[n])
        if sq in base and 1 <= of <= frames_of[sq]:
            e = base[sq] + of - 1
            expected_index[n] = e
            cls[n] = 1 if np.all(np.abs(capture[n] - flat[e]) <= TOL) else 2
        else:
            cls[n] = 2
    # A faded watermark can alias another valid id; keep only runs of >=16 frames whose ids advance by one.
    cand = np.flatnonzero(cls == 1)
    chain = np.zeros(len(capture), dtype=bool)
    for a, b in zip(cand, cand[1:]):
        if b == a + 1:
            s1, o1, s2, o2 = int(seq[a]), int(off[a]), int(seq[b]), int(off[b])
            if (s2 == s1 and o2 == o1 + 1) or (o1 == frames_of[s1] and o2 == 1 and s2 > s1):
                chain[a] = chain[b] = True
    run_start = None
    for n in range(len(capture) + 1):
        inside = n < len(capture) and chain[n]
        if inside and run_start is None:
            run_start = n
        if not inside and run_start is not None:
            if n - run_start < 16:
                chain[run_start:n] = False
            run_start = None
    cls[(cls == 1) & ~chain] = 2
    # run-length segments
    edges = np.flatnonzero(np.diff(cls)) + 1
    starts = np.r_[0, edges]
    ends = np.r_[edges, len(cls)]
    names = {0: "driver_silence", 1: "exact", 2: "altered"}
    def at_ms(frame):
        for end, ns in frame_ns:
            if frame < end:
                return round(ns / 1e6, 1)
        return None
    segments = [{"class": names[int(cls[a])], "start": int(a), "frames": int(b - a), "ms": round((b - a) / 48.0, 2),
                 "virtual_ms": at_ms(int(a)),
                 "first_id": [int(seq[a]), int(off[a])], "last_id": [int(seq[b - 1]), int(off[b - 1])]}
                for a, b in zip(starts, ends)]
    # sequence continuity over exact frames
    ex = np.flatnonzero(cls == 1)
    jumps = []
    for a, b in zip(ex, ex[1:]):
        (s1, o1), (s2, o2) = (int(seq[a]), int(off[a])), (int(seq[b]), int(off[b]))
        ok = (s2 == s1 and o2 == o1 + 1) or (o1 == frames_of[s1] and o2 == 1 and s2 > s1)
        if ok and o1 == frames_of[s1] and s2 != s1 + 1:
            missing = [q for q in range(s1 + 1, s2)]
            jumps.append({"type": "skipped_writes", "after_id": [s1, o1], "missing_seqs": missing,
                          "all_are_injected_failures": all(q in failed_seqs for q in missing)})
        elif not ok:
            jumps.append({"type": "discontinuity", "after_id": [s1, o1], "next_id": [s2, o2], "capture_frame": int(b),
                          "delta_frames": int(b - a)})
    seen = {}
    repeats = 0
    for n in ex:
        key = (int(seq[n]), int(off[n]))
        if key in seen:
            repeats += 1
        seen[key] = 1
    expected_frames_ours = sum(f for q, f in frames_of.items())
    audio_seqs = [q for q in frames_of if q not in silent_seqs]
    got_seqs = {int(q) for q in np.unique(seq[ex])}
    missing_audio = [q for q in audio_seqs if q not in got_seqs]
    first_ex, last_ex = (int(ex[0]), int(ex[-1])) if len(ex) else (0, -1)
    inner = segments_in = [x for x in segments if x["start"] >= first_ex and x["start"] <= last_ex]
    inner_silence = [x for x in inner if x["class"] == "driver_silence"]
    inner_altered = [x for x in inner if x["class"] == "altered"]
    phases, last = [], None
    for frame, marker, ns in marks:
        if marker != last:
            phases.append({"marker": marker, "capture_frame": frame, "virtual_ms": round(ns / 1e6, 1)})
            last = marker
    result = {
        "scope": "emulated TDM1 principal S16 after the stock mux; not RCC/speakers/real-time pacing",
        "capture_frames": int(len(capture)), "expected_forwarded_frames": int(cursor),
        "exact_frames": int(len(ex)), "exact_frames_fraction_of_expected": round(len(ex) / expected_frames_ours, 4),
        "failed_seqs_not_forwarded": failed_seqs, "missing_audio_seqs_in_capture": missing_audio,
        "repeated_exact_frames": repeats, "sequence_events": jumps,
        "driver_silence_segments_inside_stream": inner_silence, "altered_segments_inside_stream": inner_altered,
        "marker_phases": phases,
        "verdict": {
            "no_replay": repeats == 0,
            "only_injected_failure_skipped": all(j["type"] == "skipped_writes" and j["all_are_injected_failures"] for j in jumps) and not [j for j in jumps if j["type"] != "skipped_writes"],
            "gap_free_inside_stream": not inner_silence and not inner_altered and not jumps,
        },
    }
    (args.run / "nv-hook-tdm-analysis.json").write_text(json.dumps(result, indent=2) + "\n")
    brief = {k: v for k, v in result.items() if k not in ("sequence_events", "driver_silence_segments_inside_stream", "altered_segments_inside_stream")}
    brief.update(sequence_event_count=len(jumps), silence_segment_count=len(inner_silence), altered_segment_count=len(inner_altered),
                 first_events=jumps[:4], first_silences=inner_silence[:4], first_altered=inner_altered[:4])
    print(json.dumps(brief, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
