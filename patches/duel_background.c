// A duel board's background to a picture wider than 4:3, as patches/background.c does a board's. A duel board streams
// its map of 64x48 tiles with code of its own (src/overlays/ovl_81_name/CCD20.c), the board's own almost line for line:
// func_800E5DD8 picks the 6x6 tiles under the camera's 320x240, func_800E5D18 gives each a slot of the 50 it has and
// sends it to a thread that reads it (func_800E5618) and one that decompresses it (func_800E5784), and func_800E77D8
// draws them, each as a 64x32 and a 64x16 rectangle (func_800E5FB0), zoomed in by D_80101000 (MBDCameraZoomSet) and
// mirrored while D_80102D30 has 0x10, with no second bank. So a duel board had what a board had before
// patches/background.c: drawn in a 4:3 box with its 3D out past it on black, and a hard line along every part's and
// tile's edge at a high resolution, where a part loaded alone and filtered ends on its last texel (board_back.c).
//
// Here, as there, the game picks the tiles under a 21:9 picture at this frame's view and the last one's, those under
// 4:3 first, from the game's 50 slots and 88 more, through queues as long, and draws them through
// lib/mario-party-engine's board_back.c, each loaded with its neighbours' edge texels. A tile in a slot past the game's
// holds its compressed bytes in board_back_archive's buffer for the slot, and one past 4:3 in the game's slots is sent
// only while fewer than EXTRA_IN_FLIGHT are on their way on the heap. The board's camera is kept where the wider
// picture stays on the map (MBDBackPosClamp, as MBBackPosClamp is there). Everything else is the game's as it was.

#include "patches.h"
#include "game/board.h"
#include "mallocblock.h"
#include "malloc.h"
#include "../lib/mario-party-engine/patches/board_back.h"

#define TILE_WIDTH 64
#define TILE_HEIGHT 48
// The picture past 4:3 on each side at 21:9, in the game's pixels.
#define WIDE_SIDE 120
#define COLUMNS 11
#define ROWS 8
#define GAME_SLOTS 50
#define SLOTS (GAME_SLOTS + COLUMNS * ROWS)
#define SLOT_BYTES 0x1800
#define EXTRA_IN_FLIGHT 12

// src/overlays/ovl_81_name/CCD20.c's UnkModelStruct, as far as the map's size: a tile's width and height, and the
// map's columns and rows.
typedef struct {
    s32 unk0;
    s32 unk4;
    s32 unk8;
    s32 unkC;
} DuelMapSize;

extern DuelMapSize* D_80100FF8_name_81;
extern u32* D_80100FF4_name_81;
extern u32 D_80102D28_name_81;
extern u8 D_80100FFC_name_81;
extern f32 D_80101000_name_81;
extern Gfx D_80101010_name_81[];
extern u16 D_80102D30_name_81;
extern s16 D_80102D32_name_81;
extern s16 D_80102D34_name_81;
extern s16 D_80102D36_name_81;
extern s16 D_80102D38_name_81;
extern OverlayBgEntry D_80102D48_name_81[GAME_SLOTS];
extern OSMesgQueue D_80103210_name_81;
extern OSThread D_801032F0_name_81;
extern OSThread D_80103CA0_name_81;
extern OSPri D_80103E50_name_81;
extern OSMesgQueue D_80104660_name_81;
extern OSMesgQueue D_80104740_name_81;
extern OSMesgQueue D_80104820_name_81;
extern void* D_80104838_name_81;
extern f32 D_80104848_name_81[2];

void func_800E5618_name_81_000CD3E8(void);
void func_800E5784_name_81_000CD554(void);
void func_800E5954_name_81(void);
void func_800E5974_name_81_000CD744(void);
void func_800E890C_name_81(void* arg0, s32 arg1);
f32 mp_aspect_ratio(void);

// The slots past the game's 50, and their tiles.
static OverlayBgEntry extra_slots[SLOTS - GAME_SLOTS];
static u16 extra_slot_data[SLOTS - GAME_SLOTS][SLOT_BYTES / 2] __attribute__((aligned(16)));
// The tiles to draw: [column][row], from (first_column, first_row) on the map.
static OverlayBgEntry* grid[COLUMNS][ROWS];
static s32 first_column, first_row;
// The three queues' messages, as many as there are slots.
static OSMesg read_messages[SLOTS];
static OSMesg decompress_messages[SLOTS];
static OSMesg done_messages[SLOTS];

