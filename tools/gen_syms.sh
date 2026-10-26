#!/usr/bin/env bash
# Writes syms/marioparty3.us.syms.toml and syms/marioparty3.us.datasyms.toml from the ELF tools/build_decomp.sh brought back
# (build/decomp/marioparty3.elf) and your ROM: builds the pinned N64Recomp (tools/build_n64recomp.sh) and
# runs tools/gen_syms.py, which says how.
#
# Usage: tools/gen_syms.sh [cmake arguments for tools/build_n64recomp.sh]
set -euo pipefail
cd "$(dirname "$0")/.."
elf=build/decomp/marioparty3.elf
rom=roms/marioparty3.us.z64
[[ -f "$elf" ]] || { echo "no $elf: run tools/build_decomp.sh first" >&2; exit 1; }
[[ -f "$rom" ]] || { echo "no $rom: put your Mario Party 3 (USA) .z64 there" >&2; exit 1; }
bash tools/build_n64recomp.sh "$@"
if command -v py > /dev/null; then python=(py -3); else python=(python3); fi
"${python[@]}" -I tools/gen_syms.py build/n64recomp/N64Recomp "$elf" "$rom"
