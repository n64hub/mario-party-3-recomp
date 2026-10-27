#ifndef ENGINE_NAMES_H
#define ENGINE_NAMES_H

// Mario Party 3's names for what lib/mario-party-engine's patches use (its README lists them).

#include "game/process.h"
#include "malloc.h"

// The game's static UnlinkProcess (src/process.c), named in syms/ by its ROM address as N64Recomp names a static.
void UnlinkProcess_0004F7C8(Process** root, Process* process);

extern Process* top_process;
extern Process* current_process;
extern s16 process_count;
extern u32 sPrcSleepLoc;

#define PRC_SYS_INIT HuPrcSysInit
#define PRC_CHILD_WAIT HuPrcChildWait
#define PRC_END HuPrcExit
#define PRC_UNLINK UnlinkProcess_0004F7C8
#define PRC_HEAP_FREE HuMemMemoryFreePerm
#define PRC_TOP top_process
#define PRC_CURRENT current_process
#define PRC_COUNT process_count
#define PRC_SLEEP_LOC sPrcSleepLoc

#endif
