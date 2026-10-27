# The process scheduler

Hudson's engine runs most of a game as processes, small functions with stacks of their own that a scheduler runs in
turn once a frame and that switch with the game's own setjmp and longjmp, which recompiled code can't do. Here each
live process runs on a native thread of its own, one at a time, from the code the Mario Party recomps share:
`lib/mario-party-engine` ([its docs/processes.md](https://github.com/n64hub/mario-party-engine/blob/main/docs/processes.md)
says how it works and why). This file says what is Mario Party 3's.

## What is Mario Party 3's

- `patches/engine_names.h` gives the engine's patches the decomp's names (`src/process.c`): `HuPrcSysInit`,
  `HuPrcChildWait` and `HuPrcExit` for Mario Party's `HuPrcInit`, `HuPrcChildWatch` and `HuPrcEnd`, the list's
  `top_process`, `current_process` and `process_count`, and `HuMemMemoryFreePerm` for the heap a process leaves.
  `UnlinkProcess` is static, so N64Recomp names it by its ROM address (`UnlinkProcess_0004F7C8`), and `HuPrcCall` also
  stores `sPrcSleepLoc` (where the process last slept, for a debugger) as the game does.
- The natives' lines are in `patches/syms.ld`, and the survey's `survey_tick` in `patches/survey.c`.
- Its swap chain (`src/graphics.c`) draws each frame in several RSP tasks, so the game presents at the VI
  (`src/main/main.cpp`), as Mario Party does.

## How much it costs

Built with `-DMP_PROCESS_STATS=ON`, the program prints every 5 seconds how many processes are alive, how many threads
it has made, how many resumes a second the scheduler makes and what each costs beyond the process's own run (the two
handoffs, from the scheduler's thread to the process's and back).

On the desktop, from the title to a Battle Royale on Chilly Waters and its first turns, A pressed through it, on a busy
host (a page played beside it):
- The title, the file select and the mode select keep 3 to 7 processes alive, with 150 to 210 resumes a second.
- A party's setup and the board's opening keep 10 to 17 alive, with 300 to 490 resumes a second.
- The board keeps 19 to 26 alive, with 570 to 650 resumes a second, about 20 a frame at the game's 30. 26 threads are
  made in all.

Each resume costs 10 to 30 us beyond the process, which on the board is 10 to 16 ms a second, about 1.5% of a core.
A few 5-second stretches cost more (to 150 us a resume) as the host's other work took the cores. Mario Party:
Recompiled's docs/processes.md has the same code's costs on an idle host and in Chrome.
