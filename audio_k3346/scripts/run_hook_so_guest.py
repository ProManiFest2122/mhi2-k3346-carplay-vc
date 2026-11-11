#!/usr/bin/env python3
"""Emulator-only: run the stock CAudioDriverIF probe in the QNX guest with the built hook .so via LD_PRELOAD.

Builds an experiment SD image (hook .so + real APAC/AAC/FDK libs + the stock-driver probe), attaches it snapshot=on,
runs the probe (A) without preload and (B) with LD_PRELOAD at several MMI levels. Needs python3.13 (qnx6_image mmap).
Nothing here is a vehicle payload; the hook is built with MHI2_NV_SIX_CHANNEL_QUALIFIED=1 for this experiment only.
"""
import fcntl, hashlib, json, os, shutil, subprocess, sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
AUDIT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "runtime/qemu-mhi2/tools/mhi2"))
ALLOW = {
    "libmhi2_audio_hook.so": AUDIT / "artifacts" / (("native-hook-so-bt" if os.environ.get("HOOK_SO_LOCAL") else "native-hook-so")
        + ("-inert" if os.environ.get("HOOK_SO_INERT") else "")) / "libmhi2_audio_hook.so",
    "libmhi2_apac.so": AUDIT / "artifacts/native-type103/libmhi2_apac.so",
    "libmhi2_aac.so": AUDIT / "artifacts/fdk-decoder-qnx/libmhi2_aac.so",
    "libmhi2_fdk.so": AUDIT / "artifacts/fdk-decoder-qnx/libmhi2_fdk.so",
    "native-airplay-driver-probe": AUDIT / "artifacts/native-probe/native-airplay-driver-probe",
    "native-airplay-stream-probe": AUDIT / "artifacts/native-probe/native-airplay-stream-probe",
    "libmhi2_test_stream_stub.so": AUDIT / "artifacts/native-probe/libmhi2_test_stream_stub.so",
}
if os.environ.get("HOOK_SO_PRELOADLIST"):  # multi-library LD_PRELOAD test (constructor marker libs)
    ALLOW["preload-a.so"] = AUDIT / "artifacts/native-probe/preload-a.so"
    ALLOW["preload-b.so"] = AUDIT / "artifacts/native-probe/preload-b.so"
if os.environ.get("HOOK_SO_BENCH"):  # isolated SCENE CPU benchmark (emulator timing is NOT Tegra timing; ratios only)
    ALLOW["native-scene-bench"] = AUDIT / "artifacts/full-audio/native-scene-bench"
if os.environ.get("HOOK_SO_SPAWN"):  # does QNX spawn() run the #!/bin/sh mm-renderer shim (BT package) like medialauncher would
    ALLOW["native-spawn-probe"] = AUDIT / "artifacts/native-probe/native-spawn-probe"
    ALLOW["mm-renderer-shim.sh"] = AUDIT / "prototype/mib-hook-bt/mhi2-audio-bt/payload/mm-renderer-shim.sh"
if os.environ.get("HOOK_SO_PLAY8"):  # does stock nvaudio_play accept an 8-channel WAV on MPL1 (session-4 kit check)
    W8 = Path(os.environ["HOOK_SO_PLAY8"]); ALLOW["slot-4.wav"] = W8 / "slot-4.wav"; ALLOW["slot-7.wav"] = W8 / "slot-7.wav"
if os.environ.get("HOOK_SO_PLAYER"):  # mm-renderer-like client: stock nvaudio_play, 44.1k stereo WAV on MPL1
    ALLOW["native-tdm-mark"] = AUDIT / "artifacts/native-probe/native-tdm-mark"
    ALLOW["bt-44k-stereo.wav"] = AUDIT / "artifacts/native-probe/bt-44k-stereo.wav"
LEVELS = os.environ.get("HOOK_SO_LEVELS", "0,4").split(",")
MOUNT_CANDIDATES = os.environ.get("PIPELINE_SD_DEVICES", "/dev/sdb /dev/sdb0 /dev/sda /dev/sda0").split()


def build_sd():
    from qnx6_image import build
    out = AUDIT / "artifacts/hook-so-sd"; out.mkdir(exist_ok=True)
    src = out / "payload"
    if src.exists(): shutil.rmtree(src)
    src.mkdir()
    for name, path in ALLOW.items(): shutil.copy2(path, src / name)
    size = build(str(src), str(out / "sd.img"), 512, 16384)
    (out / "manifest.json").write_text(json.dumps({"emulator_only": True, "image_bytes": size,
        "files": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(src.iterdir())}}, indent=2) + "\n")
    return out / "sd.img"


