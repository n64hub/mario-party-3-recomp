#include "patches.h"

// The logos overlay asks whether a 64DD is attached (func_800827C0, libleo's check) by reading the drive's registers
// at PI domain 2 directly, which the recompiled code can't reach. The runtime never has a 64DD, so the answer is no,
// and the game takes its path for a console without one.

// @recomp No 64DD.
RECOMP_PATCH s32 func_800827C0(void) {
    return 0;
}
