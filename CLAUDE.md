# CLAUDE.md

A static recompilation of the N64 *Mario Party 3* (USA) with N64Recomp, N64ModernRuntime, RecompFrontend and RT64,
from the mariopartyrd/marioparty3 symbols. Almost all of it is written by AI agents, so this file is what the next one
needs: the rules, where things are, and the facts about the game the code can't say on its own.

## What lives where

This repo is the game: its recompilation, the patches over it, the desktop build and its web page.

- `syms/`: the symbols N64Recomp reads, generated (never edited by hand) by `tools/build_decomp.sh` then
  `tools/gen_syms.sh` (`tools/gen_syms.py` says how).
- `us.toml`: N64Recomp's configuration; `tools/recompile.sh` writes `RecompiledFuncs/` with it, and the audio
  microcode with `n_aspMain.us.toml`.
- `patches/` and `patches.toml`: MIPS C against `lib/marioparty3`'s headers, recompiled into PatchesLib. Its
  `ui_funcs.h`, `patch_helpers.h` and `recompui_event_structs.h` are RecompFrontend's, which includes the first from
  the game's `patches/`.
- `src/`: RecompFrontend's main (`src/main/main.cpp`, with the GameEntry), the natives the patches call
  (`src/game/recomp_api.cpp`, and the process scheduler's in `src/game/process_threads.cpp`), the libultra functions
  the runtime lacks (`src/game/libultra.cpp`) and the settings.
- `lib/`: `marioparty3` (the decomp, for the symbols and the patches' headers), upstream `rt64`, `N64ModernRuntime` and
  `RecompFrontend` at the commits N64Recomp-browser's web forks are built on, and `N64Recomp-browser` at a release
  tag. The web support every recomp's page shares is developed and documented there; `tools/build_n64recomp.sh` and
  `CMakeLists.txt` apply its `patches/` to the other three.
- `docs/processes.md`: the engine's process scheduler, how it runs on threads here and what a game on the same engine
  reuses. `docs/web.md`: the web page.
- Comments beside the code, for how and why: each file's header. Read the header of the file you change and update
  it in the same commit.

## Rules

- No ROMs, ROM-derived assets or extracted game data in git, CI or PRs (`tools/ci/check_repo.py` checks). Nothing
  from the decomp's build is committed but `syms/`. The launcher's art is text only (`icons/`): no logo, character or
  picture from the game.
- The decomp is built only where the ROM is, never in CI.
- The scope is the game as it plays, plus what a web page needs (RT64 interpolation to the display's rate, resolution
  Auto), plus modern enhancements: each an option with an Original value that plays as the console does, none
  changing how the game plays. USA only.
- The process scheduler stays game-agnostic (`docs/processes.md`): `src/game/process_threads.cpp` is Mario Party:
  Recompiled's as it is, and a change to it is made there too.
- Write for a stranger, in the present tense: nothing about a contributor's machine, servers, paths or accounts, and
  nothing about where anyone keeps ROMs.
- GPL-3.0, as the recompilations it follows.

## How it fits together

- **The code.** `main` is stored raw in the ROM after the boot code, and the runtime maps what lies in ROM 0x1000
  plus 1 MB at start. Every other code section is one of the 131 overlays, which the game loads in two ways
  (src/util.c): `OvlLoad` copies one of `_ovltbl`'s 128 (the minigames, menus and the boards' screens) whole, and
  `func_8000B1A0_main` copies the sections asked for of one of `_modeovltbl`'s 3 (the board_code segments).
  `patches/overlays.c` patches both to tell the runtime.
- **The process scheduler.** Most of the game runs as the engine's processes, which switch with its own setjmp and
  longjmp; here each runs on a thread of its own, one at a time (`docs/processes.md`). The game has no other setjmp
  or longjmp, and `src/game/libultra.cpp`'s setjmp aborts.
- **The game's threads.** The main thread (id 3) runs the scheduler once a frame; graphics (0x10), the controllers
  (0x14) and audio (0x64, 0x65, started by each overlay that plays its own) have threads of their own.
- **No 64DD.** libleo's boot of a disk game is linked in and reached only with a 64DD attached; `us.toml` stubs it
  (its code is stored scrambled, and `tools/gen_syms.py` sizes it so that it can be), and `src/game/libultra.cpp`
  has the libultra functions N64Recomp leaves out that only a 64DD or a Controller Pak reaches.
- **The C library's names.** The game links its own libm (`atan2`, `exp`, `log` and their float forms), which
  `us.toml` renames, since a recompiled function by those names would clash with the C library's.
- **Microcodes.** Graphics go to RT64. Audio: n_aspMain, recompiled by RSPRecomp from `n_aspMain.us.toml` (its
  header says how its layout was found).
- **Saves.** EEPROM, 16 Kbit. `main.cpp` declares `SaveType::Eep16k`; the runtime keeps it in `saves/`.
- **Input.** RecompFrontend's default bindings. Keyboard: WASD the stick, Space A, Left Shift B, Q Z, R R, E L,
  Enter Start, the arrows the C buttons.
