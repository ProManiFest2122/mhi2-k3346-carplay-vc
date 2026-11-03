#!/usr/bin/env python3
"""Build an experiment-only QNX6 SD image holding the native pipeline payload.

Allowlist only: executable, real APAC/AAC/FDK libraries, two fixtures, FDK NOTICE.
LINK-ONLY views, libc/libm and libnvaudio stubs must never be copied here.
"""
import hashlib
import os
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
AUDIT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "runtime/qemu-mhi2/tools/mhi2"))
from qnx6_image import build

STOCK = os.environ.get("PIPELINE_VARIANT") == "nv_stock"
DUMP = os.environ.get("PIPELINE_VARIANT") == "aac_dump"
REPLAY = os.environ.get("PIPELINE_VARIANT") == "nv_replay"
NV = os.environ.get("PIPELINE_VARIANT") in ("nv_hook", "nv_stock") or REPLAY
BIN = "native-nv-stock" if STOCK else "native-aac-dump" if DUMP else "native-nv-replay" if REPLAY else "native-nv-hook-detached" if NV else "native-buffered-pipeline"
ALLOW = {
    BIN: AUDIT / ("artifacts/native-nv-stock/" if STOCK else "artifacts/native-aac-dump/" if DUMP else "artifacts/native-nv-replay/" if REPLAY else "artifacts/native-nv-hook/" if NV else "artifacts/native-buffered-pipeline/") / BIN,
    "libmhi2_apac.so": AUDIT / "artifacts/native-type103/libmhi2_apac.so",
    "libmhi2_aac.so": AUDIT / "artifacts/fdk-decoder-qnx/libmhi2_aac.so",
    "libmhi2_fdk.so": AUDIT / "artifacts/fdk-decoder-qnx/libmhi2_fdk.so",
    "frames.bin": AUDIT / "reference-audio-a63d547/apac_decoder/tests/fixtures/frames.bin",
    "raw-aac-fixture.bin": AUDIT / "artifacts/fdk-decoder-host/raw-aac-fixture.bin",
    "scene_masknet_c.bin": AUDIT / "reference-audio-a63d547/models/scene-masknet-c/scene_masknet_c.bin",
    "raw-aac-44k-fixture.bin": AUDIT / "artifacts/fdk-decoder-host/raw-aac-44k-fixture.bin",
    "expected-writes.bin": AUDIT / "artifacts/native-nv-hook/expected-writes.bin",
}


def main():
    out = AUDIT / ("artifacts/native-pipeline-sd-stock" if STOCK else "artifacts/native-pipeline-sd-aacdump" if DUMP else "artifacts/native-pipeline-sd-replay" if REPLAY else "artifacts/native-pipeline-sd-nv" if NV else "artifacts/native-pipeline-sd")
    out.mkdir(exist_ok=True)
    src = out / "payload"
    if src.exists():
        shutil.rmtree(src)
    src.mkdir()
    for name, path in ALLOW.items():
        shutil.copy2(path, src / name)
    notice = next((AUDIT / "artifacts/fdk-decoder-qnx").glob("*NOTICE*"), None)
    if notice:
        shutil.copy2(notice, src / "FDK-NOTICE")
    size = build(str(src), str(out / "sd.img"), 512, 16384)
    (out / "manifest.json").write_text(json.dumps({
        "emulator_only": True, "image_bytes": size,
        "files": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(src.iterdir())},
    }, indent=2) + "\n")
    print("SD image", out / "sd.img", size, "bytes")


if __name__ == "__main__":
    main()
