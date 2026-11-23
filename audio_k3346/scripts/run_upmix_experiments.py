#!/usr/bin/env python3
"""Independent synthetic PCM checks of the pinned upstream scene renderer.

These are host DSP checks, not amplifier, CarPlay negotiation or listening proof.
"""
import argparse
import hashlib
import json
import subprocess
import time
import wave
from pathlib import Path

import numpy as np

SR = 48000
CHANNELS = ["L", "R", "C", "LFE", "Ls", "Rs"]


def write_wav(path, pcm):
    with wave.open(str(path), "wb") as f:
        f.setnchannels(pcm.shape[1])
        f.setsampwidth(2)
        f.setframerate(SR)
        f.writeframes(np.asarray(pcm, dtype="<i2").tobytes())


def read_wav(path):
    with wave.open(str(path), "rb") as f:
        assert f.getframerate() == SR and f.getsampwidth() == 2
        return np.frombuffer(f.readframes(f.getnframes()), dtype="<i2").reshape(-1, f.getnchannels())


def db(value):
    return float(20 * np.log10(max(float(value), 1e-15)))


def signals(seconds=8):
    t = np.arange(int(seconds * SR)) / SR
    rng = np.random.default_rng(20261014)
    envelope = np.minimum(1, t / .05) * np.minimum(1, (seconds - t) / .05)
    voice = sum(np.sin(2 * np.pi * (180 * k) * t + .03 * k * np.sin(2 * np.pi * 4 * t)) / k for k in range(1, 8))
    voice *= .18 * (.6 + .4 * np.sin(2 * np.pi * 2.2 * t) ** 2) * envelope
    pluck = np.zeros_like(t)
    for onset in np.arange(.1, seconds - .2, .4):
        u = np.maximum(0, t - onset)
        pluck += (t >= onset) * np.exp(-u * 9) * (.28 * np.sin(2 * np.pi * 440 * u) + .1 * np.sin(2 * np.pi * 880 * u))
    bass = .3 * np.sin(2 * np.pi * 50 * t) * envelope
    tone = .4 * np.sin(2 * np.pi * 997 * t) * envelope
    room = rng.standard_normal((len(t), 2)) * .025
    mixed = np.column_stack((voice + bass * .4 + pluck, voice + bass * .4 + .2 * pluck)) + room
    transitions = mixed.copy()
    transitions[(t > 2) & (t < 3)] = 0
    transitions[t > 6] = 0
    stress = np.column_stack((.98 * np.sign(np.sin(2 * np.pi * 1000 * t)), .98 * np.sign(np.sin(2 * np.pi * 1100 * t))))
    values = {
        "silence": np.zeros_like(mixed),
        "mono_voice": np.column_stack((voice, voice)),
        "hard_left_pluck": np.column_stack((pluck, np.zeros_like(pluck))),
        "anti_phase": np.column_stack((tone, -tone)),
        "bass_50hz": np.column_stack((bass, bass)),
        "stereo_scene": mixed,
        "gaps_and_stop": transitions,
        "near_full_scale": stress,
    }
    return {key: np.clip(np.rint(value * 32767), -32768, 32767).astype("<i2") for key, value in values.items()}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--upstream", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source = args.output / "sources"
    renders = args.output / "renders"
    source.mkdir(exist_ok=True)
    renders.mkdir(exist_ok=True)
    binary = args.upstream / "build/scene_port/scene_render_hook"
    blob = args.upstream / "models/scene-masknet-c/scene_masknet_c.bin"
    modes = {
        "off": ["--mode", "off"],
        "scene_spatial": ["--mode", "scene", "--lfe", "off"],
        "scene_mask": ["--mode", "scene", "--lfe", "off", "--mask-blob", str(blob)],
        "scene_mask_lfe": ["--mode", "scene", "--lfe", "assist", "--mask-blob", str(blob)],
    }
    rows, failures = [], []
    with (args.output / "render.log").open("w") as log:
        for name, pcm in signals().items():
            src = source / (name + ".wav")
            write_wav(src, pcm)
            for mode, flags in modes.items():
                baseline = None
                for block in (768, 100, 1024):
                    out = renders / f"{name}-{mode}-b{block}.wav"
                    cmd = [str(binary), *flags, "--block", str(block), str(src), str(out)]
                    start = time.perf_counter()
                    subprocess.run(cmd, check=True, stdout=log, stderr=log)
                    duration = time.perf_counter() - start
                    y = read_wav(out)
                    checks = {"exact_frame_count": len(y) == len(pcm), "six_channels": y.shape[1] == 6}
                    checks["block_invariant"] = baseline is None or np.array_equal(y, baseline)
                    if baseline is None:
                        baseline = y.copy()
                    if name == "silence":
                        checks["silence_exact_zero"] = not np.any(y)
                    if mode == "off":
                        checks["off_front_bit_exact"] = np.array_equal(y[:, :2], pcm)
                        checks["off_extra_channels_zero"] = not np.any(y[:, 2:])
                    else:
                        checks["initial_1024_frames_zero"] = not np.any(y[:1024])
                        checks["upmix_center_zero"] = not np.any(y[:, 2])
                        if mode != "scene_mask_lfe":
                            checks["lfe_disabled_zero"] = not np.any(y[:, 3])
                        if name == "gaps_and_stop":
                            checks["long_stop_reaches_silence"] = not np.any(y[-SR:])
                    for check, ok in checks.items():
                        if not ok:
                            failures.append(f"{name}/{mode}/{block}: {check}")
                    f = y.astype(np.float64) / 32768
                    rms = np.sqrt(np.mean(f * f, axis=0))
                    row = {
                        "signal": name, "mode": mode, "block": block,
                        "seconds": len(y) / SR, "render_host_seconds": duration,
                        "checks": checks, "rms_dbfs": dict(zip(CHANNELS, map(db, rms))),
                        "peak": dict(zip(CHANNELS, np.max(np.abs(f), axis=0).tolist())),
                        "saturated_samples": int(np.sum((y == 32767) | (y == -32768))),
                        "rear_to_front_db": db(np.linalg.norm(rms[4:]) / max(np.linalg.norm(rms[:2]), 1e-15)),
                        "sha256_pcm": hashlib.sha256(y.tobytes()).hexdigest(),
                    }
                    rows.append(row)
                print(name, mode, "OK" if all(rows[-j]["checks"]["block_invariant"] for j in (1, 2, 3)) else "FAIL", flush=True)
    report = {
        "scope": "host synthetic PCM, no CarPlay negotiation or real amplifier",
        "sample_rate": SR, "channel_order": CHANNELS, "seed": 20261014,
        "upstream_commit": subprocess.check_output(["git", "-C", str(args.upstream), "rev-parse", "HEAD"], text=True).strip(),
        "mask_sha256": hashlib.sha256(blob.read_bytes()).hexdigest(),
        "runs": rows, "failures": failures,
        "checks": sum(len(row["checks"]) for row in rows),
    }
    (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{len(rows)} renders, {report['checks']} checks, {len(failures)} failures")
    for failure in failures:
        print("FAIL", failure)
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
