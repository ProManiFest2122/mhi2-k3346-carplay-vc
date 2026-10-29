#!/bin/bash
# Run every documented host scenario in one go; stops at the first failure. Host only (no emulator, no vehicle).
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
cd "$here"
bash scripts/test_buffered_nvidia_pipeline.sh
bash scripts/test_nv_hook_detached.sh
bash scripts/test_nv_stock_bridge.sh
bash scripts/test_nv_local_bridge.sh
bash scripts/test_nv_inert.sh
python3 scripts/test_fdk_bridge.py
echo "ALL HOST SCENARIOS PASS"
