#!/usr/bin/env python3
"""Class-level rebuild parity, never patching bytecode. V8L source-built JAR is read-only."""
import pathlib,sys,zipfile,struct
root=pathlib.Path(sys.argv[1]);oldjar=pathlib.Path(sys.argv[2])
new={str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*.class')}
with zipfile.ZipFile(oldjar) as z: original={n:z.read(n) for n in z.namelist() if n.endswith('.class')}
added=set(new)-set(original); removed=set(original)-set(new)
assert added=={'com/luka/carplay/routeguidance/V8MFactoryTrace.class'},('unexpected added',added)
assert removed=={'com/luka/carplay/routeguidance/V8LFactoryTrace.class'},('unexpected removed',removed)
changed={n for n in set(new)&set(original) if new[n]!=original[n]}
assert changed=={'com/luka/carplay/routeguidance/BAPBridge.class'},('unexpected changed',changed)
for name,data in new.items():
 assert data[:4]==b'\xca\xfe\xba\xbe' and struct.unpack('>H',data[6:8])[0]==48,name
print('V8M_SOURCE_CLASS_PARITY=PASS total=%s changed=%s added=%s removed=%s'%(len(new),sorted(changed),sorted(added),sorted(removed)))
