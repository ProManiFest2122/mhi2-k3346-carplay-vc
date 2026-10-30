#!/usr/bin/env python3
"""Build an isolated QEMU executable with a passive raw TDM TX tap.

Reuses existing build objects and generated build commands. No shared source,
object, executable or firmware image is changed. The tap observes DMA bytes;
it does not model the RCC DSP or select amplifier channels.
"""
import argparse
import difflib
import json
import shlex
import subprocess
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source", type=Path, required=True)
    ap.add_argument("--build", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    args.source = args.source.resolve()
    args.build = args.build.resolve()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    src = args.source / "hw/misc/tegra30_apbdma.c"
    original = src.read_text()
    marker = "        /* TDM1 announcement slots: native NvAudioMuxStreamsTDM1 places"
    assert original.count(marker) == 1
    tap = '''        /* Research-only passive tap, downstream of the native mux. */
        {
            static FILE *captures[4];
            static FILE *indices[4];
            static uint64_t captured[4];
            const char *base = getenv("MHI2_TDM_CAPTURE_BASE");
            unsigned slot = (apb - 0x70080000) / 32;
            uint32_t marker_value = c->owner->ahub_regs[0xe20 / 4];
            if (base && marker_value && slot < 4 && captured[slot] < 64 * 1024 * 1024) {
                if (!captures[slot]) {
                    char path[1024];
                    snprintf(path, sizeof(path), "%s-tdm%u.raw", base, slot);
                    captures[slot] = fopen(path, "wb");
                    snprintf(path, sizeof(path), "%s-tdm%u.index", base, slot);
                    indices[slot] = fopen(path, "w");
                }
                if (captures[slot]) {
                    if (indices[slot]) {
                        fprintf(indices[slot], "%" PRId64 " %" PRIu64 " %u %u\\n",
                                qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL), captured[slot], bytes, marker_value);
                        fflush(indices[slot]);
                    }
                    captured[slot] += fwrite(buffer, 1, bytes, captures[slot]);
                    fflush(captures[slot]);
                }
            }
        }
'''
    patched = original.replace(marker, tap + marker)
    control = "    if (off == 0xe10) {"
    assert patched.count(control) == 1
    patched = patched.replace(control, "    if (off == 0xe20) {\n        s->ahub_regs[off / 4] = value;\n    } else if (off == 0xe10) {")
    target = args.output / src.name
    target.write_text(patched)
    (args.output / "passive-tdm-tap.patch").write_text("".join(difflib.unified_diff(original.splitlines(True), patched.splitlines(True), fromfile=str(src), tofile=str(target))))
    entries = json.loads((args.build / "compile_commands.json").read_text())
    entry = next(x for x in entries if x["file"].endswith("hw/misc/tegra30_apbdma.c"))
    cmd = shlex.split(entry["command"])
    obj = args.output / "tegra30_apbdma.capture.o"
    old_obj = entry["output"]
    for option in ["-MQ", "-MF", "-o"]:
        if option in cmd:
            index = cmd.index(option) + 1
            cmd[index] = str(obj) + (".d" if option == "-MF" else "")
    cmd[cmd.index("-c") + 1] = str(target)
    cmd = [x.replace("/Volumes/MacNVMe/qemu-mhi2", str(args.source)) for x in cmd]
    subprocess.run(cmd, cwd=args.build, check=True)
    commands = subprocess.check_output(["ninja", "-t", "commands", "qemu-system-arm"], cwd=args.build, text=True).splitlines()
    link = next(parts for line in commands
                if "-o" in (parts := shlex.split(line)) and "qemu-system-arm-unsigned" in parts)
    link[link.index("-o") + 1] = str(args.output / "qemu-system-arm-capture")
    matches = sum(x == old_obj for x in link)
    assert matches == 1, f"Expected direct link object, found {matches}"
    link = [str(obj) if x == old_obj else x for x in link]
    subprocess.run(link, cwd=args.build, check=True)
    (args.output / "commands.json").write_text(json.dumps({"compile": cmd, "link": link, "cwd": str(args.build)}, indent=2) + "\n")
    print(args.output / "qemu-system-arm-capture")


if __name__ == "__main__":
    main()