_Static_assert(SLOTS - GAME_SLOTS <= BOARD_BACK_ARCHIVES, "a buffer for each slot past the game's");

static OverlayBgEntry* slot(s32 i) {
    return (i < GAME_SLOTS) ? &D_80102D48_name_81[i] : &extra_slots[i - GAME_SLOTS];
}

RECOMP_PATCH void func_800E5860_name_81_000CD630(void) {
    // @recomp Queues as long as there are slots.
    osCreateMesgQueue(&D_80103210_name_81, read_messages, SLOTS);
    osCreateMesgQueue(&D_80104660_name_81, decompress_messages, SLOTS);
    osCreateMesgQueue(&D_80104740_name_81, done_messages, SLOTS);
    osCreateMesgQueue(&D_80104820_name_81, &D_80104838_name_81, 2);
    D_80103E50_name_81 = 1;
    osCreateThread(&D_80103CA0_name_81, 0x64, (void (*)(void*))func_800E5784_name_81_000CD554, NULL,
                   &D_80104660_name_81, D_80103E50_name_81);
    osStartThread(&D_80103CA0_name_81);
    osCreateThread(&D_801032F0_name_81, 0x65, (void (*)(void*))func_800E5618_name_81_000CD3E8, NULL,
                   &D_80103CA0_name_81, 4);
    osStartThread(&D_801032F0_name_81);
}

// func_800E59B0: the decompressed tiles' compressed bytes freed.
RECOMP_PATCH void func_800E59B0_name_81_000CD780(void) {
    OverlayBgEntry* s;

    while (osRecvMesg(&D_80104740_name_81, (OSMesg*)&s, OS_MESG_NOBLOCK) == 0) {
        if (s != NULL) {
            // @recomp Not board_back_archive's.
            if (!board_back_own_archive(s->compressedData)) {
                HuMemMemoryFreePerm(s->compressedData);
            }
            s->compressedData = NULL;
        }
    }
}

RECOMP_PATCH void func_800E5A00_name_81_000CD7D0(void) {
    s32 i;

    osJamMesg(&D_80103210_name_81, NULL, OS_MESG_NOBLOCK);
    osJamMesg(&D_80104660_name_81, NULL, OS_MESG_NOBLOCK);
    func_800E5954_name_81();
    func_800E5974_name_81_000CD744();
    osRecvMesg(&D_80104820_name_81, NULL, OS_MESG_BLOCK);
    osRecvMesg(&D_80104820_name_81, NULL, OS_MESG_BLOCK);
    // @recomp Every slot.
    for (i = 0; i < SLOTS; i++) {
        OverlayBgEntry* s = slot(i);
        if (s->compressedData != NULL) {
            if (!board_back_own_archive(s->compressedData)) {
                HuMemMemoryFreePerm(s->compressedData);
            }
            s->compressedData = NULL;
        }
    }
}

RECOMP_PATCH void func_800E5AC8_name_81_000CD898(void) {
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        OverlayBgEntry* s = slot(i);
        s->unk0 = 0;
        s->unk2 = -1;
        // @recomp The game's 50 from its heap, as before, the others from here.
        s->pixels = (i < GAME_SLOTS) ? HuMemAlloc(SLOT_BYTES) : extra_slot_data[i - GAME_SLOTS];
        s->compressedData = NULL;
    }
    bzero(grid, sizeof(grid));
}

RECOMP_PATCH void func_800E5B3C_name_81_000CD90C(void) {
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        OverlayBgEntry* s = slot(i);
        if ((s->compressedData == NULL) && (s->unk0 == 0)) {
            s->unk2 = -1;
        }
        s->unk0 = 0;
    }
}

