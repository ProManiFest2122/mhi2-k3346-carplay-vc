#!/usr/bin/env bash
# Host tests for the parts of the overlay that are plain C and have nothing to
# do with QNX.  Run inside the toolchain container (it has a native gcc):
#   docker run --rm -v ...:/src qnx65-armv7-toolchain:4.9 \
#     bash -c 'cd /src && bash overlay/tests/run_host_tests.sh'
# or through tools/qnx_build.sh, which already mounts the tree.
set -euo pipefail
# The image puts the QNX cross binutils on PATH, so a bare "gcc" would call
# arm-...-as and die with "unrecognized option --64".  Host tests want the host
# toolchain only.
export PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin
D="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ALT="$(cd -- "$D/../.." && pwd)"

# Build OUTSIDE the source tree, and in a directory private to this run.
#
# The tree arrives here as a Docker bind mount from Windows, shared by every
# container started against it.  Building into "$D/.hostbuild" meant two test
# runs at once wrote and executed THE SAME binaries: one container would
# overwrite an executable while another was running it, and the running process
# would spin at 100% CPU forever instead of failing.  That happened twice on
# 2026-10-27 - one container burned 5 hours of CPU before anyone noticed, and a
# second one looked like a regression in code the tests do not even compile.
# $$ keeps concurrent runs apart; /tmp is container-local, so nothing lands on
# the host and nothing is shared.
OUT="${HOST_TESTS_OUT:-/tmp/overlay-hostbuild.$$}"
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT

# UBSan + ASan: the config parser handles text a human typed, and this is the
# cheap place to catch an overrun, not the car.
CFLAGS=(-std=gnu99 -O1 -g -Wall -Wextra -Werror
        -fsanitize=address,undefined -fno-omit-frame-pointer)

# ASan here fails to start roughly one run in six, printing
# "AddressSanitizer:DEADLYSIGNAL" and then SPINNING FOREVER at 100% CPU instead
# of exiting.  It is the known ASan/ASLR-entropy mismatch on recent kernels; the
# usual workarounds are not available to us (setarch -R needs a personality
# syscall this container's seccomp profile denies, and the host sysctl is inside
# Docker Desktop's VM).  Measured on 2026-10-27: 1 failure in 6 runs, unchanged
# by ASAN_OPTIONS=detect_leaks=0.
#
# This cost two wrong conclusions in one day - once it looked like a regression
# in code these tests do not even compile, and one forgotten container burned
# five hours of CPU.  So: every test runs under a timeout and a startup crash is
# retried.  A REAL failure is not retried, because a test that ran prints its
# "N OK, M FAIL" summary and that is what we look for.
run_test() {
    label="$1"; shift
    attempt=1
    while [ "$attempt" -le 3 ]; do
        rc=0
        # 60 s, not 180: the slowest of these (the feed's socket test) finishes
        # in seconds, and a spinning ASan start is pure waiting.  -k sends
        # SIGKILL if SIGTERM is not enough, so a wedged run can never outlive
        # the suite the way one did on 2026-10-27.
        timeout -k 10 60 "$@" > "$OUT/$label.out" 2>&1 || rc=$?
        if grep -qE "OK, [0-9]+ FAIL" "$OUT/$label.out"; then
            cat "$OUT/$label.out"
            return "$rc"
        fi
        echo "   (ASan nie wystartowal - proba $attempt/3, rc=$rc)"
        attempt=$((attempt + 1))
    done
    cat "$OUT/$label.out"
    echo "FAIL: $label nie wystartowal w trzech probach"
    return 1
}

echo "== overlay_cfg (config file the user edits on the card)"
gcc "${CFLAGS[@]}" -o "$OUT/overlay_cfg_test" "$D/overlay_cfg_test.c"
( cd "$ALT" && run_test overlay_cfg "$OUT/overlay_cfg_test" )

echo
echo "== overlay_ready (the marker the shell gate trusts)"
gcc "${CFLAGS[@]}" -o "$OUT/overlay_ready_test" "$D/overlay_ready_test.c"
run_test overlay_ready "$OUT/overlay_ready_test"

echo
echo "== overlay_layout (VC gauge layout read from the hook log)"
gcc "${CFLAGS[@]}" -o "$OUT/overlay_layout_test" "$D/overlay_layout_test.c"
run_test overlay_layout "$OUT/overlay_layout_test"

echo
echo "== overlay_blit (where the rendered tile lands on the cluster)"
gcc "${CFLAGS[@]}" -o "$OUT/overlay_blit_test" "$D/overlay_blit_test.c" -lm
run_test overlay_blit "$OUT/overlay_blit_test"

echo
echo "== overlay_fbo (only a format that keeps alpha may reach the cluster)"
gcc "${CFLAGS[@]}" -o "$OUT/overlay_fbo_test" "$D/overlay_fbo_test.c"
run_test overlay_fbo "$OUT/overlay_fbo_test"

echo
# ENABLE_LOGGING=0 turns the LOG_* macros into no-ops so the test does not need
# the hook's logging backend (which is QNX-flavoured) just to check bytes.
echo
echo "== overlay_feed wire format (decoded with Luka's own cr_decode_maneuver)"
gcc "${CFLAGS[@]}" -DENABLE_LOGGING=0 \
    -I"$ALT/native" -I"$ALT/native/framework" -I"$ALT/native/routeguidance" \
    -I"$ALT/overlay/upstream" \
    -o "$OUT/overlay_feed_test" \
    "$ALT/native/tests/overlay_feed_test.c" \
    "$ALT/native/routeguidance/overlay_feed.c" \
    "$ALT/native/routeguidance/maneuver_map.c" \
    -lpthread
run_test overlay_feed "$OUT/overlay_feed_test"

echo "OVERLAY_HOST_TESTS=PASS"
