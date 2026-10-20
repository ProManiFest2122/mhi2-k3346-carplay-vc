#!/usr/bin/env python3
"""Reconstruct the CAR-TESTED v8j Java semantics in genuine editable source.
The archived v8j .class is ONLY read to establish semantics; NEVER patched.
Exact old-vs-new javap review found two no-op VC map suppression sites and
setMapPresentationBlocked forced to false in v8j.
"""
from pathlib import Path
import sys
src=Path(sys.argv[1]); bap=src/'routeguidance/BAPBridge.java'; gate=src/'routeguidance/K2161GatedCombiService.java'
s=bap.read_text()
needle='        suppressVcMapPresentation();'
assert s.count(needle)==2, 'unexpected BAPBridge source; fail closed'
s=s.replace(needle, '        /* v8j verified behavior: no VC map suppression here. */')
bap.write_text(s)
s=gate.read_text()
assert s.count('        blockMapPresentation = blocked;')==1
assert s.count('            "MAP_BLOCK=" +\n            (blocked ? "1" : "0")')==1
s=s.replace('        blockMapPresentation = blocked;', '        blockMapPresentation = false; /* v8j behavior */',1)
s=s.replace('            "MAP_BLOCK=" +\n            (blocked ? "1" : "0")', '            "MAP_BLOCK=" +\n            (false ? "1" : "0") /* v8j MAP_BLOCK=0 */',1)
gate.write_text(s)
print('V8J_CAR_TESTED_JAVA_BEHAVIOR_RECREATED_IN_SOURCE=3')
