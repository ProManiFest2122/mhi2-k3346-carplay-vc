#!/usr/bin/env bash
# Simulation of the vc_render watchdog in mib2high-lvds-control.sh (issue #7).
#
# Runs the released script (OLD) and the patched one (NEW) through the same
# sessions with fake car tools, then checks two things:
#   - healthy sessions: NEW does exactly what OLD does (same dmdt calls, same
#     process launches, never a slay of vc_render);
#   - stuck renderer: NEW stops and restarts vc_render, at most twice per
#     session, and never starts a second renderer next to one that will not die.
#
# Run in a throwaway container (it replaces /bin/sleep and writes /mnt, /eso):
#   mkdir -p old && git show v1.0.1:port_k3346/altscreen/deployment/mib2high-lvds-control.sh > old/mib2high-lvds-control.sh
#   tar -C <repo> -cf - port_k3346/altscreen tools old | docker run --rm -i debian:12 bash -c \
#     'mkdir /w && tar -C /w -xf - && apt-get update -qq && apt-get install -y -qq mksh >/dev/null &&
#      OLD_CTL=/w/old/mib2high-lvds-control.sh bash /w/tools/sim_reconnect_watchdog.sh'
set -u

NEW_CTL="${NEW_CTL:-/w/port_k3346/altscreen/deployment/mib2high-lvds-control.sh}"
OLD_CTL="${OLD_CTL:?OLD_CTL=<released mib2high-lvds-control.sh>}"
CONF_SRC="${CONF_SRC:-/w/port_k3346/altscreen/config/mib2high-overlay.conf}"
OUT=/tmp/simout
PASS=0; FAILED=0
ok(){  PASS=$((PASS+1));     echo "  OK   $*"; }
bad(){ FAILED=$((FAILED+1)); echo "  FAIL $*"; }

VRLOG=/tmp/vc_render.log
L(){ echo "100.000 [vc_render] $*" >> "$VRLOG"; }

setup_fakes(){
    mkdir -p /mnt/app/root /mnt/app/armle/bin /eso/bin/apps "$OUT"
    [ -x /bin/sleep.real ] || cp /bin/sleep /bin/sleep.real

    cat > /eso/bin/apps/dmdt <<'EOF'
#!/bin/sh
echo "dmdt $*" >> /tmp/sim_dmdt.log
exit 0
EOF
    # on -p N CMD ARGS: a fake vc_render only leaves its start lines and a
    # "process alive" marker; vc_overlay runs and reports its window.
    cat > /mnt/app/armle/bin/on <<'EOF'
#!/bin/sh
while [ $# -gt 0 ]; do case "$1" in -p|-f) shift 2 ;; *) break ;; esac; done
echo "on LD=${LD_LIBRARY_PATH:-} $*" >> /tmp/sim_on.log
case "$1" in
  */vc_render)
    : > /tmp/sim_vr_alive
    echo "200.000 [vc_render] start out=1 layer=58 800x480@30 port=19830" >> /tmp/vc_render.log
    echo "200.001 [vc_render] listening on 127.0.0.1:19830 ts mode=1 (0 raw, 1 wall, 2 zero)" >> /tmp/vc_render.log
    [ -f /tmp/sim_on_hook ] && sh /tmp/sim_on_hook
    exit 0 ;;
esac
exec "$@"
EOF
    cat > /mnt/app/armle/bin/pidin <<'EOF'
#!/bin/sh
[ -f /tmp/sim_vr_alive ] && echo "  4242   1 /mnt/app/root/vc_render  25r RECEIVE"
exit 0
EOF
    # slay [-s SIG] NAME. /tmp/sim_vr_kill: term (default) | kill | never
    cat > /mnt/app/armle/bin/slay <<'EOF'
#!/bin/sh
echo "slay $*" >> /tmp/sim_slay.log
SIG=TERM
[ "${1:-}" = "-s" ] && { SIG="$2"; shift 2; }
[ "${1:-}" = vc_render ] || exit 0
M=term; [ -f /tmp/sim_vr_kill ] && M=`cat /tmp/sim_vr_kill`
case "$M:$SIG" in
  term:*|kill:KILL) rm -f /tmp/sim_vr_alive ;;
esac
exit 0
EOF
    printf '#!/bin/sh\nexit 0\n' > /mnt/app/armle/bin/sloginfo

    # Every sleep is one tick. /tmp/sim_tick_hook runs with the tick number, so
    # a case can change the renderer log at a chosen moment. After END ticks
    # the session ends (the hook removes the ACTIVE marker).
    cat > /bin/sleep <<'EOF'
