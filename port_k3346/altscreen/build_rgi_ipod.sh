#!/usr/bin/env bash
# K3346 Etap 4: biblioteka TYLKO z RGI, ladowana (LD_PRELOAD) wylacznie do stock
# mm-ipod przez launcher deployment/etap4/mm-ipod-rgi.sh.
#
# Dlaczego osobna biblioteka, a nie libmib2high-carplay-rgi.so z dio_manager:
#  - konstruktor AltScreen (altscreen_output.c) kasuje /tmp/mib2high-lvds-active
#    w KAZDYM procesie, ktory go zaladuje; mm-ipod startuje przy kazdym
#    podlaczeniu iPhone'a, wiec moglby zgasic mapie na VC w trakcie sesji;
#  - kod AirPlay nie ma w mm-ipod nic do roboty.
# Zrodla RGI nie zaleza od altscreen_*.c (to ten sam podzial co RGI v1.1 edzenisa).
# Logi maja wlasne nazwy, zeby nie obcinac plikow kopii z dio_manager.
set -euo pipefail
D="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
: "${QNX_RUNTIME:=/mnt/data/qnx65-armv7-gcc49-runtime}"
if [ ! -f "$QNX_RUNTIME/activate.sh" ]; then
  echo "FAIL: QNX GCC 4.9.4 runtime not found at $QNX_RUNTIME" >&2; exit 2
fi
# shellcheck source=/dev/null
source "$QNX_RUNTIME/activate.sh"
G="$D/generated-rgi-ipod"
rm -rf "$G"
mkdir -p "$G" "$D/output"
SRC=(observer.c ident_hook.c message_table.c send_hook.c receive_probe.c status.c rgi_bridge.c diagnostics.c framework/logging.c framework/bus.c framework/iap2_protocol.c routeguidance/rgd_tlv.c routeguidance/rgd_hook.c)
FLAGS=(-fPIC -mfloat-abi=softfp -O2 -std=gnu99 -include stddef.h -I"$D/native" -I"$D/native/framework" -I"$D/native/routeguidance")
PATHS=(
  '-DK2161_RX_BIN_PATH="/dev/shmem/k3346-rgi-ipod-rx.bin"'
  '-DK2161_RX_LOG_PATH="/dev/shmem/k3346-rgi-ipod-rx.log"'
  '-DMIB2HIGH_RGI_DIAGNOSTICS_PATH="/dev/shmem/k3346-rgi-ipod-native.log"'
  '-DMIB2HIGH_HOOK_LOG_PATH="/tmp/k3346-rgi-ipod-hook.log"'
)
OBJ=()
for src in "${SRC[@]}"; do
  name="${src//\//__}"; name="${name%.c}"
  obj="$G/$name.o"
  qcc "${FLAGS[@]}" "${PATHS[@]}" -c "$D/native/$src" -o "$obj"
  OBJ+=("$obj")
done
qcc -shared "${FLAGS[@]}" "${OBJ[@]}" -lsocket -lc -Wl,--no-undefined \
  -o "$D/output/libmib2high-carplay-rgi-ipod.so"
python3 "$D/tools/verify_rgi_ipod.py" "$D/output/libmib2high-carplay-rgi-ipod.so"
