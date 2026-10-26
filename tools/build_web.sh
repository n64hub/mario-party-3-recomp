#!/usr/bin/env bash
# Builds the page into build/web/MarioParty3Recompiled.html (with its .js, .wasm, .data and coi_serviceworker.js) in the
# image tools/docker/Dockerfile.web: tools/recompile.sh, then the game's CMake build with Emscripten, ccache in
# build/ccache-web. It needs roms/marioparty3.us.z64, as the desktop build does. Linux with Docker; run from anywhere in the
# checkout:
#
#     tools/build_web.sh [cpus] [cmake arguments, e.g. -DN64RECOMP_BROWSER_FAST_LINK=ON for a quick try]
#
# Serve build/web as docs/web.md says, or play it with N64Recomp-browser's tools/play_page.mjs.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CPUS=${1:-$(nproc)}
shift || true
git -C "$ROOT" submodule update --init --recursive lib/N64ModernRuntime lib/rt64 lib/RecompFrontend lib/N64Recomp-browser
# The decomp only for its headers, which patches/ includes.
git -C "$ROOT" submodule update --init lib/marioparty3
IMAGE=mp3-web:$(sha256sum "$ROOT/tools/docker/Dockerfile.web" | cut -c1-12)
mkdir -p "$ROOT/build/home" "$ROOT/build/ccache-web"
docker image inspect "$IMAGE" >/dev/null 2>&1 || docker build -t "$IMAGE" -f "$ROOT/tools/docker/Dockerfile.web" "$ROOT/tools/docker"
docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:/src" -w /src -e CCACHE_DIR=/src/build/ccache-web -e HOME=/src/build/home "$IMAGE" bash -euo pipefail -c "
    git config --global --add safe.directory '*'
    bash tools/recompile.sh -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
    # The ports CMake finds and ninja depends on, in the image's own cache, which goes with the container (seconds).
    embuilder build sdl2-mt freetype-legacysjlj > /dev/null 2>&1
    [ -f build/web/build.ninja ] || emcmake cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release $* > build/web-configure.log 2>&1 ||
        { tail -20 build/web-configure.log; rm -f build/web/build.ninja; exit 1; }
    ninja -C build/web -j$CPUS MarioParty3Recompiled > build/web-build.log 2>&1 ||
        { grep -E ' error|FAILED|undefined' build/web-build.log | head -30; exit 1; }
    ls -la build/web/MarioParty3Recompiled.*
"
