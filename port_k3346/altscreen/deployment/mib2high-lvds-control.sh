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
OVERLAY=/mnt/app/root/vc_overlay
OVERLAY_CONF=/mnt/app/root/mib2high-overlay.conf
OVERLAY_READY=/tmp/vc-overlay-ready
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
    slay vc_overlay >/dev/null 2>&1 || true
    # The overlay clears this itself on a clean shutdown; do it here too in case
    # it was killed hard, so no stale report survives this session.
    rm -f "$OVERLAY_READY" "$OVERLAY_READY.tmp" 2>/dev/null || true
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

# --- Etap 7: cluster overlay (maneuver arrow + lanes) -----------------------
# Separate process, separate displayable, drawn OVER the CarPlay map.  vc_render
# is untouched: if anything here fails we fall back to context 77 and the map
# keeps working exactly as it does today.
OVERLAY_CTX=77
if [ -x "$OVERLAY" ]; then
    if ! pidin 2>/dev/null | grep vc_overlay >/dev/null; then
        # Drop any report left behind by an overlay that was killed rather than
        # shut down, so the gate below can only ever trust a fresh one.
        rm -f "$OVERLAY_READY" "$OVERLAY_READY.tmp" 2>/dev/null || true
        # Start it with /mnt/app/root as the working directory.  The renderer
        # looks for its destination-flag texture at "./flag_atlas.rgba" and
        # then at "/mnt/app/root/hooks/flag_atlas.rgba" (upstream/main.c:45-46);
        # on 2026-10-27 both missed and it logged "flag atlas open failed"
        # twice.  A cd costs nothing and needs no new directory on the car.
        ( cd /mnt/app/root 2>/dev/null || true
          on -p 25 "$OVERLAY" >> /tmp/vc_overlay-launch.log 2>&1 & )
        log "overlay start"
        sleep 1
    else
        log "overlay already running"
    fi
else
    log "overlay not installed: $OVERLAY"
fi

export LD_LIBRARY_PATH=/eso/lib
GATE_CONTEXT_OK=0
if IPL_CONFIG_DIR=/etc/eso/production "$DMDT" dc 900 58 >> "$LOG" 2>&1; then
    GATE_CONTEXT_OK=1
    log "cluster gate context=900 displayable=58 declared"
else
    rm -f "$HEARTBEAT" 2>/dev/null || true
    log "cluster gate context declaration failed; HMI redirection stays disabled"
fi

# Only take the overlay context once the overlay really has a window.
#
# Etap 7a asked `dmdt gd` for that proof and got zero bytes back, and the reason
# is now known: this DisplayManager binds the displayable number to the WINDOW,
# from its caption, and never puts it in any list `dmdt gd` prints.  So `gd`
# could not have answered this even with the window up.  The only process that
# knows is the renderer, and it says so by writing $OVERLAY_READY ("<id> <w> <h>")
# after its first presented frame.  See docs/19_displayable_na_mhi2.md.
#
# This gate is why a failed overlay costs us nothing: no marker, no context
# switch, the cluster stays on 77 and the map behaves exactly as today.
if [ -x "$OVERLAY" ]; then
    OVL_ID=`awk '$1=="displayable" { print $2; exit }' "$OVERLAY_CONF" 2>/dev/null || true`
    case "${OVL_ID:-}" in
        ''|*[!0-9]*) OVL_ID=98 ;;
    esac

    # What goes BEHIND our overlay in context 901, front to back.  Kept in the
    # conf file rather than here because we do not yet know for certain what
    # 101 and 102 draw: they appear in every stock route-guidance context
    # (73, 74, 77) alongside displayable 20, which we leave out because 20 is
    # the wrong-arrow source we are replacing.  If the black rectangle on the
    # cluster turns out to be those backings rather than our own panel, the
    # cure is to edit this line on the SD card - "context_recipe 58" - instead
    # of rebuilding.  Deliberately NOT changed blind: the green CarPlay
    # instruction card may well be one of them, and losing it is a regression.
    OVL_BEHIND=`awk '$1=="context_recipe" { $1=""; sub(/^ +/,""); print; exit }' \
                "$OVERLAY_CONF" 2>/dev/null || true`
    case "${OVL_BEHIND:-}" in
        ''|*[!0-9\ ]*) OVL_BEHIND="102 101 58" ;;
    esac

    # The renderer needs EGL up, a window realized and one swap through.  It
    # took about a second on the car (2026-10-27 log: start 00:03:04.413, GL up
    # and drawing within the same second), so ten seconds is a wide margin and
    # still short enough that a dead overlay does not hold up the map - while
    # we wait, the cluster sits on the working map context.
    READY_ID=''
    WAIT=0
    while [ "$WAIT" -lt 10 ]; do
        if [ -f "$OVERLAY_READY" ]; then
            READY_ID=`awk '{ print $1; exit }' "$OVERLAY_READY" 2>/dev/null || true`
            case "${READY_ID:-}" in
                ''|*[!0-9]*) READY_ID='' ;;
            esac
            [ -n "$READY_ID" ] && break
        fi
        WAIT=$((WAIT + 1))
        sleep 1
    done

    if [ -z "$READY_ID" ]; then
        log "overlay did not report a window within ${WAIT}s; staying on 77"
    elif [ "$READY_ID" != "$OVL_ID" ]; then
        # A stale marker from an earlier run with a different number would make
        # us declare a context for a displayable nobody is drawing.
        log "overlay reported displayable $READY_ID but conf says $OVL_ID; staying on 77"
    else
        # Order matters: index 0 is the FRONT of the composite, so our overlay
        # goes before the map.  Displayable 20 (stock MMI route guidance) is
        # deliberately left out - it is the wrong-arrow source we are replacing.
        if IPL_CONFIG_DIR=/etc/eso/production "$DMDT" dc 901 "$OVL_ID" $OVL_BEHIND >> "$LOG" 2>&1; then
            OVERLAY_CTX=901
            log "overlay ready after ${WAIT}s; context=901 {$OVL_ID $OVL_BEHIND} declared"
        else
            log "overlay context declaration failed; staying on 77"
        fi
    fi
fi

GATE_TICK=0
while [ -e "$ACTIVE" ]; do
    if [ "$GATE_CONTEXT_OK" -eq 1 ]; then
        echo $$ > "$HEARTBEAT" 2>/dev/null || true
    fi
    if [ "$GATE_TICK" -eq 0 ]; then
        IPL_CONFIG_DIR=/etc/eso/production "$DMDT" sc 4 "$OVERLAY_CTX" >> "$LOG" 2>&1 || true
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
slay vc_overlay >/dev/null 2>&1 || true
rm -f "$OVERLAY_READY" "$OVERLAY_READY.tmp" 2>/dev/null || true
IPL_CONFIG_DIR=/etc/eso/production "$DMDT" sc 4 72 >> "$LOG" 2>&1 || true
log "session inactive context=72"
exit 0
