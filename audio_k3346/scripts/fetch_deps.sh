#!/usr/bin/env bash
# Fetch the pinned public sources the audio build needs. Reads nothing from a car or firmware.
# Result: ./reference-audio-a63d547, ./upstream, ./deps/openqnx, ./deps/fdk-aac. The ARM toolchain is not downloaded:
# install xpack arm-none-eabi-gcc 13.3.1-1.1 and export MHI2_TOOLCHAIN=<its directory> (see REPRODUCE.md).
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
cd "$here"
mkdir -p deps
LUKA=https://github.com/luka-dev/mib2q-carplay-rgi.git
LUKA_A=a63d54741839c7d30d32dd4e149f693856bcb0db   # reference-audio-a63d547
LUKA_U=10fec32079477b31e03683aee453eb4faf21442b   # upstream (older pin used by some build scripts)
FDK=https://github.com/mstorsjo/fdk-aac.git
FDK_C=c41c3c7516ea8d0d78a53b96b30bed3a29745cc6
QNX=https://github.com/vocho/openqnx.git
QNX_C=cc95df35

export_luka() { # <commit> <dest> <paths...>
  local c=$1 d=$2; shift 2
  [ -d "$d" ] && { echo "keep $d"; return; }
  local t; t=$(mktemp -d)
  git -C "$t" init -q
  git -C "$t" fetch -q --depth 1 "$LUKA" "$c"
  git -C "$t" checkout -q FETCH_HEAD -- "$@"
  mkdir -p "$d"
  for p in "$@"; do mkdir -p "$d/$(dirname "$p")"; cp -R "$t/$p" "$d/$p"; done
  rm -rf "$t"
}
export_luka "$LUKA_A" reference-audio-a63d547 hook/audio hook/framework hook/coverart hook/carplay_hook.exports.map apac_decoder models docs/audio tests scripts/test_audio_interposers.sh scripts/test_hook_audio.sh LICENSE NOTICE.md
rm -rf reference-audio-a63d547/apac_decoder/build
export_luka "$LUKA_U" upstream hook/audio apac_decoder LICENSE NOTICE.md
# upstream/ keeps the layout upstream/hook/... and upstream/apac_decoder/...

if [ ! -d deps/fdk-aac ]; then
  git init -q deps/fdk-aac
  git -C deps/fdk-aac fetch -q --depth 1 "$FDK" "$FDK_C"
  git -C deps/fdk-aac checkout -q FETCH_HEAD
  echo "fdk-aac -> $FDK_C"
fi

if [ ! -d deps/openqnx ]; then
  git clone -q --filter=blob:none --no-checkout --sparse "$QNX" deps/openqnx
  git -C deps/openqnx sparse-checkout set trunk/lib/c/public trunk/services/system/public
  git -C deps/openqnx checkout -q "$QNX_C"
  echo "openqnx -> $(git -C deps/openqnx rev-parse HEAD)"
fi
echo "done. Now set MHI2_TOOLCHAIN (see REPRODUCE.md)."
