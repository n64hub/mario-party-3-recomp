// The survey's natives (patches/survey.c says what it is for): where its struct is, and the hook at omOvlGotoEx's start
// (us.toml) that sends the game to the overlay a script asked for in place of the one it was going to.

#include "recomp.h"
#include "librecomp/helpers.hpp"

namespace {
    // The struct's place in RDRAM, and its words' offsets (patches/survey.c's struct Survey).
    gpr survey_vram = 0;
    constexpr int REQUEST = 0x08;
    constexpr int FROM = 0x0C;
    constexpr int OVERLAY = 0x10;
    constexpr int EVENT = 0x14;
    constexpr int STAT = 0x18;
    constexpr int WARPS = 0x1C;
    constexpr int GROUPS = 0x28;
    constexpr int COMPUTERS = 0x2C;

    // GwPlayer (0x800D1108, 0x38 bytes a player): its group (the team) at 0 and its stat at 4, bit 0 the computer
    // playing it; GwSystem's minigame_index (0x800CD068).
    constexpr gpr GW_PLAYER = (gpr)(int32_t)0x800D1108;
    constexpr int PLAYER_SIZE = 0x38;
    constexpr gpr MINIGAME_INDEX = (gpr)(int32_t)0x800CD068;
}

// recomp_survey_register(survey): the survey's struct, once, at the first frame.
extern "C" void recomp_survey_register(uint8_t* rdram, recomp_context* ctx) {
    survey_vram = ctx->r4;
}

// The hook: omOvlGotoEx(overlay, event, stat)'s arguments, replaced by the request's if there is one and the game is
// going to the overlay it names in `from` (or `from` is -1).
extern "C" void mp_survey_goto(uint8_t* rdram, gpr* overlay, gpr* event, gpr* stat) {
    if (survey_vram == 0 || MEM_W(REQUEST, survey_vram) == 0) {
        return;
    }
    int32_t from = MEM_W(FROM, survey_vram);
    if (from != -1 && from != (int32_t)*overlay) {
        return;
    }
    *overlay = (int32_t)MEM_W(OVERLAY, survey_vram);
    *event = (int32_t)MEM_W(EVENT, survey_vram);
    *stat = (int32_t)MEM_W(STAT, survey_vram);
    uint32_t groups = MEM_W(GROUPS, survey_vram);
    uint32_t computers = MEM_W(COMPUTERS, survey_vram);
    for (int player = 0; player < 4; player++) {
        gpr base = GW_PLAYER + player * PLAYER_SIZE;
        if (groups != 0xFFFFFFFF) {
            MEM_B(0, base) = (groups >> (24 - player * 8)) & 0xFF;
        }
        if (computers & 0x80000000) {
            MEM_B(4, base) = (MEM_B(4, base) & ~1) | ((computers >> player) & 1);
        }
    }
    if (*overlay >= 0x01 && *overlay <= 0x46) {
        MEM_B(0, MINIGAME_INDEX) = (int8_t)*overlay;
    }
    MEM_W(REQUEST, survey_vram) = 0;
    MEM_W(WARPS, survey_vram) = MEM_W(WARPS, survey_vram) + 1;
}
