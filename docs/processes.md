# The process scheduler

Hudson's engine, which Mario Party, Mario Party 2 and 3 and Bomberman 64 are built on, runs most of a game as
processes: small functions, each with a stack of its own in the engine's heap, which a scheduler runs in turn once a
frame. A process runs until it sleeps (`HuPrcSleep`), waits for its children (`HuPrcChildWait`) or ends
(`HuPrcExit`), and the scheduler (`HuPrcCall`, called from the game's main thread) then goes on to the next. On a
console they switch with the game's own setjmp and longjmp: `HuPrcCall` longjmps into a process, onto its stack, and
the process setjmps where it stops and longjmps back.

Recompiled code can't do that. A function of the game is a C function, so a process's state is on the native stack as
well as in RDRAM, and a longjmp between two of them would leave the native stack behind.

## How it runs here

Each live process runs on a native thread of its own, and exactly one of them runs at a time, as the console runs one
process at a time:

- `HuPrcCall` is the game's loop, line for line, with `recomp_prc_resume` where it longjmped into a process. The first
  resume of a process starts a thread at its function with its RDRAM stack and the saved registers its jmp_buf holds
  (s0-s7, fp and f20-f31), as the longjmp loaded them; a later one wakes its thread. Either way the scheduler waits
  until the process hands the baton back, then goes on with what the longjmp back would have returned: 1 when the
  process slept, 2 when it ended.
- `HuPrcSleep` and `HuPrcChildWait` hand the baton back (`recomp_prc_yield`) and wait to be resumed, where they
  setjmped and longjmped.
- `HuPrcTerminate` ends the process (`recomp_prc_exit`), which throws out of the recompiled code to its thread's loop,
  as N64ModernRuntime ends a game thread. The scheduler frees the process's heap, on its own stack, as on a console.
- `HuPrcSysInit` ends every process's thread (`recomp_prc_reset`), since none outlives the list it was on.
- A process killed while it waits (`HuPrcKill`) is resumed in `HuPrcExit` on a console, and here too: a process that
  wakes checks for it, and one that never ran starts there.

The engine never setjmps a new process's jmp_buf, so its registers are whatever the process's heap held, and the game
reads some of them before it sets them (in Mario Party, Toad's warp-pipe room reads s3 in its first message's wait),
so a new process starts with the jmp_buf's registers, as the longjmp loaded them.

The thread a process runs on takes the identity of the game thread that resumed it (N64ModernRuntime's
`thread_self` and the FPU's rounding mode), so what a process does, a blocking `osRecvMesg` included, is that game
thread's doing, as on a console. A process's thread is kept when the process ends and handed the next new one, so a
board's processes cost no thread creation after the first time.

It is the same code on the desktop and on the web, where the page builds with threads and `-fwasm-exceptions` anyway.
Bomberman 64's recompilation and Rainchus's of this game make each process a coroutine instead (minicoro), which in
a browser needs Asyncify over the whole recompiled game: a bigger, slower page.

The code is `patches/process.c` (the five patches) and `src/game/process_threads.cpp` (the natives, with a header
that says how they hand the baton over).

## From Mario Party

The scheduler's code is Mario Party: Recompiled's (n64hub/mario-party-recomp), where its docs/processes.md says what a
second game on the engine takes:

- `src/game/process_threads.cpp` as it is, byte for byte. A change to it is made there too.
- `patches/process.c`, the same five patches with Mario Party 3's names: `HuPrcSysInit`, `HuPrcChildWait`, `HuPrcExit`
  for Mario Party's `HuPrcInit`, `HuPrcChildWatch` and `HuPrcEnd`, the list's `top_process`, `current_process` and
  `process_count`, and `HuMemMemoryFreePerm` for the heap a process leaves. `UnlinkProcess` is static, so N64Recomp
  names it by its ROM address (`UnlinkProcess_0004F7C8`), and `HuPrcCall` also stores `sPrcSleepLoc` (where the
  process last slept, for a debugger) as the game does.
- The five natives' lines in `patches/syms.ld`.

Not the scheduler's, but the same engine's: its swap chain (src/graphics.c) draws each frame in several RSP tasks, so
the game presents at the VI (`src/main/main.cpp`), as Mario Party does.

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

## From Mario Party

The scheduler's code is Mario Party: Recompiled's (n64hub/mario-party-recomp), where its docs/processes.md says what a
second game on the engine takes:

- `src/game/process_threads.cpp` as it is, byte for byte. A change to it is made there too.
- `patches/process.c`, the same five patches with Mario Party 3's names: `HuPrcSysInit`, `HuPrcChildWait`, `HuPrcExit`
  for Mario Party's `HuPrcInit`, `HuPrcChildWatch` and `HuPrcEnd`, the list's `top_process`, `current_process` and
  `process_count`, and `HuMemMemoryFreePerm` for the heap a process leaves. `UnlinkProcess` is static, so N64Recomp
  names it by its ROM address (`UnlinkProcess_0004F7C8`), and `HuPrcCall` also stores `sPrcSleepLoc` (where the
  process last slept, for a debugger) as the game does.
- The five natives' lines in `patches/syms.ld`.

Not the scheduler's, but the same engine's: its swap chain (src/graphics.c) draws each frame in several RSP tasks, so
the game presents at the VI (`src/main/main.cpp`), as Mario Party does.

## How much it costs

Built with `-DMP_PROCESS_STATS=ON`, the program prints every 5 seconds how many processes are alive, how many threads
it has made, how many resumes a second the scheduler makes and what each costs beyond the process's own run (the two
handoffs, from the scheduler's thread to the process's and back).

On the rules board Toad shows a new adventure on, which four computer players play with the board engine, a board
keeps 15 to 18 processes alive, and the scheduler makes about 450 resumes a second, 15 a frame at the game's 30. The
title screens keep the most alive: up to 29, with about 900 resumes a second. 30 threads are made in all.

What each resume costs beyond the process:
- On the desktop, on an idle host: 7 to 10 us, which is 3.3 to 4.6 ms a second, under half a percent of a core.
- In Chrome on a desktop (a busy host): 22 to 28 us, which is 10 to 12 ms a second, about 0.4 ms of each of the
  game's frames. A handoff there is a futex wait and wake between two of the page's workers.
