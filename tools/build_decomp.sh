#!/usr/bin/env bash
# Builds the Mario Party 3 decomp (lib/marioparty3's commit, US only) from your ROM and brings its ELF back for
# tools/gen_syms.sh. It runs in tools/docker/Dockerfile.decomp's image with Docker, here or on a Linux host over ssh
# (BUILD_HOST). Never in CI: the build needs the ROM.
#
# `make -C tools` downloads the compilers it builds with, `make setup` splits the ROM's code and data with splat
# (marioparty3.yaml), `make` builds them with KMC gcc 2.7.2 (libm with IDO 5.3) and checks the ROM it links against the one you gave it. gcc puts every function, static ones too, in the ELF's symbol
# table, so the ELF is used as it is linked. Into build/decomp/: marioparty3.elf. Nothing from it is committed but what
# tools/gen_syms.sh writes from it.
#
# Usage: [BUILD_HOST=<ssh host>] [DECOMP_DIR=<folder under ~, default marioparty3-decomp>] tools/build_decomp.sh [roms/marioparty3.us.z64]
set -euo pipefail
cd "$(dirname "$0")/.."
rom="${1:-roms/marioparty3.us.z64}"
sha1="6beb80ff822b96bcf85dcdb512e8b2b7969d8259"
[[ "$(sha1sum "$rom" | cut -d' ' -f1)" == "$sha1" ]] || { echo "$rom isn't Mario Party 3 (USA) as a .z64" >&2; exit 1; }
commit="$(git ls-tree HEAD lib/marioparty3 | awk '{print $3}')"
image="marioparty3-decomp:$(git hash-object tools/docker/Dockerfile.decomp | cut -c1-12)"
work="${DECOMP_DIR:-marioparty3-decomp}/${commit:0:12}"

if [[ -n "${BUILD_HOST:-}" ]]; then
    run() { ssh "$BUILD_HOST" "cd && $1"; }
    put() { scp -q "$1" "$BUILD_HOST:$2"; }
    get() { scp -q "$BUILD_HOST:$1" "$2"; }
else
    run() { (cd ~ && bash -c "$1"); }
    put() { cp "$1" ~/"$2"; }
    get() { cp ~/"$1" "$2"; }
fi

run "mkdir -p $work.docker"
put tools/docker/Dockerfile.decomp "$work.docker/Dockerfile"
run "docker image inspect $image > /dev/null 2>&1 || docker build -q -t $image $work.docker > /dev/null"
run "[ -d $work/.git ] || git clone -q https://github.com/mariopartyrd/marioparty3 $work
     git -C $work checkout -q $commit && git -C $work submodule update -q --init --recursive"
put "$rom" "$work/baserom.us.z64"
run "docker run --rm --name marioparty3-decomp-${commit:0:12} -u \$(id -u):\$(id -g) -e HOME=/tmp -v \$HOME/$work:/mp -w /mp $image bash -c '
    set -e
    [ -x venv/bin/python3 ] || { python3 -m venv venv && venv/bin/pip install -q -r requirements.txt; }
    [ -x tools/gcc_2.7.2/linux/gcc ] || make -C tools > tools.log 2>&1 || { tail -30 tools.log; exit 1; }
    make setup > setup.log 2>&1 || { tail -30 setup.log; exit 1; }
    make -j\$(nproc) > build.log 2>&1 || { tail -30 build.log; exit 1; }
    tail -1 build.log | grep -qx OK || { echo \"the build does not match the ROM\"; tail -5 build.log; exit 1; }'"

mkdir -p build/decomp
get "$work/build/marioparty3.elf" build/decomp/
echo "build/decomp/: the decomp's ELF at ${commit:0:12}, matching the ROM; next, tools/gen_syms.sh"
