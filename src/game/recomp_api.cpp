// The natives the patches call (patches/syms.ld gives each a dummy address that N64Recomp turns into a call by name).
// The process scheduler's are process_threads.cpp.

#include <cstdio>
#include <cstdlib>

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "librecomp/helpers.hpp"

// recomp_load_overlays(rom, ram, size): the code sections in that stretch of ROM are now at ram (patches/overlays.c).
extern "C" void recomp_load_overlays(uint8_t* rdram, recomp_context* ctx) {
    u32 rom = _arg<0, u32>(rdram, ctx);
    PTR(void) ram = _arg<1, PTR(void)>(rdram, ctx);
    u32 size = _arg<2, u32>(rdram, ctx);

#ifdef MP_PROCESS_STATS
    if (getenv("MP_PROCESS_LOG") != nullptr) {
        fprintf(stderr, "[processes] overlay ROM %08X at %08X\n", rom, ram);
    }
#endif
    load_overlays(rom, ram, size);
}
