#!/usr/bin/env bash
# Uruchamia skrypty budowania edzenisa w obrazie qnx65-armv7-toolchain:4.9.
#
# DLACZEGO TO NIE JEST ZWYKŁE `docker run ... bash /src/build.sh`:
# QNX binutils z SDP 6.5 (nm, ar, ld) to 32-bitowe binarki x86 zbudowane bez
# obsługi dużych numerów i-węzłów. Bind-mount Docker Desktop na Windows podaje
# i-węzły nie mieszczące się w 32 bitach, więc stat() w nm zwraca EOVERFLOW:
#
#   nm: Warning: could not locate '....o'. reason: Value too large for defined data type
#   collect2: error: ... /bin/nm returned 1 exit status
#
# Kompilacja (gcc) przechodzi, pada dopiero linkowanie, bo collect2 skanuje
# obiekty przez nm. Dlatego kopiujemy drzewo źródeł do systemu plików kontenera
# (overlayfs, małe i-węzły), budujemy tam i kopiujemy artefakty z powrotem.
#
# Skryptów budowania edzenisa NIE modyfikujemy - to obejście środowiska, nie łatka
# na jego kod. Kompilator jest ten sam: GCC 4.9.4 przez shim qcc.
#
# Użycie:
#   tools/qnx_build.sh <skrypt-względem-altscreen> [artefakt-względem-altscreen ...]
# Przykład:
#   tools/qnx_build.sh build.sh output/libmib2high-carplay-rgi.so

set -euo pipefail

# Katalog projektu (ten, w ktorym lezy tools/). Git Bash na Windows: pwd -W daje
# sciezke w postaci C:/... potrzebnej Docker Desktop; na Linuksie zwykle pwd.
# Mozna nadpisac zmienna PROJ_WIN.
PROJ_WIN="${PROJ_WIN:-$(cd "$(dirname "$0")/.." && (pwd -W 2>/dev/null || pwd))}"
IMAGE="qnx65-armv7-toolchain:4.9"

SCRIPT="${1:?uzycie: qnx_build.sh <skrypt> [artefakt ...]}"
shift
ARTIFACTS=("$@")

# Polecenie wykonywane w kontenerze. /src to bind-mount (wolno czytać/zapisywać,
# ale nie wolno na nim linkować), /build to kopia w systemie plików kontenera.
remote_cmd="set -e
cp -a /src /build
cd /build
bash /build/${SCRIPT}
"
for a in "${ARTIFACTS[@]}"; do
    remote_cmd="${remote_cmd}
if [ ! -e \"/build/${a}\" ]; then echo \"FAIL: brak artefaktu /build/${a}\" >&2; exit 3; fi
mkdir -p \"/src/\$(dirname \"${a}\")\"
cp -a \"/build/${a}\" \"/src/${a}\"
echo \"ARTIFACT ${a}\"
file \"/src/${a}\"
sha256sum \"/src/${a}\"
"
done

MSYS_NO_PATHCONV=1 exec docker run --rm --platform=linux/amd64 \
    -v "${PROJ_WIN}/port_k3346/altscreen:/src" \
    -v "${PROJ_WIN}/work:/work" \
    -e QNX_RUNTIME=/work/qnx_runtime \
    "${IMAGE}" bash -c "${remote_cmd}"
