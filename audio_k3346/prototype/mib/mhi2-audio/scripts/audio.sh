#!/bin/sh
# Exact K3346 preview installer. ACTION defaults CHECK; never changes datasets/IFS.
set -u
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/usr/bin
LD_LIBRARY_PATH=/lib:/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/usr/lib
export PATH LD_LIBRARY_PATH
unset LD_PRELOAD
case $0 in */*) PACKAGE=${0%/scripts/audio.sh} ;; *) exit 1 ;; esac
APP=/mnt/app
SYSTEM=/mnt/system
SHA=/net/mmx/fs/sda0/apps/sbin/sha1sum
AWK=/net/mmx/fs/sda0/apps/sbin/awk
STATE=$APP/root/mhi2-audio
LOCK=/tmp/mhi2-audio.lock
MUTATING=0
[ ! -L "$STATE" ] && [ ! -L "$STATE/baseline" ] || exit 1
[ -x "$SHA" ] && [ -x "$AWK" ] || { echo 'Missing M.I.B. sha1sum'; exit 1; }
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
say "MHI2 K3346 audio PREVIEW / $ACTION"
if [ "$ACTION" != CHECK ]; then
 echo CHECK > "$PACKAGE/ACTION.txt.new" || die 'Cannot reset SD action'
 mv "$PACKAGE/ACTION.txt.new" "$PACKAGE/ACTION.txt" || die 'Cannot reset SD action'
fi
# The package is executable data: verify every shipped file before mutation.
COUNT=0
SEEN="|"
while read -r EXPECTED REL; do
 case $REL in payload/libmhi2_audio.so|payload/mhi2_audio_ui.jar|payload/lsd.sh|payload/smartphone_integrator.json|payload/lang-stock.zip|stock/lsd.sh|stock/smartphone_integrator.json|stock/lang-stock.zip|target/core.sha1|scripts/audio.sh|HMI.sha1) ;; *) die 'Unexpected manifest path' ;; esac
 case $SEEN in *"|$REL|"*) die 'Duplicate manifest entry' ;; esac
 SEEN=$SEEN$REL'|'
 ACTUAL=$(hash "$PACKAGE/$REL") || die "Missing package file: $REL"
 [ "$ACTUAL" = "$EXPECTED" ] || die "Package hash mismatch: $REL"
 COUNT=$((COUNT+1))
done < "$PACKAGE/SHA1SUMS"
[ "$COUNT" = 11 ] || die 'Incomplete package manifest'
CORE_COUNT=0
CORE_SEEN="|"
while read -r EXPECTED REL; do
 case $REL in eso/lib/libairplay.so|armle/usr/lib/libnvaudio.so|root/bin-target/deva-nvaudio|eso/bin/apps/smartphone_integrator|eso/bin/apps/mediarouter|eso/bin/apps/dio_manager) ;; *) die 'Unexpected core fingerprint path' ;; esac
 case $CORE_SEEN in *"|$REL|"*) die 'Duplicate core entry' ;; esac
 CORE_SEEN=$CORE_SEEN$REL'|'
 [ "$(hash "$APP/$REL")" = "$EXPECTED" ] || die "Unsupported core binary: $REL"
 CORE_COUNT=$((CORE_COUNT+1))
done < "$PACKAGE/target/core.sha1"
[ "$CORE_COUNT" = 6 ] || die 'Incomplete core fingerprints'
[ "$(hash /ifs/lsd.jxe)" = "$(cat "$PACKAGE/HMI.sha1")" ] || die 'Unsupported HMI JXE'
LSD=$APP/eso/hmi/lsd
JSON=$SYSTEM/etc/eso/production/smartphone_integrator.json
LIB=$APP/eso/lib/libmhi2_audio.so
JAR=$LSD/jars/mhi2_audio_ui.jar
for DIR in "$APP/root" "$APP/eso/lib" "$LSD" "$LSD/jars" "${JSON%/*}"; do
 [ -d "$DIR" ] && [ ! -L "$DIR" ] || die "Unsafe directory: $DIR"
done
if [ "$ACTION" = RESTORE ]; then
 [ -f "$STATE/baseline/COMPLETE" ] && [ ! -L "$STATE/baseline/COMPLETE" ] || die 'No complete baseline'
 [ "$(cat "$STATE/baseline/COMPLETE")" = ORIGINALS_AND_ABSENT_AUDIO_FILES ] || die 'Unknown baseline marker' 
 VARIANT=$(cat "$STATE/baseline/variant") || die 'Missing saved variant'
 case $VARIANT in stock) ;; *) die 'Invalid saved variant' ;; esac
 equal "$STATE/baseline/lsd.sh" "$PACKAGE/stock/lsd.sh" || die 'Corrupt startup backup'
 equal "$STATE/baseline/smartphone_integrator.json" "$PACKAGE/stock/smartphone_integrator.json" || die 'Corrupt child config backup'
 equal "$STATE/baseline/lang_data.zip" "$PACKAGE/stock/lang-$VARIANT.zip" || die 'Corrupt language backup'
 equal "$LSD/lsd.sh" "$PACKAGE/stock/lsd.sh" || equal "$LSD/lsd.sh" "$PACKAGE/payload/lsd.sh" || die 'Unknown startup changes'
 equal "$JSON" "$PACKAGE/stock/smartphone_integrator.json" || equal "$JSON" "$PACKAGE/payload/smartphone_integrator.json" || die 'Unknown child config changes'
 equal "$LSD/jars/lang_data.zip" "$PACKAGE/stock/lang-$VARIANT.zip" || equal "$LSD/jars/lang_data.zip" "$PACKAGE/payload/lang-$VARIANT.zip" || die 'Unknown language changes'
 for F in "$LIB" "$JAR"; do
  [ ! -L "$F" ] || die 'Unsafe installed symlink' 
  [ ! -e "$F" ] || equal "$F" "$PACKAGE/payload/${F##*/}" || die 'Unknown installed audio file'
 done
