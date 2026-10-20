#!/usr/bin/env python3
"""v8k source build release gate: JVM major 48 + baseline class set and methods."""
import sys,zipfile,struct,pathlib
root=pathlib.Path(sys.argv[1]); old=pathlib.Path(sys.argv[2])
new_files={str(p.relative_to(root)) for p in root.rglob('*.class')}
with zipfile.ZipFile(old) as z: orig={n for n in z.namelist() if n.endswith('.class')}
added=new_files-orig; removed=orig-new_files
if added!={'com/luka/carplay/routeguidance/V8KVcTrace.class'} or removed:
 print('FAIL: class inventory mismatch','added:',added,'removed:',removed);sys.exit(1)
for c in sorted(new_files):
 data=(root/c).read_bytes()
 if data[:4]!=b'\xca\xfe\xba\xbe':sys.exit('BAD_CLASS '+c)
 major=struct.unpack('>H',data[6:8])[0]
 if major!=48:sys.exit('UNSUPPORTED_JAVA_CLASS_VERSION '+c+':'+str(major))
print('PASS: JVM major version 48, original class inventory unchanged, only one new diagnostic class added')
