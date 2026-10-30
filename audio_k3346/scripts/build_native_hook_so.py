#!/usr/bin/env python3
"""Build the K3346 NVIDIA audio hook as a shared object (emulator experiment; link-only libc/libm views never ship)."""
from pathlib import Path
import os, subprocess, sys
A = Path(__file__).resolve().parents[1]; W = A.parent.parent
R = A / 'reference-audio-a63d547'; H = Path(os.environ.get('MHI2_QNX_HEADERS', str(A/'deps/openqnx/trunk')))
TC = str(Path(os.environ.get('MHI2_TOOLCHAIN', str(A/'deps/xpack-arm-none-eabi-gcc-13.3.1-1.1')))/'bin/arm-none-eabi')
V = A / 'artifacts/native-nv-hook'   # link-only views made by build_native_buffered_pipeline.py (PIPELINE_VARIANT=nv_hook)
Q = os.environ.get('HOOK_SO_QUALIFIED', '1')   # 1 = emulator experiment; 0 = inert/fail-closed package variant
LOCAL = os.environ.get('HOOK_SO_LOCAL') == '1'   # separate Bluetooth/USB/SD build for mm-renderer (MPL1, 44.1k claim, own media route)
DEV = os.environ.get('HOOK_SO_DEVICE', 'MPL1' if Q == '1' or LOCAL else 'MPL5')
O = A / (('artifacts/native-hook-so-bt' if Q == '1' else 'artifacts/native-hook-so-bt-inert') if LOCAL else
         ('artifacts/native-hook-so' if Q == '1' else 'artifacts/native-hook-so-inert')); O.mkdir(exist_ok=True)
FDK = Path(os.environ.get('MHI2_FDK_SRC', str(A/'deps/fdk-aac')))
def run(args):
    return subprocess.check_output(list(map(str, args)), stderr=subprocess.STDOUT, text=True)
def main():
    run([sys.executable, A / 'scripts/prepare_full_audio_port.py'])
    flags = ['-mfloat-abi=softfp', '-mfpu=neon', '-mcpu=cortex-a9', '-marm', '-mno-unaligned-access', '-fno-builtin', '-nostdlib', '-O2',
             '-ffp-contract=off', '-std=gnu99', '-fPIC', '-fvisibility=hidden', '-ffunction-sections', '-fdata-sections', '-D__QNXNTO__', '-D__QNX__', '-D__ARM__',
             '-D__LITTLEENDIAN__', '-D_QNX_SOURCE', '-DMHI2_NV_SIX_CHANNEL_QUALIFIED=' + Q, '-DMHI2_NV_STOCK_BRIDGE', '-DSPA_NV_STOCK_DEVICE="' + DEV + '"'] + (['-DMHI2_NV_BRIDGE_LOCAL_MEDIA'] if LOCAL else [])
    for d in (FDK / 'cpp').glob('*/include'): flags += ['-isystem', str(d)]
    for p in [A / 'prototype/qnx-compat', H / 'lib/c/public', H / 'services/system/public', R / 'tests/mocks', R / 'hook', R / 'hook/audio', R / 'hook/framework', A / 'prototype']:
        flags += ['-I' + str(p)]
    sources = [A / 'artifacts/full-audio/port/audio_hook_k3346_nv.c', A / 'prototype/nvidia_pcm_sink.c', A / 'prototype/fdk_aac_bridge.c', A / 'prototype/hook_main_audio_only.c']
    sources += [R / 'hook/audio' / n for n in ['apac_codec_loader.c', 'stereo_upmix.c', 'soundcheck.c', 'scene_engine_math.c', 'scene_clusters.c', 'scene_engine.c',
                'scene_mask.c', 'scene_mask_worker.c', 'spatial_downmix.c', 'apap_lifecycle.c', 'apap_crypto.c']]
    sources += [R / 'hook/framework' / n for n in ['iap2_protocol.c', 'logging.c', 'state_trace.c', 'airplay_seams.c']]
    objs = []
    for s in sources:
        o = O / (s.stem + '.o')
        try: run([TC + '-gcc', *flags, '-c', s, '-o', o])
        except subprocess.CalledProcessError as e: print(e.output[-3000:]); raise
        objs.append(o)
    out = O / 'libmhi2_audio_hook.so'
    deps = [A / 'artifacts/native-type103/libmhi2_apac.so', A / 'artifacts/fdk-decoder-qnx/libmhi2_aac.so', A / 'artifacts/fdk-decoder-qnx/libmhi2_fdk.so',
            A / 'artifacts/native-nvidia-sink/nvaudio-link-stub.so', V / 'libc.so.3-LINK-ONLY.so', V / 'libm.so.2-LINK-ONLY.so']
    run([TC + '-gcc', *flags, '-shared', '-Wl,-soname,libmhi2_audio_hook.so,--gc-sections,--allow-shlib-undefined,--version-script=' + str(A / 'scripts/hook_so.exports.map'), '-o', out, *objs, *deps, '-lgcc'])
    print(run([TC + '-readelf', '-d', out])); (O / 'undefined.txt').write_text(run([TC + '-nm', '-D', '--undefined-only', out]))
    print('hook .so build PASS ->', out)
if __name__ == '__main__':
    try: main()
    except subprocess.CalledProcessError as e: print(e.output); raise SystemExit(e.returncode)
