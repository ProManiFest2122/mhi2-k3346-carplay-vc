#!/usr/bin/env bash
# Builds vc_overlay - the K3346 cluster overlay renderer (Etap 7b).
#
#   bash tools/qnx_build.sh overlay/build_overlay.sh overlay/output/vc_overlay
#
# The renderer is Luka's maneuver_render with ONE file replaced: his
# platform_qnx.c + common/cluster_surface.c (QNX Screen) become our
# platform_harman.c + kd_surface.c (OpenKODE).  Everything under upstream/ is
# his code; see PROVENANCE.md for the exact list and the one upstream line we
# had to make overridable.
#
# BOTH platform macros are defined on purpose:
#   PLATFORM_QNX     - his sources use it for "this is QNX": GLES2 headers,
#                      setprio(), and skipping the macOS-only frame pacing.
#                      All of that is true on K3346 too.
#   PLATFORM_HARMAN  - selects OUR platform file; his platform_qnx.c is simply
#                      not in the source list.
# Defining only PLATFORM_HARMAN would send gl_compat.h down its #error path.
set -euo pipefail
D="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
: "${QNX_RUNTIME:=/mnt/data/qnx65-armv7-gcc49-runtime}"
if [ ! -f "$QNX_RUNTIME/activate.sh" ]; then
  echo "FAIL: QNX GCC 4.9.4 runtime not found at $QNX_RUNTIME" >&2; exit 2
fi
# shellcheck source=/dev/null
source "$QNX_RUNTIME/activate.sh"

rm -rf "$D/build"
mkdir -p "$D/build" "$D/output"

DEFS=(-DPLATFORM_QNX -DPLATFORM_HARMAN
      -DGLPC_DIR='"/var/app/icab/tmp/k3346_overlay_glpc"')
INC=(-I"$D" -I"$D/upstream" -I"$D/upstream/common")
COMMON=(-mfloat-abi=softfp -O2 -g -Wall -Wextra "${DEFS[@]}" "${INC[@]}")
CFLAGS=(-std=gnu99 "${COMMON[@]}")
# -fno-exceptions -fno-rtti: the scene compiler must not pull in the C++
# runtime (checked below), the same constraint Luka builds it under.
CXXFLAGS=(-std=c++11 -fno-exceptions -fno-rtti
          -include "$D/scene_compat_qnx65.hpp"
          "${COMMON[@]}")

C_SRC=(upstream/main.c upstream/maneuver.c upstream/render.c
       upstream/route_path.c upstream/server.c
       platform_harman.c kd_surface.c)
CXX_SRC=(upstream/scene/scene.cpp upstream/scene/geometry.cpp
         upstream/scene/layout.cpp upstream/scene/lane_panel.cpp)

OBJ=()
for src in "${C_SRC[@]}"; do
  name="${src//\//__}"; obj="$D/build/${name%.c}.o"
  qcc "${CFLAGS[@]}" -c "$D/$src" -o "$obj"
  OBJ+=("$obj")
done
for src in "${CXX_SRC[@]}"; do
  name="${src//\//__}"; obj="$D/build/${name%.cpp}.o"
  # the qcc shim routes .cpp to g++ by extension; linking stays on the C
  # driver below so libstdc++ never gets pulled in
  qcc "${CXXFLAGS[@]}" -c "$D/$src" -o "$obj"
  OBJ+=("$obj")
done

# See build_probe.sh for why --allow-shlib-undefined is required here.
qcc "${CFLAGS[@]}" -o "$D/output/vc_overlay" "${OBJ[@]}" \
  -lKD -lEGL -lGLESv2 -lsocket -lm \
  -Wl,--allow-shlib-undefined

# The scene engine must not drag in libstdc++/libcpp: the unit has no C++
# runtime for us to rely on.  Same guard Luka runs in scripts/build_renderers.sh.
NM="${QCC_TARGET:-arm-unknown-nto-qnx6.5.0eabi}-nm"
command -v "$NM" >/dev/null 2>&1 || NM=nm
if "$NM" -u "$D/output/vc_overlay" | grep -E '__cxa|_ZSt|_ZTI|_ZTV|_Zn[aw]|_Zd[al]|gxx_personality'; then
  echo "FAIL: C++ runtime symbols leaked into vc_overlay" >&2
  exit 3
fi

file "$D/output/vc_overlay"
echo "OVERLAY_BUILD=OK"
