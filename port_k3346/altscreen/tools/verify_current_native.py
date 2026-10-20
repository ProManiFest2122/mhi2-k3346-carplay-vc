#!/usr/bin/env python3
"""Source-built ARM ELF sanity check without hardcoded private reference paths."""
import struct
import sys
from pathlib import Path
p=Path(sys.argv[1])
data=p.read_bytes()
assert len(data)>4096 and data[:4]==bytes([0x7f,0x45,0x4c,0x46]),"Not an ELF"
assert data[4]==1 and data[5]==1,"Not 32-bit little-endian ELF"
assert struct.unpack_from("<H",data,18)[0]==40,"Not ARM machine"
assert b"AirPlayReceiverSession" in data,"Missing expected AirPlay hook symbols"
print("QNX_ARM_SOURCE_BUILD_ELF=PASS",p,len(data))
