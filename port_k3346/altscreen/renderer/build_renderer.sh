#!/usr/bin/env sh
# Compile the tracked upstream C source; this does NOT claim exact binary equivalence to v8S.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${QNX_HOST:?Set QNX_HOST for your licensed QNX SDP}"
: "${QNX_TARGET:?Set QNX_TARGET for your licensed QNX SDP}"
command -v qcc >/dev/null 2>&1 || { echo "qcc missing" >&2; exit 2; }
mkdir -p "$HERE/build"
qcc -Vgcc_ntoarmv7le -mfloat-abi=softfp -O2 -g -std=gnu99 -Wall -Wextra \
  -o "$HERE/build/vc_render" "$HERE/upstream/vc_render.c" -lsocket
echo "Built $HERE/build/vc_render (unverified against historical v8S binary)"