RECOMP_PATCH void func_800E5B80_name_81_000CD950(void) {
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        OverlayBgEntry* s = slot(i);
        s->unk0 = 0;
        s->unk2 = -1;
        // @recomp Only the game's 50 came from its heap.
        if ((i < GAME_SLOTS) && (s->pixels != NULL)) {
            HuMemFree(s->pixels);
        }
        s->pixels = NULL;
    }
}

// func_800E5C94: a tile's index in the map.
static u16 tile_index(s32 x, s32 y) {
    return (((D_80100FF8_name_81->unkC - y) - 1) * D_80100FF8_name_81->unk8) + x + 1;
}

// func_800E5D18: the slot holding a tile, or a free one it is sent to be read into, or NULL if there is none or it
// isn't the game's own and too many are on their way on the heap.
static OverlayBgEntry* tile_slot(u16 tile, s32 games) {
    OverlayBgEntry* free_slot = NULL;
    void* archive = NULL;
    s32 free_index = 0;
    s32 in_flight = 0;
    u16 index;
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        OverlayBgEntry* s = slot(i);
        if (s->unk2 == -1) {
            free_slot = s;
            free_index = i;
        } else if (s->unk2 == tile) {
            s->unk0 = 1;
            return s;
        }
        in_flight += (s->compressedData != NULL) && !board_back_own_archive(s->compressedData);
    }

    if (free_slot == NULL) {
        return NULL;
    }
    // func_800E5CEC and func_800E5CC4: its bytes' place in the ROM and their size.
    index = tile + 1;
    free_slot->romAddr = D_80102D28_name_81 + D_80100FF4_name_81[index];
    free_slot->compressedSize = D_80100FF4_name_81[index + 1] - D_80100FF4_name_81[index];
    // @recomp A slot past the game's has its own buffer for them, and a tile past 4:3 takes the heap only while few
    // are on their way there.
    if (free_index >= GAME_SLOTS) {
        archive = board_back_archive(free_index - GAME_SLOTS, free_slot->compressedSize);
    }
    if ((archive != NULL) || games || (in_flight < EXTRA_IN_FLIGHT)) {
        free_slot->unk0 = 1;
        free_slot->unk2 = tile;
        free_slot->compressedData = (archive != NULL) ? archive : HuMemMemoryAllocPerm(free_slot->compressedSize);
        osSendMesg(&D_80103210_name_81, free_slot, OS_MESG_NOBLOCK);
        return free_slot;
    }
    return NULL;
}

// The game's own func_800E5D18, which only func_800E5DD8 calls: as tile_slot, never held back.
RECOMP_PATCH OverlayBgEntry* func_800E5D18_name_81(u16 tile) {
    return tile_slot(tile, TRUE);
}

// How much of the map the window shows past each side of the 4:3 picture, at the zoom: a screen x is the map's
// (x - left) * zoom, and the window reaches 120 * aspect from the picture's middle, as far as the map has room for.
static f32 map_past_4x3(f32 zoom) {
    f32 side = 120.0f * mp_aspect_ratio() - 160.0f;

    if (side > D_80102D36_name_81 * zoom - 160.0f) {
        side = D_80102D36_name_81 * zoom - 160.0f;
    }
    if (side < 0.0f) {
        side = 0.0f;
    }
    return side / zoom;
}

// MBDBackPosClamp: the camera's place at the scale arg1, kept where the picture stays on the map; what it was kept
// from, a bit for each edge.
RECOMP_PATCH u16 MBDBackPosClamp_000CD22C(Vec2f* arg0, f32 arg1) {
    // @recomp Each x bound in by the map the window shows past 4:3.
    f32 side = map_past_4x3(D_80101000_name_81) / arg1;
    u16 var_v1;

    var_v1 = 0;
    if (arg0->x <= (((-D_80102D36_name_81 + 160.0f) / arg1) + 160.0f + side)) {
        arg0->x = ((-D_80102D36_name_81 + 160.0f) / arg1) + 160.0f + side;
        var_v1 = 1;
    }
    if (((D_80102D36_name_81 - 160.0f) / arg1) + 160.0f + ((320.0f / arg1) - (320.0f / (arg1 * D_80101000_name_81))) - side <= arg0->x) {
        arg0->x = ((D_80102D36_name_81 - 160.0f) / arg1) + 160.0f + ((320.0f / arg1) - (320.0f / (arg1 * D_80101000_name_81))) - side;
        var_v1 |= 2;
    }
    if (arg0->y <= ((-D_80102D38_name_81 + 120.0f) / arg1) + 120.0f) {
        arg0->y = ((-D_80102D38_name_81 + 120.0f) / arg1) + 120.0f;
        var_v1 |= 4;
    }
    if (((D_80102D38_name_81 - 120.0f) / arg1) + 120.0f + ((240.0f / arg1) - (240.0f / (arg1 * D_80101000_name_81))) <= arg0->y) {
        arg0->y = ((D_80102D38_name_81 - 120.0f) / arg1) + 120.0f + ((240.0f / arg1) - (240.0f / (arg1 * D_80101000_name_81)));
        var_v1 |= 8;
    }
    return var_v1;
}

