#!/usr/bin/env bash
# K3346 Etap 5.3: narzedzie pobierania okladki (coverart/k3346_coverart_fetch.c).
# Budowanie: tools/qnx_build.sh coverart/build_coverart_fetch.sh coverart/k3346-coverart-fetch
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
: "${QNX_RUNTIME:=/mnt/data/qnx65-armv7-gcc49-runtime}"
. "$QNX_RUNTIME/activate.sh"
qcc -O2 -std=gnu99 -mfloat-abi=softfp -Wall -Wextra -Werror -Wl,--no-undefined \
 "$ROOT/k3346_coverart_fetch.c" -lc -o "$ROOT/k3346-coverart-fetch"
file "$ROOT/k3346-coverart-fetch"
sha256sum "$ROOT/k3346-coverart-fetch"
