#!/usr/bin/env bash
# Builds the Etap 7a calibration probe (probe/vc_overlay_probe.c).
#
# Run it through tools/qnx_build.sh, not directly on Windows:
#   bash tools/qnx_build.sh overlay/build_probe.sh overlay/output/vc_overlay_probe
# (QNX binutils are 32-bit and choke on Docker bind-mount inodes; qnx_build.sh
#  copies the tree into the container filesystem first - see its header.)
set -euo pipefail
D="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
: "${QNX_RUNTIME:=/mnt/data/qnx65-armv7-gcc49-runtime}"
if [ ! -f "$QNX_RUNTIME/activate.sh" ]; then
  echo "FAIL: QNX GCC 4.9.4 runtime not found at $QNX_RUNTIME" >&2; exit 2
fi
# shellcheck source=/dev/null
source "$QNX_RUNTIME/activate.sh"

mkdir -p "$D/output"

FLAGS=(-mfloat-abi=softfp -O2 -g -std=gnu99 -Wall -Wextra -I"$D")

# WHY --allow-shlib-undefined:
# The SDP's libKD.so is the reference io-winmgr implementation and is itself
# linked against libiow.so.  The car's libKD.so is NVIDIA's, self-contained,
# and there is NO libiow on the unit - adding -liow would satisfy the host
# linker and then fail to load in the car.  So we take the kd* symbols from
# the SDP library for type checking, keep DT_NEEDED=libKD.so, and let the
# car's loader resolve libKD's own internals at runtime.
# tools/verify_overlay_elf.py asserts libiow never sneaks into NEEDED.
qcc "${FLAGS[@]}" \
  -o "$D/output/vc_overlay_probe" \
  "$D/probe/vc_overlay_probe.c" "$D/kd_surface.c" \
  -lKD -lEGL -lGLESv2 -lm \
  -Wl,--allow-shlib-undefined

file "$D/output/vc_overlay_probe"
echo "PROBE_BUILD=OK"
