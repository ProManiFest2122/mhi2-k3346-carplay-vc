#!/bin/bash
# Run the reference APAP -> authenticated decrypt -> APAC -> PCM6 test on QNX.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
workspace=$(cd "$here/../.." && pwd)
tc="${MHI2_TOOLCHAIN:-$here/deps/xpack-arm-none-eabi-gcc-13.3.1-1.1}/bin/arm-none-eabi"
ref="$here/reference-audio-a63d547"
out="$here/artifacts/native-type103"
mkdir -p "$out"
headers="${MHI2_QNX_HEADERS:-$here/deps/openqnx/trunk}"
mkdir -p "$out"
base=(-mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -marm -mno-unaligned-access -fno-builtin -nostdlib -O2 -ffp-contract=off)
cpp=("${base[@]}" -std=c++11 -fno-exceptions -fno-rtti -I"$ref/apac_decoder/include" -I"$ref/hook/audio")
c=("${base[@]}" -std=gnu99 -D__QNXNTO__ -D__QNX__ -D__ARM__ -D__LITTLEENDIAN__ -D_QNX_SOURCE -I"$here/prototype/qnx-compat" -I"$headers/lib/c/public" -I"$headers/services/system/public" -I"$ref/hook/audio")
for file in "$ref"/apac_decoder/src/*.cpp "$ref/hook/audio/apac_bridge.cpp";do "$tc-g++" "${cpp[@]}" -c "$file" -o "$out/$(basename "${file%.cpp}").o";done
for file in "$ref/hook/audio/apap_crypto.c" "$ref/hook/audio/spatial_downmix.c";do "$tc-gcc" "${c[@]}" -c "$file" -o "$out/$(basename "${file%.c}").o";done
"$tc-gcc" "${c[@]}" -Dmain=reference_type103_main -c "$ref/tests/apac_type103_test.c" -o "$out/test.o"
cat > "$out/start.c" <<'C'
extern int reference_type103_main(int,char **);
extern void exit(int);
extern int write(int,const void *,unsigned);
void _start(void){char *args[]={"type103","/etc/type103-frames.bin",0};int r=reference_type103_main(2,args);const char done[]="TYPE103_NATIVE_TEST_FINISHED\n";write(1,done,sizeof(done)-1);exit(r);}
C
"$tc-gcc" "${c[@]}" -c "$out/start.c" -o "$out/start.o"
cat > "$out/libc-extra.c" <<'C'
int printf(void){return 0;} int puts(void){return 0;} void *fopen(void){return 0;} int fclose(void){return 0;} int fseek(void){return 0;} long ftell(void){return 0;} unsigned fread(void){return 0;} void *malloc(void){return 0;} void *calloc(void){return 0;} void *realloc(void){return 0;} void free(void){} void *memcpy(void){return 0;} void *memset(void){return 0;} void *memmove(void){return 0;} int memcmp(void){return 0;} int snprintf(void){return 0;} void exit(void){} int write(void){return 0;} void _assert(void){} void __assert(void){}
C
"$tc-gcc" "${base[@]}" -fPIC -shared -Wl,-soname,libc.so.3 "$out/libc-extra.c" -o "$out/libc-extra.so"
"$tc-g++" "${cpp[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start -o "$out/native-type103-probe" "$out"/*.o "$out/libc-extra.so" "$here/artifacts/native-apac/math-link-stub.so" -lgcc

# Shared codec is independent of the hook and every non-audio module.
# Recompile PIC objects; never reuse executable objects for a shared library.
mkdir -p "$out/pic"
for file in "$ref"/apac_decoder/src/*.cpp "$ref/hook/audio/apac_bridge.cpp";do "$tc-g++" "${cpp[@]}" -fPIC -c "$file" -o "$out/pic/$(basename "${file%.cpp}").o";done
"$tc-g++" "${cpp[@]}" -shared -Wl,-soname,libmhi2_apac.so -o "$out/libmhi2_apac.so" "$out/pic"/*.o "$out/libc-extra.so" "$here/artifacts/native-apac/math-link-stub.so" -lgcc
"$tc-readelf" -d "$out/libmhi2_apac.so" | sed -n '1,16p'
