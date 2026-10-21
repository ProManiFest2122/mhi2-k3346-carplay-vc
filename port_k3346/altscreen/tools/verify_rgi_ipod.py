#!/usr/bin/env python3
"""K3346 Etap 4: sanity check of the mm-ipod-only RGI library.

Must be a QNX ARM ELF, must carry the RGI hooks and its own log paths, and must
NOT carry any AltScreen code (whose constructor deletes the VC session marker).
"""
import struct
import sys
from pathlib import Path

p = Path(sys.argv[1])
data = p.read_bytes()
assert len(data) > 4096 and data[:4] == b"\x7fELF", "Not an ELF"
assert data[4] == 1 and data[5] == 1, "Not 32-bit little-endian ELF"
assert struct.unpack_from("<H", data, 18)[0] == 40, "Not ARM machine"
for needed in (b"ipod_module", b"packet_addparam_blob", b"RouteGuidanceDisplayComponent",
               b"/dev/shmem/k3346-rgi-ipod-rx.log", b"/dev/shmem/k3346-rgi-ipod-rx.bin",
               b"/dev/shmem/k3346-rgi-ipod-native.log", b"/tmp/k3346-rgi-ipod-hook.log",
               b"StartListUpdates", b"InitiateCall", b"/tmp/k3346-tel-recents.txt", b"/tmp/k3346-tel-cmd"):
    assert needed in data, "missing %r" % needed
for forbidden in (b"AirPlayReceiverSession", b"mib2high-lvds-active", b"mib2high-lvds-control",
                  b"/dev/shmem/k2161-rgi-rx", b"mib2high-carplay-rgi-native.log",
                  b"/tmp/carplay_hook.log"):
    assert forbidden not in data, "forbidden %r present" % forbidden
print("RGI_IPOD_ELF=PASS", p, len(data))