#!/bin/sh
# SIM_VR_SLEEP_STUB
N=0; [ -f /tmp/sim_ticks ] && N=`cat /tmp/sim_ticks`
N=$((N+1)); echo "$N" > /tmp/sim_ticks
[ -f /tmp/sim_tick_hook ] && sh /tmp/sim_tick_hook "$N"
/bin/sleep.real 0.03
END=45; [ -f /tmp/sim_end ] && END=`cat /tmp/sim_end`
[ "$N" -ge "$END" ] && rm -f /tmp/mib2high-lvds-active
exit 0
EOF
    chmod +x /eso/bin/apps/dmdt /mnt/app/armle/bin/* /bin/sleep
    grep -q SIM_VR_SLEEP_STUB /bin/sleep || { echo "cannot replace /bin/sleep"; exit 2; }

    printf '#!/bin/sh\nexit 0\n' > /mnt/app/root/vc_render
    printf '#!/bin/sh\necho "98 210 153" > /tmp/vc-overlay-ready\nexit 0\n' > /mnt/app/root/vc_overlay
    chmod +x /mnt/app/root/vc_render /mnt/app/root/vc_overlay
    cp "$CONF_SRC" /mnt/app/root/mib2high-overlay.conf
}

reset_state(){
    rm -f /tmp/sim_* /tmp/mib2high-lvds-control.log /tmp/mib2high-lvds-control.pid \
          /tmp/vc-overlay-ready /tmp/vc-overlay-ready.tmp /tmp/k2161-altscreen-heartbeat \
          /tmp/mib2high-lvds-reacquire /tmp/mib2high-vc-render.profile \
          /tmp/mib2high-lvds-sessions "$VRLOG"
    : > /tmp/mib2high-lvds-active
}

# A renderer still running from an earlier session, started with the same
# profile (the earlier session wrote it), so the profile-change restart in the
# script does not fire.
alive(){ : > /tmp/sim_vr_alive; echo "800 480 30" > /tmp/mib2high-vc-render.profile; }

# A renderer log as left by one healthy earlier session.
healthy_history(){
    L "start out=1 layer=58 800x480@30 port=19830"
    L "listening on 127.0.0.1:19830 ts mode=1 (0 raw, 1 wall, 2 zero)"
    L "hook connected"
    L "NvSSVideoOpen(outputDevice=1 layer=58) -> 0x0 handle=0x1"
    L "NvSSVideoStreamConfigure(800x480@30) -> 0x0"
    L "NvSSVideoGetAttribs -> 0x0"
    L "decode#1 len=4000 rc=0x0 flag=1"
    L "STATS 150 frames in 5000 ms = 30 fps; decode avg 900 us max 3000 us; 900 KB; ts span 5000000"
    L "hook disconnected frames=150 dec_err=0"
    L "NvSSVideoClose ENTER handle=0x1 frames=150 dec_err=0"
    L "NvSSVideoClose EXIT rc=0x0"
}

# Run one script; keep what it did to the car.
run_one(){ # name variant shell script
    local d="$OUT/$1-$2-`basename $3`"
    rm -rf "$d"; mkdir -p "$d"
    cp "$VRLOG" /tmp/sim_vrlog.start 2>/dev/null || rm -f /tmp/sim_vrlog.start
    "$3" "$4" >/dev/null 2>&1
    /bin/sleep.real 0.2
    for f in sim_dmdt.log sim_on.log sim_slay.log mib2high-lvds-control.log; do
        cp "/tmp/$f" "$d/$f" 2>/dev/null || : > "$d/$f"
    done
    echo "$d"
}

# Same case for OLD and NEW from the same starting state.
# $2 = function that prepares the state (called after reset_state).
both(){ # case prep shell
    reset_state; $2; OLDD=`run_one "$1" old "$3" "$OLD_CTL"`
    reset_state; $2; NEWD=`run_one "$1" new "$3" "$NEW_CTL"`
}

vr_slays(){ grep -c 'vc_render' "$1/sim_slay.log" 2>/dev/null || true; }
vr_launches(){ grep -c '/vc_render ' "$1/sim_on.log" 2>/dev/null || true; }

# The loop repeats "sc 4 <ctx>" once per few ticks, and the watchdog's own waits
# use ticks, so compare the sequence of distinct consecutive calls.
same_ctx(){ cmp -s <(uniq "$OLDD/sim_dmdt.log") <(uniq "$NEWD/sim_dmdt.log"); }

same_as_old(){ # label
    if same_ctx; then ok "$1: dmdt calls identical to 1.0.0"
    else bad "$1: dmdt calls differ from 1.0.0"; diff "$OLDD/sim_dmdt.log" "$NEWD/sim_dmdt.log" | head -5; fi
    if cmp -s "$OLDD/sim_on.log" "$NEWD/sim_on.log"; then ok "$1: process launches identical to 1.0.0"
    else bad "$1: process launches differ from 1.0.0"; diff "$OLDD/sim_on.log" "$NEWD/sim_on.log" | head -5; fi
    if cmp -s "$OLDD/sim_slay.log" "$NEWD/sim_slay.log"; then ok "$1: slay calls identical to 1.0.0"
    else bad "$1: slay calls differ from 1.0.0"; diff "$OLDD/sim_slay.log" "$NEWD/sim_slay.log" | head -5; fi
    if grep -q 'renderer watchdog' "$NEWD/mib2high-lvds-control.log"; then bad "$1: watchdog acted"
    else ok "$1: watchdog stayed silent"; fi
}

# ---------------------------------------------------------------- healthy ----
prep_first(){ :; }
prep_reconnect_healthy(){
    alive; healthy_history
    # New session: the hook connects and frames flow, a STATS line every 5 ticks.
    cat > /tmp/sim_tick_hook <<'EOF'
case "$1" in
  2) echo "300.000 [vc_render] hook connected" >> /tmp/vc_render.log
     echo "300.001 [vc_render] NvSSVideoOpen(outputDevice=1 layer=58) -> 0x0 handle=0x2" >> /tmp/vc_render.log
     echo "300.002 [vc_render] NvSSVideoStreamConfigure(800x480@30) -> 0x0" >> /tmp/vc_render.log
     echo "300.003 [vc_render] NvSSVideoGetAttribs -> 0x0" >> /tmp/vc_render.log ;;
esac
[ $(( $1 % 5 )) -eq 0 ] && echo "3$1.000 [vc_render] STATS 150 frames in 5000 ms = 30 fps" >> /tmp/vc_render.log
exit 0
EOF
}
prep_connected_silent(){
    alive; healthy_history
    L "hook connected"
    L "NvSSVideoOpen(outputDevice=1 layer=58) -> 0x0 handle=0x2"
    L "NvSSVideoStreamConfigure(800x480@30) -> 0x0"
    L "NvSSVideoGetAttribs -> 0x0"
}
prep_no_log(){ alive; }
prep_transient_close(){
    alive; healthy_history
    L "hook connected"; L "hook disconnected frames=10 dec_err=0"
    L "NvSSVideoClose ENTER handle=0x2 frames=10 dec_err=0"
    printf 'case "$1" in 1) echo "301.000 [vc_render] NvSSVideoClose EXIT rc=0x0" >> /tmp/vc_render.log ;; esac\nexit 0\n' > /tmp/sim_tick_hook
}
prep_old_failure_then_ok(){
    alive; healthy_history
    L "hook connected"; L "NvSS start failed; discarding frames until the hook reconnects"
    L "hook disconnected frames=0 dec_err=0"
    L "hook connected"; L "NvSSVideoOpen(outputDevice=1 layer=58) -> 0x0 handle=0x3"
}
prep_close_slow_but_logging(){
    # Renderer busy closing but still writing: the log grows between looks.
    alive; healthy_history
    L "NvSSVideoClose ENTER handle=0x9 frames=1 dec_err=0"
    printf 'echo "4$1.000 [vc_render] NvSSVideoClose ENTER handle=0x9 frames=1 dec_err=0" >> /tmp/vc_render.log\nexit 0\n' > /tmp/sim_tick_hook
}

# ------------------------------------------------------------------ stuck ----
prep_close_hung(){
    alive; healthy_history
    L "hook connected"; L "hook disconnected frames=900 dec_err=0"
    L "NvSSVideoClose ENTER handle=0x2 frames=900 dec_err=0"
}
prep_close_hung_needs_kill(){ prep_close_hung; echo kill > /tmp/sim_vr_kill; }
prep_close_hung_unkillable(){ prep_close_hung; echo never > /tmp/sim_vr_kill; }
prep_start_failed_mid(){
    alive; healthy_history
    printf 'case "$1" in 8) echo "500.000 [vc_render] hook connected" >> /tmp/vc_render.log; echo "500.001 [vc_render] NvSS start failed; discarding frames until the hook reconnects" >> /tmp/vc_render.log ;; esac\nexit 0\n' > /tmp/sim_tick_hook
}
prep_start_failed_always(){
    prep_start_failed_mid
    # Every new renderer fails again as soon as the hook reconnects.
    printf 'echo "600.000 [vc_render] hook connected" >> /tmp/vc_render.log\necho "600.001 [vc_render] NvSS start failed; discarding frames until the hook reconnects" >> /tmp/vc_render.log\n' > /tmp/sim_on_hook
    echo 80 > /tmp/sim_end
}

run_all(){
    SH="$1"
    echo "--- $SH: healthy sessions must behave exactly like 1.0.0"
    both first prep_first "$SH";                       same_as_old "first session after boot"
    if [ "`vr_launches "$NEWD"`" = 1 ]; then ok "first session: one renderer started"; else bad "first session: renderer starts=`vr_launches "$NEWD"`"; fi
    both reconnect prep_reconnect_healthy "$SH";       same_as_old "healthy reconnect"
    both silent prep_connected_silent "$SH";           same_as_old "hook connected, no frames"
    both nolog prep_no_log "$SH";                      same_as_old "renderer log missing"
    both transient prep_transient_close "$SH";         same_as_old "close finishes after the first look"
    both oldfail prep_old_failure_then_ok "$SH";       same_as_old "old start failure, then a good start"
    both slowclose prep_close_slow_but_logging "$SH";  same_as_old "close still logging"
    if grep -q 'session #1 since boot' "$NEWD/mib2high-lvds-control.log"; then ok "session counter logged"; else bad "no session counter"; fi

    echo "--- $SH: stuck renderer"
    both hung prep_close_hung "$SH"
    [ "`vr_slays "$OLDD"`" = 0 ] && [ "`vr_launches "$OLDD"`" = 0 ] && ok "1.0.0 leaves the hung renderer alone (the bug)" || bad "1.0.0 baseline unexpected"
    grep -q 'renderer watchdog: close_hung confirmed' "$NEWD/mib2high-lvds-control.log" && ok "hung close detected" || bad "hung close not detected"
    [ "`vr_slays "$NEWD"`" = 1 ] && ok "one TERM, no KILL needed" || bad "slays=`vr_slays "$NEWD"`"
    [ "`vr_launches "$NEWD"`" = 1 ] && ok "renderer started again once" || bad "launches=`vr_launches "$NEWD"`"
    grep -q 'renderer start out=1' "$NEWD/mib2high-lvds-control.log" && ok "started through the normal start line" || bad "no normal start line"

    both hungkill prep_close_hung_needs_kill "$SH"
    grep -q 'slay -s KILL vc_render' "$NEWD/sim_slay.log" && ok "KILL after an ignored TERM" || bad "no KILL"
    [ "`vr_launches "$NEWD"`" = 1 ] && ok "renderer started again once" || bad "launches=`vr_launches "$NEWD"`"

    both unkillable prep_close_hung_unkillable "$SH"
    [ "`vr_launches "$NEWD"`" = 0 ] && ok "never starts a second renderer next to a live one" || bad "launches=`vr_launches "$NEWD"`"
    [ "`grep -c 'did not exit' "$NEWD/mib2high-lvds-control.log"`" = 2 ] && ok "gives up after 2 attempts" || bad "attempts=`grep -c 'did not exit' "$NEWD/mib2high-lvds-control.log"`"
    same_ctx && ok "cluster contexts unchanged vs 1.0.0" || bad "cluster contexts differ"

    both startfail prep_start_failed_mid "$SH"
    grep -q 'renderer watchdog: start_failed confirmed' "$NEWD/mib2high-lvds-control.log" && ok "failed NvSS start detected mid-session" || bad "start failure not detected"
    [ "`vr_launches "$NEWD"`" = 1 ] && ok "renderer restarted once" || bad "launches=`vr_launches "$NEWD"`"
    grep -q "on LD=/mnt/app/armle/lib:.* /mnt/app/root/vc_render " "$NEWD/sim_on.log" && ok "restart uses the renderer library path" || bad "wrong LD_LIBRARY_PATH: `cat "$NEWD/sim_on.log"`"
    same_ctx && ok "cluster contexts unchanged vs 1.0.0" || bad "cluster contexts differ"

    both startfailloop prep_start_failed_always "$SH"
    [ "`vr_launches "$NEWD"`" = 2 ] && ok "at most 2 restarts per session" || bad "launches=`vr_launches "$NEWD"`"
}

echo "=========================================================="
echo " SIMULATION: vc_render watchdog (issue #7)"
echo "=========================================================="
setup_fakes
RAN=0
for cand in /bin/dash /bin/mksh; do
    if [ -x "$cand" ]; then run_all "$cand"; RAN=$((RAN+1)); else echo "--- $cand: skipped (missing)"; fi
done
[ "$RAN" -gt 0 ] || run_all /bin/sh
echo "=========================================================="
echo " RESULT: $PASS OK, $FAILED FAIL"
echo "=========================================================="
[ "$FAILED" -eq 0 ]
