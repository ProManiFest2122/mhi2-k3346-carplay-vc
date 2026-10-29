#!/usr/bin/env python3
"""Sanity check for the Etap 7 cluster-overlay binaries (vc_overlay_probe,
vc_overlay) before they are allowed onto an SD card.

The check that matters most is DT_NEEDED.  We link against the QNX SDP's
reference libKD.so only to get the kd* prototypes and symbol resolution; the
car runs NVIDIA's libKD.so, which is self-contained.  The SDP library drags in
libiow.so, which does NOT exist on the unit - if it ever lands in DT_NEEDED the
binary dies at load time in the car with nothing but a dynamic-linker error to
go on, and the only place that is cheap to catch is here.

Also refuses libscreen: that is Luka's MHI2Q windowing stack and K3346 has no
such library at all, so its appearance means someone pulled his platform_qnx.c
back into the build.

Usage: python3 tools/verify_overlay_elf.py <elf> [<elf> ...]
"""

import struct
import sys

# Present on the car.  Both spellings are real files there - the Etap 1
# collector listed /net/mmx/lib/libKD.so and /net/mmx/lib/libKD.so.1 (same for
# EGL and GLESv2).  The stock loadandshowimage records the unversioned names;
# linking against the SDP picks up its SONAMEs, which end in .so.1.  Either
# loads, so accept both rather than fight the SONAME.
ALLOWED = {
    "libKD.so", "libEGL.so", "libGLESv2.so",
    "libKD.so.1", "libEGL.so.1", "libGLESv2.so.1",
    "libm.so.2", "libc.so.3", "libsocket.so.3", "libcpp.so.4",
}
# Never on the car - see the module docstring.
FORBIDDEN_SUBSTRINGS = ("iow", "screen")
# Required by base name, so a SONAME bump does not fail the check.
REQUIRED_BASE = {"libKD", "libEGL", "libGLESv2"}


def _sections(data):
    phoff = struct.unpack_from("<I", data, 0x1C)[0]
    phent = struct.unpack_from("<H", data, 0x2A)[0]
    phnum = struct.unpack_from("<H", data, 0x2C)[0]
    segs = []
    dyn = None
    for i in range(phnum):
        off = phoff + i * phent
        p_type, p_off, p_va, _pa, p_fsz, _msz, _fl, _al = struct.unpack_from("<8I", data, off)
        segs.append((p_type, p_off, p_va, p_fsz))
        if p_type == 2:  # PT_DYNAMIC
            dyn = (p_off, p_fsz)
    if dyn is None:
        raise ValueError("no PT_DYNAMIC segment")
    return segs, dyn


def _vaddr_to_off(segs, vaddr):
    for p_type, p_off, p_va, p_fsz in segs:
        if p_type == 1 and p_va <= vaddr < p_va + p_fsz:  # PT_LOAD
            return p_off + (vaddr - p_va)
    raise ValueError("vaddr 0x%x not in any PT_LOAD" % vaddr)


def dt_needed(path):
    data = open(path, "rb").read()
    if data[:4] != b"\x7fELF":
        raise ValueError("not an ELF")
    if data[4] != 1:
        raise ValueError("not ELF32 (car is 32-bit ARM)")
    machine = struct.unpack_from("<H", data, 0x12)[0]
    if machine != 40:
        raise ValueError("e_machine=%d, expected 40 (ARM)" % machine)

    segs, (dyn_off, dyn_size) = _sections(data)
    entries = []
    pos = dyn_off
    while pos < dyn_off + dyn_size:
        tag, val = struct.unpack_from("<iI", data, pos)
        pos += 8
        entries.append((tag, val))
        if tag == 0:  # DT_NULL
            break

    strtab = [v for t, v in entries if t == 5]
    if not strtab:
        raise ValueError("no DT_STRTAB")
    stroff = _vaddr_to_off(segs, strtab[0])

    out = []
    for tag, val in entries:
        if tag == 1:  # DT_NEEDED
            end = data.index(b"\0", stroff + val)
            out.append(data[stroff + val:end].decode())
    return out


def check(path):
    problems = []
    try:
        needed = dt_needed(path)
    except Exception as exc:  # noqa: BLE001 - report, do not crash the build
        print("FAIL %s: %s" % (path, exc))
        return False

    print("%s" % path)
    print("  DT_NEEDED: %s" % ", ".join(needed))

    for lib in needed:
        low = lib.lower()
        for bad in FORBIDDEN_SUBSTRINGS:
            if bad in low:
                problems.append("%s is not present on the car" % lib)
        if lib not in ALLOWED:
            problems.append("%s is not on the known-present list" % lib)

    present_base = {lib.split(".so")[0] for lib in needed}
    missing = REQUIRED_BASE - present_base
    if missing:
        problems.append("missing required: %s" % ", ".join(sorted(missing)))

    for p in problems:
        print("  FAIL: %s" % p)
    if not problems:
        print("  OK")
    return not problems


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    ok = True
    for path in argv[1:]:
        ok = check(path) and ok
    print("OVERLAY_ELF_VERIFY=%s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
