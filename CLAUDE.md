# CLAUDE.md

A static recompilation of the N64 *Mario Party 3* (USA) with N64Recomp, N64ModernRuntime, RecompFrontend and RT64,
from the mariopartyrd/marioparty3 symbols. Almost all of it is written by AI agents, so this file is what the next one
needs: the rules, where things are, and the facts about the game the code can't say on its own.

## What lives where

This repo is the game: its recompilation, the patches over it, the desktop build and its web page.

- `syms/`: the symbols N64Recomp reads, generated (never edited by hand) by `tools/build_decomp.sh` then
  `tools/gen_syms.sh` (`lib/mario-party-engine/tools/gen_syms.py` says how).
- `us.toml`: N64Recomp's configuration; `tools/recompile.sh` writes `RecompiledFuncs/` with it, and the audio
  microcode with `n_aspMain.us.toml`.
- `patches/` and `patches.toml`: MIPS C against `lib/marioparty3`'s headers, recompiled into PatchesLib. Its
  `ui_funcs.h`, `patch_helpers.h` and `recompui_event_structs.h` are RecompFrontend's, which includes the first from
  the game's `patches/`.
- `src/`: RecompFrontend's main (`src/main/main.cpp`, with the GameEntry), the natives the patches call
  (`src/game/recomp_api.cpp`; the process scheduler's are `lib/mario-party-engine`'s), the libultra functions the
  runtime lacks (`src/game/libultra.cpp`) and the settings.
- `lib/`: `marioparty3` (the decomp, for the symbols and the patches' headers), `mario-party-engine` (the code the
  Mario Party recomps share, below), upstream `rt64`, `N64ModernRuntime` and
  `RecompFrontend` at the commits N64Recomp-browser's web forks are built on, and `N64Recomp-browser` at a release
  tag. The web support every recomp's page shares is developed and documented there; `tools/build_n64recomp.sh` and
  `CMakeLists.txt` apply its `patches/` to the other three.
- `docs/processes.md`: what is this game's in the engine's process scheduler, whose code and how it runs on threads
  are `lib/mario-party-engine`'s. `docs/web.md`: the web page.
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
- The code the Mario Party recomps share is `lib/mario-party-engine` (n64hub/mario-party-engine): the process
  scheduler's patches and natives, a camera's view, a board's background and its HUD past 4:3 and `gen_syms.py`,
  written once against this game's names in `patches/engine_names.h` (the HUD's against the addresses its `us.toml`
  hooks pass). A change to it is a commit there that each game's submodule
  moves onto, and code this game would copy from another Mario Party recomp goes there instead.
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
  (its code is stored scrambled, and `tools/gen_syms.sh` sizes it so that it can be), and `src/game/libultra.cpp`
  has the libultra functions N64Recomp leaves out that only a 64DD or a Controller Pak reaches.
- **The C library's names.** The game links its own libm (`atan2`, `exp`, `log` and their float forms), which
  `us.toml` renames, since a recompiled function by those names would clash with the C library's.
- **Microcodes.** Graphics go to RT64. Audio: n_aspMain, recompiled by RSPRecomp from `n_aspMain.us.toml` (its
  header says how its layout was found).
- **Saves.** EEPROM, 16 Kbit. `main.cpp` declares `SaveType::Eep16k`; the runtime keeps it in `saves/`.
- **Drawing.** RT64 interpolates the game's frames to the display's rate with no tags in the game's display lists. The
  game draws a frame in several RSP tasks, which RT64 interpolates as one game frame with RT64_GAME_FRAME_WORKLOADS, on
  with Emscripten and in `CMakeLists.txt` on the desktop too. It draws a picture wider than 4:3 with Aspect Ratio
  Expand: `patches/widescreen.c` takes a camera's view to the window's edges and `patches/background.c` a board's
  background, through lib/mario-party-engine's `camera_view.c` and `board_back.c` (their headers say how), and
  `us.toml`'s hooks in the sprite manager the board's HUD, the players' panels, to its left and right edges, through
  lib/mario-party-engine's `src/hud.cpp`, and its hooks in the wipe module draw an iris's, the star's and a fade's
  colour to them too, through its `src/wipe.cpp`. `us.toml` also makes pfDrawFonts's scissors the whole
  screen's (its comment says why). The board's camera stops where the wider picture reaches the map's edge
  (`patches/background.c`, with the engine's `src/aspect_ratio.cpp`). A map no wider than 320 (a board's opening,
  opboard's, 5x5 tiles) keeps black at its sides, the game having nothing beside it to draw, and its 3D and the HUD
  keep inside 4:3 with it (`board_back.c`). So does a scene `patches/widescreen.c` lists as drawn at 4:3 (a board's
  results on the star stage and thirteen minigames), black drawn over the window's sides at each frame's end (`us.toml`,
  the engine's `src/wipe.cpp`).
- **Input.** RecompFrontend's default bindings. Keyboard: WASD the stick, Space A, Left Shift B, Q Z, R R, E L,
  Enter Start, the arrows the C buttons.