else
 if [ -f "$STATE/baseline/RESTORED" ]; then
  VARIANT=$(cat "$STATE/baseline/variant") || die 'Missing variant'
  case $VARIANT in stock) ;; *) die 'Invalid saved variant' ;; esac
  equal "$LSD/lsd.sh" "$PACKAGE/stock/lsd.sh" && equal "$JSON" "$PACKAGE/stock/smartphone_integrator.json" && equal "$LSD/jars/lang_data.zip" "$PACKAGE/stock/lang-$VARIANT.zip" || die 'Restored baseline changed'
  [ ! -e "$LIB" ] && [ ! -L "$LIB" ] && [ ! -e "$JAR" ] && [ ! -L "$JAR" ] || die 'Unexpected audio file after restore'
  [ "$ACTION" = CHECK ] || die 'Restored backup preserved; archive it explicitly before a fresh installation'
  say 'CHECK PASS: restored baseline, no active audio payload'; exit 0
 fi
 if [ -f "$STATE/baseline/COMPLETE" ]; then
  VARIANT=$(cat "$STATE/baseline/variant") || die 'Missing variant'
  case $VARIANT in stock) ;; *) die 'Invalid saved variant' ;; esac
  equal "$LSD/lsd.sh" "$PACKAGE/payload/lsd.sh" || die 'Installed startup changed'
  equal "$JSON" "$PACKAGE/payload/smartphone_integrator.json" || die 'Installed child config changed'
  equal "$LSD/jars/lang_data.zip" "$PACKAGE/payload/lang-$VARIANT.zip" || die 'Installed language ZIP changed'
  equal "$LIB" "$PACKAGE/payload/libmhi2_audio.so" || die 'Installed native library changed'
  equal "$JAR" "$PACKAGE/payload/mhi2_audio_ui.jar" || die 'Installed UI JAR changed'
  [ "$ACTION" = CHECK ] || die 'Already installed; use RESTORE before a different install'
  say 'CHECK PASS: installed payload hashes match. Preview validation remains required.'; exit 0
 fi
 [ ! -e "$STATE" ] && [ ! -L "$STATE" ] || die 'Incomplete/unknown state; inspect before proceeding'
 equal "$LSD/lsd.sh" "$PACKAGE/stock/lsd.sh" || die 'Startup differs from reviewed baseline'
 equal "$JSON" "$PACKAGE/stock/smartphone_integrator.json" || die 'Child config differs from reviewed baseline'
 VARIANT=
 for V in stock; do equal "$LSD/jars/lang_data.zip" "$PACKAGE/stock/lang-$V.zip" && VARIANT=$V; done
 [ -n "$VARIANT" ] || die 'Language ZIP differs from standalone audio stock baseline'
 [ ! -e "$LIB" ] && [ ! -L "$LIB" ] && [ ! -e "$JAR" ] && [ ! -L "$JAR" ] || die 'Audio files already exist'
