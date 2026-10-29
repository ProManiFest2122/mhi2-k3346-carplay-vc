#!/bin/bash
# Build the isolated SCENE CPU benchmark: host (Mac) and ARM/QNX (runs on the unit from /tmp or in the emulator guest).
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
workspace=$(cd "$here/../.." && pwd)
tc="${MHI2_TOOLCHAIN:-$here/deps/xpack-arm-none-eabi-gcc-13.3.1-1.1}/bin/arm-none-eabi"
ref="$here/reference-audio-a63d547"
out="$here/artifacts/full-audio"
mkdir -p "$out"
headers="${MHI2_QNX_HEADERS:-$here/deps/openqnx/trunk}"
test -f "$out/mask.o" -a -f "$out/libc-full-link-stub.so" || bash "$here/scripts/build_full_scene.sh" >/dev/null
sources=("$ref/hook/audio/stereo_upmix.c" "$ref/hook/audio/scene_engine.c" "$ref/hook/audio/scene_engine_math.c" "$ref/hook/audio/scene_clusters.c" "$ref/hook/audio/scene_mask.c")
cc -O2 -ffp-contract=off -DMHI2_HOST -I"$ref/hook/audio" "$here/scripts/native_scene_bench.c" "$out/mask.c" "${sources[@]}" -lm -o "$out/scene-bench-host"
flags=(-mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -marm -mno-unaligned-access -fno-builtin -nostdlib -O2 -ffp-contract=off -std=gnu99 -D__QNXNTO__ -D__QNX__ -D__ARM__ -D__LITTLEENDIAN__ -D_QNX_SOURCE -I"$here/prototype/qnx-compat" -I"$headers/lib/c/public" -I"$headers/services/system/public" -I"$ref/hook/audio")
# link-only libc view for the extra symbols the bench uses (the unit's real libc.so.3 provides them at run time; never shipped)
cp "$out/libc-full-link-stub.c" "$out/libc-bench-link-stub.c"
cat >> "$out/libc-bench-link-stub.c" <<'STUB'
int vsnprintf(void){return 0;} void qsort(void){} int clock_gettime(void){return 0;}
STUB
"$tc-gcc" -mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -nostdlib -fPIC -fno-builtin -shared -Wl,-soname,libc.so.3 -o "$out/libc-bench-link-stub.so" "$out/libc-bench-link-stub.c"
"$tc-gcc" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start -o "$out/native-scene-bench" "$here/scripts/native_scene_bench.c" "$out/mask.o" "${sources[@]}" "$out/libm-full-link-stub.so" "$out/libc-bench-link-stub.so" -lgcc
echo "scene bench build PASS: $out/scene-bench-host $out/native-scene-bench"
