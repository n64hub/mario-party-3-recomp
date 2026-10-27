# Mario Party 3: Recompiled

> **Made with AI.** Almost all of the code, documentation and tests in this repo were written by AI agents
> (Claude), directed and play-tested by a person. None of it is offered to the upstream projects it builds on, which
> don't take AI-written code.

A native PC port of the N64 *Mario Party 3* (USA), made by static recompilation
([N64Recomp](https://github.com/N64Recomp/N64Recomp) + [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) +
[RT64](https://github.com/rt64/rt64), with [RecompFrontend](https://github.com/N64Recomp/RecompFrontend)'s launcher
and menus): the game's own MIPS code translated to C and run natively, with RT64 drawing it. The symbols come from the
[mariopartyrd/marioparty3](https://github.com/mariopartyrd/marioparty3) decompilation. It is not an emulator.

The repo holds no ROM, no asset from the game and no data extracted from it: you supply your own ROM, and nothing in
git or CI is made from one except the symbol names and addresses in `syms/`.

**Status:** plays its menus, its six boards and its minigames, on the desktop and in a browser, interpolated to the
display's rate and drawn at the window's size and shape (RT64's Framerate Display, Resolution Auto and Aspect Ratio
Expand, a board's camera widened to the window's edges). The game's process scheduler, which switches between dozens
of small processes with its own setjmp and longjmp, runs each process on a thread of its own, one at a time, on the
desktop and in a browser alike ([docs/processes.md](docs/processes.md)), the same code as
[Mario Party: Recompiled](https://github.com/n64hub/mario-party-recomp)'s, which this repo started from. The browser's
page is built from your ROM ([docs/web.md](docs/web.md)).

## The ROM

*Mario Party 3* (USA), as a big-endian `.z64` of 32 MiB with SHA-1 `6beb80ff822b96bcf85dcdb512e8b2b7969d8259`. Other
regions are not supported.

## Building

You need that ROM at `roms/marioparty3.us.z64`. On Windows: LLVM's clang-cl, CMake 3.31 and Ninja, and a clang with a
MIPS backend plus ld.lld for `patches/`. On Linux: `clang lld libsdl2-dev libgtk-3-dev libfreetype-dev`, CMake 3.31
and Ninja.

From the repo's root in a bash shell:

```sh
git submodule update --init --recursive
bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl   # the game's code as C
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl \
      -DPATCHES_C_COMPILER=<MIPS clang> -DPATCHES_LD=<ld.lld>
ninja -C build MarioParty3Recompiled
```

`tools/recompile.sh` builds N64Recomp from the submodule the first time, then translates the game. Run
`build/MarioParty3Recompiled.exe` from the repo's root, pick the ROM with Load ROM, then Start Game.

The symbols in `syms/` are made from the decomp's own build of the game (`tools/build_decomp.sh`, which needs the ROM
and Docker, then `tools/gen_syms.sh`). They only change when `lib/marioparty3` moves.

## License

GPL-3.0 (`LICENSE`), as the recompilations it follows. The decomp in `lib/marioparty3` is its own project.

Rainchus's [MarioParty3-Recomp](https://github.com/Rainchus/MarioParty3-Recomp) (GPL-3.0), the first recompilation
of the game, showed where its audio microcode lies and which of its overlay loaders to hook.
