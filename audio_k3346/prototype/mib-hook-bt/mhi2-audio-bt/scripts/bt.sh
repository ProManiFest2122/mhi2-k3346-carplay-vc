#!/bin/sh
# Exact K3346 Bluetooth/USB/SD audio hook installer (INERT build). ACTION defaults CHECK.
# Touches only mm-renderer (shim + saved original) and /mnt/app/eso/lib/mhi2bt. Never datasets/IFS/HMI/CarPlay files.
set -u
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/usr/bin
LD_LIBRARY_PATH=/lib:/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/usr/lib
export PATH LD_LIBRARY_PATH
unset LD_PRELOAD
case $0 in */*) PACKAGE=${0%/scripts/bt.sh} ;; *) exit 1 ;; esac
APP=/mnt/app
SHA=/net/mmx/fs/sda0/apps/sbin/sha1sum
STATE=$APP/root/mhi2-audio-bt
LOCK=/tmp/mhi2-audio-bt.lock
MUTATING=0
[ ! -L "$STATE" ] && [ ! -L "$STATE/baseline" ] || exit 1
[ -x "$SHA" ] || { echo 'Missing M.I.B. sha1sum'; exit 1; }
[ "$(uname -s)" = QNX ] || { echo 'Must execute on MMX/QNX'; exit 1; }
[ ! -L "$PACKAGE/logs" ] || exit 1
mkdir -p "$PACKAGE/logs" || exit 1
LOG=$PACKAGE/logs/run-$$.log
[ ! -e "$LOG" ] && [ ! -L "$LOG" ] || exit 1
: > "$LOG" || exit 1
say(){ echo "$*"; echo "$*" >> "$LOG"; }
die(){ say "ERROR: $*"; [ "$MUTATING" = 0 ] || say 'Do not reboot; select RESTORE with the same SD package.'; exit 1; }
hash(){
 [ -f "$1" ] && [ ! -L "$1" ] || return 1
 H=$("$SHA" "$1") || return 1
 set -- $H
 case ${1:-} in ''|*[!0-9a-f]*) return 1 ;; esac
 [ "${#1}" = 40 ] || return 1
 echo "$1"
}
equal(){ A=$(hash "$1") && B=$(hash "$2") && [ "$A" = "$B" ]; }
copy_checked(){ [ ! -L "$2" ] && [ ! -d "$2" ] || die 'Unsafe copy destination'; cp -p "$1" "$2" || die 'Copy failed'; equal "$1" "$2" || die 'Copy mismatch'; }
activate(){ copy_checked "$1" "$2.audio-new"; mv "$2.audio-new" "$2" || die 'Atomic activation failed'; equal "$1" "$2" || die 'Readback mismatch'; }
mkdir "$LOCK" 2>/dev/null || die 'Another operation or stale lock exists'
trap 'rmdir "$LOCK" 2>/dev/null' 0
trap 'exit 1' 1 2 15
ACTION=$(cat "$PACKAGE/ACTION.txt") || die 'Missing ACTION.txt'
case $ACTION in CHECK|INSTALL_PREVIEW|RESTORE) ;; *) die 'Unknown action' ;; esac
say "MHI2 K3346 audio BT/USB/SD PREVIEW / $ACTION"
if [ "$ACTION" != CHECK ]; then
 echo CHECK > "$PACKAGE/ACTION.txt.new" || die 'Cannot reset SD action'
 mv "$PACKAGE/ACTION.txt.new" "$PACKAGE/ACTION.txt" || die 'Cannot reset SD action'
fi
# The package is executable data: verify every shipped file before mutation.
COUNT=0
SEEN="|"
while read -r EXPECTED REL; do
 case $REL in payload/libmhi2_audio_hook.so|payload/libmhi2_apac.so|payload/libmhi2_aac.so|payload/libmhi2_fdk.so|payload/mm-renderer-shim.sh|stock/mm-renderer|target/core.sha1|scripts/bt.sh|HMI.sha1) ;; *) die 'Unexpected manifest path' ;; esac
 case $SEEN in *"|$REL|"*) die 'Duplicate manifest entry' ;; esac
 SEEN=$SEEN$REL'|'
 ACTUAL=$(hash "$PACKAGE/$REL") || die "Missing package file: $REL"
 [ "$ACTUAL" = "$EXPECTED" ] || die "Package hash mismatch: $REL"
 COUNT=$((COUNT+1))
done < "$PACKAGE/SHA1SUMS"
[ "$COUNT" = 9 ] || die 'Incomplete package manifest'
CORE_COUNT=0
CORE_SEEN="|"
while read -r EXPECTED REL; do
 case $REL in armle/usr/sbin/mm-renderer|armle/usr/lib/libnvaudio.so|root/bin-target/deva-nvaudio) ;; *) die 'Unexpected core fingerprint path' ;; esac
 case $CORE_SEEN in *"|$REL|"*) die 'Duplicate core entry' ;; esac
 CORE_SEEN=$CORE_SEEN$REL'|'
 case $REL in armle/usr/sbin/mm-renderer) [ "$EXPECTED" = "$(hash "$PACKAGE/stock/mm-renderer")" ] || die 'Package stock mm-renderer mismatch' ;; esac
 # An interrupted install/restore may leave the shim at mm-renderer; RESTORE accepts that, others do not.
 if [ "$REL" = armle/usr/sbin/mm-renderer ] && [ "$ACTION" = RESTORE ]; then CORE_COUNT=$((CORE_COUNT+1)); continue; fi
 if [ "$REL" = armle/usr/sbin/mm-renderer ] && [ -f "$STATE/baseline/COMPLETE" ] && [ "$ACTION" = CHECK ]; then CORE_COUNT=$((CORE_COUNT+1)); continue; fi
 [ "$(hash "$APP/$REL")" = "$EXPECTED" ] || die "Unsupported core binary: $REL"
 CORE_COUNT=$((CORE_COUNT+1))
done < "$PACKAGE/target/core.sha1"
[ "$CORE_COUNT" = 3 ] || die 'Incomplete core fingerprints'
[ "$(hash /ifs/lsd.jxe)" = "$(cat "$PACKAGE/HMI.sha1")" ] || die 'Unsupported HMI JXE'
BIN=$APP/armle/usr/sbin/mm-renderer
LIBDIR=$APP/eso/lib/mhi2bt
REALDIR=$LIBDIR/real
REAL=$REALDIR/mm-renderer   # same basename as stock: anything matching the process by name keeps working
LIBNAMES="libmhi2_audio_hook.so libmhi2_apac.so libmhi2_aac.so libmhi2_fdk.so"
no_audio_files(){ [ ! -e "$REAL" ] && [ ! -L "$REAL" ] && [ ! -e "$LIBDIR" ] && [ ! -L "$LIBDIR" ]; }
for DIR in "$APP/root" "$APP/eso/lib" "${BIN%/*}"; do
 [ -d "$DIR" ] && [ ! -L "$DIR" ] || die "Unsafe directory: $DIR"
done
[ ! -L "$BIN" ] || die 'Unsafe mm-renderer symlink'
if [ "$ACTION" = RESTORE ]; then
 [ -f "$STATE/baseline/COMPLETE" ] && [ ! -L "$STATE/baseline/COMPLETE" ] || die 'No complete baseline'
 [ "$(cat "$STATE/baseline/COMPLETE")" = ORIGINAL_MM_RENDERER_AND_ABSENT_AUDIO_FILES ] || die 'Unknown baseline marker'
 equal "$STATE/baseline/mm-renderer" "$PACKAGE/stock/mm-renderer" || die 'Corrupt mm-renderer backup'
 equal "$BIN" "$PACKAGE/stock/mm-renderer" || equal "$BIN" "$PACKAGE/payload/mm-renderer-shim.sh" || die 'Unknown mm-renderer changes'
 [ ! -e "$REAL" ] || equal "$REAL" "$PACKAGE/stock/mm-renderer" || die 'Unknown saved renderer'
 [ ! -L "$REAL" ] && [ ! -L "$REALDIR" ] || die 'Unsafe saved renderer symlink'
 [ ! -L "$LIBDIR" ] || die 'Unsafe library directory'
 for N in $LIBNAMES; do
  [ ! -L "$LIBDIR/$N" ] || die 'Unsafe installed symlink'
  [ ! -e "$LIBDIR/$N" ] || equal "$LIBDIR/$N" "$PACKAGE/payload/$N" || die 'Unknown installed audio file'
 done
else
 if [ -f "$STATE/baseline/RESTORED" ]; then
  equal "$BIN" "$PACKAGE/stock/mm-renderer" || die 'Restored baseline changed'
  no_audio_files || die 'Unexpected audio file after restore'
  [ "$ACTION" = CHECK ] || die 'Restored backup preserved; archive it explicitly before a fresh installation'
  say 'CHECK PASS: restored baseline, no active BT audio payload'; exit 0
 fi
 if [ -f "$STATE/baseline/COMPLETE" ]; then
  equal "$BIN" "$PACKAGE/payload/mm-renderer-shim.sh" || die 'Installed shim changed'
  equal "$REAL" "$PACKAGE/stock/mm-renderer" || die 'Saved renderer changed'
  for N in $LIBNAMES; do equal "$LIBDIR/$N" "$PACKAGE/payload/$N" || die "Installed native library changed: $N"; done
  [ "$ACTION" = CHECK ] || die 'Already installed; use RESTORE before a different install'
  say 'CHECK PASS: installed payload hashes match. Preview validation remains required.'; exit 0
 fi
 [ ! -e "$STATE" ] && [ ! -L "$STATE" ] || die 'Incomplete/unknown state; inspect before proceeding'
 equal "$BIN" "$PACKAGE/stock/mm-renderer" || die 'mm-renderer differs from reviewed baseline'
 no_audio_files || die 'Audio files already exist'
fi
SPACE=$(df -k "$APP" | tail -1) || die 'Cannot read free space'
set -- $SPACE
[ "$#" -ge 4 ] || die 'Unknown df format'
case $4 in ''|*[!0-9]*) die 'Unknown free space' ;; esac
MIN_SPACE=20000
[ "$ACTION" != RESTORE ] || MIN_SPACE=2000
[ "$4" -ge "$MIN_SPACE" ] || die "At least $MIN_SPACE KiB free required"
[ "$ACTION" != CHECK ] || { say "CHECK PASS: exact target, payload and space. No app changes."; exit 0; }
mount -uw "$APP" || die 'Cannot remount app read-write'
if [ "$ACTION" = INSTALL_PREVIEW ]; then
 mkdir -p "$STATE/baseline" || die 'Cannot create baseline'
 copy_checked "$BIN" "$STATE/baseline/mm-renderer"
 echo ORIGINAL_MM_RENDERER_AND_ABSENT_AUDIO_FILES > "$STATE/baseline/COMPLETE" || die 'Cannot seal baseline'
 sync || die 'Cannot sync baseline'
 MUTATING=1
 mkdir "$LIBDIR" && mkdir "$REALDIR" || die 'Cannot create library directory'
 for N in $LIBNAMES; do activate "$PACKAGE/payload/$N" "$LIBDIR/$N"; done
 activate "$PACKAGE/stock/mm-renderer" "$REAL"
 for N in $LIBNAMES; do chmod 755 "$LIBDIR/$N" || die 'Cannot set executable permissions'; done
 chmod 755 "$REAL" || die 'Cannot set executable permissions'
 # The shim goes live last, so the stock path always holds a runnable file.
 activate "$PACKAGE/payload/mm-renderer-shim.sh" "$BIN"
 chmod 755 "$BIN" || die 'Cannot set executable permissions'
else
 MUTATING=1
 activate "$STATE/baseline/mm-renderer" "$BIN"
 for F in "$BIN.audio-new" "$REAL.audio-new"; do [ ! -e "$F" ] || rm "$F" || die 'Cannot remove temporary file'; done
 [ ! -e "$REAL" ] || rm "$REAL" || die 'Cannot remove owned renderer copy'
 [ ! -d "$REALDIR" ] || rmdir "$REALDIR" || die 'Cannot remove renderer directory'
 if [ -d "$LIBDIR" ]; then
  for N in $LIBNAMES; do
   [ ! -e "$LIBDIR/$N.audio-new" ] || rm "$LIBDIR/$N.audio-new" || die 'Cannot remove temporary file'
   [ ! -e "$LIBDIR/$N" ] || rm "$LIBDIR/$N" || die 'Cannot remove owned native payload'
  done
  rmdir "$LIBDIR" || die 'Library directory not empty'
 fi
 echo RESTORED > "$STATE/baseline/RESTORED" || die 'Cannot record restore'
fi
sync || die 'Cannot sync changes'
MUTATING=0
say "SUCCESS: $ACTION verified. Takes effect at the next mm-renderer start (reboot). Preserve this SD and baseline."
