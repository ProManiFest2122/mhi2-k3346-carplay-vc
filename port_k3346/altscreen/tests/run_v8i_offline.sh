#!/bin/bash
set -euo pipefail
D="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$D/tests/test_v8i_fixes.py"
python3 "$D/tests/test_v8i_trace_scope.py"
python3 "$D/tools/verify_v8i.py"
J="$D/reference/mib2high-carplay-rgi.live.jar"
[ -f "$J" ]
javap -c -p -classpath "$J" com.luka.carplay.routeguidance.BAPBridge > /tmp/v8i_BAPBridge_javap.txt
grep -q 'updateHUDDisplayContent:(Z)V' /tmp/v8i_BAPBridge_javap.txt
grep -q 'Field VC_RGI_ENABLED:Z' /tmp/v8i_BAPBridge_javap.txt
echo 'V8I_JAVA_STATIC_AUDIT=PASS (NO JAVA MODIFICATIONS)'
