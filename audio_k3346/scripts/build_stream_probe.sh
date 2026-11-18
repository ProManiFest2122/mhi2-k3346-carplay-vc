#!/usr/bin/env bash
# Emulator-only: stream-bracketed driver probe + RTLD_NEXT stub for AudioStreamPrepare/Stop.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
cc="${MHI2_TOOLCHAIN:-$here/deps/xpack-arm-none-eabi-gcc-13.3.1-1.1}/bin/arm-none-eabi-gcc"
out="$here/artifacts/native-probe"
mkdir -p "$out"
flags=(-mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -marm -mno-unaligned-access -ffreestanding -fno-builtin -nostdlib -O2)
cat > "$out/airplay-stream-link-stub.c" <<'STUB'
void ctor(void) __asm__("_ZN14CAudioDriverIFC1EPKc");void ctor(void){}
int op(void) __asm__("_ZN14CAudioDriverIF4openEv");int op(void){return 0;}
int cf(void) __asm__("_ZN14CAudioDriverIF9configureEjjj");int cf(void){return 0;}
int wr(void) __asm__("_ZN14CAudioDriverIF5writeEPKhjj");int wr(void){return 0;}
int cl(void) __asm__("_ZN14CAudioDriverIF5closeEv");int cl(void){return 0;}
int AudioStreamPrepare(void *s){(void)s;return 0;} void AudioStreamStop(void *s,unsigned char d){(void)s;(void)d;}
STUB
"$cc" "${flags[@]}" -fPIC -shared -Wl,-soname,libairplay.so -o "$out/airplay-stream-link-stub.so" "$out/airplay-stream-link-stub.c"
"$cc" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start \
  -o "$out/native-airplay-stream-probe" "$here/scripts/native_airplay_stream_probe.c" "$out/airplay-stream-link-stub.so" "$out/libc-link-stub.so"
"$cc" "${flags[@]}" -fPIC -shared -Wl,-soname,libmhi2_test_stream_stub.so -o "$out/libmhi2_test_stream_stub.so" "$here/scripts/native_test_stream_stub.c"
echo "stream probe build PASS"
