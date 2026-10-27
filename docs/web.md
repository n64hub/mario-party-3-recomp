# The web page

The game plays in a browser: built with Emscripten on N64Recomp-browser's `frontend/` into
`MarioParty3Recompiled.html` (the launcher, the recomp's menus, SDL2's audio and keyboard, the ROM from Load ROM, and
the saves kept in IndexedDB; `lib/N64Recomp-browser/frontend/CMakeLists.txt`'s header says what each part does), drawn
through WebGPU. The page is built from your ROM like the desktop program, so it is never built in CI and never
published with the game in it.

What is the game's own stays here: the `EMSCRIPTEN` branches of `CMakeLists.txt` that make the game a page,
`tools/build_web.sh`, the shader list (`src/rt64_shaders.txt`, `tools/shader_plan.json`, `tools/record_shaders.sh`),
the play plan (`tools/play_plan.json`) and the survey (`patches/survey.c`, `tools/survey.js`). The rest is
N64Recomp-browser's: `docs/porting.md` (WebGPU and the shaders, prebuilt shader lists, Emscripten), `docs/playing.md`
(playing and measuring a page), `docs/forks.md` and `docs/findings.md`. Every play of this game's page is a line in
N64Recomp-browser's `plays.jsonl` (game `mp3`), never a record here.

The engine's process scheduler runs on threads on the page as on the desktop (`docs/processes.md`): the page has
threads and C++ exceptions anyway, so it needs no Asyncify.

## Build it

On Linux with Docker, with the ROM at `roms/marioparty3.us.z64`:

```bash
tools/build_web.sh                  # [cpus] [cmake arguments]
```

It builds `build/web/MarioParty3Recompiled.html` with its `.js`, `.wasm` (about 24 MB) and `.data` (about 9 MB: the
recomp's fonts and menus, nothing of the game) in `tools/docker/Dockerfile.web`, the image every N64Recomp-browser page
is built in, with ccache in `build/ccache-web`. The script's header says each step. The patches library (`patches/`,
built for MIPS) is recompiled by the native N64Recomp that `tools/recompile.sh` builds, not the runtime's, which
Emscripten would build for the browser.

Serve `build/web` over HTTP from localhost (any static server; the page brings its own `coi_serviceworker.js` for the
headers threads need) and open `MarioParty3Recompiled.html` in Chrome or Firefox with WebGPU, then Load ROM.

## The shader list

`src/rt64_shaders.txt` is the page's prebuilt shader list, in sections: the boot's, built before the first frame, then
one a board and one a run of minigames, each built as the game first draws from it. It is recorded by code, not by
playing (`tools/record_shaders.sh`'s header says how to run it): `tools/shader_plan.json` reaches the party's setup
as the play plan does, then the survey takes it to each of the six boards, with four computer players, and through
every minigame in turn, each played to its end by four computer players. A section the game moves past without
drawing from it is recorded again alone with `--only`.

## The survey

A plan can take the game to any overlay by itself, without anyone playing a board to its minigames: `patches/survey.c`
hooks `omOvlGotoEx` so that the next time the game leaves for the overlay a script names, it goes to the one asked for
instead, and leaves where it was in its own way. `tools/survey.js` is what a plan evaluates in the page to use it (its
header lists the calls). Going to a minigame, a request also sets what a board sets as it starts one: the teams
(GwPlayer's group: a 1-vs-3 minigame makes its objects for the players whose group isn't 0, so without them Coconut
Conk reads through a null pointer), which players the computer plays, and the minigame's index. A minigame is left as
it ends, never by a call of `omOvlGotoEx` from elsewhere: its own ending closes its files and stops its sound, and
without that the next one can't load.

## The plan

`tools/play_plan.json` reaches a board as a player does, by the keyboard's defaults (Enter is Start, Space A): Start
through the logos and the title to the file select, A on a new file into the mode select, the stick onto the Battle
Royale star, and A through the party's setup onto Chilly Waters, where A rolls the dice for player 1 and the computer
plays the other three until the turn ends and the game leaves for its minigame. Which screen is up is the overlay
`omOvlGotoEx` last went to, `omovl`, a word at 0x800CE200: the file select is 0x79, the mode select 0x77, the setup
0x78, the boards 0x48 (Chilly Waters) to 0x4D (Waluigi's Island), a minigame's instructions 0x70 and the minigames
0x01 to 0x46. The plan's header says each step.
