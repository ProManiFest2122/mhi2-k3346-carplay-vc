#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
cc="${MHI2_TOOLCHAIN:-$here/deps/xpack-arm-none-eabi-gcc-13.3.1-1.1}/bin/arm-none-eabi-gcc"
out="$here/artifacts/native-probe"; mkdir -p "$out"
cat > "$out/libc-spawn-stub.c" <<'STUB'
char **environ;int spawn(void){return 0;}int waitpid(void){return 0;}int getpid(void){return 0;}long write(void){return 0;}void exit(void){}
STUB
flags=(-mfloat-abi=soft -mcpu=cortex-a9 -marm -mno-unaligned-access -fno-builtin -nostdlib -O2)
"$cc" "${flags[@]}" -fPIC -shared -Wl,-soname,libc.so.3 -o "$out/libc-spawn-stub.so" "$out/libc-spawn-stub.c"
cat > "$out/spawn-start.c" <<'S'
extern int main(int,char**);extern void exit(int);
void _start_c(long *sp){int argc=(int)sp[0];exit(main(argc,(char **)(sp+1)));}
__asm__(".global _start\n_start:\n mov r0, sp\n bl _start_c\n");
S
"$cc" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start -o "$out/native-spawn-probe" "$here/scripts/native_spawn_probe.c" "$out/spawn-start.c" "$out/libc-spawn-stub.so"
"${cc%gcc}readelf" -h "$out/native-spawn-probe" | rg -i "class|machine|entry"
