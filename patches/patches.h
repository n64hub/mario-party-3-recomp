#ifndef __PATCHES_H__
#define __PATCHES_H__

// What every patch includes: the sections N64Recomp reads a patch's role from, the natives (syms.ld) and the decomp's
// headers.

#define RECOMP_EXPORT __attribute__((section(".recomp_export")))
#define RECOMP_PATCH __attribute__((section(".recomp_patch")))
#define RECOMP_FORCE_PATCH __attribute__((section(".recomp_force_patch")))
#define RECOMP_DECLARE_EVENT(func)                                                          \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"") \
        __attribute__((noinline, weak, used, section(".recomp_event"))) void func {         \
    }                                                                                       \
    _Pragma("GCC diagnostic pop")

// libultra functions the runtime reimplements are called by their _recomp names (syms.ld).
#define osRecvMesg osRecvMesg_recomp
#define osEPiStartDma osEPiStartDma_recomp

#include "common.h"

// The game's natives (src/game/recomp_api.cpp; process.c declares the scheduler's).
void recomp_load_overlays(u32 rom, void* ram, u32 size);

#endif
