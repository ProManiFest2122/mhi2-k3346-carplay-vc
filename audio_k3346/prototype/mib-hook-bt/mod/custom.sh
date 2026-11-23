#!/bin/sh
# M.I.B. GEM entry point; SD1. No GLOBALS sourced and no automatic reboot.
PACKAGE=/net/mmx/fs/sda0/mhi2-audio-bt
[ -f "$PACKAGE/scripts/bt.sh" ] || { echo 'Missing mhi2-audio-bt package on SD1'; exit 1; }
on -f mmx /bin/sh "$PACKAGE/scripts/bt.sh"
STATUS=$?
echo "MHI2 audio BT exit code: $STATUS. Read mhi2-audio-bt/logs; outer M.I.B. Finished is not a verdict."
exit "$STATUS"
