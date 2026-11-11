#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
workspace=$(cd "$here/../.." && pwd)
tc="${MHI2_TOOLCHAIN:-$here/deps/xpack-arm-none-eabi-gcc-13.3.1-1.1}/bin/arm-none-eabi"
source_dir="$here/upstream/apac_decoder"
out="$here/artifacts/native-apac"
mkdir -p "$out"
flags=(-mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -marm -mno-unaligned-access -fno-builtin -nostdlib -O2 -ffp-contract=off -std=c++11 -fno-exceptions -fno-rtti -I"$source_dir/include")
cat > "$out/math-link-stub.c" <<'STUB'
float acosf(float x){return x;} float ceilf(float x){return x;} float log2f(float x){return x;} float powf(float x,float y){return x+y;} double cos(double x){return x;} double sin(double x){return x;} double sqrt(double x){return x;} double pow(double x,double y){return x+y;}
float cosf(float x){return x;} float sinf(float x){return x;} float expf(float x){return x;} float logf(float x){return x;} float sqrtf(float x){return x;} float expm1f(float x){return x;} float fabsf(float x){return x;}
STUB
"$tc-gcc" -mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -fno-builtin -nostdlib -fPIC -shared -Wl,-soname,libm.so.2 -o "$out/math-link-stub.so" "$out/math-link-stub.c"
for file in "$source_dir"/src/*.cpp;do "$tc-g++" "${flags[@]}" -c "$file" -o "$out/$(basename "${file%.cpp}").o";done
cat > "$out/fixture.S" <<ASM
.section .rodata
.global apac_fixture
.global apac_fixture_end
apac_fixture:
.incbin "$source_dir/tests/fixtures/frames.bin"
apac_fixture_end:
ASM
"$tc-gcc" -c "$out/fixture.S" -o "$out/fixture.o"
"$tc-g++" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start \
 -o "$out/native-apac-probe" "$here/scripts/native_apac_probe.cpp" "$out"/*.o \
 "$out/math-link-stub.so" "$here/artifacts/native-probe/libc-link-stub.so" -lgcc
