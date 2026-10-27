#!/usr/bin/env bash
# Records src/rt64_shaders.txt, the shaders the page builds before the game and as each board and minigame starts, by
# code rather than by playing: N64Recomp-browser's tools/record_shaders.mjs serves the page built in <build dir>
# (tools/build_web.sh's build/web), drives it in headless Chrome through tools/shader_plan.json, a page load a section,
# and writes the list. Needs node 22 or later and Chrome. Then build the page again, so that it prebuilds the new list.
#
#   bash tools/record_shaders.sh <build dir> <ROM> [--profile <dir>] [--port <n>] [--only <section>,...]
set -euo pipefail
[ $# -ge 2 ] || { sed -n '7p' "$0"; exit 1; }
cd "$(dirname "$0")/.."
build="$1" rom="$2"
shift 2
exec node lib/N64Recomp-browser/tools/record_shaders.mjs tools/shader_plan.json "$build" "$rom" src/rt64_shaders.txt "$@"
