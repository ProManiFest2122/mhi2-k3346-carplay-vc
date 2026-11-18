#!/bin/sh
# Exact K3346 preview installer. ACTION defaults CHECK; never changes datasets/IFS.
set -u
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/bin:/mnt/app/armle/usr/sbin
LD_LIBRARY_PATH=/lib:/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/usr/lib
export PATH LD_LIBRARY_PATH
unset LD_PRELOAD
case $0 in */*) PACKAGE=${0%/scripts/audio.sh} ;; *) exit 1 ;; esac
APP=/mnt/app
SYSTEM=/mnt/system
SHA=/net/mmx/fs/sda0/apps/sbin/sha1sum
AWK=/net/mmx/fs/sda0/apps/sbin/awk
STATE=$APP/root/mhi2-audio
# M.I.B. V3.7+ no longer ships apps/sbin/awk; the unit's own awk (gawk) is on PATH.
if [ ! -x "$AWK" ]; then
    OLD_IFS=$IFS
    IFS=:
    for FIND_DIR in $PATH; do
        if [ -f "$FIND_DIR/awk" ] && [ -x "$FIND_DIR/awk" ]; then AWK=$FIND_DIR/awk; break; fi
    done
    IFS=$OLD_IFS
fi
# RAM directory for the lock and temp files. QNX /dev/shmem (often behind /tmp) holds only files,
# never folders, so everything here is a plain file; the first candidate that accepts one wins.
RUNTIME=
for RUNTIME_CANDIDATE in /tmp /dev/shmem /net/rcc/dev/shmem; do
    # { ...; } 2>/dev/null: a failed redirection of a special builtin would end a POSIX ksh script.
    if [ -d "$RUNTIME_CANDIDATE" ] && { echo probe > "$RUNTIME_CANDIDATE/mhi2-audio.probe.$$"; } 2>/dev/null; then
        rm -f "$RUNTIME_CANDIDATE/mhi2-audio.probe.$$"
        RUNTIME=$RUNTIME_CANDIDATE
        break
    fi
done
[ -n "$RUNTIME" ] || RUNTIME=/tmp
LOCK=$RUNTIME/mhi2-audio.lock
CAND=$RUNTIME/mhi2-audio.cand.$$
CAND2=$RUNTIME/mhi2-audio.check.$$
MUTATING=0
[ ! -L "$STATE" ] && [ ! -L "$STATE/baseline" ] || exit 1
[ -x "$SHA" ] && [ -x "$AWK" ] || { echo 'Missing M.I.B. sha1sum or awk'; exit 1; }
[ "$(uname -s)" = QNX ] || { echo 'Must execute on MMX/QNX'; exit 1; }
[ ! -L "$PACKAGE/logs" ] || exit 1
# The SD is mounted read-only by default; logs and ACTION.txt need it writable.
mount -uw "${PACKAGE%/*}" 2>/dev/null
# A log on the SD is a convenience, not a requirement: if the card cannot be written, log in RAM.
LOG=
if mkdir -p "$PACKAGE/logs" 2>/dev/null; then
    LOG_CANDIDATE=$PACKAGE/logs/run-$$.log
    [ ! -e "$LOG_CANDIDATE" ] && [ ! -L "$LOG_CANDIDATE" ] || exit 1
    if { cat /dev/null > "$LOG_CANDIDATE"; } 2>/dev/null; then LOG=$LOG_CANDIDATE; fi
fi
if [ -z "$LOG" ]; then
    LOG=$RUNTIME/mhi2-audio.log
    { cat /dev/null > "$LOG"; } 2>/dev/null || LOG=/dev/null
fi
say(){ echo "$*"; echo "$*" >> "$LOG"; }
die(){ say "ERROR: $*"; [ "$MUTATING" = 0 ] || say 'Do not reboot; select RESTORE with the same SD package.'; exit 1; }
# Never name a function after a ksh alias (hash, type, r, stop, history, integer, local, ...): parse error on the unit.
file_sha1(){
 [ -f "$1" ] && [ ! -L "$1" ] || return 1
 H=$("$SHA" "$1") || return 1
 set -- $H
 case ${1:-} in ''|*[!0-9a-f]*) return 1 ;; esac
 [ "${#1}" = 40 ] || return 1
 echo "$1"
}
equal(){ A=$(file_sha1 "$1") && B=$(file_sha1 "$2") && [ "$A" = "$B" ]; }
copy_checked(){ [ ! -L "$2" ] && [ ! -d "$2" ] || die 'Unsafe copy destination'; cp -p "$1" "$2" || die 'Copy failed'; equal "$1" "$2" || die 'Copy mismatch'; }
activate(){ copy_checked "$1" "$2.audio-new"; mv "$2.audio-new" "$2" || die 'Atomic activation failed'; equal "$1" "$2" || die 'Readback mismatch'; }
# The lock is a file holding its owner's PID, created atomically (noclobber); a dead owner's lock is stale.
make_lock(){ ( set -C; echo $$ > "$LOCK" ) 2>/dev/null; }
if ! make_lock; then
    LOCK_OWNER=$(cat "$LOCK" 2>/dev/null)
    case $LOCK_OWNER in ''|*[!0-9]*) LOCK_OWNER= ;; esac
    if [ -n "$LOCK_OWNER" ] && kill -0 "$LOCK_OWNER" 2>/dev/null; then die 'Another operation is running'; fi
    rm -f "$LOCK" 2>/dev/null
    make_lock || die 'Cannot create the lock file'
    say 'Replaced a stale lock left by an interrupted run.'
fi
trap 'rm -f "$LOCK" "$CAND" "$CAND2"' 0
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
 case $REL in payload/libmhi2_audio_hook.so|payload/libmhi2_apac.so|payload/libmhi2_aac.so|payload/libmhi2_fdk.so|payload/mhi2_audio_ui.jar|payload/lang-stock.zip|payload/lang-uk.zip|stock/lsd.sh|stock/smartphone_integrator.json|stock/lang-stock.zip|stock/lang-uk.zip|target/core.sha1|scripts/audio.sh|scripts/json.awk|HMI.sha1) ;; *) die 'Unexpected manifest path' ;; esac
 case $SEEN in *"|$REL|"*) die 'Duplicate manifest entry' ;; esac
 SEEN=$SEEN$REL'|'
 ACTUAL=$(file_sha1 "$PACKAGE/$REL") || die "Missing package file: $REL"
 [ "$ACTUAL" = "$EXPECTED" ] || die "Package hash mismatch: $REL"
 COUNT=$((COUNT+1))
done < "$PACKAGE/SHA1SUMS"
[ "$COUNT" = 15 ] || die 'Incomplete package manifest'
CORE_COUNT=0
CORE_SEEN="|"
while read -r EXPECTED REL; do
 case $REL in eso/lib/libairplay.so|armle/usr/lib/libnvaudio.so|root/bin-target/deva-nvaudio|eso/bin/apps/smartphone_integrator|eso/bin/apps/mediarouter|eso/bin/apps/dio_manager) ;; *) die 'Unexpected core fingerprint path' ;; esac
 case $CORE_SEEN in *"|$REL|"*) die 'Duplicate core entry' ;; esac
 CORE_SEEN=$CORE_SEEN$REL'|'
 [ "$(file_sha1 "$APP/$REL")" = "$EXPECTED" ] || die "Unsupported core binary: $REL"
 CORE_COUNT=$((CORE_COUNT+1))
done < "$PACKAGE/target/core.sha1"
[ "$CORE_COUNT" = 6 ] || die 'Incomplete core fingerprints'
[ "$(file_sha1 /ifs/lsd.jxe)" = "$(cat "$PACKAGE/HMI.sha1")" ] || die 'Unsupported HMI JXE'
LSD=$APP/eso/hmi/lsd
JSON=$SYSTEM/etc/eso/production/smartphone_integrator.json
LIBDIR=$APP/eso/lib
LIBNAMES="libmhi2_audio_hook.so libmhi2_apac.so libmhi2_aac.so libmhi2_fdk.so"
JAR=$LSD/jars/mhi2_audio_ui.jar
# Paths as they appear on the unit (not the host test root).
OURS_PATH=/mnt/app/eso/lib/libmhi2_audio_hook.so
CODEC_PATH=/mnt/app/eso/lib/libmhi2_apac.so
no_audio_files(){ for N in $LIBNAMES; do [ ! -e "$LIBDIR/$N" ] && [ ! -L "$LIBDIR/$N" ] || return 1; done; [ ! -e "$JAR" ] && [ ! -L "$JAR" ]; }
# smartphone_integrator.json is edited in place (scripts/json.awk), never replaced: other components
# (the CarPlay VC base adds its own LD_PRELOAD) edit the same carplay entry.
# json_edit MODE IN OUT; json_state sets JRC: 0 both audio entries, 1 none, 2 partial, other = unusable.
json_edit(){ "$AWK" -v mode="$1" -v ours="$OURS_PATH" -v codec="$CODEC_PATH" -f "$PACKAGE/scripts/json.awk" "$2" "$2" > "$3"; }
json_state(){ json_edit has "$1" /dev/null; JRC=$?; }
# stock/lsd.sh must stay untouched: the stock lsd.sh already puts every lsd/jars/*.jar on the bootclasspath,
# which is how mhi2_audio_ui.jar is loaded (no lsd.sh replacement needed).
equal "$LSD/lsd.sh" "$PACKAGE/stock/lsd.sh" || die 'Startup differs from reviewed baseline'
for DIR in "$APP/root" "$LIBDIR" "$LSD" "$LSD/jars" "${JSON%/*}"; do
 [ -d "$DIR" ] && [ ! -L "$DIR" ] || die "Unsafe directory: $DIR"
done
if [ "$ACTION" = RESTORE ]; then
 [ -f "$STATE/baseline/COMPLETE" ] && [ ! -L "$STATE/baseline/COMPLETE" ] || die 'No complete baseline'
 [ "$(cat "$STATE/baseline/COMPLETE")" = ORIGINALS_AND_ABSENT_AUDIO_FILES ] || die 'Unknown baseline marker'
 VARIANT=$(cat "$STATE/baseline/variant") || die 'Missing saved variant'
 case $VARIANT in stock|uk) ;; *) die 'Invalid saved variant' ;; esac
 [ -f "$STATE/baseline/smartphone_integrator.json" ] || die 'Missing child config backup'
 equal "$STATE/baseline/lang_data.zip" "$PACKAGE/stock/lang-$VARIANT.zip" || die 'Corrupt language backup'
 json_state "$JSON"
 case $JRC in 0|1|2) ;; *) die "Child config layout is not supported (code $JRC)" ;; esac
 equal "$LSD/jars/lang_data.zip" "$PACKAGE/stock/lang-$VARIANT.zip" || equal "$LSD/jars/lang_data.zip" "$PACKAGE/payload/lang-$VARIANT.zip" || die 'Unknown language changes'
 for N in $LIBNAMES mhi2_audio_ui.jar; do
  case $N in *.jar) F=$JAR ;; *) F=$LIBDIR/$N ;; esac
  [ ! -L "$F" ] || die 'Unsafe installed symlink'
  [ ! -e "$F" ] || equal "$F" "$PACKAGE/payload/$N" || die 'Unknown installed audio file'
 done
else
 if [ -f "$STATE/baseline/RESTORED" ]; then
  VARIANT=$(cat "$STATE/baseline/variant") || die 'Missing variant'
  case $VARIANT in stock|uk) ;; *) die 'Invalid saved variant' ;; esac
  json_state "$JSON"
  [ "$JRC" = 1 ] || die 'Restored baseline changed (child config)'
  equal "$LSD/jars/lang_data.zip" "$PACKAGE/stock/lang-$VARIANT.zip" || die 'Restored baseline changed (language ZIP)'
  no_audio_files || die 'Unexpected audio file after restore'
  [ "$ACTION" = CHECK ] || die 'Restored backup preserved; archive it explicitly before a fresh installation'
  say 'CHECK PASS: restored baseline, no active audio payload'; exit 0
 fi
 if [ -f "$STATE/baseline/COMPLETE" ]; then
  VARIANT=$(cat "$STATE/baseline/variant") || die 'Missing variant'
  case $VARIANT in stock|uk) ;; *) die 'Invalid saved variant' ;; esac
  json_state "$JSON"
  [ "$JRC" = 0 ] || die "Installed child config changed (code $JRC)"
  equal "$LSD/jars/lang_data.zip" "$PACKAGE/payload/lang-$VARIANT.zip" || die 'Installed language ZIP changed'
  for N in $LIBNAMES; do equal "$LIBDIR/$N" "$PACKAGE/payload/$N" || die "Installed native library changed: $N"; done
  equal "$JAR" "$PACKAGE/payload/mhi2_audio_ui.jar" || die 'Installed UI JAR changed'
  [ "$ACTION" = CHECK ] || die 'Already installed; use RESTORE before a different install'
  say 'CHECK PASS: installed payload hashes match. Preview validation remains required.'; exit 0
 fi
 [ ! -e "$STATE" ] && [ ! -L "$STATE" ] || die 'Incomplete/unknown state; inspect before proceeding'
 json_state "$JSON"
 [ "$JRC" = 1 ] || die "Child config is not ready for the audio entries (code $JRC: 0/2 already present, 3 foreign codec, 4 empty list, 7 unsupported layout)"
 VARIANT=
 for V in stock uk; do equal "$LSD/jars/lang_data.zip" "$PACKAGE/stock/lang-$V.zip" && VARIANT=$V; done
 [ -n "$VARIANT" ] || die 'Language ZIP differs from standalone audio stock baseline (accepted: stock, or the exact Ukrainian package lang_data.zip)'
 no_audio_files || die 'Audio files already exist'
fi
# Join all lines after the header: a long device name may wrap onto its own line.
SPACE=$(df -k "$APP" | "$AWK" 'NR > 1 { printf "%s ", $0 }') || die 'Cannot read free space'
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
SYSTEM_SPACE=$(df -k "$SYSTEM" | "$AWK" 'NR > 1 { printf "%s ", $0 }') || die 'Cannot read system space'
set -- $SYSTEM_SPACE
[ "$#" -ge 4 ] || die 'Unknown system df format'
case $4 in ''|*[!0-9]*) die 'Unknown system free space' ;; esac
[ "$4" -ge 256 ] || die 'At least256KiB system space required'
mount -uw "$SYSTEM" || die 'Cannot remount system read-write'
mount -uw "$APP" || die 'Cannot remount app read-write'
# Rewrite the child config through a same-mode copy so owner and permissions survive.
write_json(){
 [ ! -L "$JSON.audio-new" ] && [ ! -d "$JSON.audio-new" ] || die 'Unsafe child config temp'
 rm -f "$JSON.audio-new"
 cp -p "$JSON" "$JSON.audio-new" || die 'Cannot stage child config'
 cat "$CAND" > "$JSON.audio-new" || die 'Cannot write child config'
 equal "$CAND" "$JSON.audio-new" || die 'Child config write verification failed'
 mv "$JSON.audio-new" "$JSON" || die 'Atomic activation failed'
 equal "$CAND" "$JSON" || die 'Readback mismatch'
}
if [ "$ACTION" = INSTALL_PREVIEW ]; then
 mkdir -p "$STATE/baseline" || die 'Cannot create baseline'
 copy_checked "$JSON" "$STATE/baseline/smartphone_integrator.json"
 copy_checked "$LSD/jars/lang_data.zip" "$STATE/baseline/lang_data.zip"
 echo "$VARIANT" > "$STATE/baseline/variant" || die 'Cannot save variant'
 echo ORIGINALS_AND_ABSENT_AUDIO_FILES > "$STATE/baseline/COMPLETE" || die 'Cannot seal baseline'
 sync || die 'Cannot sync baseline'
 # Build the new child config and prove the edit is exactly reversible before touching anything.
 json_edit apply "$JSON" "$CAND" || die 'Cannot build the child config'
 json_edit strip "$CAND" "$CAND2" || die 'Cannot verify the child config edit'
 equal "$CAND2" "$STATE/baseline/smartphone_integrator.json" || die 'Child config edit is not exactly reversible'
 MUTATING=1
 for N in $LIBNAMES; do activate "$PACKAGE/payload/$N" "$LIBDIR/$N"; done
 activate "$PACKAGE/payload/mhi2_audio_ui.jar" "$JAR"
 activate "$PACKAGE/payload/lang-$VARIANT.zip" "$LSD/jars/lang_data.zip"
 write_json
 echo FIRST_BOOT_OFF > "$STATE/FIRST_BOOT" || die 'Cannot prepare first boot Off'
 for N in $LIBNAMES; do chmod 755 "$LIBDIR/$N" || die 'Cannot set executable permissions'; done
 json_state "$JSON"
 [ "$JRC" = 0 ] || die 'Child config verification failed'
else
 MUTATING=1
 # Remove only the audio entries; entries other components added to the same list stay.
 if [ "$JRC" != 1 ]; then
  json_edit strip "$JSON" "$CAND" || die 'Cannot strip the audio entries from the child config'
  write_json
  json_state "$JSON"
  [ "$JRC" = 1 ] || die 'Child config still holds audio entries'
 fi
 if equal "$JSON" "$STATE/baseline/smartphone_integrator.json"; then say 'Child config equals the install-time baseline.'
 else say 'Child config differs from the install-time baseline (other components changed it; kept).'; fi
 activate "$STATE/baseline/lang_data.zip" "$LSD/jars/lang_data.zip"
 # Only remove the files previously verified as this exact audio payload.
 for N in $LIBNAMES; do [ ! -e "$LIBDIR/$N" ] || rm "$LIBDIR/$N" || die 'Cannot remove owned native payload'; done
 [ ! -e "$JAR" ] || rm "$JAR" || die 'Cannot remove owned UI payload'
 echo RESTORED > "$STATE/baseline/RESTORED" || die 'Cannot record restore'
fi
sync || die 'Cannot sync changes'
MUTATING=0
say "SUCCESS: $ACTION verified. No automatic reboot. Preserve this SD and baseline."
