#!/bin/bash
# Host: fail-closed 6ch gate (QUALIFIED=0 inert, QUALIFIED=1 control).
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
ref="$here/reference-audio-a63d547"
out="$here/artifacts/native-nvidia-sink"
mkdir -p "$out"
python3 "$here/scripts/prepare_full_audio_port.py" >/dev/null
fdk_includes=()
for dir in ${MHI2_FDK_SRC:-$here/deps/fdk-aac}/cpp/*/include;do fdk_includes+=(-isystem "$dir");done
for q in 0 1; do
cc -std=gnu99 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -DMHI2_NV_SIX_CHANNEL_QUALIFIED=$q -DMHI2_NV_STOCK_BRIDGE \
 -I"$ref/tests/mocks" -I"$ref/hook" -I"$ref/hook/audio" -I"$ref/hook/framework" -I"$here/prototype" "${fdk_includes[@]}" \
 "$here/prototype/test_nv_inert.c" "$here/prototype/nvidia_pcm_sink.c" "$here/prototype/fdk_aac_bridge.c" \
 "$ref/hook/audio/stereo_upmix.c" "$ref/hook/audio/soundcheck.c" "$ref/hook/audio/scene_engine_math.c" "$ref/hook/audio/scene_clusters.c" "$ref/hook/audio/scene_engine.c" "$ref/hook/audio/scene_mask.c" "$ref/hook/audio/scene_mask_worker.c" \
 "$ref/hook/audio/spatial_downmix.c" "$ref/hook/audio/apap_lifecycle.c" "$ref/hook/audio/apap_crypto.c" "$ref/hook/framework/iap2_protocol.c" \
 "$out/libpipeline_apac.dylib" "$here/artifacts/fdk-decoder-host/libmhi2_fdk.dylib" -lpthread -lm -o "$out/test-nv-inert-q$q"
"$out/test-nv-inert-q$q"
done
