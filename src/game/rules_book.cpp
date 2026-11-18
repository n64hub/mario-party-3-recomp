// The book on a minigame's rules screen (the inst overlay, 0x70): its cover is eight strips side by side, each a part
// of its own so the cover can bend as it opens, each 37 units wide (x 0 to 37) and placed 37.5 apart, so half a unit
// lies between each strip and the next. At the console's 320x240 that is a third of a pixel, which its antialiasing
// covers; drawn bigger it opens to a light line down the cover at every join, the gap's background showing through.
//
// The hook (us.toml) is where func_8001B0B4_main has walked a model's parts for the frame, with the model's data. On
// the rules screen it looks through the model's vertex buffers (the data's at 0x40 and 0x44 to 0x4C, each a block of
// the game's heap, its size 0x10 before it) for the strips' vertices: x 37, y 0, z 199 or -199, s at a join between
// two strips (a multiple of 288, the strip's 9 texels, up to the seventh) and t at the cover's top or bottom (0 or
// 3039). Each is moved to x 38, so each strip reaches half a unit under its neighbour and the joins close; the
// cover's own edges (x 0 at s 2298 and x 37 at s 0) stay where they are. The game builds the buffers once and doesn't
// write them again, so the vertices are moved at the first frame, and later frames find them moved.

#include "recomp.h"
#include "librecomp/helpers.hpp"

namespace {
    // omovl, the overlay playing, and the rules screen's.
    constexpr gpr OMOVL = (gpr)(int32_t)0x800CE200;
    constexpr int32_t INST = 0x70;

    // The data's vertex buffers, and the most of one looked through.
    constexpr int BUFFERS = 0x40;
    constexpr int BUFFER_COUNT = 4;
    constexpr uint32_t BUFFER_MOST = 0x8000;

    // A strip's vertex (Vtx: x, y, z at 0, 2, 4, s and t at 8 and 10, in texels times 32).
    constexpr int VTX_SIZE = 16;
    constexpr int16_t EDGE = 37;
    constexpr int16_t HALF_DEPTH = 199;
    constexpr int16_t JOIN = 9 * 32;
    constexpr int16_t LAST_JOIN = 7 * JOIN;
    constexpr int16_t BOTTOM = 3039;

    bool in_rdram(gpr address) {
        return ((uint32_t)address >= 0x80000000u) && ((uint32_t)address < 0x80800000u);
    }
}

// The hook: a model's data, its parts walked for the frame.
extern "C" void mp_model_walked(uint8_t* rdram, gpr data) {
    if (((int32_t)MEM_W(0, OMOVL) != INST) || !in_rdram(data)) {
        return;
    }

    gpr seen[BUFFER_COUNT] = {};
    for (int b = 0; b < BUFFER_COUNT; b++) {
        const gpr buffer = (gpr)(int32_t)MEM_W(BUFFERS + b * 4, data);
        bool again = false;
        for (int i = 0; i < b; i++) {
            again = again || (seen[i] == buffer);
        }

        seen[b] = buffer;
        if (again || !in_rdram(buffer)) {
            continue;
        }

        uint32_t size = (uint32_t)MEM_W(-0x10, buffer);
        if (size > BUFFER_MOST) {
            size = BUFFER_MOST;
        }

        for (uint32_t at = 0; at + VTX_SIZE <= size; at += VTX_SIZE) {
            const gpr v = buffer + at;
            const int16_t x = MEM_H(0, v);
            const int16_t y = MEM_H(2, v);
            const int16_t z = MEM_H(4, v);
            const int16_t s = MEM_H(8, v);
            const int16_t t = MEM_H(10, v);
            if ((x == EDGE) && (y == 0) && ((z == HALF_DEPTH) || (z == -HALF_DEPTH)) && (s > 0) && (s <= LAST_JOIN) &&
                ((s % JOIN) == 0) && ((t == 0) || (t == BOTTOM))) {
                MEM_H(0, v) = EDGE + 1;
            }
        }
    }
}
