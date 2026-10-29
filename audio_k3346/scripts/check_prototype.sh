#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
source_dir="$here/upstream/hook/audio"
out="$here/artifacts/native-probe"
mkdir -p "$out"
units=("$source_dir/scene_engine.c" "$source_dir/scene_engine_math.c" "$source_dir/scene_clusters.c")
cc -O2 -ffp-contract=off -I"$source_dir" "$here/prototype/mhi2_scene_adapter.c" "$here/prototype/test_scene_adapter.c" "${units[@]}" -lm -o "$out/test-scene-adapter"
"$out/test-scene-adapter"
cc -O2 -ffp-contract=off -D_start=native_scene_entry -I"$here/prototype" -I"$source_dir" "$here/scripts/native_adapter_probe.c" "$here/scripts/native_scene_host_shim.c" "$here/prototype/mhi2_scene_adapter.c" "${units[@]}" -lm -o "$out/native-adapter-host"
MHI2_HOST_PCM="$out/adapter-reference.pcm" "$out/native-adapter-host"
cc -O2 -ffp-contract=off -I"$here/prototype" -I"$source_dir" -DMHI2_EXPERIMENTAL_AIRPLAY_CONFIGURE -DFIXTURE_DEVICE=\"MPL1\" \
 -Ddlopen=fixture_dlopen -Ddlsym=fixture_dlsym \
 -Dnvaudio_pcm_open=fixture_open -Dnvaudio_set_pcm_info=fixture_config -Dnvaudio_pcm_write=fixture_write -Dnvaudio_pcm_close=fixture_close \
 "$here/prototype/nvaudio_upmix_fixture.c" "$here/prototype/test_nvaudio_fixture.c" "$here/prototype/mhi2_scene_adapter.c" "${units[@]}" -lm -o "$out/test-nvaudio-fixture"
"$out/test-nvaudio-fixture"
cc -O2 -ffp-contract=off -I"$here/prototype" -I"$source_dir" -DMHI2_EXPERIMENTAL_AIRPLAY_CONFIGURE -DMHI2_AUDIO_SETTINGS -DFIXTURE_DEVICE=\"MPL1\" \
 -Ddlopen=fixture_dlopen -Ddlsym=fixture_dlsym -Dopen=fixture_file_open -Dread=fixture_file_read -Dclose=fixture_file_close \
 -Dnvaudio_pcm_open=fixture_open -Dnvaudio_set_pcm_info=fixture_config -Dnvaudio_pcm_write=fixture_write -Dnvaudio_pcm_close=fixture_close \
 "$here/prototype/nvaudio_upmix_fixture.c" "$here/prototype/test_audio_settings.c" "$here/prototype/mhi2_scene_adapter.c" "${units[@]}" -lm -o "$out/test-audio-settings"
"$out/test-audio-settings"
