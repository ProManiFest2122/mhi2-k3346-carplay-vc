#!/bin/bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
workspace=$(cd "$here/../.." && pwd)
tc="${MHI2_TOOLCHAIN:-$here/deps/xpack-arm-none-eabi-gcc-13.3.1-1.1}/bin/arm-none-eabi"
out="$here/artifacts/fdk-decoder-qnx"
mkdir -p "$out"
headers="${MHI2_QNX_HEADERS:-$here/deps/openqnx/trunk}"
cat > "$out/fixture.S" <<ASM
.section .rodata
.balign 16
.global aac_fixture,aac_fixture_end
aac_fixture:
.incbin "$here/artifacts/fdk-decoder-host/raw-aac-fixture.bin"
aac_fixture_end:
ASM
flags=(-mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -marm -mno-unaligned-access -fno-builtin -nostdlib -O2 -std=gnu99 -D__QNXNTO__ -D__QNX__ -D__ARM__ -D__LITTLEENDIAN__ -D_QNX_SOURCE -I"$headers/lib/c/public" -I"$headers/services/system/public")
flags+=(-I"$here/reference-audio-a63d547/hook/audio" -I"$here/prototype" -I"$here/prototype/qnx-compat")
libm_view="$here/artifacts/native-buffered-pipeline/libm.so.2-LINK-ONLY.so"  # link-only, never shipped
fdk_includes=()
for dir in ${MHI2_FDK_SRC:-$here/deps/fdk-aac}/cpp/*/include;do fdk_includes+=(-isystem "$dir");done
"$tc-gcc" "${flags[@]}" "${fdk_includes[@]}" -fPIC -shared -Wl,-soname,libmhi2_aac.so -o "$out/libmhi2_aac.so" "$here/prototype/fdk_aac_bridge.c" "$out/libmhi2_fdk.so" "$here/artifacts/native-type103/libc-extra.so" "$libm_view" -lgcc
"$tc-gcc" "${flags[@]}" -c "$out/fixture.S" -o "$out/fixture.o"
"$tc-gcc" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start -o "$out/native-aac-probe" "$here/scripts/native_aac_probe.c" "$out/fixture.o" "$here/artifacts/native-nvidia-sink/libc-link-stub.so" -lgcc
