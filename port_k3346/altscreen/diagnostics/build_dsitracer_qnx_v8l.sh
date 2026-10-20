#!/usr/bin/env bash
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
: "${QNX_RUNTIME:=/mnt/data/qnx65-armv7-gcc49-runtime}"
. "$QNX_RUNTIME/activate.sh"
qcc -O2 -std=gnu99 -mfloat-abi=softfp -Wall -Wextra -Werror -Wl,--no-undefined \
 "$ROOT/v8l_dsitracer_capture.c" -lsocket -lc -o "$ROOT/v8l-dsitracer-capture"
file "$ROOT/v8l-dsitracer-capture"
sha256sum "$ROOT/v8l-dsitracer-capture"
