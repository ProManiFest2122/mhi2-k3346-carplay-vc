#!/usr/bin/env python3
"""K3346 Etap 5: weryfikacja klas podmieniajacych fabryczne.

Dla kazdej klasy w katalogu klas:
  - format pliku .class = Java 1.4 (major 48), bo HMI to J9 z JXE;
  - jesli klasa istnieje w fabrycznym lsd.jar: API publiczne i chronione
    (sygnatury pol i metod, nadklasa, interfejsy) MUSI byc identyczne;
    dopuszczalne sa wylacznie dodatki prywatne/pakietowe (np. pomocnicze
    metody sondy). Klasy nieobecne w lsd.jar sa nowe (dozwolone).
"""
import struct
import subprocess
import sys
from pathlib import Path

classes_dir = Path(sys.argv[1])
base_jar = sys.argv[2]
javap = sys.argv[3]


def api(cp, name):
    r = subprocess.run([javap, "-classpath", cp, "-protected", name],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return None
    lines = [l.strip() for l in r.stdout.splitlines()
             if l.strip() and not l.startswith("Compiled from")]
    return sorted(lines)


ok = True
for cf in sorted(classes_dir.rglob("*.class")):
    data = cf.read_bytes()
    major = struct.unpack(">H", data[6:8])[0]
    name = str(cf.relative_to(classes_dir))[:-6].replace("/", ".").replace("\\", ".")
    if major != 48:
        print("FAIL major=%d %s" % (major, name)); ok = False; continue
    stock = api(base_jar, name)
    if stock is None:
        print("NEW  %s (brak w lsd.jar - klasa nowa)" % name)
        continue
    ours = api(str(classes_dir), name)
    if ours != stock:
        ok = False
        print("FAIL API %s" % name)
        for l in sorted(set(stock) - set(ours)):
            print("   - stock: " + l)
        for l in sorted(set(ours) - set(stock)):
            print("   + nasze: " + l)
    else:
        print("OK   %s (API = fabryczne K3346, major 48)" % name)
print("OVERRIDES_VERIFY=" + ("PASS" if ok else "FAIL"))
sys.exit(0 if ok else 1)
