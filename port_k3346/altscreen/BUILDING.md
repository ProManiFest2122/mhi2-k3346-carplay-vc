# Building AltScreen v1.0 from public editable sources

**Important:** The car-tested v8S *compiled installer* has not been modified. Its fixed archive SHA-256 is `162792d28e1a91ade216f41de6b8ff30f848955e285d7de990b34c64fcb7b0ae`. The compiled artifacts distributed in v1.0 are **not automatically reproduced byte-for-byte** by contemporary QNX/JDK installations.

## Source-only policy

All behavioral changes must be made to editable sources and recompiled. **Never patch ELF, `.so`, `.o`, `.class` or `.jar` files directly.** A new compiled build requires fresh vehicle validation before replacing the car-tested release.

## Native CarPlay / AltScreen receiver

Use a licensed QNX SDP 6.5 ARMv7 GCC 4.9 toolchain, with a compatible `QNX_RUNTIME/activate.sh`. Set `QNX_RUNTIME` and run:

```sh
cd altscreen
QNX_RUNTIME=/path/to/qnx65-armv7-gcc49-runtime ./build.sh
```

Output: `output/libmib2high-carplay-rgi.so`. `build.sh` runs the portable `tools/verify_current_native.py` ELF sanity gate. The original `tools/verify_v8i.py` relies on a private known-good v8g binary and a developer-specific `readelf` path; it is retained as **historical parity tooling only**, enabled explicitly via `V8I_HISTORICAL_PARITY=1`.

## Java RGI integration

Use **JDK 8** and the authorized OEM-derived compile-only K2161 dependency JAR; the vendor JAR must be obtained separately and is **not redistributable here**.

```sh
cd altscreen
JAVA_HOME=/path/to/jdk8 K2161_BASE_JAR=/path/to/authorized/lsd-K2161-base.jar \
  sh java/build_java_release.sh
```

Output: `output/mib2high-carplay-rgi.jar`. The release build checks legacy JVM class-major **48** and expected entrypoints without a historical private comparison JAR.

`java/build_java_v8m.sh` and `java/build_java_v8k.sh` remain preserved **historical** scripts. Their private inputs must not be confused with the new supported source build path.

## Instrument-cluster video renderer

Pinned upstream editable code: `renderer/upstream/vc_render.c` from Joey's MHI2-altScreen (GPL-3.0). With QNX variables and `qcc` configured:

```sh
QNX_HOST=/path/to/qnx-host QNX_TARGET=/path/to/qnx-target \
  sh altscreen/renderer/build_renderer.sh
```

The source tree also contains [`renderer/k2161/vc_render.c`](renderer/k2161/vc_render.c), recovered exactly from this project's K2161 GEN2/AV11 checkpoint. This code differs from the upstream renderer's defaults. To compile **that adapted checkpoint** instead, with the same licensed QNX environment, pass its C file to the documented `qcc -Vgcc_ntoarmv7le -mfloat-abi=softfp -O2 -g -std=gnu99 -Wall -Wextra -lsocket` build recipe. This test was performed with the archived toolchain and generated a valid ARM ELF, but its hash did not match the car-tested v8S binary. See [provenance](renderer/PROVENANCE.md).

The renderer is invoked with `--out 1 --layer 58 --w 1440 --h 542 --fps 30 --ts wall` by the **unchanged** controller. The released v8S executable itself is hash-preserved. **The exact compiled v8S renderer's adaptation/source correspondence has not been proven**; see `renderer/PROVENANCE.md`. Do not swap this source-built renderer into v8S and claim it has passed car tests.

## SD installer

Editable sources: `deployment/custom.sh`, `deployment/mib2high-lvds-control.sh`, `deployment/{bundles,smartphone,trace}.awk`, and `config/*`. All three AWK files are copied exactly from the verified v8S compiled installer.

Source integrity check (works without QNX):

```sh
python3 altscreen/tools/verify_public_source.py
python3 altscreen/tools/verify_public_source.py /path/to/AltScreen-v1.0-install.zip
```

The second command checks every packaged file against the immutable v8S manifest. Tests of the actual installed runtime remain separate.

## Host control and installer tests

```sh
cc -std=gnu99 -O0 -pthread -ffunction-sections -fdata-sections \
  altscreen/tests/test_v8s_control_mock.c -Wl,--gc-sections -ldl -o /tmp/v8s-control-test
/tmp/v8s-control-test
```

`diagnostics/test_v8s_release.py` is an archived integration fixture that hardcodes private original development inputs. It is **not** a portable CI test. The live-tested v8S installer and source hashes are recorded in `release_evidence/`.

## Licensing and archived manifests

See [repository licensing](../LICENSE), [renderer provenance](renderer/PROVENANCE.md) and [archived manifests](ARCHIVED_MANIFEST_SCOPE.md).
