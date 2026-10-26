# The web page

The game plays in a browser: built with Emscripten on N64Recomp-browser's `frontend/` into
`MarioPartyRecompiled.html` (the launcher, the recomp's menus, SDL2's audio and keyboard, the ROM from Load ROM, and
the saves kept in IndexedDB; `lib/N64Recomp-browser/frontend/CMakeLists.txt`'s header says what each part does), drawn
through WebGPU. The page is built from your ROM like the desktop program, so it is never built in CI and never
published with the game in it.

What is the game's own stays here: the `EMSCRIPTEN` branches of `CMakeLists.txt` that make the game a page,
`tools/build_web.sh` and the play plan (`tools/play_plan.json`). The rest is N64Recomp-browser's: `docs/porting.md`
(WebGPU and the shaders, prebuilt shader lists, Emscripten), `docs/playing.md` (playing and measuring a page),
`docs/forks.md` and `docs/findings.md`. Every play of this game's page is a line in N64Recomp-browser's `plays.jsonl`
(game `mp1`), never a record here.

The engine's process scheduler runs on threads on the page as on the desktop (`docs/processes.md`): the page has
threads and C++ exceptions anyway, so it needs no Asyncify.

## Build it

On Linux with Docker, with the ROM at `roms/marioparty.us.z64`:

```bash
tools/build_web.sh                  # [cpus] [cmake arguments]
```

It builds `build/web/MarioPartyRecompiled.html` with its `.js`, `.wasm` (about 17 MB) and `.data` (about 9 MB: the
recomp's fonts and menus, nothing of the game) in `tools/docker/Dockerfile.web`, the image every N64Recomp-browser page
is built in, with ccache in `build/ccache-web`. The script's header says each step. The patches library (`patches/`,
built for MIPS) is recompiled by the native N64Recomp that `tools/recompile.sh` builds, not the runtime's, which
Emscripten would build for the browser.

The page has no prebuilt shader list yet, so RT64 builds each pipeline the first time the game draws with it, and the
first minutes stutter as they come.

Serve `build/web` over HTTP from localhost (any static server; the page brings its own `coi_serviceworker.js` for the
headers threads need) and open `MarioPartyRecompiled.html` in Chrome or Firefox with WebGPU, then Load ROM.

## The plan

`tools/play_plan.json` reaches a board as a player does, by the keyboard's defaults (Enter is Start, Space A): Start
through the logos, the opening and the title into Mushroom Village, then A through Toad's messages and the warp pipe,
on a new save, into the board Toad teaches the rules on, which four computer players play with the board engine. Which
screen is up is the overlay `omOvlGotoEx` last went to, a word at 0x800F09F4: Mushroom Village is 0x69, the boards 0x36
(DK's Jungle Adventure) to 0x3D (Eternal Star) and the rules' board 0x3E.
