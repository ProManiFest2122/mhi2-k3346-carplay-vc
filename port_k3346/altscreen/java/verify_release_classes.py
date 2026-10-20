#!/usr/bin/env python3
"""Independent Java class-format and inventory gate; no private reference JAR required."""
from pathlib import Path
import struct
import sys
root = Path(sys.argv[1])
classes = sorted(root.rglob("*.class"))
assert len(classes) >= 10, "too few compiled classes"
names = {p.relative_to(root).as_posix() for p in classes}
for required in (
    "com/luka/carplay/routeguidance/BAPBridge.class",
    "com/luka/carplay/routeguidance/RouteGuidance.class",
    "org/mib2high/carplay/rgi/Activator.class",
):
    assert required in names, required
for path in classes:
    data = path.read_bytes()
    assert data[:4] == bytes([0xCA,0xFE,0xBA,0xBE]) and struct.unpack(">H", data[6:8])[0] == 48, path
print("JAVA_CLASS_FORMAT=PASS", len(classes), "classes (major 48)")
