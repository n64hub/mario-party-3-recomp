#!/usr/bin/env bash
# Builds the desktop program's release, for Windows and for Linux, and publishes both as one GitHub release. CI has no
# ROM (none is ever uploaded), so it can't recompile the game; a release is built where the ROM is, from
# roms/marioparty3.us.z64, and holds nothing from it: the player picks theirs with Load ROM, as on the page.
#
#     tools/release.sh windows   # Git Bash on Windows, with the README's toolchain: out/<name>-windows-x64.zip
#     tools/release.sh linux     # Linux with Docker: out/<name>-linux-x64.tar.gz
#     tools/release.sh boot windows   # Git Bash on Windows: boots that archive from a new folder, out/boot-windows
#     tools/release.sh boot linux     # Linux with Docker: the same for the Linux archive, out/boot-linux
#     tools/release.sh publish [--prerelease]   # both archives, built from this commit: the GitHub release (gh
#                                               # signed in), marked a prerelease with --prerelease
#
# One release carries every change merged since the last: each change's PR adds what the player gets to
# releases/next.md, and a release is a PR titled "Release v<version>" that bumps version_string in src/main/main.cpp and
# moves releases/next.md to releases/v<version>.md: what a player gets, not a list of merged PRs. The tag is
# v<version_string>. Both builds and the publish run on that PR's merge commit, checked out with a clean tree: each
# build moves the submodules onto their pins (unpatch, below), refuses a tree with changes and records the commit it was
# built from (out/<archive>.commit), and publish refuses archives from another one or that haven't booted (boot), a tree
# with changes, a commit not on origin/main or still holding releases/next.md, or a tag that exists. A change merged
# meanwhile goes in the next release and doesn't stop this one.
#
# Windows: Release (CMakeLists.txt makes that a windowed program, with no console), the DLLs beside the exe as the
# build leaves them (SDL2, dxcompiler, dxil). It needs the Visual C++ 2015-2022 redistributable (x64). clang-cl, CMake
# and Ninja are on PATH, and MIPS_CC and MIPS_LD name the MIPS clang and ld.lld that build patches/.
# Windows builds through ccache when it is on PATH (CMakeLists.txt says how): its cache is the user's, outside the
# tree, so a release built in a fresh clone compiles only what changed since the last one built here (the shaders dxc
# builds always build). A clone kept from one release to the next keeps build/release-windows as well, so there only
# what changed builds, the shaders included (tools/recompile.sh keeps the times of the files that come out the same).
# boot runs $BOOT_RELEASE on the archive, which unpacks it into a new folder with the ROM stored as Load ROM stores it,
# runs it with --boot (and any arguments after the OS) for 30 s with its own window captured, and writes the
# <archive>.booted publish asks for.
# Linux: in the image tools/docker/Dockerfile.linux, Ubuntu 22.04, so the binary needs glibc 2.35 or newer. It carries
# the libSDL2 it was built with and libatomic (not on every system) beside the binary (RUNPATH $ORIGIN), as the Windows
# zip carries SDL2.dll, and is a .tar.gz so the binary stays executable. GTK 3, FreeType and a Vulkan driver are the
# system's.
# With BUILD_HOST set (an ssh host with Docker) both archives build on that Linux host instead, and the Linux one boots
# there: REMOTE_BUILD names rush-2049-recomp's tools/remote_build.sh, which runs each step in a container there (holding
# a turn in the host's queue when it has one) in a copy of this commit kept between releases, release-<this folder's
# name>, and brings back only the archives and the boot's captures. The Windows one is cross-built there (remote_build.sh
# windows, a Release build in its tools/docker/Dockerfile.windows, which says how), the Linux one in
# tools/docker/Dockerfile.linux as here. publish and boot windows stay on Windows, where the clone is.
# Both hold assets/ (the launcher's), recompcontrollerdb.txt and LICENSE beside the program, which works from its own
# folder when it is started from another (src/main/main.cpp).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

