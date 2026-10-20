#!/usr/bin/env bash
# K3346 Etap 5.3: sonda okladki (diagnostics/k3346_artwork_probe.c).
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
: "${QNX_RUNTIME:=/mnt/data/qnx65-armv7-gcc49-runtime}"
. "$QNX_RUNTIME/activate.sh"
qcc -O2 -std=gnu99 -mfloat-abi=softfp -Wall -Wextra -Werror -Wl,--no-undefined \
 "$ROOT/k3346_artwork_probe.c" -lc -o "$ROOT/k3346-artwork-probe"
file "$ROOT/k3346-artwork-probe"
sha256sum "$ROOT/k3346-artwork-probe"
