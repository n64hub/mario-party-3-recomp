#include "patches.h"
#include "game/process.h"
#include "malloc.h"

// Hudson's process scheduler (src/process.c) on native threads (src/game/process_threads.cpp says how). On a
// console each process has a stack in its heap, and the scheduler and the processes switch with the game's setjmp and
// longjmp: HuPrcCall longjmps into each process that is due, and a process setjmps where it sleeps and longjmps back.
// Here the four functions that switch call the natives instead, and everything else is the game's as it was. The
// scheduler's loop is the game's, line for line, with recomp_prc_resume where it longjmped into a process, and the
// heap of a process that ended is freed by the scheduler, on its own stack, as on a console.
//
// A process killed while it waits (HuPrcKill sets EXEC_PROCESS_DEAD) is resumed in HuPrcExit on a console: the
// scheduler makes HuPrcExit its jmp_buf's function. A new process starts at its jmp_buf's function, which covers one
// killed before it ran, and one that was running checks for it when it wakes (prc_wait). It starts with the registers
// its jmp_buf holds, as the longjmp into it loaded them: the game reads some before it sets them.

// The game's static UnlinkProcess (src/process.c), named in syms/ by its ROM address as N64Recomp names a static.
void UnlinkProcess_0004F7C8(Process** root, Process* process);

extern Process* top_process;
extern Process* current_process;
extern s16 process_count;
extern u32 sPrcSleepLoc;

// The natives (src/game/process_threads.cpp).
s32 recomp_prc_resume(Process* process, jmp_buf* jump);
void recomp_prc_yield(void);
void recomp_prc_exit(void);
void recomp_prc_reset(process_func end);
void survey_tick(void);

// What a process's setjmp-then-longjmp to the scheduler was: back to the scheduler until it resumes this process,
// then on into HuPrcExit if the process was killed meanwhile.
static void prc_wait(Process* process) {
    recomp_prc_yield();
    if (process->exec_mode == EXEC_PROCESS_DEAD) {
        HuPrcExit();
    }
}

RECOMP_PATCH void HuPrcSysInit(void) {
    // @recomp No process outlives the list it was on.
    recomp_prc_reset(HuPrcExit);
    process_count = 0;
    top_process = NULL;
}

RECOMP_PATCH void HuPrcChildWait(void) {
    Process* process = HuPrcCurrentGet();
    if (process->oldest_child) {
        process->exec_mode = EXEC_PROCESS_WATCH;
        // @recomp setjmp(&process->prc_jump), longjmp(&process_jmp_buf, 1)
        prc_wait(process);
    }
}

RECOMP_PATCH void HuPrcTerminate(Process* process) {
    if (process->destructor) {
        process->destructor();
    }

    UnlinkProcess_0004F7C8(&top_process, process);
    process_count--;
    // @recomp longjmp(&process_jmp_buf, 2): the scheduler frees the process's heap.
    recomp_prc_exit();
}

RECOMP_PATCH void HuPrcSleep(s32 time) {
    Process* process = HuPrcCurrentGet();
    if (time != 0 && process->exec_mode != EXEC_PROCESS_DEAD) {
        process->exec_mode = EXEC_PROCESS_SLEEPING;
        process->sleep_time = time;
    }

    // @recomp setjmp(&process->prc_jump), longjmp(&process_jmp_buf, 1)
    prc_wait(process);
}

RECOMP_PATCH void HuPrcCall(s32 time) {
    Process* cur_proc_local;
    s32 ret;

    // @recomp A script's survey (patches/survey.c).
    survey_tick();

    current_process = top_process;
    // @recomp setjmp(&process_jmp_buf), which returns 0 here and what the process longjmped with after each one.
    ret = 0;
    while (1) {
        switch (ret) {
            case 2:
                HuMemMemoryFreePerm(current_process->heap);
                current_process = current_process->next;
                break;
            case 1:
                current_process = current_process->next;
                break;
        }

        cur_proc_local = current_process;
        if (cur_proc_local == 0) {
            break;
        }

        // Where the process last slept, as a word its setjmp left on its stack, kept for a debugger: nothing in the
        // game reads it. Here the jmp_buf keeps the sp the process started with, so it is a word of the process's heap.
        sPrcSleepLoc = current_process->prc_jump.sp[10];

        if ((cur_proc_local->stat & 0x1)) {
            if (cur_proc_local->exec_mode != 3) {
                ret = 1;
                continue;
            }
        }

        switch (cur_proc_local->exec_mode) {
            case EXEC_PROCESS_SLEEPING:
                if (cur_proc_local->sleep_time > 0 && (cur_proc_local->sleep_time -= time) <= 0) {
                    cur_proc_local->sleep_time = 0;
                    cur_proc_local->exec_mode = EXEC_PROCESS_DEFAULT;
                }

                ret = 1;
                break;

            case EXEC_PROCESS_WATCH:
                if (cur_proc_local->oldest_child != 0) {
                    ret = 1;
                } else {
                    cur_proc_local->exec_mode = EXEC_PROCESS_DEFAULT;
                    ret = 0;
                }

                break;

            case EXEC_PROCESS_DEAD:
                cur_proc_local->prc_jump.func = HuPrcExit;

            case 0:
                // @recomp longjmp(&cur_proc_local->prc_jump, 1)
                ret = recomp_prc_resume(cur_proc_local, &cur_proc_local->prc_jump);
                break;
        }
    }
}