# Windows' paths stop at 260 characters, and the submodules' deepest files (rt64's spirv-cross tests) are 184 long
# inside the clone, so a clone in a deep folder can't check them out. core.longpaths set in the clone doesn't reach
# the submodules' own clones; set in the environment, it reaches every git command below, theirs included. Elsewhere
# than Windows it changes nothing.
export GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=core.longpaths GIT_CONFIG_VALUE_0=true

version=$(sed -n 's/^const std::string version_string = "\(.*\)";$/\1/p' src/main/main.cpp)
[ -n "$version" ] || { echo "no version_string in src/main/main.cpp" >&2; exit 1; }
tag="v$version"
name="MarioParty3Recompiled-$tag"
commit=$(git rev-parse HEAD)
mkdir -p out

clean() {
    [ -z "$(git status --porcelain --ignore-submodules=dirty)" ] || { echo "the tree has uncommitted changes" >&2; exit 1; }
}

# Each configure patches the submodules' files (N64Recomp-browser's patches/apply.cmake, from CMakeLists.txt), so a
# clone kept from one release to the next holds the last build's patches. Where the pins or the patches have moved
# since, the files go back to their commits before the submodules move, so the new patches go onto pinned files as in
# a fresh clone and never merge over the old ones. The commit that last moved them is kept in .git, outside the tree.
unpatch() {
    local stamp moved
    stamp=$(git rev-parse --git-path release-patched-for)
    moved=$(git log -1 --format=%H -- .gitmodules lib runtime-patches)
    if [ "$(cat "$stamp" 2> /dev/null)" != "$moved" ]; then
        git submodule foreach -q --recursive 'git checkout -q -- . && git clean -qfd'
        echo "$moved" > "$stamp"
    fi
}

setup() {
    [ -f roms/marioparty3.us.z64 ] || { echo "no roms/marioparty3.us.z64 (README, Building)" >&2; exit 1; }
    unpatch
    git submodule sync -q --recursive
    git submodule update --init --recursive
    clean
}

# The steps BUILD_HOST runs (the header), from a clean tree with the ROM, which the copy gets too.
rb() {
    [ -f roms/marioparty3.us.z64 ] || { echo "no roms/marioparty3.us.z64 (README, Building)" >&2; exit 1; }
    [ -f "${REMOTE_BUILD:-}" ] || { echo "set REMOTE_BUILD to rush-2049-recomp's tools/remote_build.sh" >&2; exit 1; }
    clean
    bash "$REMOTE_BUILD" -C "$ROOT" -n "release-${ROOT##*/}" "$@"
}

case "${1:-}" in
windows)
    if [ -n "${BUILD_HOST:-}" ]; then
        build=build/release-windows-cross
        rb windows MarioParty3Recompiled -DCMAKE_BUILD_TYPE=Release
        rm -rf "$build"
        rb get build/xwin-windows "$build"
    else
        [ -n "${MIPS_CC:-}" ] && [ -n "${MIPS_LD:-}" ] || { echo "set MIPS_CC and MIPS_LD (README, Building)" >&2; exit 1; }
        setup
        bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
        build=build/release-windows
        cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl \
            -DPATCHES_C_COMPILER="$MIPS_CC" -DPATCHES_LD="$MIPS_LD" > "$build.log" 2>&1 || { tail -20 "$build.log"; exit 1; }
        ninja -C "$build" MarioParty3Recompiled > "$build/build.log" 2>&1 ||
            { grep -E " error|FAILED|undefined" "$build/build.log" | head -20; exit 1; }
    fi
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
    # The build, in tools/docker/Dockerfile.linux's image: on BUILD_HOST (the header) or in this host's Docker.
    linux_build="set -euo pipefail
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
    if [ -n "${BUILD_HOST:-}" ]; then
        rb -d tools/docker/Dockerfile.linux sync > /dev/null
        rb -d tools/docker/Dockerfile.linux run "rm -rf out; $linux_build"
        rb get out out
    else
        setup
        image=marioparty3-linux:$(sha256sum tools/docker/Dockerfile.linux | cut -c1-12)
        mkdir -p build/home build/ccache
        docker image inspect "$image" > /dev/null 2>&1 || docker build -t "$image" -f tools/docker/Dockerfile.linux tools/docker
        docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:/src" -w /src -e CCACHE_DIR=/src/build/ccache -e HOME=/src/build/home \
            "$image" bash -c "$linux_build"
    fi
    echo "$commit" > "out/$name-linux-x64.tar.gz.commit"
    echo "out/$name-linux-x64.tar.gz ($(du -h "out/$name-linux-x64.tar.gz" | cut -f1))"
    ;;
