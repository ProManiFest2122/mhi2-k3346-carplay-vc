#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
workspace=$(cd "$here/../.." && pwd)
cc="${MHI2_TOOLCHAIN:-$here/deps/xpack-arm-none-eabi-gcc-13.3.1-1.1}/bin/arm-none-eabi-gcc"
out="$here/artifacts/native-probe"
mkdir -p "$out"
cat > "$out/libc-link-stub.c" <<'STUB'
int snprintf(void){return 0;} int write(void){return 0;} void exit(void){} void *memset(void){return 0;} int usleep(void){return 0;} int read(void){return 0;} int open(void){return 0;} int close(void){return 0;} void *mmap64(void){return 0;} void *dlopen(void){return 0;} void *dlsym(void){return 0;} int strcmp(void){return 0;} void *memcpy(void){return 0;}
STUB
flags=(-mfloat-abi=soft -mcpu=cortex-a9 -marm -mno-unaligned-access -ffreestanding -fno-builtin -nostdlib -O2)
"$cc" "${flags[@]}" -fPIC -shared -Wl,-soname,libc.so.3 -o "$out/libc-link-stub.so" "$out/libc-link-stub.c"
"$cc" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start \
  -o "$out/nvaudio-probe" "$here/scripts/nvaudio_probe.c" \
  "$here/artifacts/stock/armle/usr/lib/libnvaudio.so" "$out/libc-link-stub.so"
"$cc" "${flags[@]}" -fPIC -shared -Wl,-soname,audio-route5-fixture.so \
  -o "$out/audio-route5-fixture.so" "$here/scripts/route5_fixture.c" "$out/libc-link-stub.so"
"${cc%gcc}readelf" -d "$out/nvaudio-probe" | sed -n '1,16p'
"$cc" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start \
  -o "$out/nvaudio-tdm-probe" "$here/scripts/nvaudio_tdm_probe.c" \
  "$here/artifacts/stock/armle/usr/lib/libnvaudio.so" "$out/libc-link-stub.so"

"$cc" "${flags[@]}" -DPROBE_ROUTE -DPROBE_DEVICE=\"MPL5\" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start \
  -o "$out/nvaudio-mpl5-probe" "$here/scripts/nvaudio_tdm_probe.c" \
  "$here/artifacts/stock/armle/usr/lib/libnvaudio.so" "$out/libc-link-stub.so"

"$cc" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start \
 -o "$out/nvaudio-stereo-client" "$here/scripts/nvaudio_stereo_client.c" \
 "$here/artifacts/stock/armle/usr/lib/libnvaudio.so" "$out/libc-link-stub.so"
