// The camera's list func_8001B0B4_main (hmfman) writes for the models on a board (patches/widescreen.c says why its
// scissor's patch ends it): the hook before its first call of the scissor's function (us.toml) marks that call.

#include "recomp.h"
#include "librecomp/helpers.hpp"

namespace {
    bool list_ends = false;
}

// The hook.
extern "C" void mp_camera_list_ends(uint8_t* rdram) {
    list_ends = true;
}

// recomp_camera_list_ends(): whether this call of the scissor's function is the one the hook marked, once.
extern "C" void recomp_camera_list_ends(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, (int32_t)list_ends);
    list_ends = false;
}
