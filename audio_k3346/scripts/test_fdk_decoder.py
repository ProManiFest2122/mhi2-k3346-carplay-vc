#!/usr/bin/env python3
"""Local AAC-LC fixture and host decoder test; no phone capture or vehicle data."""
import ctypes as C
import json
import struct
import subprocess
import wave
from pathlib import Path
import numpy as np

A = Path(__file__).resolve().parents[1]


def main():
    out = A / 'artifacts/fdk-decoder-host'
    t = np.arange(24000) / 48000
    pcm = np.column_stack([6000 * np.sin(2 * np.pi * f * t) for f in (697, 1209)]).astype('<i2')
    with wave.open(str(out / 'tones.wav'), 'wb') as f:
        f.setnchannels(2); f.setsampwidth(2); f.setframerate(48000); f.writeframes(pcm.tobytes())
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(out / 'tones.wav'), '-c:a', 'aac', '-b:a', '192k', '-f', 'adts', str(out / 'tones.aac')], check=True)
    data = (out / 'tones.aac').read_bytes(); packets = []; at = 0
    while at < len(data):
        h = data[at:at + 7]
        assert len(h) == 7 and h[0] == 255 and h[1] & 0xf6 == 0xf0
        assert h[2] >> 6 == 1 and (h[2] >> 2) & 15 == 3
        assert ((h[2] & 1) << 2) | (h[3] >> 6) == 2
        n = ((h[3] & 3) << 11) | (h[4] << 3) | (h[5] >> 5)
        assert n >= 7 and at + n <= len(data) and h[6] & 3 == 0
        packets.append(data[at + 7:at + n]); at += n
    fixture = b'MHI2AAC1' + struct.pack('<I', len(packets))
    fixture += b''.join(struct.pack('<I', len(p)) + p for p in packets)
    (out / 'raw-aac-fixture.bin').write_bytes(fixture)
    lib = C.CDLL(str(out / 'libmhi2_fdk.dylib'))
    ptr = C.POINTER(C.c_ubyte)
    lib.aacDecoder_Open.argtypes = [C.c_int, C.c_uint]; lib.aacDecoder_Open.restype = C.c_void_p
    lib.aacDecoder_ConfigRaw.argtypes = [C.c_void_p, C.POINTER(ptr), C.POINTER(C.c_uint)]
    lib.aacDecoder_Fill.argtypes = [C.c_void_p, C.POINTER(ptr), C.POINTER(C.c_uint), C.POINTER(C.c_uint)]
    lib.aacDecoder_DecodeFrame.argtypes = [C.c_void_p, C.POINTER(C.c_short), C.c_int, C.c_uint]
    lib.aacDecoder_GetStreamInfo.argtypes = [C.c_void_p]; lib.aacDecoder_GetStreamInfo.restype = C.POINTER(C.c_int)
    lib.aacDecoder_Close.argtypes = [C.c_void_p]
    lib.aacDecoder_SetParam.argtypes = [C.c_void_p, C.c_int, C.c_int]

    def decode():
        h = lib.aacDecoder_Open(0, 1); assert h
        try:
            asc = (C.c_ubyte * 2)(0x11, 0x90); p = C.cast(asc, ptr); n = C.c_uint(2)
            assert lib.aacDecoder_ConfigRaw(h, C.byref(p), C.byref(n)) == 0
            assert lib.aacDecoder_SetParam(h, 0x0004, 0) == 0
            rows = []
            for packet in packets:
                b = (C.c_ubyte * len(packet)).from_buffer_copy(packet); p = C.cast(b, ptr)
                n = C.c_uint(len(packet)); valid = C.c_uint(len(packet))
                assert lib.aacDecoder_Fill(h, C.byref(p), C.byref(n), C.byref(valid)) == 0 and valid.value == 0
                samples = (C.c_short * 2048)()
                assert lib.aacDecoder_DecodeFrame(h, samples, 2048, 0) == 0
                info = lib.aacDecoder_GetStreamInfo(h)
                assert list(info[:3]) == [48000, 1024, 2]
                rows.append(np.ctypeslib.as_array(samples).copy().reshape(1024, 2))
            return np.concatenate(rows)
        finally:
            lib.aacDecoder_Close(h)

    decoded = decode(); assert np.array_equal(decoded, decode())
    fnv = 2166136261
    for byte in decoded.astype('<i2').tobytes():
        fnv = ((fnv ^ byte) * 16777619) & 0xffffffff
    # Check distinct channel content after codec priming; no reference-decoder
    # bit-exactness claim (independent AAC implementations can round differently).
    signal = decoded[4096:20480].astype(float)
    spectrum = np.abs(np.fft.rfft(signal, axis=0))
    frequency = np.argmax(spectrum, axis=0) * 48000 / len(signal)
    assert np.all(np.abs(frequency - [697, 1209]) < 4)
    result = dict(packets=len(packets), frames=len(decoded), sample_rate=48000, channels=2,
                  channel_peak=np.abs(decoded.astype(np.int32)).max(0).tolist(),
                  dominant_hz=frequency.tolist(), independent_recreate_exact=True,
                  pcm_fnv1a32=fnv,
                  implicit_peak_limiter=False,
                  scope='Host raw AAC-LC decoder only; no CarPlay receiver, codec switching or native QNX proof', passed=True)
    (out / 'analysis.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
