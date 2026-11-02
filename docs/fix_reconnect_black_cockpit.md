# Issue #7: black cluster map after a USB reconnect (renderer watchdog)

**Status:** script-only mitigation. Host-simulated, and run in one car (A4 B9,
K3346 / MU 1438) on top of 1.0.1 with no regressions; see "Car test".

## Symptom

After unplugging the iPhone and plugging it back in without restarting the MMI, the
Virtual Cockpit map area stays black. Only an MMI restart helps, and every later
reconnect breaks it again (A4 B9, K3346 / MU1438, 1.0.0, NavIgnore installed).

A black map, rather than the stock Audi map, means the cluster is still on the
CarPlay context (77 or 901) with nothing drawing into displayable 58.

## Candidate causes

| # | Cause | Where | Fixable without a QNX build |
|---|---|---|---|
| 1 | The stub NvSS handle in `dio_manager` is never closed (no stream-111 teardown on unplug), so the next open is refused | `native/altscreen_output.c` (`stub open refused reason=already_active`) | No |
| 2a | `vc_render` hangs in `NvSSVideoClose` after the hook disconnects and never accepts the next session | `renderer/upstream/vc_render.c` `nv_stop()` | **Yes** |
| 2b | `vc_render` accepts the next session but `NvSSVideoOpen` fails; it then discards frames "until the hook reconnects", which the hook never does on its own | `vc_render.c` `serve_client()` | **Yes** |
| 3 | Something in the second overlay start / context 901 declaration | `deployment/mib2high-lvds-control.sh` | Unknown without logs |

The reporter's logs decide between them; see "What the logs will show".

## Change

Only `deployment/mib2high-lvds-control.sh`. Every binary, the JARs and the installer
stay exactly as in 1.0.1. The script itself did not change between 1.0.0 and 1.0.1.

- **Session counter.** `session #N since boot` in `mib2high-lvds-control.log`;
  N > 1 marks a reconnect.
- **Renderer watchdog** for 2a and 2b. It reads `/tmp/vc_render.log` and acts only
  when the same stuck state is seen twice with no new log line in between:
  - `close_hung`: the last line is `NvSSVideoClose ENTER`;
  - `start_failed`: after the last `hook connected`, the renderer logged `NvSS start
    failed` and the hook is still connected.
- **When it looks:** once at session start (a second look 3 s later only if the
  first one saw a stuck state), then every 5 s in the session loop.
- **What it does:** `slay vc_render`, then `slay -s KILL vc_render` only if the
  process is still there, then starts the renderer with the same command and
  library path as the normal start.
  - At most 2 restarts per session.
  - If the renderer survives both signals it is left exactly as it was, and no
    second renderer is started next to it.
- **Why a restart is enough:** the hook already retries its connection every
  250 ms and asks the phone for a fresh IDR (`altscreen_output.c`
  `fwd_connect_locked` / `fwd_send_access_unit`). The existing script already
  restarts `vc_render` the same way when the profile changes.

## Why it cannot make a working setup worse

- In every healthy history the watchdog stays silent, and the script's visible
  actions are identical to the released script. Simulated: first session, healthy reconnect,
  hook connected without frames, missing log, a close that finishes between the
  two looks, an old start failure followed by a good start, and a close that
  keeps logging.
- A working renderer never matches either state. A healthy close logs `EXIT`
  within milliseconds, and a working connection never logs `NvSS start failed`.
- The only states it acts in are ones where the cluster is already black until
  an MMI restart.
- It uses only tools the script already used on the car (`awk`, `grep`, `pidin`,
  `slay`, `sleep`, `cat`). None of the tools known to be missing are used.

**The remaining risk is small and is limited to the already-broken state.** Killing a
`vc_render` that is stuck inside the NvSS driver might leave the cluster decoder
unusable until a restart, which is no worse than today. A restart clears it, and
the uninstall returns the car to factory files.

## Not covered

- **Cause 1** needs a change in `libmib2high-carplay-rgi.so`: close or adopt a stale
  stub when a new session's Screen B opens. That needs the QNX toolchain.
- **Cause 3:** the overlay path is unchanged.

If the logs show either of these, the patched script behaves exactly like the
released one.

## Host evidence

- `tools/sim_reconnect_watchdog.sh` runs the released script and the patched
  script through the same sessions with fake car tools, under dash and mksh:
  90 OK, 0 FAIL.
- Run with the released script in place of the patched one, it fails 24 checks
  (all in the stuck cases), so it does tell the two apart.
- `tools/sim_gate.sh` (the existing overlay-gate simulation): 62 OK, 0 FAIL for
  both the released and the patched script.

Usage: `OLD_CTL=<released mib2high-lvds-control.sh> bash tools/sim_reconnect_watchdog.sh`
(`NEW_CTL` defaults to the script in `deployment/`).

## Car test

A4 B9, MHI2_ER_AUG22_K3346 / MU 1438, M.I.B. V3.6.0, NavActiveIgnore. Installed as
the official 1.0.1 install zip with only `payload/mib2high-lvds-control.sh`
replaced, on a fresh car (no earlier version).

- Install: `INSTALL_ALL_RESULT=PASS`. Map, route and maneuver tile on the cluster.
- Several unplug / replug cycles, and a full shutdown with a cold start: the
  cluster map came back every time.
- The logs have not yet been checked for `renderer watchdog:` lines. This car
  recovered on every reconnect, so the test shows no regressions. It does not
  show whether the watchdog ever acted.

To test: connect the iPhone, check the map, unplug for about 20 s, plug in again
and wait up to 30 s. The watchdog may need two 5 s looks plus the restart. Then,
without restarting the MMI, run the script again to collect logs.

## What the logs will show

In `collected/run-NN/logs/` (logs contain the VIN and phone data, so do not post
them publicly):

| Line | Meaning |
|---|---|
| `mib2high-lvds-control.log`: `session #2 since boot` | the reconnect started a new control session |
| `mib2high-lvds-control.log`: `renderer watchdog: close_hung confirmed` or `start_failed confirmed` | cause 2a / 2b was present and the watchdog acted |
| `mib2high-lvds-control.log`: `renderer watchdog: vc_render did not exit` | the renderer could not be stopped; a restart is still needed |
| `vc_render.log`: `NvSSVideoClose ENTER` without a following `EXIT` | cause 2a |
| `vc_render.log`: `NvSS start failed` | cause 2b |
| `carplay_hook.log`: `stub open refused reason=already_active` | cause 1 (not fixed here) |
| `mib2high-lvds-control.log`: no `session #2` at all after the reconnect | the new session never reached the control script (cause 1 likely) |
