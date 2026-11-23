#!/usr/bin/env python3
"""Synthetic 44.1 kHz AAC-LC fixture (1000 Hz left, 3000 Hz right) for the rate44 host scenario; no phone or car data.
Same container as raw-aac-fixture.bin: 8-byte header, u32 packet count, then (u32 length, raw AAC access unit)."""
import struct
import subprocess
import tempfile
from pathlib import Path

A = Path(__file__).resolve().parents[1]


def main():
    out = A / 'artifacts/fdk-decoder-host'
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as d:
        adts = Path(d) / 'tones44.aac'
        subprocess.run(['ffmpeg', '-v', 'error', '-y',
                        '-f', 'lavfi', '-i', 'sine=frequency=1000:sample_rate=44100:duration=1.25',
                        '-f', 'lavfi', '-i', 'sine=frequency=3000:sample_rate=44100:duration=1.25',
                        '-filter_complex', '[0:a][1:a]amerge=inputs=2,volume=0.5[a]', '-map', '[a]',
                        '-c:a', 'aac', '-b:a', '128k', '-f', 'adts', str(adts)], check=True)
        data = adts.read_bytes()
    packets = []; at = 0
    while at < len(data):
        h = data[at:at + 7]
        assert len(h) == 7 and h[0] == 255 and h[1] & 0xf6 == 0xf0
        assert (h[2] >> 2) & 15 == 4                      # 44.1 kHz
        assert ((h[2] & 1) << 2) | (h[3] >> 6) == 2       # stereo
        n = ((h[3] & 3) << 11) | (h[4] << 3) | (h[5] >> 5)
        assert n >= 7 and at + n <= len(data) and h[6] & 3 == 0
        packets.append(data[at + 7:at + n]); at += n
    blob = b'MHI2AAC1' + struct.pack('<I', len(packets)) + b''.join(struct.pack('<I', len(p)) + p for p in packets)
    (out / 'raw-aac-44k-fixture.bin').write_bytes(blob)
    print('raw-aac-44k-fixture.bin', len(packets), 'packets')


if __name__ == '__main__':
    main()
