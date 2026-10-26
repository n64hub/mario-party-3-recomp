# CLAUDE.md

A static recompilation of the N64 *Mario Party* (USA) with N64Recomp, N64ModernRuntime, RecompFrontend and RT64, from
the mariopartyrd/marioparty symbols. Almost all of it is written by AI agents, so this file is what the next one
needs: the rules, where things are, and the facts about the game the code can't say on its own.

## What lives where

This repo is the game: its recompilation, the patches over it, the desktop build and its web page.

- `syms/`: the symbols N64Recomp reads, generated (never edited by hand) by `tools/build_decomp.sh` then
  `tools/gen_syms.sh` (`tools/gen_syms.py` says how).
- `us.toml`: N64Recomp's configuration; `tools/recompile.sh` writes `RecompiledFuncs/` with it, and the audio
  microcode with `aspMain.us.toml`.
- `patches/` and `patches.toml`: MIPS C against `lib/marioparty`'s headers, recompiled into PatchesLib. Its
  `ui_funcs.h`, `patch_helpers.h` and `recompui_event_structs.h` are RecompFrontend's, which includes the first from
  the game's `patches/`.
- `src/`: RecompFrontend's main (`src/main/main.cpp`, with the GameEntry), the natives the patches call
  (`src/game/recomp_api.cpp`, and the process scheduler's in `src/game/process_threads.cpp`), the libultra functions
  the runtime lacks (`src/game/libultra.cpp`) and the settings.
- `lib/`: `marioparty` (the decomp, for the symbols and the patches' headers), upstream `rt64`, `N64ModernRuntime` and
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
- The process scheduler stays game-agnostic (`docs/processes.md`): Mario Party 2 and 3 take
  `src/game/process_threads.cpp` as it is.
- Write for a stranger, in the present tense: nothing about a contributor's machine, servers, paths or accounts, and
  nothing about where anyone keeps ROMs.
- GPL-3.0, as the recompilations it follows.

## How it fits together

- **The code.** `main` is stored raw in the ROM after the boot code, and the runtime maps what lies in ROM 0x1000
  plus 1 MB at start. Every other code section is one of the 131 overlays (a minigame, a board, a menu), most of them
  at VRAM 0x800F65E0, which the game loads through `OvlLoad` (src/engine/ovlman.c) by its index in `overlay_table`;
  `patches/overlays.c` patches it to tell the runtime.
- **The process scheduler.** Most of the game runs as the engine's processes, which switch with its own setjmp and
  longjmp; here each runs on a thread of its own, one at a time (`docs/processes.md`). The game has no other setjmp
  or longjmp, and `src/game/libultra.cpp`'s setjmp aborts.
- **The game's threads.** The main thread (id 3) runs the scheduler once a frame; graphics (0x10), the controllers
  (0x14) and audio (0x64, 0x65) have threads of their own.
- **No 64DD.** The logos overlay asks whether one is attached by reading the drive's registers directly;
  `patches/leo.c` answers no, as a console without one does. `us.toml` stubs the 64DD's boot of a disk game, and
  `src/game/libultra.cpp` has the 64DD's and the Controller Pak's libultra functions N64Recomp leaves out, which only
  that hardware reaches.
- **Microcodes.** Graphics go to RT64. Audio: aspMain, recompiled by RSPRecomp from `aspMain.us.toml` (its header
  says how its layout was found; it is Bomberman 64's microcode, byte for byte).
- **Saves.** EEPROM, 4 Kbit. `main.cpp` declares `SaveType::Eep4k`; the runtime keeps it in `saves/`.
- **Input.** RecompFrontend's default bindings. Keyboard: WASD the stick, Space A, Left Shift B, Q Z, R R, E L,
  Enter Start, the arrows the C buttons.
