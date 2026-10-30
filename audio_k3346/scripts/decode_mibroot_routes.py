#!/usr/bin/env python3
"""Read-only: decode the K3346 RCC MIBRoot connection->resource tables (143 x 20 B each, layout from
reference-audio-a63d547/docs/audio/INTERNAL_AMP_PATH.md) and list the CarPlay (DIO/GAL) records. vaddr = file offset + 0x100000."""
import struct, sys
from pathlib import Path
A = Path(__file__).resolve().parents[1]
D = (A / 'artifacts/rcc-k3346-stage2-extracted/usr/apps/MIBRoot').read_bytes()
CONN162 = bytes.fromhex('a200000001000000000000007800000001000000')
RES78 = bytes.fromhex('780000006e04000001000000' '6f04000002000000')
SINKS = {1: 'SPEAKERS', 2: 'DTCP51', 3: 'DTCP20', 4: 'SDS', 5: 'MIC', 6: 'MU_REF', 9: 'TEL', 15: 'MUHP', 31: 'SPEAKERS_MC'}
def rec(off): return struct.unpack_from('<5I', D, off)
def main():
    c = D.find(CONN162); r = D.find(RES78)
    assert c > 0 and r > 0 and D.count(CONN162) == 1 and D.count(RES78) == 1
    print('conn162 file 0x%x vaddr 0x%x ; res0x78 file 0x%x vaddr 0x%x' % (c, c + 0x100000, r, r + 0x100000))
    # table starts: scan back from conn162 while records look like {conn<0x200, term<8, ...}
    start = c
    while True:
        a, b, _, _, _ = rec(start - 20)
        if a < 0x200 and b < 8 and start - 20 > 0: start -= 20
        else: break
    n = 0; off = start
    while True:
        a, b, _, _, _ = rec(off)
        if a < 0x200 and b < 8: n += 1; off += 20
        else: break
    print('connection table: file 0x%x vaddr 0x%x records %d (conn162 is #%d)' % (start, start + 0x100000, n, (c - start) // 20))
    rstart = r
    while True:
        a = rec(rstart - 20)[0]
        if 0x50 <= a < 0xff and rstart - 20 > 0 and rec(rstart - 20)[1] in range(0x400, 0x600): rstart -= 20
        else: break
    rn = 0; off = rstart
    while rec(off)[1] in range(0x400, 0x600) or rec(off)[3] in range(0x400, 0x600): rn += 1; off += 20
    print('resource table (heuristic): file 0x%x vaddr 0x%x records >= %d' % (rstart, rstart + 0x100000, rn))
    print('resource  intSrc  intSnk       extSrc  extSnk   (res 0x78 = DIO media)')
    for i in range(rn):
        a, s1, k1, s2, k2 = rec(rstart + 20 * i)
        if 0x78 <= a <= 0x88 or a in (0x7f, 0x88):
            print('  0x%02x   0x%03x   %-10s   0x%03x   %-10s' % (a, s1, SINKS.get(k1, k1), s2, SINKS.get(k2, k2)))
if __name__ == '__main__': main()
