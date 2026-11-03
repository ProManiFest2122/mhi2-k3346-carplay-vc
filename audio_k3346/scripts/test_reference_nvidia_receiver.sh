#!/bin/bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
ref="$here/reference-audio-a63d547"
out="$here/artifacts/native-nvidia-sink"
mkdir -p "$out"
cc -std=gnu99 -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function \
 -I"$ref/tests/mocks" -I"$ref/hook" -I"$here/prototype" \
 "$here/prototype/test_reference_nvidia_receiver.c" "$here/prototype/nvidia_receiver_pump.c" "$here/prototype/nvidia_pcm_sink.c" \
 "$ref/hook/audio/stereo_upmix.c" "$ref/hook/audio/soundcheck.c" \
 "$ref/hook/audio/scene_engine_math.c" "$ref/hook/audio/scene_clusters.c" "$ref/hook/audio/scene_engine.c" "$ref/hook/audio/scene_mask.c" "$ref/hook/audio/scene_mask_worker.c" \
 "$ref/hook/audio/spatial_downmix.c" "$ref/hook/audio/apap_lifecycle.c" "$ref/hook/audio/apap_crypto.c" "$ref/hook/audio/aac_bridge.c" "$ref/hook/framework/iap2_protocol.c" \
 -lpthread -lm -o "$out/test-reference-nvidia-receiver"
"$out/test-reference-nvidia-receiver"
