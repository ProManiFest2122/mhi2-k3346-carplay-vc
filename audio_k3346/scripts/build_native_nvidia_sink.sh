#!/bin/bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
workspace=$(cd "$here/../.." && pwd)
tc="${MHI2_TOOLCHAIN:-$here/deps/xpack-arm-none-eabi-gcc-13.3.1-1.1}/bin/arm-none-eabi"
ref="$here/reference-audio-a63d547"
out="$here/artifacts/native-nvidia-sink"
mkdir -p "$out"
headers="${MHI2_QNX_HEADERS:-$here/deps/openqnx/trunk}"
mkdir -p "$out"
cc -std=gnu99 -O2 -Wall -Wextra -Werror -pthread "$here/prototype/nvidia_pcm_sink.c" "$here/prototype/test_nvidia_pcm_sink.c" -o "$out/test-nvidia-sink"
"$out/test-nvidia-sink"
cc -std=gnu99 -O2 -Wall -Wextra -Werror -pthread "$here/prototype/nvidia_pcm_sink.c" "$here/prototype/test_nvidia_sink_concurrency.c" -o "$out/test-nvidia-concurrency"
"$out/test-nvidia-concurrency"
cat > "$out/fixture.S" <<ASM
.section .rodata
.balign 16
.global apac_fixture,apac_fixture_end
apac_fixture:
.incbin "$ref/apac_decoder/tests/fixtures/frames.bin"
apac_fixture_end:
ASM
flags=(-mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -marm -mno-unaligned-access -fno-builtin -nostdlib -O2 -std=gnu99 -D__QNXNTO__ -D__QNX__ -D__ARM__ -D__LITTLEENDIAN__ -D_QNX_SOURCE -I"$here/prototype/qnx-compat" -I"$headers/lib/c/public" -I"$headers/services/system/public" -I"$here/prototype" -I"$ref/hook/audio")
cat > "$out/libc-link-stub.c" <<'STUB'
int snprintf(void){return 0;} int write(void){return 0;} void exit(void){} void *memset(void){return 0;} void *memcpy(void){return 0;} int pthread_mutex_init(void){return 0;} int pthread_mutex_lock(void){return 0;} int pthread_mutex_unlock(void){return 0;} int pthread_mutex_destroy(void){return 0;} void *dlopen(void){return 0;} void *dlsym(void){return 0;} int dlclose(void){return 0;} int open(void){return 0;} int close(void){return 0;} void *mmap64(void){return 0;} int usleep(void){return 0;}
STUB
"$tc-gcc" "${flags[@]}" -fPIC -shared -Wl,-soname,libc.so.3 -o "$out/libc-link-stub.so" "$out/libc-link-stub.c"
cat > "$out/nvaudio-link-stub.c" <<'STUB'
int nvaudio_pcm_open(void){return 0;} int nvaudio_set_pcm_info(void){return 0;} int nvaudio_create_metadata(void){return 0;} int nvaudio_pcm_write(void){return 0;} int nvaudio_pcm_close(void){return 0;}
STUB
"$tc-gcc" "${flags[@]}" -fPIC -shared -Wl,-soname,libnvaudio.so -o "$out/nvaudio-link-stub.so" "$out/nvaudio-link-stub.c"
"$tc-gcc" "${flags[@]}" -c "$out/fixture.S" -o "$out/fixture.o"
"$tc-gcc" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start -o "$out/native-apac-nvidia-probe" "$here/scripts/native_apac_nvidia_probe.c" "$here/prototype/nvidia_pcm_sink.c" "$ref/hook/audio/spatial_downmix.c" "$out/fixture.o" "$out/libc-link-stub.so" "$out/nvaudio-link-stub.so" -lgcc
