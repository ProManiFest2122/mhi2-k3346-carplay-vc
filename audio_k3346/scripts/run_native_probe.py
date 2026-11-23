#!/usr/bin/env python3
"""Run copied K3346 media and helpers in an isolated emulator session."""
import fcntl
import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
AUDIT = Path(__file__).resolve().parents[1]


def main():
    helpers = ROOT / "runtime/qemu-mhi2/tools/mhi2"
    run = AUDIT / "artifacts/runs" / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    run.mkdir(parents=True, exist_ok=False)
    temp = Path("/private/tmp/mhi2-audio-audit-20261014")
    temp.mkdir(exist_ok=True)
    for name in ["debug_shell_local.py", "rcc_peer.py", "audio_host.py"]:
        text = (helpers / name).read_text()
        text = text.replace("/tmp/mhi2", str(temp / "mhi2"))
        text = text.replace("Path('/tmp',f)", "Path(" + repr(str(temp)) + ",f)")
        text = "import sys\nsys.path.insert(0, " + repr(str(helpers)) + ")\n" + text
        if name == "rcc_peer.py":
            # Prefer the copied audio helper over the shared one.
            text = "import sys\nsys.path.insert(0, " + repr(str(helpers)) + ")\nsys.path.insert(0, " + repr(str(temp)) + ")\n" + (helpers / name).read_text().replace("/tmp/mhi2", str(temp / "mhi2"))
        (temp / name).write_text(text)
    commands = """echo UI_DIAGNOSTIC_SHELL_READY
export LD_LIBRARY_PATH=/eso/lib:/armle/lib:/armle/usr/lib:/lib:/usr/lib
echo AUDIO_NATIVE_WAIT
sleep 35
echo AUDIO_NATIVE_DEVICES
ls /dev/nvaudio* /dev/shmem/NVshm-tdm-* /sbin/nvaudio-probe
echo AUDIO_NATIVE_CAPABILITIES
/sbin/nvaudio-probe
echo AUDIO_NATIVE_TDM_ONEHOT
sleep 10
/sbin/nvaudio-tdm-probe
echo AUDIO_STOCK_PLAYER_6CH
/armle/sbin/nvaudio_play -d MPL1 -f /etc/audio-audit-onehot6.wav -v 1
echo AUDIO_NATIVE_SLOG
sloginfo | tail -100
echo END_DIAG
"""
    if os.environ.get("AUDIO_PROBE_ROUTE") == "MPL5":
        commands = commands.replace("/sbin/nvaudio-tdm-probe", """slay -s SIGSTOP servicemgrmibhigh
slay mediarouter
sleep 2
cd /eso
echo EMULATOR_MPL5_ROUTE_CONTROL
sleep 2
/sbin/nvaudio-mpl5-probe""")
        commands = commands.replace("/armle/sbin/nvaudio_play -d MPL1 -f /etc/audio-audit-onehot6.wav -v 1", "echo STOCK_PLAYER_SKIPPED_MPL5_FIXTURE")
    if os.environ.get("AUDIO_PROBE_SCENE"):
        commands = commands.replace("echo AUDIO_STOCK_PLAYER_6CH", "echo AUDIO_SCENE_NATIVE\n/sbin/native-scene-probe\necho AUDIO_STOCK_PLAYER_6CH")
    if os.environ.get("AUDIO_PROBE_ADAPTER"):
        commands = commands.replace("echo AUDIO_STOCK_PLAYER_6CH", "echo AUDIO_ADAPTER_NATIVE\n/sbin/native-adapter-probe\necho AUDIO_STOCK_PLAYER_6CH")
    if os.environ.get("AUDIO_PROBE_INTERPOSE"):
        commands = commands.replace("echo AUDIO_STOCK_PLAYER_6CH", "echo AUDIO_UPMIX_INTERPOSE\nLD_PRELOAD=/lib/nvaudio-upmix-fixture.so /sbin/nvaudio-stereo-client\necho AUDIO_STOCK_PLAYER_6CH")
    if os.environ.get("AUDIO_PROBE_APAC"):
        commands = commands.replace("echo AUDIO_STOCK_PLAYER_6CH", "echo AUDIO_APAC_NATIVE\n/sbin/native-apac-probe\necho APAC_NATIVE_EXIT=$?\necho AUDIO_STOCK_PLAYER_6CH")
    if os.environ.get("AUDIO_PROBE_AIRPLAY"):
        commands = commands.replace("echo AUDIO_STOCK_PLAYER_6CH", "echo AUDIO_AIRPLAY_DRIVER_NATIVE\nLD_LIBRARY_PATH=/root/lib-target:$LD_LIBRARY_PATH LD_PRELOAD=/lib/nvaudio-upmix-airplay-test.so /sbin/native-airplay-driver-probe\necho AIRPLAY_DRIVER_NATIVE_EXIT=$?\necho AUDIO_STOCK_PLAYER_6CH")
    if os.environ.get("AUDIO_PROBE_SETTINGS"):
        sequence = []
        for level in range(5):
            sequence += [f"echo AUDIO_SETTINGS_LEVEL={level}", f"echo {level} > /tmp/mhi2_audio_level", "LD_LIBRARY_PATH=/root/lib-target:$LD_LIBRARY_PATH LD_PRELOAD=/lib/libmhi2_audio_test.so /sbin/native-airplay-driver-probe", "echo AUDIO_SETTINGS_EXIT=$?"]
        commands = commands.replace("echo AUDIO_STOCK_PLAYER_6CH", "\n".join(sequence) + "\necho AUDIO_STOCK_PLAYER_6CH")
    if os.environ.get("AUDIO_PROBE_FULL_SCENE"):
        commands = "echo FULL_SCENE_BEGIN\nsleep 35\n/sbin/native-full-scene-probe\necho FULL_SCENE_EXIT=$?\necho END_DIAG\n"
    if os.environ.get("AUDIO_PROBE_TYPE103"):
        commands = "echo TYPE103_NATIVE_BEGIN\nsleep 35\n/sbin/native-type103-probe\necho TYPE103_NATIVE_EXIT=$?\necho END_DIAG\n"
    if os.environ.get("AUDIO_PROBE_NVIDIA_SINK"):
        commands = "echo APAC_NVIDIA_BEGIN\nexport LD_LIBRARY_PATH=/root/lib-target:/lib:/eso/lib:/armle/lib:/armle/usr/lib\nsleep 35\n/sbin/native-apac-nvidia-probe\necho APAC_NVIDIA_EXIT=$?\necho END_DIAG\n"
    if os.environ.get("AUDIO_PROBE_AAC"):
        commands = "echo AAC_NATIVE_BEGIN\nexport LD_LIBRARY_PATH=/root/lib-target:/lib:/eso/lib:/armle/lib:/armle/usr/lib\nsleep 35\n/sbin/native-aac-probe\necho AAC_NATIVE_EXIT=$?\necho END_DIAG\n"
    (temp / "mhi2-diag-commands.txt").write_text(commands)
    media = AUDIT / "artifacts/native-media"
    if os.environ.get("AUDIO_PROBE_AAC"):
        media = AUDIT / "artifacts/native-media-aac"
    installed = json.loads((media / "manifest.json").read_text())["added"]
    required = {
        "AUDIO_PROBE_AAC": ["sbin/native-aac-probe", "lib/libmhi2_fdk.so", "lib/libmhi2_aac.so"],
        "AUDIO_PROBE_NVIDIA_SINK": ["sbin/native-apac-nvidia-probe", "lib/libmhi2_apac.so"],
        "AUDIO_PROBE_TYPE103": ["sbin/native-type103-probe", "etc/type103-frames.bin"],
        "AUDIO_PROBE_FULL_SCENE": ["sbin/native-full-scene-probe"],
        "AUDIO_PROBE_SETTINGS": ["sbin/native-airplay-driver-probe", "lib/libmhi2_audio_test.so"],
        "AUDIO_PROBE_AIRPLAY": ["sbin/native-airplay-driver-probe", "lib/nvaudio-upmix-airplay-test.so"],
        "AUDIO_PROBE_APAC": ["sbin/native-apac-probe"],
        "AUDIO_PROBE_ADAPTER": ["sbin/native-adapter-probe"],
        "AUDIO_PROBE_SCENE": ["sbin/native-scene-probe"],
        "AUDIO_PROBE_INTERPOSE": ["sbin/nvaudio-stereo-client", "lib/nvaudio-upmix-fixture.so"],
    }
    for flag, paths in required.items():
        if os.environ.get(flag) and any(path not in installed for path in paths):
            raise RuntimeError(f"{flag}: required probe absent from this emulator profile; prepare matching media first")
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
        MHI2_TIMEOUT="150", MHI2_TDM_CAPTURE_BASE=str(run / "native"),
    )
    env.pop("MHI2_MOST", None)
    (media / "tmp").mkdir(exist_ok=True)
    # Use the ordinary session lock to avoid starting beside an owner's session.
    with open("/tmp/mhi2-ui.lock", "a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with (run / "session.log").open("w") as log:
            result = subprocess.run([sys.executable, str(temp / "debug_shell_local.py")], env=env, stdout=log, stderr=subprocess.STDOUT, timeout=175)
    for name in ["mhi2-debug-console.log", "mhi2-debug-host.log", "mhi2-rcc-peer.log", "mhi2-glhost.log"]:
        path = temp / name
        if path.exists():
            (run / name).write_bytes(path.read_bytes())
    print("session status", result.returncode)
    console = (temp / "mhi2-debug-console.log").read_text(errors="replace")
    for line in console.splitlines():
        if "AAC_NATIVE" in line or "APAC_NVIDIA" in line or "TYPE103_NATIVE" in line or line.startswith("OK  ") or "FULL_SCENE" in line or "AUDIO_SETTINGS_" in line or "AIRPLAY_DRIVER_NATIVE" in line or "UPMIX_FIXTURE" in line or "APAC_NATIVE" in line or "ADAPTER_NATIVE" in line or "SCENE_NATIVE" in line or "NVAUDIO_PROBE" in line or "TDM_PROBE" in line or "AUDIO_NATIVE" in line or "Unable to start" in line or "END_DIAG" in line:
            print(line)
    (run / "result.json").write_text(json.dumps({
        "exit_code": result.returncode, "probe_completed": "NVAUDIO_PROBE_DONE" in console,
        "emulated": True, "source_emmc_snapshot": True,
        "tdm_captures": [{"path": str(p), "bytes": p.stat().st_size} for p in run.glob("native-*.raw")],
    }, indent=2) + "\n")
    (AUDIT / "artifacts/latest-run.txt").write_text(str(run) + "\n")
    print("Evidence:", run)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
