#!/bin/bash
# Run every documented host scenario in one go; stops at the first failure. Host only (no emulator, no vehicle).
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
cd "$here"
# Bootstrap (needs ./deps from scripts/fetch_deps.sh and ffmpeg): host FDK decoder and the synthetic AAC fixture
[ -f artifacts/fdk-decoder-host/raw-aac-fixture.bin ] || { python3 scripts/build_fdk_decoder.py host; python3 scripts/test_fdk_decoder.py; }
[ -f artifacts/fdk-decoder-host/raw-aac-44k-fixture.bin ] || python3 scripts/make_aac44_fixture.py
bash scripts/test_buffered_nvidia_pipeline.sh
bash scripts/test_nv_hook_detached.sh
bash scripts/test_nv_stock_bridge.sh
bash scripts/test_nv_local_bridge.sh
bash scripts/test_nv_inert.sh
python3 scripts/test_fdk_bridge.py
echo "ALL HOST SCENARIOS PASS"
