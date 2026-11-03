#!/bin/bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
ref="$here/reference-audio-a63d547"
out="$here/artifacts/native-nvidia-sink"
mkdir -p "$out"
python3 "$here/scripts/prepare_full_audio_port.py" >/dev/null
fdk_includes=()
for dir in ${MHI2_FDK_SRC:-$here/deps/fdk-aac}/cpp/*/include;do fdk_includes+=(-isystem "$dir");done
cc -std=gnu99 -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function \
 -DMHI2_NV_SIX_CHANNEL_QUALIFIED=1 -DSPA_NV_SHIM_DEBUG \
 -I"$ref/tests/mocks" -I"$ref/hook" -I"$ref/hook/audio" -I"$ref/hook/framework" -I"$here/prototype" "${fdk_includes[@]}" \
 "$here/prototype/test_nv_hook_detached.c" "$here/prototype/nvidia_pcm_sink.c" "$here/prototype/fdk_aac_bridge.c" \
 "$ref/hook/audio/stereo_upmix.c" "$ref/hook/audio/soundcheck.c" \
 "$ref/hook/audio/scene_engine_math.c" "$ref/hook/audio/scene_clusters.c" "$ref/hook/audio/scene_engine.c" "$ref/hook/audio/scene_mask.c" "$ref/hook/audio/scene_mask_worker.c" \
 "$ref/hook/audio/spatial_downmix.c" "$ref/hook/audio/apap_lifecycle.c" "$ref/hook/audio/apap_crypto.c" "$ref/hook/framework/iap2_protocol.c" \
 "$out/libpipeline_apac.dylib" "$here/artifacts/fdk-decoder-host/libmhi2_fdk.dylib" \
 -lpthread -lm -o "$out/test-nv-hook-detached"
"$out/test-nv-hook-detached" "$ref/apac_decoder/tests/fixtures/frames.bin" "$here/artifacts/fdk-decoder-host/raw-aac-fixture.bin"
MHI2_NV_SCENARIO=level "$out/test-nv-hook-detached" "$ref/apac_decoder/tests/fixtures/frames.bin" "$here/artifacts/fdk-decoder-host/raw-aac-fixture.bin"
MHI2_NV_SCENARIO=soundcheck "$out/test-nv-hook-detached" "$ref/apac_decoder/tests/fixtures/frames.bin" "$here/artifacts/fdk-decoder-host/raw-aac-fixture.bin"
MHI2_NV_SCENARIO=switch "$out/test-nv-hook-detached" "$ref/apac_decoder/tests/fixtures/frames.bin" "$here/artifacts/fdk-decoder-host/raw-aac-fixture.bin"
AUDIO_SCENE_MASK_BLOB="$ref/models/scene-masknet-c/scene_masknet_c.bin" MHI2_NV_SCENARIO=mask "$out/test-nv-hook-detached" "$ref/apac_decoder/tests/fixtures/frames.bin" "$here/artifacts/fdk-decoder-host/raw-aac-fixture.bin"
NV_FIX44="$here/artifacts/fdk-decoder-host/raw-aac-44k-fixture.bin" MHI2_NV_SCENARIO=rate44 "$out/test-nv-hook-detached" "$ref/apac_decoder/tests/fixtures/frames.bin" "$here/artifacts/fdk-decoder-host/raw-aac-fixture.bin"
