#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
workspace=$(cd "$here/../.." && pwd)
cc="${MHI2_TOOLCHAIN:-$here/deps/xpack-arm-none-eabi-gcc-13.3.1-1.1}/bin/arm-none-eabi-gcc"
out="$here/artifacts/native-probe"
mkdir -p "$out"
source_dir="$here/upstream/hook/audio"
flags=(-mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -marm -mno-unaligned-access -ffreestanding -fno-builtin -nostdlib -O2 -ffp-contract=off -I"$source_dir")
cat > "$out/libm-link-stub.c" <<'STUB'
float fabsf(float x){return x;} float cosf(float x){return x;} float sinf(float x){return x;} float expf(float x){return x;} float logf(float x){return x;} float sqrtf(float x){return x;} float expm1f(float x){return x;}
STUB
"$cc" "${flags[@]}" -fPIC -shared -Wl,-soname,libm.so.2 -o "$out/libm-link-stub.so" "$out/libm-link-stub.c"
cat > "$out/nvaudio-link-stub.c" <<'STUB'
int nvaudio_pcm_open(void){return 0;} int nvaudio_set_pcm_info(void){return 0;} int nvaudio_create_metadata(void){return 0;} int nvaudio_pcm_write(void){return 0;} int nvaudio_pcm_close(void){return 0;}
STUB
"$cc" "${flags[@]}" -fPIC -shared -Wl,-soname,libnvaudio.so -o "$out/nvaudio-link-stub.so" "$out/nvaudio-link-stub.c"
for unit in scene_engine scene_engine_math scene_clusters; do
 "$cc" "${flags[@]}" -c "$source_dir/$unit.c" -o "$out/$unit.o"
done
"$cc" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start \
 -o "$out/native-scene-probe" "$here/scripts/native_scene_probe.c" "$out/scene_engine.o" "$out/scene_engine_math.o" "$out/scene_clusters.o" \
 "$out/nvaudio-link-stub.so" "$out/libm-link-stub.so" "$out/libc-link-stub.so" -lgcc

"$cc" "${flags[@]}" -I"$here/prototype" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start \
 -o "$out/native-adapter-probe" "$here/scripts/native_adapter_probe.c" "$here/prototype/mhi2_scene_adapter.c" "$out/scene_engine.o" "$out/scene_engine_math.o" "$out/scene_clusters.o" \
 "$out/nvaudio-link-stub.so" "$out/libm-link-stub.so" "$out/libc-link-stub.so" -lgcc

"$cc" "${flags[@]}" -fPIC -I"$here/prototype" -DFIXTURE_DEVICE=\"MPL1\" -shared -Wl,-soname,nvaudio-upmix-fixture.so \
 -o "$out/nvaudio-upmix-fixture.so" "$here/prototype/nvaudio_upmix_fixture.c" "$here/prototype/mhi2_scene_adapter.c" \
 "$source_dir/scene_engine.c" "$source_dir/scene_engine_math.c" "$source_dir/scene_clusters.c" \
 "$out/libm-link-stub.so" "$out/libc-link-stub.so" -lgcc

# Separate experimental native AirPlay seam; never injected automatically.
"$cc" "${flags[@]}" -fPIC -I"$here/prototype" -DMHI2_EXPERIMENTAL_AIRPLAY_CONFIGURE -shared -Wl,-soname,nvaudio-upmix-airplay-experimental.so \
 -o "$out/nvaudio-upmix-airplay-experimental.so" "$here/prototype/nvaudio_upmix_fixture.c" "$here/prototype/mhi2_scene_adapter.c" \
 "$source_dir/scene_engine.c" "$source_dir/scene_engine_math.c" "$source_dir/scene_clusters.c" \
 "$out/libm-link-stub.so" "$out/libc-link-stub.so" -lgcc
"$cc" "${flags[@]}" -fPIC -I"$here/prototype" -DMHI2_EXPERIMENTAL_AIRPLAY_CONFIGURE -DFIXTURE_DEVICE=\"MPL1\" -shared -Wl,-soname,nvaudio-upmix-airplay-test.so \
 -o "$out/nvaudio-upmix-airplay-test.so" "$here/prototype/nvaudio_upmix_fixture.c" "$here/prototype/mhi2_scene_adapter.c" \
 "$source_dir/scene_engine.c" "$source_dir/scene_engine_math.c" "$source_dir/scene_clusters.c" \
 "$out/libm-link-stub.so" "$out/libc-link-stub.so" -lgcc
cat > "$out/airplay-api-link-stub.c" <<'STUB'
void ctor(void) __asm__("_ZN14CAudioDriverIFC1EPKc");void ctor(void){}
int op(void) __asm__("_ZN14CAudioDriverIF4openEv");int op(void){return 0;}
int cf(void) __asm__("_ZN14CAudioDriverIF9configureEjjj");int cf(void){return 0;}
int wr(void) __asm__("_ZN14CAudioDriverIF5writeEPKhjj");int wr(void){return 0;}
int cl(void) __asm__("_ZN14CAudioDriverIF5closeEv");int cl(void){return 0;}
STUB
"$cc" "${flags[@]}" -fPIC -shared -Wl,-soname,libairplay.so -o "$out/airplay-api-link-stub.so" "$out/airplay-api-link-stub.c"
"$cc" "${flags[@]}" -Wl,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start \
 -o "$out/native-airplay-driver-probe" "$here/scripts/native_airplay_driver_probe.c" "$out/airplay-api-link-stub.so" "$out/libc-link-stub.so"

# Preview settings build: Off by default; setting sampled only at configure.
"$cc" "${flags[@]}" -fPIC -I"$here/prototype" -DMHI2_EXPERIMENTAL_AIRPLAY_CONFIGURE -DMHI2_AUDIO_SETTINGS -shared -Wl,-soname,libmhi2_audio.so \
 -o "$out/libmhi2_audio.so" "$here/prototype/nvaudio_upmix_fixture.c" "$here/prototype/mhi2_scene_adapter.c" \
 "$source_dir/scene_engine.c" "$source_dir/scene_engine_math.c" "$source_dir/scene_clusters.c" \
 "$out/libm-link-stub.so" "$out/libc-link-stub.so" -lgcc

"$cc" "${flags[@]}" -fPIC -I"$here/prototype" -DMHI2_EXPERIMENTAL_AIRPLAY_CONFIGURE -DMHI2_AUDIO_SETTINGS -DFIXTURE_DEVICE=\"MPL1\" -shared -Wl,-soname,libmhi2_audio_test.so \
 -o "$out/libmhi2_audio_test.so" "$here/prototype/nvaudio_upmix_fixture.c" "$here/prototype/mhi2_scene_adapter.c" \
 "$source_dir/scene_engine.c" "$source_dir/scene_engine_math.c" "$source_dir/scene_clusters.c" \
 "$out/libm-link-stub.so" "$out/libc-link-stub.so" -lgcc
