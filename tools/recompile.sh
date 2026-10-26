#!/usr/bin/env bash
# Generates RecompiledFuncs/ from roms/marioparty3.us.z64: the game's code from the symbols in syms/ with N64Recomp
# (us.toml), and the audio microcode with RSPRecomp (n_aspMain.us.toml, RecompiledFuncs/n_aspMain_rsp.cpp). It is wiped and
# written again, and the game's CMake configure needs it; a file that comes out the same as before keeps its old time,
# so a build recompiles only the ones that changed. N64Recomp and RSPRecomp are the ones the runtime pins, built by
# tools/build_n64recomp.sh, which takes the arguments.
#
# Usage: tools/recompile.sh [cmake arguments, e.g. -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl]
set -euo pipefail
cd "$(dirname "$0")/.."
bash tools/build_n64recomp.sh "$@"
old="$(mktemp -d)"
trap 'rm -rf "$old"' EXIT
mkdir -p RecompiledFuncs
mv RecompiledFuncs "$old/"
mkdir RecompiledFuncs
build/n64recomp/N64Recomp us.toml > recomp.log 2>&1 || { tail -20 recomp.log; exit 1; }
build/n64recomp/RSPRecomp n_aspMain.us.toml >> recomp.log 2>&1 || { tail -20 recomp.log; exit 1; }
for f in RecompiledFuncs/*; do
    prev="$old/$f"
    if cmp -s "$f" "$prev"; then touch -r "$prev" "$f"; fi
done
