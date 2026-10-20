#!/bin/sh
set -u

PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/bin:/mnt/app/armle/usr/sbin
export PATH

ACTIVE=/tmp/mib2high-lvds-active
REACQUIRE=/tmp/mib2high-lvds-reacquire
HEARTBEAT=/tmp/k2161-altscreen-heartbeat
PIDFILE=/tmp/mib2high-lvds-control.pid
LOG=/tmp/mib2high-lvds-control.log
PROFILE=/mnt/app/root/mib2high-lvds-profile.conf
RENDERER=/mnt/app/root/vc_render
DMDT=/eso/bin/apps/dmdt

log() { echo "`date 2>/dev/null` [lvds-control] $*" >> "$LOG"; }

if [ -f "$PIDFILE" ]; then
    OLD=`cat "$PIDFILE" 2>/dev/null || true`
    if [ -n "${OLD:-}" ] && kill -0 "$OLD" 2>/dev/null; then
        log "duplicate start ignored pid=$OLD"
        exit 0
    fi
    rm -f "$PIDFILE"
fi

echo $$ > "$PIDFILE" || exit 1

cleanup() {
    rm -f "$PIDFILE" "$HEARTBEAT"
    if [ ! -e "$ACTIVE" ] && [ -x "$DMDT" ]; then
        export LD_LIBRARY_PATH=/eso/lib
        IPL_CONFIG_DIR=/etc/eso/production "$DMDT" sc 4 72 >> "$LOG" 2>&1 || true
        log "context=72 native restore"
    fi
}
trap 'cleanup' 0 1 2 15

[ -e "$ACTIVE" ] || { rm -f "$HEARTBEAT" 2>/dev/null || true; exit 0; }

# v8l diagnostics only: read-only bounded /tmp rings for known factory
# DSITracer appenders. Launch output and process status are collected separately.
TRACE_CAPTURE=/mnt/app/root/v8l-dsitracer-capture
if [ -x "$TRACE_CAPTURE" ]; then
    "$TRACE_CAPTURE" 1798 >> /tmp/v8l-dsitracer-launch-1798.log 2>&1 &
    log "v8l dsitracer 1798 capture requested pid=$!"
    "$TRACE_CAPTURE" 20991 >> /tmp/v8l-dsitracer-launch-20991.log 2>&1 &
    log "v8l dsitracer 20991 capture requested pid=$!"
else
    log "v8l dsitracer capture binary missing"
fi

W=800
H=480
FPS=30
FEATURES=0
if [ -f "$PROFILE" ]; then
    set -- `cat "$PROFILE" 2>/dev/null`
    [ $# -ge 1 ] && W="$1"
    [ $# -ge 2 ] && H="$2"
    [ $# -ge 3 ] && FPS="$3"
    [ $# -ge 4 ] && FEATURES="$4"
fi

case "$W:$H:$FPS" in
    *[!0-9:]*|'') W=800; H=480; FPS=30 ;;
esac

[ -x "$RENDERER" ] || { log "renderer missing: $RENDERER"; exit 1; }

export LD_LIBRARY_PATH=/mnt/app/armle/lib:/mnt/app/armle/usr/lib:/mnt/app/root/lib-target:/eso/lib:/armle/lib:/armle/lib/dll:/armle/usr/lib:/lib:/usr/lib

SIG="$W $H $FPS"
OLD_SIG=`cat /tmp/mib2high-vc-render.profile 2>/dev/null || true`
if pidin 2>/dev/null | grep vc_render >/dev/null && [ "$OLD_SIG" != "$SIG" ]; then
    log "renderer profile changed old='$OLD_SIG' new='$SIG'; restarting"
    slay vc_render >/dev/null 2>&1 || true
    sleep 1
fi

if ! pidin 2>/dev/null | grep vc_render >/dev/null; then
    echo "$SIG" > /tmp/mib2high-vc-render.profile
    on -p 25 "$RENDERER" --out 1 --layer 58 --w "$W" --h "$H" --fps "$FPS" --ts wall >> /tmp/vc_render-launch.log 2>&1 &
    log "renderer start out=1 layer=58 ${W}x${H}@${FPS}"
    sleep 1
else
    log "renderer already running profile='$OLD_SIG'"
fi

NAV0=`sloginfo 2>/dev/null | grep -c 'Change NavStateOfOperation to 5' || true`
REACQ_DONE=0
[ "$NAV0" -ge 2 ] && REACQ_DONE=1
log "session active nav_ready_count=$NAV0 features=$FEATURES"

export LD_LIBRARY_PATH=/eso/lib
GATE_CONTEXT_OK=0
if IPL_CONFIG_DIR=/etc/eso/production "$DMDT" dc 900 58 >> "$LOG" 2>&1; then
    GATE_CONTEXT_OK=1
    log "cluster gate context=900 displayable=58 declared"
else
    rm -f "$HEARTBEAT" 2>/dev/null || true
    log "cluster gate context declaration failed; HMI redirection stays disabled"
fi

GATE_TICK=0
while [ -e "$ACTIVE" ]; do
    if [ "$GATE_CONTEXT_OK" -eq 1 ]; then
        echo $$ > "$HEARTBEAT" 2>/dev/null || true
    fi
    if [ "$GATE_TICK" -eq 0 ]; then
        IPL_CONFIG_DIR=/etc/eso/production "$DMDT" sc 4 77 >> "$LOG" 2>&1 || true
    fi
    GATE_TICK=$((GATE_TICK + 1))
    [ "$GATE_TICK" -ge 3 ] && GATE_TICK=0

    if [ "$REACQ_DONE" -eq 0 ]; then
        NAV=`sloginfo 2>/dev/null | grep -c 'Change NavStateOfOperation to 5' || true`
        if [ "$NAV" -ge 2 ]; then
            : > "$REACQUIRE"
            REACQ_DONE=1
            log "second nav-ready observed; reacquire marker set"
        fi
    fi

    sleep 1
done

rm -f "$REACQUIRE" "$HEARTBEAT"
IPL_CONFIG_DIR=/etc/eso/production "$DMDT" sc 4 72 >> "$LOG" 2>&1 || true
log "session inactive context=72"
exit 0