fi
SPACE=$(df -k "$APP" | tail -1) || die 'Cannot read free space'
set -- $SPACE
[ "$#" -ge 4 ] || die 'Unknown df format'
case $4 in ''|*[!0-9]*) die 'Unknown free space' ;; esac
MIN_SPACE=40000
[ "$ACTION" != RESTORE ] || MIN_SPACE=8000
[ "$4" -ge "$MIN_SPACE" ] || die "At least$MIN_SPACE KiB free required"
[ "$ACTION" != CHECK ] || { say "CHECK PASS: exact target, $VARIANT languages, payload and space. No app changes."; exit 0; }
# No active CarPlay process may retain a loaded library/config during mutation.
PROCESSES=$(pidin ar) || die 'Cannot verify CarPlay is disconnected'
if echo "$PROCESSES" | "$AWK" '$0 ~ /(^|[\/ ])dio_manager([ ]|$)/ { found=1 } END { exit !found }'; then die 'Disconnect iPhone/CarPlay before install or restore'; fi
unset PROCESSES
SYSTEM_SPACE=$(df -k "$SYSTEM" | tail -1) || die 'Cannot read system space'
set -- $SYSTEM_SPACE
[ "$#" -ge 4 ] || die 'Unknown system df format'
case $4 in ''|*[!0-9]*) die 'Unknown system free space' ;; esac
[ "$4" -ge 256 ] || die 'At least256KiB system space required'
mount -uw "$SYSTEM" || die 'Cannot remount system read-write'
mount -uw "$APP" || die 'Cannot remount app read-write'
if [ "$ACTION" = INSTALL_PREVIEW ]; then
 mkdir -p "$STATE/baseline" || die 'Cannot create baseline'
 copy_checked "$LSD/lsd.sh" "$STATE/baseline/lsd.sh"
 copy_checked "$JSON" "$STATE/baseline/smartphone_integrator.json"
 copy_checked "$LSD/jars/lang_data.zip" "$STATE/baseline/lang_data.zip"
 echo "$VARIANT" > "$STATE/baseline/variant" || die 'Cannot save variant'
 echo ORIGINALS_AND_ABSENT_AUDIO_FILES > "$STATE/baseline/COMPLETE" || die 'Cannot seal baseline'
 sync || die 'Cannot sync baseline'
 MUTATING=1
 activate "$PACKAGE/payload/libmhi2_audio.so" "$LIB"
 activate "$PACKAGE/payload/mhi2_audio_ui.jar" "$JAR"
 activate "$PACKAGE/payload/lang-$VARIANT.zip" "$LSD/jars/lang_data.zip"
 activate "$PACKAGE/payload/lsd.sh" "$LSD/lsd.sh"
 activate "$PACKAGE/payload/smartphone_integrator.json" "$JSON"
 echo FIRST_BOOT_OFF > "$STATE/FIRST_BOOT" || die 'Cannot prepare first boot Off' 
 chmod 755 "$LIB" "$LSD/lsd.sh" || die 'Cannot set executable permissions'
else
 MUTATING=1
 activate "$STATE/baseline/smartphone_integrator.json" "$JSON"
 activate "$STATE/baseline/lsd.sh" "$LSD/lsd.sh"
 activate "$STATE/baseline/lang_data.zip" "$LSD/jars/lang_data.zip"
 # Only remove the two files previously verified as this exact audio payload.
 [ ! -e "$LIB" ] || rm "$LIB" || die 'Cannot remove owned native payload'
 [ ! -e "$JAR" ] || rm "$JAR" || die 'Cannot remove owned UI payload'
 echo RESTORED > "$STATE/baseline/RESTORED" || die 'Cannot record restore'
fi
sync || die 'Cannot sync changes'
MUTATING=0
say "SUCCESS: $ACTION verified. No automatic reboot. Preserve this SD and baseline."
