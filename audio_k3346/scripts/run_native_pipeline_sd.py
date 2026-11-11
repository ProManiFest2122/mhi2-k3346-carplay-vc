#!/usr/bin/env python3
"""Run the native buffered pipeline from an experiment-owned SD image in the emulator.

Uses private copies of the shared helpers; the shared QEMU tree is not modified.
eMMC and the SD image are both attached snapshot=on. Emulator only.
"""
import fcntl
import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
AUDIT = Path(__file__).resolve().parents[1]
STOCK = os.environ.get("PIPELINE_VARIANT") == "nv_stock"
DUMP = os.environ.get("PIPELINE_VARIANT") == "aac_dump"
REPLAY = os.environ.get("PIPELINE_VARIANT") == "nv_replay"
NV = os.environ.get("PIPELINE_VARIANT") in ("nv_hook", "nv_stock") or REPLAY
BIN = "native-nv-stock" if STOCK else "native-aac-dump" if DUMP else "native-nv-replay" if REPLAY else "native-nv-hook-detached" if NV else "native-buffered-pipeline"
MOUNT_CANDIDATES = os.environ.get("PIPELINE_SD_DEVICES", "/dev/sdb /dev/sdb0 /dev/sda /dev/sda0").split()


def main():
    helpers = ROOT / "runtime/qemu-mhi2/tools/mhi2"
    run = AUDIT / "artifacts/runs" / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    run.mkdir(parents=True, exist_ok=False)
    temp = Path("/private/tmp/mhi2-audio-sd-20261015")
    temp.mkdir(exist_ok=True)
    sd = AUDIT / ("artifacts/native-pipeline-sd-stock" if STOCK else "artifacts/native-pipeline-sd-aacdump" if DUMP else "artifacts/native-pipeline-sd-replay" if REPLAY else "artifacts/native-pipeline-sd-nv" if NV else "artifacts/native-pipeline-sd") / "sd.img"
    for name in ["debug_shell_local.py", "rcc_peer.py", "audio_host.py"]:
        text = (helpers / name).read_text().replace("/tmp/mhi2", str(temp / "mhi2"))
        text = text.replace("Path('/tmp',f)", "Path(" + repr(str(temp)) + ",f)")
        if name == "debug_shell_local.py":
            marker = "'-display','none'"
            assert marker in text
            text = text.replace(marker, "'-drive','if=sd,index=1,format=raw,snapshot=on,file=" + str(sd) + "'," + marker, 1)
        text = "import sys\nsys.path.insert(0, " + repr(str(helpers)) + ")\n" + text
        (temp / name).write_text(text)
    mounts = "\n".join(f"mount -t qnx6 -o ro {d} /mnt/audio-pipeline && echo PIPELINE_MOUNT_OK={d} && break" for d in MOUNT_CANDIDATES)
    extra = "".join(f"{'AUDIO_SCENE_MASK_BLOB=/mnt/audio-pipeline/scene_masknet_c.bin ' if s == 'mask' else 'NV_FIX44=/mnt/audio-pipeline/raw-aac-44k-fixture.bin ' if s == 'rate44' else ''}MHI2_NV_SCENARIO={s} /mnt/audio-pipeline/{BIN}\necho PIPELINE_{s.upper()}_EXIT=$?\n" for s in os.environ.get("PIPELINE_SCENARIOS", "stock" if STOCK else "level,soundcheck,switch,mask,rate44").split(",")) if NV and not os.environ.get("PIPELINE_ONLY_BASE") else ""
    base_run = "" if STOCK else f"/mnt/audio-pipeline/{BIN}\necho PIPELINE_EXIT=$?\n"
    wm = (("" if REPLAY else "export NV_WATERMARK=1\n") + ("export NV_PACE=1\n" if os.environ.get("PIPELINE_PACE") else "") + ("export NV_RAMP=1\n" if os.environ.get("PIPELINE_RAMP") else "")) if NV and os.environ.get("PIPELINE_ONLY_BASE") else ""
    commands = f"""echo PIPELINE_SD_BEGIN
{wm}export LD_LIBRARY_PATH=/mnt/audio-pipeline:/lib:/eso/lib:/armle/lib:/armle/usr/lib
sleep 35
echo PIPELINE_DEVICES
ls -l /dev/sd* /dev/hd* /dev/mmc*
mkdir -p /mnt/audio-pipeline
echo PIPELINE_MKDIR=$?
for i in 1; do
{mounts}
done
ls -l /mnt/audio-pipeline
{base_run}{extra}echo END_DIAG
"""
    (temp / "mhi2-diag-commands.txt").write_text(commands)
    media = AUDIT / "artifacts/native-media-aac"
    env = os.environ.copy()
    env.update(
        QEMU_SYSTEM_ARM=str(AUDIT / "artifacts/qemu/qemu-system-arm-capture"),
        MHI2_DEBUG_NOR=str(media / "nor.bin"), MHI2_IRAM=str(media / "iram.bin"),
        MHI2_EMMC=str(media / "emmc.raw"),
        MHI2_MEDIA_ROOT=str(media), MHI2_SNAPSHOT="on", MHI2_RCC="1", MHI2_GL="1",
        MHI2_OSCILLATOR_12MHZ="1", MHI2_GL_WIDTH="1024", MHI2_GL_HEIGHT="480",
        MHI2_GLHOST=str(ROOT / "runtime/k3346-build/bridge1024/mhi2-glhost"),
        MHI2_SHADER_CACHE=str(ROOT / "runtime/k3346-build/media/ui1024-uk/shaders"),
        MHI2_FIRMWARE="audi-a4", MHI2_HMI_LANG="uk_UA", MHI2_WIFI="none",
        MHI2_TIMEOUT=os.environ.get("PIPELINE_TIMEOUT", "170"), MHI2_TDM_CAPTURE_BASE=str(run / "native"),
    )
    env.pop("MHI2_MOST", None)
    with open("/tmp/mhi2-ui.lock", "a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with (run / "session.log").open("w") as log:
            result = subprocess.run([sys.executable, str(temp / "debug_shell_local.py")], env=env, stdout=log, stderr=subprocess.STDOUT, timeout=int(os.environ.get("PIPELINE_TIMEOUT", "170")) + 30)
    for name in ["mhi2-debug-console.log", "mhi2-debug-host.log", "mhi2-rcc-peer.log"]:
        path = temp / name
        if path.exists():
            (run / name).write_bytes(path.read_bytes())
    console = (temp / "mhi2-debug-console.log").read_text(errors="replace") if (temp / "mhi2-debug-console.log").exists() else ""
    keys = ("PIPELINE", "NVIDIA buffered", "NVIDIA hook", "NVIDIA replay", "replay:", "AACDUMP", "stock", "NVIDIA stock", "pending after", "nominal:", "recovery:", "flush:", "Assertion", "segment", "frames:", "mask ", "rate44", "soundcheck", "A(off)", "B(aggr", "C(off", "END_DIAG", "/dev/sd", "Unable", "Segmentation", "not found", "Cannot")
    for line in console.splitlines():
        if any(k in line for k in keys):
            print(line)
    (run / "result.json").write_text(json.dumps({
        "exit_code": result.returncode, "emulated": True, "sd_image": str(sd),
        "pipeline_exit_line": [l for l in console.splitlines() if "PIPELINE_EXIT=" in l],
    }, indent=2) + "\n")
    print("Evidence:", run)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
