#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
workspace=$(cd "$here/../.." && pwd)
jdk="$workspace/runtime/k3346-decomp/tc/zulu8.96.0.205-ca-jdk8.0.504-macosx_aarch64/Contents/Home"
stock="$workspace/runtime/k3346-decomp/lsd-k3346.jar"
out="$here/artifacts/java"
mkdir -p "$out/classes"
"$jdk/bin/javac" -source 1.4 -target 1.4 -Xlint:-options -encoding UTF-8 -cp "$stock" -d "$out/classes" \
 "$here/prototype/java/src/com/luka/carplay/core/UpmixSetting.java" \
 "$here/prototype/java/src/com/luka/carplay/tone/UpmixToneUi.java" \
 "$here/prototype/java/src/de/audi/tghu/tone/hmi/Activator.java"
"$jdk/bin/jar" cf "$out/mhi2_audio_ui.jar" -C "$out/classes" .
