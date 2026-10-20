#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
: "${QNX_RUNTIME:=/mnt/data/qnx65-armv7-gcc49-runtime}"
[ -f "$QNX_RUNTIME/activate.sh" ] || { echo 'Missing saved portable QNX toolchain' >&2; exit 1; }
# shellcheck source=/dev/null
source "$QNX_RUNTIME/activate.sh"
qcc -O2 -std=gnu99 -mfloat-abi=softfp -Wall -Wextra -Wl,--no-undefined \
 "$HERE/v8k_dsitracer_capture.c" -lsocket -lc -o "$HERE/v8k-dsitracer-capture"
file "$HERE/v8k-dsitracer-capture"
