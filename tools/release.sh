#!/usr/bin/env bash
# Builds the desktop program's release, for Windows and for Linux, and publishes both as one GitHub release. CI has no
# ROM (none is ever uploaded), so it can't recompile the game; a release is built where the ROM is, from
# roms/marioparty3.us.z64, and holds nothing from it: the player picks theirs with Load ROM, as on the page.
#
#     tools/release.sh windows   # Git Bash on Windows, with the README's toolchain: out/<name>-windows-x64.zip
#     tools/release.sh linux     # Linux with Docker: out/<name>-linux-x64.tar.gz
#     tools/release.sh publish [--prerelease]   # both archives, built from this commit: the GitHub release (gh
#                                               # signed in), marked a prerelease with --prerelease
#
# The tag is v<version_string> from src/main/main.cpp, so bump that first, and write the release's notes beside it in
# releases/v<version>.md: what a player gets, not a list of merged PRs. Both builds and the publish run on the same
# commit, origin/main's, with a clean tree: each archive records the commit it was built from (out/<archive>.commit),
# and publish refuses archives from another one, a tree with changes, or a tag that exists.
#
# Windows: Release (CMakeLists.txt makes that a windowed program, with no console), the DLLs beside the exe as the
# build leaves them (SDL2, dxcompiler, dxil). It needs the Visual C++ 2015-2022 redistributable (x64). clang-cl, CMake
# and Ninja are on PATH, and MIPS_CC and MIPS_LD name the MIPS clang and ld.lld that build patches/.
# Linux: in the image tools/docker/Dockerfile.linux, Ubuntu 22.04, so the binary needs glibc 2.35 or newer. It carries
# the libSDL2 it was built with and libatomic (not on every system) beside the binary (RUNPATH $ORIGIN), as the Windows
# zip carries SDL2.dll, and is a .tar.gz so the binary stays executable. GTK 3, FreeType and a Vulkan driver are the
# system's. Run from another folder, the program works from its own (src/main/main.cpp).
# Both hold assets/ (the launcher's), recompcontrollerdb.txt and LICENSE beside the program.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

version=$(sed -n 's/^const std::string version_string = "\(.*\)";$/\1/p' src/main/main.cpp)
[ -n "$version" ] || { echo "no version_string in src/main/main.cpp" >&2; exit 1; }
tag="v$version"
name="MarioParty3Recompiled-$tag"
commit=$(git rev-parse HEAD)
mkdir -p out

setup() {
    [ -f roms/marioparty3.us.z64 ] || { echo "no roms/marioparty3.us.z64 (README, Building)" >&2; exit 1; }
    git submodule update --init --recursive
}

case "${1:-}" in
windows)
    [ -n "${MIPS_CC:-}" ] && [ -n "${MIPS_LD:-}" ] || { echo "set MIPS_CC and MIPS_LD (README, Building)" >&2; exit 1; }
    setup
    bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
    build=build/release-windows
    cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl \
        -DPATCHES_C_COMPILER="$MIPS_CC" -DPATCHES_LD="$MIPS_LD" > "$build.log" 2>&1 || { tail -20 "$build.log"; exit 1; }
    ninja -C "$build" MarioParty3Recompiled > "$build/build.log" 2>&1 ||
        { grep -E " error|FAILED|undefined" "$build/build.log" | head -20; exit 1; }
    stage="out/$name-windows-x64"
    rm -rf "$stage" "$stage.zip"
    mkdir -p "$stage"
    cp "$build"/{MarioParty3Recompiled.exe,SDL2.dll,dxil.dll,dxcompiler.dll} recompcontrollerdb.txt LICENSE "$stage"/
    cp -r assets "$stage"/
    # CMake's own zip, so packing needs nothing the build doesn't.
    (cd out && cmake -E tar cf "$name-windows-x64.zip" --format=zip "$name-windows-x64")
    echo "$commit" > "$stage.zip.commit"
    echo "$stage.zip ($(du -h "$stage.zip" | cut -f1))"
    ;;
linux)
    setup
    image=marioparty3-linux:$(sha256sum tools/docker/Dockerfile.linux | cut -c1-12)
    mkdir -p build/home build/ccache
    docker image inspect "$image" > /dev/null 2>&1 || docker build -t "$image" -f tools/docker/Dockerfile.linux tools/docker
    docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:/src" -w /src -e CCACHE_DIR=/src/build/ccache -e HOME=/src/build/home \
        "$image" bash -euo pipefail -c "
        git config --global --add safe.directory '*'
        bash tools/recompile.sh -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
        build=build/release-linux
        mkdir -p \$build
        cmake -S . -B \$build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
            -DPATCHES_C_COMPILER=clang -DPATCHES_LD=ld.lld -DCMAKE_C_COMPILER_LAUNCHER=ccache \
            -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > \$build.log 2>&1 || { tail -20 \$build.log; exit 1; }
        ninja -C \$build MarioParty3Recompiled > \$build/build.log 2>&1 ||
            { grep -E ' error|FAILED|undefined' \$build/build.log | head -20; exit 1; }
        stage=out/$name-linux-x64
        rm -rf \$stage \$stage.tar.gz
        mkdir -p \$stage
        cp \$build/MarioParty3Recompiled recompcontrollerdb.txt LICENSE \$stage/
        cp -L /usr/local/lib/libSDL2-2.0.so.0 /usr/lib/x86_64-linux-gnu/libatomic.so.1 \$stage/
        patchelf --set-rpath '\$ORIGIN' \$stage/MarioParty3Recompiled
        cp -r assets \$stage/
        tar -C out -czf \$stage.tar.gz $name-linux-x64"
    echo "$commit" > "out/$name-linux-x64.tar.gz.commit"
    echo "out/$name-linux-x64.tar.gz ($(du -h "out/$name-linux-x64.tar.gz" | cut -f1))"
    ;;
publish)
    case "${2:-}" in
    --prerelease) kind=(--prerelease) ;;
    "") kind=() ;;
    *) echo "publish takes --prerelease or nothing" >&2; exit 1 ;;
    esac
    notes="releases/$tag.md"
    [ -s "$notes" ] || { echo "no $notes: write the release notes for players first" >&2; exit 1; }
    [ -z "$(git status --porcelain --ignore-submodules=dirty)" ] || { echo "the tree has uncommitted changes" >&2; exit 1; }
    git fetch -q origin main
    [ "$commit" = "$(git rev-parse origin/main)" ] || { echo "HEAD is not origin/main" >&2; exit 1; }
    ! gh release view "$tag" > /dev/null 2>&1 || { echo "release $tag exists: bump version_string first" >&2; exit 1; }
    files=("out/$name-windows-x64.zip" "out/$name-linux-x64.tar.gz")
    for f in "${files[@]}"; do
        [ -f "$f" ] || { echo "no $f: run tools/release.sh windows and linux first" >&2; exit 1; }
        [ "$(cat "$f.commit" 2> /dev/null)" = "$commit" ] || { echo "$f wasn't built from $commit" >&2; exit 1; }
    done
    gh release create "$tag" "${files[@]}" --target "$commit" --title "Mario Party 3: Recompiled $tag" \
        --notes-file "$notes" "${kind[@]}"
    ;;
*)
    echo "usage: tools/release.sh windows|linux|publish [--prerelease]" >&2
    exit 1
    ;;
esac
