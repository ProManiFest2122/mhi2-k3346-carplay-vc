#!/usr/bin/env python3
"""Host measurement: does the reference SCENE upmixer keep spoken voice in the front/centre?
Inputs (48 kHz stereo): voice_centre (macOS `say` speech, L=R), voice_left (speech panned hard left), bed (synthetic wide
'music': panned plucks + decorrelated ambience, no centre), mix (voice at -6 dB over bed). Each is rendered with
artifacts/full-audio/scene-render-wav (levels 1-4, mask net on) and the rear/front energy ratio is measured over active frames.
Host numerical evidence only (synthetic bed, one TTS voice); not an amplifier or listening result.
usage: voice_leak_test.py [outdir]"""
import json, subprocess, sys, wave
from pathlib import Path
import numpy as np
A = Path(__file__).resolve().parents[1]
TOOL = A / 'artifacts/full-audio/scene-render-wav'
SR = 48000
out = Path(sys.argv[1]) if len(sys.argv) > 1 else A / 'artifacts/voice-leak'
out.mkdir(parents=True, exist_ok=True)

def say(text, name):
    aiff = out / (name + '.aiff'); wav = out / (name + '-mono.wav')
    subprocess.check_call(['say', '-o', str(aiff), text])
    subprocess.check_call(['afconvert', '-f', 'WAVE', '-d', 'LEI16@48000', '-c', '1', str(aiff), str(wav)])
    with wave.open(str(wav)) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(np.float64) / 32768
    return x / max(1e-9, np.abs(x).max())

def write(name, x):
    p = out / (name + '.wav')
    with wave.open(str(p), 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(np.clip(np.rint(x * 32767), -32768, 32767).astype('<i2').tobytes())
    return p

voice = say('In four hundred metres turn right onto the main road, then continue straight for two kilometres. '
            'Your destination will be on the left.', 'speech_en')
n = len(voice) + SR // 2
voice = np.pad(voice, (SR // 4, n - len(voice) - SR // 4)) * 0.5
t = np.arange(n) / SR
rng = np.random.default_rng(1)
bed = np.zeros((n, 2))
for onset in np.arange(0.1, n / SR - 0.5, 0.3):
    f = rng.choice([220, 262, 330, 392, 440, 523, 659]); pan = rng.uniform(0.1, 0.9)
    u = np.maximum(0, t - onset); s = (t >= onset) * np.exp(-u * 6) * (0.4 * np.sin(2 * np.pi * f * u) + 0.15 * np.sin(2 * np.pi * 2 * f * u))
    bed[:, 0] += s * (1 - pan); bed[:, 1] += s * pan
bed += rng.standard_normal((n, 2)) * 0.01
bed *= 0.5 / max(1e-9, np.abs(bed).max())
sig = {
    'voice_centre': np.column_stack((voice, voice)),
    'voice_left': np.column_stack((voice, np.zeros(n))),
    'bed': bed,
    'mix': bed + np.column_stack((voice, voice)) * 0.5,
}
res = {}
for name, x in sig.items():
    src = write('in_' + name, x)
    res[name] = {}
    for level in (1, 2, 3, 4):
        dst = out / f'out_{name}_L{level}.wav'
        subprocess.check_call([str(TOOL), str(level), '1', str(src), str(dst)])
        with wave.open(str(dst)) as w:
            y = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(np.float64).reshape(-1, 6) / 32768
        active = np.abs(y).max(1) > 0.003; y = y[active]
        front = np.sqrt((y[:, :2] ** 2).sum(1).mean()); rear = np.sqrt((y[:, 4:] ** 2).sum(1).mean()); cl = np.sqrt((y[:, 2:4] ** 2).sum(1).mean())
        res[name][level] = {'rear_to_front_db': round(20 * np.log10(max(rear, 1e-12) / front), 1), 'frames': int(active.sum()), 'centre_lfe_rms': round(float(cl), 6)}
for name in res:
    print(name.ljust(13), ' '.join(f'L{l}:{res[name][l]["rear_to_front_db"]:6.1f}dB' for l in (1, 2, 3, 4)))
(out / 'result.json').write_text(json.dumps(res, indent=1))
