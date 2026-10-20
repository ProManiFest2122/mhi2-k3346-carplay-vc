#!/usr/bin/env bash
set -euo pipefail
D="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
: "${QNX_RUNTIME:=/mnt/data/qnx65-armv7-gcc49-runtime}"
if [ ! -f "$QNX_RUNTIME/activate.sh" ]; then
  echo "FAIL: QNX GCC 4.9.4 runtime not found at $QNX_RUNTIME" >&2; exit 2
fi
# shellcheck source=/dev/null
source "$QNX_RUNTIME/activate.sh"
rm -rf "$D/generated"
mkdir -p "$D/generated" "$D/output"
SRC=(observer.c altscreen_output.c altscreen_airplay.c altscreen_session.c ident_hook.c message_table.c send_hook.c receive_probe.c status.c rgi_bridge.c diagnostics.c framework/logging.c framework/bus.c framework/iap2_protocol.c routeguidance/rgd_tlv.c routeguidance/rgd_hook.c)
FLAGS=(-fPIC -mfloat-abi=softfp -O2 -std=gnu99 -include stddef.h -I"$D/native" -I"$D/native/framework" -I"$D/native/routeguidance")
OBJ=()
for src in "${SRC[@]}"; do
  name="${src//\//__}"; name="${name%.c}"
  obj="$D/generated/$name.o"
  qcc "${FLAGS[@]}" -c "$D/native/$src" -o "$obj"
  OBJ+=("$obj")
done
echo "V8S_SOURCE_ONLY_DIRECT_C_BUILD=1"
qcc -shared "${FLAGS[@]}" "${OBJ[@]}" -lsocket -lc -Wl,--no-undefined \
  -o "$D/output/libmib2high-carplay-rgi.so"
# Historical parity against unpublished v8g reference is OPTIONAL, not a public build requirement.
# Verify the output is a QNX ARM ELF and has expected exported names, without private binaries.
python3 "$D/tools/verify_current_native.py" "$D/output/libmib2high-carplay-rgi.so"
if [ "${V8I_HISTORICAL_PARITY:-0}" = "1" ]; then
  python3 "$D/tools/verify_v8i.py"
fi
