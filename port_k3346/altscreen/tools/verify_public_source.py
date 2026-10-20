#!/usr/bin/env python3
"""Offline public source/integrity checks (no proprietary SDK or live car)."""
from pathlib import Path
import hashlib
import json
import sys
import zipfile
ROOT=Path(__file__).resolve().parents[1]
EXPECTED={
    "deployment/bundles.awk":"07360523e8287d8fe2d2b96917a1ba16eb885652a294d4d746437d9457053b66",
    "deployment/smartphone.awk":"d201b18245603de369537786c2adb8a2bda7d369c5af799ca051eb497888a935",
    "deployment/trace.awk":"f45236e20854779956df06c4fc02730f54c50834f23950a9701909176e8e9627",
}
def digest(data): return hashlib.sha256(data).hexdigest()
for name, expected in EXPECTED.items():
    data=(ROOT/name).read_bytes()
    assert digest(data)==expected,(name,digest(data),expected)
for name in [
    "renderer/upstream/vc_render.c",
    "renderer/k2161/vc_render.c",
    "renderer/PROVENANCE.md",
    "java/build_java_release.sh",
    "java/verify_release_classes.py",
    "native/altscreen_session.c",
    "deployment/custom.sh",
]:
    assert (ROOT/name).is_file(),name
# Guard recovered renderer source against accidental rewriting.
assert __import__("hashlib").sha1(b"blob 10999" + bytes([0]) + (ROOT/"renderer/k2161/vc_render.c").read_bytes()).hexdigest() == "9c2185f6a3ea099177f6c4fb9f88d45dc7da595e"
upstream=(ROOT/"renderer/upstream/vc_render.c").read_text()
assert "NvSSVideoDecode" in upstream and "AV11" in upstream
assert "VC" in (ROOT/"README.md").read_text() or "CarPlay" in (ROOT/"README.md").read_text()
if len(sys.argv)>1:
    archive=Path(sys.argv[1])
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        expected=json.loads((ROOT/"release_evidence/V8S_PACKAGE_MANIFEST.json").read_text())["package_files_sha256"]
        assert len(z.namelist())==len(expected)==12
        for path, sha in expected.items():
            assert digest(z.read(path))==sha,path
        for script, legacy in [
            ("deployment/bundles.awk","mod/mib2high-carplay-rgi-v8s-simple/source/bundles.awk"),
            ("deployment/smartphone.awk","mod/mib2high-carplay-rgi-v8s-simple/source/smartphone.awk"),
            ("deployment/trace.awk","mod/mib2high-carplay-rgi-v8s-simple/source/trace.awk"),
        ]:
            assert (ROOT/script).read_bytes()==z.read(legacy),script
    print("V8S_IMMUTABLE_INSTALLER=PASS")
print("PUBLIC_SOURCE_AND_INSTALLER_SCRIPT_INTEGRITY=PASS")