def main():
    helpers = ROOT / "runtime/qemu-mhi2/tools/mhi2"
    sd = build_sd()
    run = AUDIT / "artifacts/runs" / ("hookso-" + datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")); run.mkdir(parents=True, exist_ok=False)
    temp = Path("/private/tmp/mhi2-audio-sd-20261015"); temp.mkdir(exist_ok=True)
    for name in ["debug_shell_local.py", "rcc_peer.py", "audio_host.py"]:
        text = (helpers / name).read_text().replace("/tmp/mhi2", str(temp / "mhi2"))
        text = text.replace("Path('/tmp',f)", "Path(" + repr(str(temp)) + ",f)")
        if name == "debug_shell_local.py":
            marker = "'-display','none'"; assert marker in text
            text = text.replace(marker, "'-drive','if=sd,index=1,format=raw,snapshot=on,file=" + str(sd) + "'," + marker, 1)
        (temp / name).write_text("import sys\nsys.path.insert(0, " + repr(str(helpers)) + ")\n" + text)
    mounts = "\n".join(f"mount -t qnx6 -o ro {d} /mnt/audio-pipeline && echo HOOKSO_MOUNT_OK={d} && break" for d in MOUNT_CANDIDATES)
    P = "/mnt/audio-pipeline"
    body = f"echo HOOKSO_A_NOPRELOAD\n{P}/native-airplay-driver-probe\necho HOOKSO_A_EXIT=$?\n"
    for lv in LEVELS:
        body += (f"echo HOOKSO_B_LEVEL={lv}\necho {lv} > /tmp/mhi2_audio_level\nrm -f /tmp/carplay_hook.log\n"
                 f"AUDIO_REALTIME_UPMIX=on AUDIO_SPATIAL=1 AUDIO_STEREO_UPMIX=on MHI2_NV_DEVICE=MPL1 LD_PRELOAD={P}/libmhi2_audio_hook.so {P}/native-airplay-driver-probe\n"
                 f"echo HOOKSO_B_EXIT_{lv}=$?\nsleep 2\necho HOOKSO_LOG_{lv}\nls -l /tmp/carplay_hook.log\ntail -40 /tmp/carplay_hook.log\necho HOOKSO_LOG_END_{lv}\n")
    if os.environ.get("HOOK_SO_PLAYER"):
        play = f"/armle/sbin/nvaudio_play -d MPL1 -f {P}/bt-44k-stereo.wav -v 1"
        body = f"echo HOOKSO_P_NOPRELOAD\necho 199 > /tmp/mhi2_tdm_marker\n{P}/native-tdm-mark\n{play}\necho HOOKSO_P_EXIT_NOPRELOAD=$?\n"
        for lv in LEVELS:
            body += (f"echo HOOKSO_P_LEVEL={lv}\necho {lv} > /tmp/mhi2_audio_level\necho {200 + int(lv)} > /tmp/mhi2_tdm_marker\n{P}/native-tdm-mark\n"
                     f"rm -f /tmp/carplay_hook.log\n"
                     f"AUDIO_REALTIME_UPMIX=on AUDIO_SPATIAL=1 AUDIO_STEREO_UPMIX=on MHI2_NV_DEVICE=MPL1 LD_PRELOAD={P}/libmhi2_audio_hook.so {play}\n"
                     f"echo HOOKSO_P_EXIT_{lv}=$?\nsleep 2\necho HOOKSO_LOG_{lv}\ntail -30 /tmp/carplay_hook.log\necho HOOKSO_LOG_END_{lv}\n")
        body += "echo 0 > /tmp/mhi2_tdm_marker\n" + f"{P}/native-tdm-mark\n"
    if os.environ.get("HOOK_SO_SPAWN"):
        # shim paths rewritten into /tmp; the fake real prints what the child sees
        body = (f"echo HOOKSO_SPAWN_BEGIN\nmkdir -p /tmp/bt/lib/real /tmp/bt/state\n"
                f"printf '#!/bin/sh\\necho CHILD_PID=$$ ARGV0=$0 ARGS=$* PRELOAD=${{LD_PRELOAD:-none}}\\n' > /tmp/bt/lib/real/mm-renderer\nchmod 755 /tmp/bt/lib/real/mm-renderer\n"
                f"cp {P}/libmhi2_audio_hook.so /tmp/bt/lib/libmhi2_audio_hook.so\n"
                f"cat {P}/mm-renderer-shim.sh | while read -r L; do echo \"$L\"; done | "
                f"sed -e 's#/mnt/app/eso/lib/mhi2bt#/tmp/bt/lib#g' -e 's#/mnt/app/root/mhi2-audio-bt#/tmp/bt/state#g' > /tmp/bt/mm-renderer\n"
                f"chmod 755 /tmp/bt/mm-renderer\nhead -4 /tmp/bt/mm-renderer\n"
                f"echo HOOKSO_SPAWN_DIRECT\ncat /tmp/bt/lib/real/mm-renderer\n/tmp/bt/mm-renderer -r x -k\necho HOOKSO_SPAWN_DIRECT_EXIT=$?\n"
                f"echo HOOKSO_SPAWN_RUN1\n{P}/native-spawn-probe /tmp/bt/mm-renderer\n"
                f"touch /tmp/bt/state/DISABLE\necho HOOKSO_SPAWN_RUN_DISABLED\n{P}/native-spawn-probe /tmp/bt/mm-renderer\n"
                f"rm /tmp/bt/state/DISABLE /tmp/bt/lib/real/mm-renderer\necho HOOKSO_SPAWN_RUN_NOREAL\n{P}/native-spawn-probe /tmp/bt/mm-renderer\necho HOOKSO_SPAWN_END\n")
    if os.environ.get("HOOK_SO_BENCH"):
        body = f"echo HOOKSO_BENCH_BEGIN\n{P}/native-scene-bench\necho HOOKSO_BENCH_EXIT=$?\n"
    if os.environ.get("HOOK_SO_PLAY8"):
        body = ""
        for n in ("4", "7"):
            body += f"echo HOOKSO_P8_SLOT{n}\n/armle/sbin/nvaudio_play -d MPL1 -f {P}/slot-{n}.wav -v 1\necho HOOKSO_P8_EXIT_{n}=$?\n"
    if os.environ.get('HOOK_SO_DISCOVER'):
        body = os.environ['HOOK_SO_DISCOVER'].replace(';', '\n') + '\n'
    if os.environ.get("HOOK_SO_STREAM"):
        body = ""
        for lv in LEVELS:
            body += (f"echo HOOKSO_C_LEVEL={lv}\necho {lv} > /tmp/mhi2_audio_level\nrm -f /tmp/carplay_hook.log\n"
                     f"AUDIO_REALTIME_UPMIX=on AUDIO_SPATIAL=1 AUDIO_STEREO_UPMIX=on MHI2_NV_DEVICE=MPL1 LD_PRELOAD={P}/libmhi2_audio_hook.so:{P}/libmhi2_test_stream_stub.so {P}/native-airplay-stream-probe\n"
                     f"echo HOOKSO_C_EXIT_{lv}=$?\nsleep 2\necho HOOKSO_LOG_{lv}\ntail -40 /tmp/carplay_hook.log\necho HOOKSO_LOG_END_{lv}\n")
    commands = f"""echo HOOKSO_BEGIN
export LD_LIBRARY_PATH={P}:/lib:/mnt/app/root/lib-target:/mnt/app/eso/lib:/mnt/app/armle/lib:/mnt/app/armle/lib/dll:/mnt/app/armle/usr/lib:/mnt/app/usr/lib
sleep 35
mkdir -p {P}
for i in 1; do
{mounts}
done
ls -l {P}
{body}echo HOOKSO_SLOG
sloginfo | tail -40
echo END_DIAG
"""
    (temp / "mhi2-diag-commands.txt").write_text(commands)
    media = AUDIT / "artifacts/native-media-aac"
    env = os.environ.copy()
    env.update(QEMU_SYSTEM_ARM=str(AUDIT / "artifacts/qemu/qemu-system-arm-capture"), MHI2_DEBUG_NOR=str(media / "nor.bin"), MHI2_IRAM=str(media / "iram.bin"),
        MHI2_EMMC=str(media / "emmc.raw"), MHI2_MEDIA_ROOT=str(media), MHI2_SNAPSHOT="on", MHI2_RCC="1", MHI2_GL="1", MHI2_OSCILLATOR_12MHZ="1",
        MHI2_GL_WIDTH="1024", MHI2_GL_HEIGHT="480", MHI2_GLHOST=str(ROOT / "runtime/k3346-build/bridge1024/mhi2-glhost"),
        MHI2_SHADER_CACHE=str(ROOT / "runtime/k3346-build/media/ui1024-uk/shaders"), MHI2_FIRMWARE="audi-a4", MHI2_HMI_LANG="uk_UA", MHI2_WIFI="none",
        MHI2_TIMEOUT=os.environ.get("PIPELINE_TIMEOUT", "170"), MHI2_TDM_CAPTURE_BASE=str(run / "native"))
    env.pop("MHI2_MOST", None)
    with open("/tmp/mhi2-ui.lock", "a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with (run / "session.log").open("w") as log:
            result = subprocess.run([sys.executable, str(temp / "debug_shell_local.py")], env=env, stdout=log, stderr=subprocess.STDOUT, timeout=int(env["MHI2_TIMEOUT"]) + 30)
    for name in ["mhi2-debug-console.log", "mhi2-debug-host.log", "mhi2-rcc-peer.log"]:
        path = temp / name
        if path.exists(): (run / name).write_bytes(path.read_bytes())
    console = (temp / "mhi2-debug-console.log").read_text(errors="replace") if (temp / "mhi2-debug-console.log").exists() else ""
    keys = ("HOOKSO", "AIRPLAY_DRIVER_NATIVE", "NVIDIA", "END_DIAG", "Unable", "Segmentation", "not found", "Cannot", "mhi2", "spa_", "audio")
    for line in console.splitlines():
        if any(k in line for k in keys): print(line)
    (run / "result.json").write_text(json.dumps({"exit_code": result.returncode, "emulated": True, "sd_image": str(sd), "levels": LEVELS}, indent=2) + "\n")
    print("Evidence:", run)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
