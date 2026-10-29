#!/usr/bin/env python3
"""Rewrite the class-file major version of every class in a jar to 46 (Java 1.2 format), like all stock/known-working
patch jars of this platform (chefranov dpad/coverart/rgd jars are major 46; our UI jar was major 48).
Refuses a class that needs a >46 feature: ldc of a Class constant (CONSTANT_Class via ldc/ldc_w), which javac targets >=1.5
only; javac -target 1.4 emits class$ helpers instead.  usage: downgrade_jar_major.py in.jar out.jar"""
import struct, sys, zipfile

JAVAP = '/opt/homebrew/opt/openjdk/bin/javap'

def uses_ldc_class(data):
    """Exact check with javap -c: an ldc/ldc_w whose operand is a Class constant needs class-file version >= 49."""
    import re, subprocess, tempfile, os
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, 'X.class'); open(path, 'wb').write(data)
        out = subprocess.run([JAVAP, '-p', '-c', path], capture_output=True, text=True).stdout
    if not out: raise SystemExit('javap could not read a class: refusing to guess')
    return re.search(r'ldc(_w)?\s+#\d+\s+// class', out) is not None

def main():
    src, dst = sys.argv[1:3]
    with zipfile.ZipFile(src) as zi, zipfile.ZipFile(dst, 'w', zipfile.ZIP_DEFLATED) as zo:
        for e in zi.infolist():
            data = zi.read(e.filename)
            if e.filename.endswith('.class'):
                major = struct.unpack('>H', data[6:8])[0]
                if major > 46:
                    if uses_ldc_class(data): print('REFUSED (ldc of a Class constant):', e.filename); sys.exit(1)
                    data = data[:6] + struct.pack('>H', 46) + data[8:]
            zo.writestr(e, data)
    print('downgraded ->', dst)
main()
