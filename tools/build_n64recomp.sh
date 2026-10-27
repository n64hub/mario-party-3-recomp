#!/usr/bin/env bash
# Builds the recompilers the runtime pins (lib/N64ModernRuntime/N64Recomp: N64Recomp and RSPRecomp) into
# build/n64recomp, configuring it the first time; arguments go to that configure. tools/recompile.sh and
# tools/gen_syms.sh run it first, and a build that is up to date costs nothing.
#
# It patches lib/ first with N64Recomp-browser's patches (which does nothing once they are in): the code the patched
# N64Recomp writes uses the patched recomp.h (RECOMP_TAIL_CALL), and CMakeLists.txt patches the same checkouts.
#
# Usage: tools/build_n64recomp.sh [cmake arguments, e.g. -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl]
set -euo pipefail
cd "$(dirname "$0")/.."
bin=build/n64recomp
mkdir -p build
cmake -P lib/N64Recomp-browser/patches/apply.cmake lib
if [[ ! -f "$bin/build.ninja" ]]; then
    cmake -S lib/N64ModernRuntime/N64Recomp -B "$bin" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@" > "$bin.log" 2>&1 ||
        { tail -20 "$bin.log"; exit 1; }
fi
ninja -C "$bin" N64RecompCLI RSPRecomp >> "$bin.log" 2>&1 || { tail -20 "$bin.log"; exit 1; }
