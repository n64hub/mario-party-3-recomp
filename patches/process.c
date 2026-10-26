#include "patches.h"
#include "engine/process.h"

// Hudson's process scheduler (src/engine/process.c) on native threads (src/game/process_threads.cpp says how). On a
// console each process has a stack in its heap, and the scheduler and the processes switch with the game's setjmp and
// longjmp: HuPrcCall longjmps into each process that is due, and a process setjmps where it sleeps and longjmps back.
// Here the four functions that switch call the natives instead, and everything else is the game's as it was. The
// scheduler's loop is the game's, line for line, with recomp_prc_resume where it longjmped into a process, and the
// heap of a process that ended is freed by the scheduler, on its own stack, as on a console.
//
// A process killed while it waits (HuPrcKill sets EXEC_PROCESS_DEAD) is resumed in HuPrcEnd on a console: the
// scheduler makes HuPrcEnd its jmp_buf's function. A new process starts at its jmp_buf's function, which covers one
// killed before it ran, and one that was running checks for it when it wakes (prc_wait). It starts with the registers
// its jmp_buf holds, as the longjmp into it loaded them: the game reads some before it sets them.

void HuUnlinkPrc(Process** root, Process* process);
void HuPrcEnd(void);

// The natives (src/game/process_threads.cpp).
s32 recomp_prc_resume(Process* process, jmp_buf* jump);
void recomp_prc_yield(void);
void recomp_prc_exit(void);
void recomp_prc_reset(process_func end);

// What a process's setjmp-then-longjmp to the scheduler was: back to the scheduler until it resumes this process,
// then on into HuPrcEnd if the process was killed meanwhile.
static void prc_wait(Process* process) {
    recomp_prc_yield();
    if (process->exec_mode == EXEC_PROCESS_DEAD) {
        HuPrcEnd();
    }
}

RECOMP_PATCH void HuPrcInit(void) {
    // @recomp No process outlives the list it was on.
    recomp_prc_reset(HuPrcEnd);
    processcnt = 0;
    processtop = NULL;
}

RECOMP_PATCH void HuPrcChildWatch(void) {
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

    HuUnlinkPrc(&processtop, process);
    processcnt--;
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

    processcur = processtop;
    // @recomp setjmp(&process_jmp_buf), which returns 0 here and what the process longjmped with after each one.
    ret = 0;
    while (1) {
        switch (ret) {
            case 2:
                HuMemDirectFree(processcur->heap);
                processcur = processcur->next;
                break;
            case 1:
                processcur = processcur->next;
                break;
        }

        cur_proc_local = processcur;

        if (cur_proc_local == 0) {
            break;
        }

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
                cur_proc_local->prc_jump.func = HuPrcEnd;

            case 0:
                // @recomp longjmp(&cur_proc_local->prc_jump, 1)
                ret = recomp_prc_resume(cur_proc_local, &cur_proc_local->prc_jump);
                break;
        }
    }
}
