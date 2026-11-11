#!/usr/bin/env python3
"""Add experiment-only resources to a copy of prepared K3346 emulator NOR.

The source images stay untouched. eMMC will be attached with snapshot=on.
This output is emulator media, not a vehicle installation or update package.
"""
import ctypes
import hashlib
import json
import struct
import sys
import io
import wave
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
AUDIT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "runtime/qemu-mhi2/tools/mhi2/glforward"))
sys.path.insert(0, str(ROOT / "runtime/qemu-mhi2/tools/mhi2"))
from ifs_resource import add_file
from lzo_library import load_lzo2
from mhi2_ifs import decode_lzoz, read_imagefs
from qnx6_read import Qnx6
from sparse_copy import copy_sparse


def main():
    source = ROOT / "runtime/k3346-build/media/ui1024-uk"
    output = AUDIT / "artifacts/native-media"
    # Use the existing stock-language baseline for this independent experiment.
    # Its own cloned disk keeps all previous native-media runs recoverable.
    if "--aac" in sys.argv:
        source = ROOT / "runtime/k3346-build/media/ui1024"
        output = AUDIT / "artifacts/native-media-aac"
    output.mkdir(exist_ok=True)
    nor = bytearray((source / "nor.bin").read_bytes())
    manifest = json.loads((ROOT / "runtime/k3346-build/media/base/manifest.json").read_text())
    _, offset, capacity = manifest["partitions"]["MAIN_STAGE2"]
    lib = load_lzo2()
    raw, blocks = decode_lzoz(bytes(nor[offset:offset + capacity]), lib._name)
    original_entries = read_imagefs(raw)
    print("original IFS validated", len(raw), "bytes", len(blocks), "blocks", len(original_entries), "files", flush=True)
    additions = {
        "sbin/native-apac-probe": AUDIT / "artifacts/native-apac/native-apac-probe",
        "lib/nvaudio-upmix-fixture.so": AUDIT / "artifacts/native-probe/nvaudio-upmix-fixture.so",
        "sbin/nvaudio-stereo-client": AUDIT / "artifacts/native-probe/nvaudio-stereo-client",
        "sbin/native-adapter-probe": AUDIT / "artifacts/native-probe/native-adapter-probe",
        "sbin/nvaudio-mpl5-probe": AUDIT / "artifacts/native-probe/nvaudio-mpl5-probe",
        "sbin/nvaudio-probe": AUDIT / "artifacts/native-probe/nvaudio-probe",
        "sbin/nvaudio-tdm-probe": AUDIT / "artifacts/native-probe/nvaudio-tdm-probe",
    }
    if "--audio-seam" in sys.argv or "--audio-settings" in sys.argv:
        additions.pop("sbin/native-apac-probe", None)
        additions.pop("sbin/native-adapter-probe", None)
        additions["lib/nvaudio-upmix-airplay-test.so"] = AUDIT / "artifacts/native-probe/nvaudio-upmix-airplay-test.so"
        additions["sbin/native-airplay-driver-probe"] = AUDIT / "artifacts/native-probe/native-airplay-driver-probe"
    if "--audio-settings" in sys.argv:
        additions["lib/libmhi2_audio_test.so"] = AUDIT / "artifacts/native-probe/libmhi2_audio_test.so"
    if "--full-scene" in sys.argv:
        additions = {"sbin/native-full-scene-probe": AUDIT / "artifacts/full-audio/native-full-scene-probe"}
    if "--type103" in sys.argv:
        additions = {"sbin/native-type103-probe": AUDIT / "artifacts/native-type103/native-type103-probe", "etc/type103-frames.bin": AUDIT / "reference-audio-a63d547/apac_decoder/tests/fixtures/frames.bin"}
    if "--nvidia-sink" in sys.argv:
        additions = {"sbin/native-apac-nvidia-probe": AUDIT / "artifacts/native-nvidia-sink/native-apac-nvidia-probe", "lib/libmhi2_apac.so": AUDIT / "artifacts/native-type103/libmhi2_apac.so"}
    if "--aac" in sys.argv:
        additions = {"sbin/native-aac-probe": AUDIT / "artifacts/fdk-decoder-qnx/native-aac-probe", "lib/libmhi2_fdk.so": AUDIT / "artifacts/fdk-decoder-qnx/libmhi2_fdk.so", "lib/libmhi2_aac.so": AUDIT / "artifacts/fdk-decoder-qnx/libmhi2_aac.so"}
    # A small, independently generated one-hot WAV for the stock player.
    import numpy as np
    pcm = np.zeros((512 * 12 * 6, 6), dtype="<i2")
    for channel in range(6):
        pcm[channel * 512 * 12:(channel + 1) * 512 * 12, channel] = (channel + 1) * 1000
    wav = io.BytesIO()
    with wave.open(wav, "wb") as f:
        f.setnchannels(6); f.setsampwidth(2); f.setframerate(48000); f.writeframes(pcm.tobytes())
    wav_path = AUDIT / "artifacts/native-probe/onehot6.wav"
    wav_path.write_bytes(wav.getvalue())
    additions["etc/audio-audit-onehot6.wav"] = wav_path
    for name, host in additions.items():
        raw = add_file(raw, name, host.read_bytes())
    entries = read_imagefs(raw)
    before = {x["path"]: x.get("sha256") for x in original_entries}
    after = {x["path"]: x.get("sha256") for x in entries}
    assert all(after.get(name) == sha for name, sha in before.items())
    block = 2 * 1024 * 1024
    packed = bytearray(0x800)
    packed[:8] = b"LZOZ" + struct.pack("<I", block)
    work = ctypes.create_string_buffer(16 * 1024 * 1024)
    for index, at in enumerate(range(0, len(raw), block)):
        data = bytes(raw[at:at + block])
        buf = ctypes.create_string_buffer(len(data) + len(data) // 16 + 128)
        size = ctypes.c_size_t(len(buf))
        assert lib.lzo1z_999_compress(data, ctypes.c_size_t(len(data)), buf, ctypes.byref(size), work) == 0
        check, n = ctypes.create_string_buffer(block), ctypes.c_size_t(block)
        assert lib.lzo1z_decompress_safe(buf, size, check, ctypes.byref(n), None) == 0
        assert check.raw[:n.value] == data
        struct.pack_into("<I", packed, 8 + 4 * index, size.value)
        packed += buf.raw[:size.value]
        packed += bytes((-len(packed)) % 512)
    assert len(packed) < capacity
    # IFSLOADER may select its eMMC restore copy instead of NOR. Keep both
    # copies identical on an isolated cloned disk, never on the owner disk.
    disk = output / "emmc.raw"
    if not disk.exists():
        clone = subprocess.run(["/bin/cp", "-c", str(source / "emmc.raw"), str(disk)], capture_output=True)
        if clone.returncode:
            if disk.exists():
                raise RuntimeError("Incomplete disk clone; preserve it and inspect before retrying")
            copy_sparse(source / "emmc.raw", disk)
    with disk.open("rb") as f:
        mbr = f.read(512)
    start = struct.unpack_from("<I", mbr, 454)[0] * 512
    fs = Qnx6(str(disk), start)
    inode = 1
    for name in ("img_restore", "main_stage2.ifs.lzo"):
        inode = dict(fs.listdir(inode))[name]
    node = fs.inode(inode)
    assert len(packed) <= node["size"], (len(packed), node["size"])
    restore = packed + bytes(node["size"] - len(packed))
    with disk.open("r+b") as f:
        for index, at in enumerate(range(0, len(restore), fs.bs)):
            physical = fs._bmap(node, index)
            assert physical is not None
            f.seek(start + (physical + fs.sblks_off) * fs.bs)
            f.write(restore[at:at + fs.bs])
    fs.f.close()
    print("synchronized isolated eMMC restore IFS", len(packed), "bytes", flush=True)
    nor[offset:offset + capacity] = packed + bytes([255]) * (capacity - len(packed))
    (output / "nor.bin").write_bytes(nor)
    (output / "iram.bin").write_bytes((source / "iram.bin").read_bytes())
    record = {
        "emulator_only": True, "snapshot_emmc": True,
        "source_nor": str(source / "nor.bin"),
        "source_nor_sha256": hashlib.sha256((source / "nor.bin").read_bytes()).hexdigest(),
        "output_nor_sha256": hashlib.sha256(nor).hexdigest(),
        "source_emmc": str(source / "emmc.raw"),
        "experiment_emmc": str(disk), "nor_and_restore_ifs_identical": True,
        "source_ifS_bytes": len(raw), "compressed_bytes": len(packed),
        "original_resources_preserved": True,
        "added": {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in additions.items()},
    }
    (output / "manifest.json").write_text(json.dumps(record, indent=2) + "\n")
    print("prepared", output, "compressed", len(packed), "bytes", flush=True)


if __name__ == "__main__":
    main()
