#!/usr/bin/env bash
# Emulator-only: two constructor libs to test multi-library LD_PRELOAD on the QNX loader.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
cc="${MHI2_TOOLCHAIN:-$here/deps/xpack-arm-none-eabi-gcc-13.3.1-1.1}/bin/arm-none-eabi-gcc"
out="$here/artifacts/native-probe"; mkdir -p "$out"
flags=(-mfloat-abi=soft -mcpu=cortex-a9 -marm -mno-unaligned-access -fno-builtin -nostdlib -O2 -fPIC -shared)
for n in a b; do
cat > "$out/preload-$n.c" <<SRC
extern long write(int,const void *,unsigned long);
__attribute__((constructor)) static void init(void){ write(1,"PRELOAD_CTOR_${n}\n",15); }
SRC
"$cc" "${flags[@]}" -Wl,-soname,preload-$n.so -o "$out/preload-$n.so" "$out/preload-$n.c" "$out/libc-link-stub.so"
done