boot)
    case "${2:-}" in
    windows) archive="out/$name-windows-x64.zip" ;;
    linux) archive="out/$name-linux-x64.tar.gz" ;;
    *) echo "usage: tools/release.sh boot windows|linux [program args...]" >&2; exit 1 ;;
    esac
    boot=${BOOT_RELEASE:-}
    [ -n "$boot" ] && [ -f "$boot" ] || {
        echo "set BOOT_RELEASE to the script that boots a release archive: <archive> <rom> <game id> <dir> [args...]" >&2
        exit 1
    }
    if [ "$2" = linux ] && [ -n "${BUILD_HOST:-}" ]; then
        # In the copy the archive was built in, inside the image $BOOT_RELEASE boots it in (its .Dockerfile), which
        # has no Docker, so the script boots it right there. Its <archive>.booted comes back with the captures.
        rb -d "${boot%.sh}.Dockerfile" run "mkdir -p ~/boot && cat > ~/boot/boot_release.sh << 'BOOT'
$(cat "$boot")
BOOT
            bash ~/boot/boot_release.sh \"$archive\" roms/marioparty3.us.z64 marioparty3.n64.us.1.0 out/boot-linux ${*:3} &&
            cp \"$archive.booted\" out/boot-linux/"
        rm -rf out/boot-linux
        rb get out/boot-linux out/boot-linux
        mv out/boot-linux/"${archive##*/}.booted" "$archive.booted"
        [ "$(cat "$archive.booted")" = "$(sha256sum "$archive" | cut -d' ' -f1)" ] ||
            { rm "$archive.booted"; echo "$archive is not the archive BUILD_HOST booted" >&2; exit 1; }
    else
        bash "$boot" "$archive" roms/marioparty3.us.z64 marioparty3.n64.us.1.0 "out/boot-$2" "${@:3}"
    fi
    ;;
publish)
    case "${2:-}" in
    --prerelease) kind=(--prerelease) ;;
    "") kind=() ;;
    *) echo "publish takes --prerelease or nothing" >&2; exit 1 ;;
    esac
    notes="releases/$tag.md"
    [ -s "$notes" ] || { echo "no $notes: write the release notes for players first" >&2; exit 1; }
    clean
    git fetch -q origin main
    git merge-base --is-ancestor "$commit" origin/main || { echo "HEAD is not on origin/main" >&2; exit 1; }
    [ ! -e releases/next.md ] || { echo "releases/next.md is still there: move it to $notes in the release's PR" >&2; exit 1; }
    ! gh release view "$tag" > /dev/null 2>&1 || { echo "release $tag exists: bump version_string first" >&2; exit 1; }
    files=("out/$name-windows-x64.zip" "out/$name-linux-x64.tar.gz")
    for f in "${files[@]}"; do
        [ -f "$f" ] || { echo "no $f: run tools/release.sh windows and linux first" >&2; exit 1; }
        [ "$(cat "$f.commit" 2> /dev/null)" = "$commit" ] || { echo "$f wasn't built from $commit" >&2; exit 1; }
        [ "$(cat "$f.booted" 2> /dev/null)" = "$(sha256sum "$f" | cut -d' ' -f1)" ] ||
            { echo "$f hasn't booted: run tools/release.sh boot windows and linux first" >&2; exit 1; }
    done
    gh release create "$tag" "${files[@]}" --target "$commit" --title "Mario Party 3: Recompiled $tag" \
        --notes-file "$notes" "${kind[@]}"
    ;;
*)
    echo "usage: tools/release.sh windows|linux|boot <os>|publish [--prerelease]" >&2
    exit 1
    ;;
esac
