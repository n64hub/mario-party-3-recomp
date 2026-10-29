#include "patches.h"
#include "overlay.h"
#include "rom.h"

// The runtime registers the code inside ROM 0x1000 plus 1 MB at startup, which is main, and any other code only when
// it is told it was loaded (recomp_load_overlays). The overlays come in two ways (src/util.c), each by its index in a
// table: OvlLoad copies one of _ovltbl's (the minigames, menus and the boards' screens) whole, and func_8000B1A0_main
// copies the sections a call asks for of one of _modeovltbl's (the three board_code segments, which share their VRAM),
// text and data each on its own. Both DMA with dmaReadOvl.

#define HAS_TEXT_SECTION 1
#define HAS_DATA_SECTION 2
#define HAS_BSS_SECTION 4

extern u8 D_800962F0_main;
// widescreen.c's: whether the scene an overlay plays is drawn at 4:3.
void widescreen_overlay(s32 overlay);

RECOMP_PATCH void func_8000B1A0_main(s32 arg0, u8 sectionFlags) {
    u8* textVramEnd;
    u8* textVramStart;
    u32 romStart;
    u8* dataVramStart;
    u8* dataVramEnd;
    u8* bss_end;
    u8* bss_start;
    u8* curBssAddr;
    u8* textStart;
    u8* tmp;

    romStart = _modeovltbl[arg0].romStart;
    textVramStart = _modeovltbl[arg0].textVramStart;
    textVramEnd = _modeovltbl[arg0].textVramEnd;
    dataVramStart = _modeovltbl[arg0].dataVramStart;
    dataVramEnd = _modeovltbl[arg0].dataVramEnd;
    bss_start = _modeovltbl[arg0].bssVramStart;
    bss_end = _modeovltbl[arg0].bssVramEnd;
    textStart = _modeovltbl[arg0].vramStart;

    if (sectionFlags & HAS_TEXT_SECTION) {
        // @recomp Register the code this brings in. The runtime takes a section only when the range it is told about
        // holds all of it, and the decomp's section is the whole overlay (text, data and rodata), so the range is.
        recomp_load_overlays(romStart, textStart, _modeovltbl[arg0].romEnd - romStart);
        dmaReadOvl(romStart, textStart, textVramEnd - textVramStart);
    }
    if (sectionFlags & HAS_DATA_SECTION) {
        dmaReadOvl(romStart + (textVramEnd - textVramStart), dataVramStart, dataVramEnd - dataVramStart);
    }
    if (sectionFlags & HAS_BSS_SECTION) {
        curBssAddr = tmp = bss_start;
        while (tmp < bss_end) {
            *curBssAddr++ = 0;
            tmp++;
        }
    }
}

RECOMP_PATCH void OvlLoad(s32 overlayIndex) {
    u32 rom_start;
    u32 rom_end;
    u8* bss_start;
    u8* bss_end;
    u8* curBssAddr;

    rom_start = _ovltbl[overlayIndex].romStart;
    rom_end = _ovltbl[overlayIndex].romEnd;
    bss_start = _ovltbl[overlayIndex].bssVramStart;
    bss_end = _ovltbl[overlayIndex].bssVramEnd;

    // @recomp Register the code this brings in.
    recomp_load_overlays(rom_start, _ovltbl[overlayIndex].vramStart, rom_end - rom_start);

    dmaReadOvl(rom_start, _ovltbl[overlayIndex].vramStart, rom_end - rom_start);

    curBssAddr = bss_start;
    while (bss_start < bss_end) {
        *curBssAddr++ = 0;
        bss_start++;
    }

    D_800962F0_main = 0;

    // @recomp Whether the scene is drawn at 4:3 (widescreen.c).
    widescreen_overlay(overlayIndex);
}
