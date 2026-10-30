#!/usr/bin/env python3
"""Read-only helper: K3346 RCC C674x flat image (memory.bin) overlaid with the TI cinit records (initialized data).
Usage: rcc_dsp_image.py words ADDR [COUNT] | str ADDR | hex ADDR [COUNT]. Static evidence only."""
import json, struct, sys
from pathlib import Path
S = Path(__file__).resolve().parents[1] / 'artifacts/rcc-dsp-sections'
BASE = 0x87500180
class Image:
    def __init__(self, overlay=True):
        self.mem = bytearray((S / 'memory.bin').read_bytes())
        self.overlaid = []
        if overlay:
            for r in json.loads((S / 'initialized/records.json').read_text()):
                dst = int(r['destination'], 16); data = (S / 'initialized' / (r['destination'] + '.bin')).read_bytes()
                if BASE <= dst and dst + len(data) <= BASE + len(self.mem):
                    self.mem[dst - BASE:dst - BASE + len(data)] = data; self.overlaid.append((dst, len(data)))
    def u32(self, a): return struct.unpack_from('<I', self.mem, a - BASE)[0]
    def s32(self, a): return struct.unpack_from('<i', self.mem, a - BASE)[0]
    def u16(self, a): return struct.unpack_from('<H', self.mem, a - BASE)[0]
    def cstr(self, a, n=96):
        b = bytes(self.mem[a - BASE:a - BASE + n]); b = b.split(b'\0')[0]
        return b.decode('ascii', 'replace')
    def words(self, a, n): return [self.u32(a + 4 * i) for i in range(n)]
if __name__ == '__main__':
    im = Image(); cmd, addr = sys.argv[1], int(sys.argv[2], 0); n = int(sys.argv[3]) if len(sys.argv) > 3 else 8
    if cmd == 'str': print(im.cstr(addr))
    elif cmd == 'words':
        for i in range(n): print(hex(addr + 4 * i), hex(im.u32(addr + 4 * i)), im.s32(addr + 4 * i))
    elif cmd == 'hex': print(bytes(im.mem[addr - BASE:addr - BASE + n]).hex())
