#include "patches.h"
#include "engine/ovlman.h"

// The runtime registers the code inside ROM 0x1000 plus 1 MB at startup, which is main, and any other code only when
// it is told it was loaded (recomp_load_overlays). Every overlay (a minigame, a board, a menu, most of them at VRAM
// 0x800F65E0) comes in through OvlLoad (src/engine/ovlman.c), by its index in overlay_table.

RECOMP_PATCH void OvlLoad(s32 overlayIndex) {
    u8 *rom_start;
    u8 *rom_end;
    u8 *bss_start;
    u8 *bss_end;
    u8* curBssAddr;

    rom_start = overlay_table[overlayIndex].rom_start;
    rom_end = overlay_table[overlayIndex].rom_end;
    bss_start = overlay_table[overlayIndex].bss_start;
    bss_end = overlay_table[overlayIndex].bss_end;

    // @recomp Register the code this brings in.
    recomp_load_overlays((u32) rom_start, overlay_table[overlayIndex].ram_start, rom_end - rom_start);

    HuRomDmaCodeRead(rom_start, overlay_table[overlayIndex].ram_start, rom_end - rom_start);

    curBssAddr = bss_start;
    while (bss_start < bss_end) {
        *curBssAddr++ = 0;
        bss_start++;
    }

    D_800C1C70 = 0;
}
