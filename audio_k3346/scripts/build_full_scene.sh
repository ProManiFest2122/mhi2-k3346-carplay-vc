#!/bin/bash
# Build unchanged complete reference DSP for an isolated numerical experiment.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
workspace=$(cd "$here/../.." && pwd)
tc="${MHI2_TOOLCHAIN:-$here/deps/xpack-arm-none-eabi-gcc-13.3.1-1.1}/bin/arm-none-eabi"
ref="$here/reference-audio-a63d547"
out="$here/artifacts/full-audio"
mkdir -p "$out"
headers="${MHI2_QNX_HEADERS:-$here/deps/openqnx/trunk}"
mkdir -p "$out"
cat > "$out/mask.S" <<ASM
.section .rodata
.global full_scene_mask,full_scene_mask_end
.balign 16
full_scene_mask:
.incbin "$ref/models/scene-masknet-c/scene_masknet_c.bin"
full_scene_mask_end:
ASM
# Host and ARM share the exact byte blob; emit a portable C definition for host.
python3 - "$ref/models/scene-masknet-c/scene_masknet_c.bin" "$out/mask.c" <<'PY'
import sys
from pathlib import Path
b=Path(sys.argv[1]).read_bytes();Path(sys.argv[2]).write_text('const unsigned char full_scene_mask[]={'+','.join(map(str,b))+'};\n__asm__(".globl _full_scene_mask_end\\n_full_scene_mask_end = _full_scene_mask + '+str(len(b))+'\\n");\n')
PY
sources=("$ref/hook/audio/stereo_upmix.c" "$ref/hook/audio/scene_engine.c" "$ref/hook/audio/scene_engine_math.c" "$ref/hook/audio/scene_clusters.c" "$ref/hook/audio/scene_mask.c")
cc -O2 -ffp-contract=off -DMHI2_HOST -I"$ref/hook/audio" "$here/scripts/native_full_scene_probe.c" "$out/mask.c" "${sources[@]}" -lm -o "$out/full-scene-host"
"$out/full-scene-host"
flags=(-mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -marm -mno-unaligned-access -fno-builtin -nostdlib -O2 -ffp-contract=off -std=gnu99 -D__QNXNTO__ -D__QNX__ -D__ARM__ -D__LITTLEENDIAN__ -D_QNX_SOURCE -I"$here/prototype/qnx-compat" -I"$headers/lib/c/public" -I"$headers/services/system/public" -I"$ref/hook/audio")
cat > "$out/libc-full-link-stub.c" <<'STUB'
int snprintf(void){return 0;} int write(void){return 0;} void exit(void){} void *memset(void){return 0;} void *memcpy(void){return 0;} void *malloc(void){return 0;} void free(void){} void *memmove(void){return 0;} int memcmp(void){return 0;} void *_Stdin,*_Stdout,*_Stderr;
STUB
"$tc-gcc" -mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -nostdlib -fPIC -fno-builtin -shared -Wl,-soname,libc.so.3 -o "$out/libc-full-link-stub.so" "$out/libc-full-link-stub.c"
cat > "$out/math-extra.c" <<'STUB'
long lrintf(float x){return 0;} float floorf(float x){return x;} float ceilf(float x){return x;}
STUB
"$tc-gcc" -mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -nostdlib -fPIC -fno-builtin -shared -Wl,-soname,libm.so.2 -o "$out/libm-full-link-stub.so" "$here/artifacts/native-probe/libm-link-stub.c" "$out/math-extra.c"
"$tc-gcc" -c "$out/mask.S" -o "$out/mask.o"
"$tc-gcc" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start -o "$out/native-full-scene-probe" "$here/scripts/native_full_scene_probe.c" "$out/mask.o" "${sources[@]}" "$out/libm-full-link-stub.so" "$out/libc-full-link-stub.so" -lgcc