// Where the picture's left and top edges are on the map at a zoom of 1, as func_800E5FB0 draws: the scroll, less the
// part of a pixel the 3D's viewport (D_80104848, in quarter pixels) is past a whole one.
static f32 left_edge(void) {
    f32 quarters = D_80104848_name_81[0] / 4.0f;
    return (f32)((D_80102D36_name_81 + D_80102D32_name_81) - 160) - (quarters - (f32)(s32)quarters);
}

static f32 top_edge(void) {
    f32 quarters = D_80104848_name_81[1] / 4.0f;
    return (f32)((D_80102D38_name_81 + D_80102D34_name_81) - 120) - (quarters - (f32)(s32)quarters);
}

static s32 floor_div(f32 value, s32 by) {
    s32 whole = (s32)value;
    if ((f32)whole > value) {
        whole--;
    }
    return (whole >= 0) ? (whole / by) : -((by - 1 - whole) / by);
}

// Whether a picture's x (or y) on the map, from..to, is past [low, high) of the 4:3 picture's.
static s32 inside(s32 from, s32 to, f32 low, f32 high) {
    return (to > low) && (from < high);
}

RECOMP_PATCH void func_800E5DD8_name_81_000CDBA8(void) {
    f32 zoom = D_80101000_name_81;
    f32 left = left_edge();
    f32 top = top_edge();
    // The map under the 4:3 picture: a screen x is the map's (x - left) * zoom, or mirrored, (320 - (x - left)) * zoom.
    f32 low_x = (D_80102D30_name_81 & 0x10) ? (left + 320.0f - 320.0f / zoom) : left;
    f32 high_x = low_x + 320.0f / zoom;
    f32 low_y = top;
    f32 high_y = top + 240.0f / zoom;
    f32 area[4];
    s32 pass, i, j;

    // From the map under the wide picture at this frame's view and the last one's: COLUMNS x ROWS from there cover
    // both at a zoom of 1 or more.
    board_back_area(left, top, zoom, (D_80102D30_name_81 & 0x10) != 0, WIDE_SIDE, area);
    first_column = floor_div(area[0], TILE_WIDTH);
    first_row = floor_div(area[1], TILE_HEIGHT);

    // @recomp The tiles under the 4:3 picture first, so that they come first when there are too few slots and are
    // never held back, then the others.
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < ROWS; i++) {
            s32 row = first_row + i;
            for (j = 0; j < COLUMNS; j++) {
                s32 column = first_column + j;
                s32 games = inside(column * TILE_WIDTH, (column + 1) * TILE_WIDTH, low_x, high_x) &&
                            inside(row * TILE_HEIGHT, (row + 1) * TILE_HEIGHT, low_y, high_y);
                if (games != (pass == 0)) {
                    continue;
                }
                if ((column < 0) || (row < 0) || (column >= D_80100FF8_name_81->unk8) ||
                    (row >= D_80100FF8_name_81->unkC)) {
                    grid[j][i] = NULL;
                } else {
                    grid[j][i] = tile_slot(tile_index(column, row), games);
                }
            }
        }
    }
}

