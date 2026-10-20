#!/usr/bin/env python3
"""Verify Java 1.4 classes and only expected v8l diagnostic class changes."""
import pathlib,sys,zipfile,struct,hashlib
root=pathlib.Path(sys.argv[1]);old_zip=pathlib.Path(sys.argv[2])
new={str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*.class')}
with zipfile.ZipFile(old_zip) as z:orig={name:z.read(name) for name in z.namelist() if name.endswith('.class')}
added=set(new)-set(orig);removed=set(orig)-set(new)
assert added=={'com/luka/carplay/routeguidance/V8LFactoryTrace.class'},('unexpected added',added)
assert not removed,('removed classes',removed)
changed={n for n in set(new)&set(orig) if new[n]!=orig[n]}
assert changed=={'com/luka/carplay/routeguidance/BAPBridge.class'},('unexpected changed classes',changed)
for name,data in new.items():
 assert data[:4]==b'\xca\xfe\xba\xbe' and struct.unpack('>H',data[6:8])[0]==48,name
print('V8L_SOURCE_CLASS_PARITY=PASS total=%d changed=%s added=%s'%(len(new),sorted(changed),sorted(added)))
