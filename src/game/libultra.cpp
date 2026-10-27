// libultra functions the game links that N64Recomp leaves out (its ignored_funcs) and the runtime doesn't implement,
// so each call to one in RecompiledFuncs/ comes here.
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "recomp.h"

namespace {
    // A function only hardware the runtime never reports reaches: the 64DD, a Controller Pak's own banks after
    // osPfsIsPlug said no pak is plugged in, or a development board's debugger (the KMC and MSP debuggers' printf,
    // which libultra's osInitialize installs only when it finds one, and the runtime's never does).
    [[noreturn]] void unreachable(const char* name) {
        fprintf(stderr, "%s: called, which only a 64DD, a Controller Pak or a debugger reaches\n", name);
        std::abort();
    }
}

// The process scheduler's setjmp: only the recompiled originals of the functions patches/process.c replaces call it.
extern "C" void setjmp_recomp(uint8_t* rdram, recomp_context* ctx) {
    fprintf(stderr, "setjmp: called outside the process scheduler's patches\n");
    std::abort();
}

// No Controller Pak in any port (the runtime's osPfsInitPak says the same): an empty bit pattern.
extern "C" void osPfsIsPlug_recomp(uint8_t* rdram, recomp_context* ctx) {
    MEM_B(0, ctx->r5) = 0;
    ctx->r2 = 0;
}

extern "C" void __osPfsSelectBank_recomp(uint8_t* rdram, recomp_context* ctx) {
    unreachable("__osPfsSelectBank");
}

extern "C" void __osContRamRead_recomp(uint8_t* rdram, recomp_context* ctx) {
    unreachable("__osContRamRead");
}

extern "C" void __osContRamWrite_recomp(uint8_t* rdram, recomp_context* ctx) {
    unreachable("__osContRamWrite");
}

extern "C" void osPiReadIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    unreachable("osPiReadIo");
}

extern "C" void osEPiWriteIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    unreachable("osEPiWriteIo");
}

extern "C" void osLeoDiskInit_recomp(uint8_t* rdram, recomp_context* ctx) {
    unreachable("osLeoDiskInit");
}

extern "C" void __osPopThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    unreachable("__osPopThread");
}

extern "C" void __osEnqueueThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    unreachable("__osEnqueueThread");
}
