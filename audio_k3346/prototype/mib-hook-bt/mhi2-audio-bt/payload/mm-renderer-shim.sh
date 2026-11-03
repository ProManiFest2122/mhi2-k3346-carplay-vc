#!/bin/sh
# MHI2 audio BT/USB/SD shim. Fail-open: any missing piece runs the stock mm-renderer unchanged.
LIB=/mnt/app/eso/lib/mhi2bt
REAL=$LIB/real/mm-renderer
BASE=/mnt/app/root/mhi2-audio-bt/baseline/mm-renderer
if [ -f "$LIB/libmhi2_audio_hook.so" ] && [ ! -e /mnt/app/root/mhi2-audio-bt/DISABLE ]; then
 LD_PRELOAD=$LIB/libmhi2_audio_hook.so
 AUDIO_SPATIAL_CODEC_LIB=$LIB/libmhi2_apac.so
 LD_LIBRARY_PATH=$LIB:$LD_LIBRARY_PATH
 export LD_PRELOAD AUDIO_SPATIAL_CODEC_LIB LD_LIBRARY_PATH
fi
[ -x "$REAL" ] || { REAL=$BASE; LD_PRELOAD=; export LD_PRELOAD; unset LD_PRELOAD; }
exec "$REAL" "$@"
