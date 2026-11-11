#!/bin/bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
ref="$here/reference-audio-a63d547"
out="$here/artifacts/native-nvidia-sink"
mkdir -p "$out"
# Only the TEST codec-loader boundary changes. Production reference files stay intact.
python3 - "$ref" "$out" <<'PY'
from pathlib import Path
import sys
ref,out=map(Path,sys.argv[1:]);s=(ref/'tests/audio_interposer_test.c').read_text()
for old,new in [('#include "../hook/audio/audio_hook.c"','#include "'+str(ref/'hook/audio/audio_hook.c')+'"'),('#include "../hook/framework/airplay_seams.c"','#include "'+str(ref/'hook/framework/airplay_seams.c')+'"'),('const spa_apac_codec_api_t *spa_apac_runtime_load(const char **path)','static const spa_apac_codec_api_t *test_runtime_codec;\nconst spa_apac_codec_api_t *spa_apac_runtime_load(const char **path)'),('fault_pause(2);\n    return NULL;','fault_pause(2);\n    return test_runtime_codec;')]:
 assert s.count(old)==1,old;s=s.replace(old,new)
(out/'reference_interposer_with_codec.c').write_text(s)
PY
c++ -std=c++11 -O2 -ffp-contract=off -fno-exceptions -fno-rtti -fPIC -shared \
 -I"$ref/apac_decoder/include" -I"$ref/hook/audio" \
 "$ref"/apac_decoder/src/*.cpp "$ref/hook/audio/apac_bridge.cpp" -o "$out/libpipeline_apac.dylib"
fdk_includes=()
for dir in ${MHI2_FDK_SRC:-$here/deps/fdk-aac}/cpp/*/include;do fdk_includes+=(-isystem "$dir");done
cc -std=gnu99 -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function \
 -I"$ref/tests/mocks" -I"$ref/hook" -I"$ref/hook/audio" -I"$here/prototype" "${fdk_includes[@]}" \
 "$here/prototype/test_buffered_nvidia_pipeline.c" "$here/prototype/nvidia_receiver_pump.c" "$here/prototype/nvidia_pcm_sink.c" "$here/prototype/fdk_aac_bridge.c" \
 "$ref/hook/audio/stereo_upmix.c" "$ref/hook/audio/soundcheck.c" \
 "$ref/hook/audio/scene_engine_math.c" "$ref/hook/audio/scene_clusters.c" "$ref/hook/audio/scene_engine.c" "$ref/hook/audio/scene_mask.c" "$ref/hook/audio/scene_mask_worker.c" \
 "$ref/hook/audio/spatial_downmix.c" "$ref/hook/audio/apap_lifecycle.c" "$ref/hook/audio/apap_crypto.c" "$ref/hook/framework/iap2_protocol.c" \
 "$out/libpipeline_apac.dylib" "$here/artifacts/fdk-decoder-host/libmhi2_fdk.dylib" \
 -lpthread -lm -o "$out/test-buffered-nvidia-pipeline"
"$out/test-buffered-nvidia-pipeline" "$ref/apac_decoder/tests/fixtures/frames.bin" "$here/artifacts/fdk-decoder-host/raw-aac-fixture.bin"