RECOMP_PATCH void func_800E924C_name_81(s16 arg0) {
    s32 i;
    u32 offset;

    switch (arg0) {
        case 0:
            func_800E890C_name_81(NULL, 0);
            break;

        case 1:
            // @recomp Every slot's tile in gray.
            for (i = 0; i < SLOTS; i++) {
                OverlayBgEntry* entry = slot(i);
                if (entry->pixels == NULL) {
                    continue;
                }
                for (offset = 0; offset < TILE_WIDTH * TILE_HEIGHT; offset++) {
                    u16 color = entry->pixels[offset];
                    u8 r = (((color >> 11) & 0x1F) * 8) + 4;
                    u8 g = (((color >> 6) & 0x1F) * 8) + 4;
                    u8 b = (((color >> 1) & 0x1F) * 8) + 4;
                    s32 lum = (r * 0.299f) + (g * 0.587f) + (b * 0.114f);
                    if (lum > 255) {
                        lum = 255;
                    }
                    lum = (lum >> 3) & 0x1F;
                    entry->pixels[offset] = (lum << 11) + (lum << 6) + (lum << 1);
                }
            }
            break;
    }
}

// The pixels of the tile at (j, i) in the grid, NULL past the grid or where there is none.
static u16* tile_pixels(s32 j, s32 i) {
    return ((j < COLUMNS) && (i < ROWS) && (grid[j][i] != NULL)) ? grid[j][i]->pixels : NULL;
}

RECOMP_PATCH void func_800E77D8_name_81(Gfx** game_gfx, s32 arg1, u8 arg2) {
    s16 saved;
    s32 i, j;
    Gfx** gfx;

    if (arg2 || !(D_80102D30_name_81 & 4)) {
        return;
    }
    func_800E59B0_name_81_000CD780();
    if (!(D_80102D30_name_81 & 1)) {
        return;
    }
    func_800E5B3C_name_81_000CD90C();
    saved = func_8004D6AC_main(0xC8, 0, 0);
    func_800E5DD8_name_81_000CDBA8();
    func_8004D6E8_main(saved);

    // @recomp In a list of the tiles' own, black behind them, to the window's edges.
    gfx = board_back_begin(game_gfx);
    gSPDisplayList((*gfx)++, D_80101010_name_81);
    gDPSetEnvColor((*gfx)++, 0xFF, 0xFF, 0xFF, D_80100FFC_name_81);
    if (D_80100FFC_name_81 < 0xFF) {
        if (D_80102D30_name_81 & 8) {
            gDPSetCombineLERP((*gfx)++, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, 0, ENVIRONMENT, ENVIRONMENT, TEXEL0,
                              ENVIRONMENT, 0, 0, 0, 0, ENVIRONMENT);
        } else {
            gDPSetCombineLERP((*gfx)++, 0, 0, 0, TEXEL0, 0, 0, 0, ENVIRONMENT, 0, 0, 0, TEXEL0, 0, 0, 0, ENVIRONMENT);
        }
        gDPSetRenderMode((*gfx)++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    } else {
        if (D_80102D30_name_81 & 8) {
            gDPSetCombineLERP((*gfx)++, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, 0, TEXEL0, ENVIRONMENT, TEXEL0,
                              ENVIRONMENT, 0, 0, 0, 0, TEXEL0);
        }
        gDPSetRenderMode((*gfx)++, G_RM_AA_SUB_SURF, G_RM_AA_SUB_SURF2);
    }

    // @recomp The tiles at their places on the map, moved by the scroll, zoomed and mirrored as the game draws them,
    // each loaded with its neighbours' edge texels.
    board_back_view(gfx, left_edge(), top_edge(), D_80101000_name_81, (D_80102D30_name_81 & 0x10) != 0,
                    D_80100FF8_name_81->unk8 * TILE_WIDTH);
    for (i = 0; i < ROWS; i++) {
        for (j = 0; j < COLUMNS; j++) {
            if (grid[j][i] == NULL) {
                continue;
            }
            board_back_tile(gfx, (first_column + j) * TILE_WIDTH, (first_row + i) * TILE_HEIGHT, tile_pixels(j, i),
                            tile_pixels(j + 1, i), tile_pixels(j, i + 1), tile_pixels(j + 1, i + 1));
        }
    }
    board_back_end(gfx);
    func_800E5974_name_81_000CD744();
}
