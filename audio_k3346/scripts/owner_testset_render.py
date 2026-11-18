#!/usr/bin/env python3
"""Host characterisation of the reference SCENE upmixer with the OWNER's own tuning test tones (A4B9_test_tones_FLAC:
01 sweep 20-200 Hz, 02 sweep 200-8000, 03/09 sub steps, 04 centre steps, 05 channel ID L/pause/R/both, 06 polarity,
07 pink mono, 08 kick). Converts each FLAC to 48 kHz stereo S16 WAV (ffmpeg), renders it at levels 1..4 with the
mask net on (artifacts/full-audio/scene-render-wav) and prints per-file numbers: rear/front energy ratio, centre/LFE
energy, and low-band (<130 Hz) retention in the front pair vs the input (does the sub get its bass from the fronts?).
Host numerical evidence only.  usage: owner_testset_render.py <dir with the FLACs> [outdir]"""
import json, subprocess, sys, wave
from pathlib import Path
import numpy as np
A = Path(__file__).resolve().parents[1]; TOOL = A / 'artifacts/full-audio/scene-render-wav'
src = Path(sys.argv[1]); out = Path(sys.argv[2]) if len(sys.argv) > 2 else A / 'artifacts/owner-testset'
out.mkdir(parents=True, exist_ok=True); SR = 48000

def lowband_energy(x):  # energy below 130 Hz via FFT
    X = np.fft.rfft(x * np.hanning(len(x))); f = np.fft.rfftfreq(len(x), 1 / SR)
    return float((np.abs(X[f < 130]) ** 2).sum())

def readwav(p):
    with wave.open(str(p)) as w:
        return np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').reshape(-1, w.getnchannels()).astype(np.float64) / 32768

res = {}
for flac in sorted(src.glob('0[1-9]_*.flac')):
    wav = out / (flac.stem + '.wav')
    subprocess.check_call(['ffmpeg', '-v', 'error', '-y', '-i', str(flac), '-ar', str(SR), '-ac', '2', '-sample_fmt', 's16', str(wav)])
    x = readwav(wav); res[flac.stem] = {}
    with wave.open(str(wav), 'wb') as w:  # canonical 44-byte header for the C tool (ffmpeg adds a LIST chunk)
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(np.clip(np.rint(x * 32768), -32768, 32767).astype('<i2').tobytes())
    lb_in = lowband_energy(x[:, 0]) + lowband_energy(x[:, 1])
    for level in (1, 2, 3, 4):
        dst = out / f'{flac.stem}_L{level}.wav'
        subprocess.check_call([str(TOOL), str(level), '1', str(wav), str(dst)])
        y = readwav(dst)
        n = min(len(x), len(y)); act = np.abs(y[:n]).max(1) > 0.003
        def rms(c): return float(np.sqrt((y[:n][act][:, c] ** 2).mean())) if act.any() else 0.0
        front = np.sqrt(rms(0) ** 2 + rms(1) ** 2); rear = np.sqrt(rms(4) ** 2 + rms(5) ** 2)
        lb_out = lowband_energy(y[:n, 0]) + lowband_energy(y[:n, 1])
        res[flac.stem][level] = {
            'rear_front_db': round(20 * np.log10(max(rear, 1e-12) / max(front, 1e-12)), 1),
            'centre_lfe_rms': round(float(np.sqrt(rms(2) ** 2 + rms(3) ** 2)), 5),
            'front_lowband_retention_db': round(10 * np.log10(max(lb_out, 1e-12) / max(lb_in, 1e-12)), 1) if lb_in > 1e-9 else None}
print('file'.ljust(36), 'rear/front dB L1..L4'.ljust(34), 'front <130Hz vs input dB L1..L4')
for k, v in res.items():
    print(k.ljust(36), ' '.join(f'{v[l]["rear_front_db"]:6.1f}' for l in (1, 2, 3, 4)).ljust(34),
          ' '.join('   n/a' if v[l]['front_lowband_retention_db'] is None else f'{v[l]["front_lowband_retention_db"]:6.1f}' for l in (1, 2, 3, 4)))
(out / 'result.json').write_text(json.dumps(res, indent=1))
