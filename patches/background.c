// A board's background to a picture wider than 4:3. A board is a map of 64x48 tiles, which the game streams in from
// the ROM as its camera moves (src/overlays/ovl_80_shared_board/FA250.c): func_800E7514 picks the 6x6 tiles under the
// camera's 320x240, func_800E7384 gives each a slot of the 36 it has and sends it to a thread that reads it
// (func_800E6C80) and one that decompresses it (func_800E6DEC), and func_800E86CC draws them, each as a 64x32 and a
// 64x16 rectangle (func_800E76EC), before the board's 3D, zoomed in by D_80101250 (MBCameraZoomSet), mirrored while
// D_80102DC0 has 0x10, and with another bank's tiles in place of some, or faded in over them, while D_80101268 is set.
// RT64's Aspect Ratio Expand widens the 3D but can't draw tiles the game never drew, and RT64 keeps a 4:3 picture's
// rectangles inside 4:3, so the board drew in a 4:3 box with its 3D spaces and characters out past it on black.
//
// Here the game picks the tiles under a 21:9 picture at this frame's view and the last one's (board_back_area: what a
// frame RT64 draws between two of the game's can show), those under 4:3 first, from 124 slots (the game's 36 and 88
// more here), through queues as long, and draws them through lib/mario-party-engine's board_back.c: as quads at their
// places on the map, in a list of their own with black behind them, kept to the picture's aspect and moved by an
// interpolated scroll, so the window's sides show more of the map, and the board pans with its 3D spaces. A tile past
// 4:3 is sent to be read only while fewer than EXTRA_IN_FLIGHT are on their way, since each holds its compressed bytes
// on the game's heap until it is decompressed, and the game sized that heap for its 36. Everything else is the game's
// as it was.

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
#define GAME_SLOTS 36
#define SLOTS (GAME_SLOTS + COLUMNS * ROWS)
#define SLOT_BYTES 0x1800
#define EXTRA_IN_FLIGHT 12

// src/overlays/ovl_80_shared_board/FA250.c's own.
typedef struct {
    s32 unk0;
    s32 unk4;
    s32 unk8;
    s32 unkC;
} MapSize;

typedef struct {
    s32 unk_00;
    s32* unk_04;
    char pad08[4];
} TileBank;

extern MapSize* D_80101248_shared_board;
extern u32* D_80101244_shared_board;
extern s32 D_80102DB8_shared_board;
extern u16 D_80102DC0_shared_board;
extern s16 D_80102DC2_shared_board;
extern s16 D_80102DC4_shared_board;
extern s16 D_80102DC6_shared_board;
extern s16 D_80102DC8_shared_board;
extern OverlayBgEntry D_80102DD8_shared_board[GAME_SLOTS];
extern OverlayBgEntry D_80103140_shared_board[];
extern s32 D_8010345C_shared_board;
extern s32 D_80103460_shared_board;
extern TileBank* D_80103464_shared_board;
extern OSMesgQueue D_80103468_shared_board;
extern OSThread D_80103510_shared_board;
extern OSThread D_80103EC0_shared_board;
extern s32 D_80104070_shared_board;
extern OSMesgQueue D_80104880_shared_board;
extern OSMesgQueue D_80104928_shared_board;
extern OSMesgQueue D_801049D0_shared_board;
extern void* D_801049E8_shared_board;
extern f32 D_801049F8_shared_board[];
extern f32 D_80101250_shared_board;
extern u8 D_8010124C_shared_board;
extern s32 D_80101268_shared_board;
extern u8 D_8010126C_shared_board;
extern Gfx D_80101270_shared_board[];

void func_800E6C80_shared_board_000FA8A0(void);
void func_800E6DEC_shared_board_000FAA0C(void);
void func_800E6FBC_shared_board(void);
void func_800E6FDC_shared_board_000FABFC(void);
void func_800E7018_shared_board_000FAC38(void);
void func_800E9AF0_shared_board(void* arg0, s32 arg1);

// The slots past the game's 36, and their tiles.
static OverlayBgEntry extra_slots[SLOTS - GAME_SLOTS];
static u16 extra_slot_data[SLOTS - GAME_SLOTS][SLOT_BYTES / 2] __attribute__((aligned(16)));
// The tiles to draw: [column][row], from (first_column, first_row) on the map.
static OverlayBgEntry* grid[COLUMNS][ROWS];
static s32 first_column, first_row;
// The three queues' messages, as many as there are slots.
static OSMesg read_messages[SLOTS];
static OSMesg decompress_messages[SLOTS];
static OSMesg done_messages[SLOTS];

static OverlayBgEntry* slot(s32 i) {
    return (i < GAME_SLOTS) ? &D_80102DD8_shared_board[i] : &extra_slots[i - GAME_SLOTS];
}

RECOMP_PATCH void func_800E6EC8_shared_board_000FAAE8(void) {
    // @recomp Queues as long as there are slots.
    osCreateMesgQueue(&D_80103468_shared_board, read_messages, SLOTS);
    osCreateMesgQueue(&D_80104880_shared_board, decompress_messages, SLOTS);
    osCreateMesgQueue(&D_80104928_shared_board, done_messages, SLOTS);
    osCreateMesgQueue(&D_801049D0_shared_board, &D_801049E8_shared_board, 2);
    D_80104070_shared_board = 1;
    osCreateThread(&D_80103EC0_shared_board, 0x64, (void (*)(void*))func_800E6DEC_shared_board_000FAA0C, NULL,
                   &D_80104880_shared_board, 1);
    osStartThread(&D_80103EC0_shared_board);
    osCreateThread(&D_80103510_shared_board, 0x65, (void (*)(void*))func_800E6C80_shared_board_000FA8A0, NULL,
                   &D_80103EC0_shared_board, 4);
    osStartThread(&D_80103510_shared_board);
}

RECOMP_PATCH void func_800E7068_shared_board_000FAC88(void) {
    s32 i;

    osJamMesg(&D_80103468_shared_board, NULL, OS_MESG_NOBLOCK);
    osJamMesg(&D_80104880_shared_board, NULL, OS_MESG_NOBLOCK);
    func_800E6FBC_shared_board();
    func_800E6FDC_shared_board_000FABFC();
    osRecvMesg(&D_801049D0_shared_board, NULL, OS_MESG_BLOCK);
    osRecvMesg(&D_801049D0_shared_board, NULL, OS_MESG_BLOCK);
    // @recomp Every slot.
    for (i = 0; i < SLOTS; i++) {
        OverlayBgEntry* s = slot(i);
        if (s->compressedData != NULL) {
            HuMemMemoryFreePerm(s->compressedData);
            s->compressedData = NULL;
        }
    }
}

RECOMP_PATCH void func_800E7130_shared_board_000FAD50(void) {
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        OverlayBgEntry* s = slot(i);
        s->unk0 = 0;
        s->unk1 = 0;
        s->unk2 = -1;
        // @recomp The game's 36 from its heap, as before, the others from here.
        s->pixels = (i < GAME_SLOTS) ? HuMemAlloc(SLOT_BYTES) : extra_slot_data[i - GAME_SLOTS];
        s->compressedData = NULL;
    }
    bzero(grid, sizeof(grid));
}

RECOMP_PATCH void func_800E71A8_shared_board_000FADC8(void) {
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        OverlayBgEntry* s = slot(i);
        if ((s->compressedData == NULL) && (s->unk0 == 0)) {
            s->unk2 = -1;
        }
        s->unk0 = 0;
    }
}

RECOMP_PATCH void func_800E71EC_shared_board_000FAE0C(void) {
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        OverlayBgEntry* s = slot(i);
        s->unk0 = 0;
        s->unk2 = -1;
        // @recomp Only the game's 36 came from its heap.
        if ((i < GAME_SLOTS) && (s->pixels != NULL)) {
            HuMemFree(s->pixels);
        }
        s->pixels = NULL;
    }
}

// func_800E7300, func_800E7330 and func_800E7358: a tile's index in the map, and its bytes' size and place in the ROM.
static u16 tile_index(s32 x, s32 y) {
    return (((D_80101248_shared_board->unkC - y) - 1) * D_80101248_shared_board->unk8) + x + 1;
}

// func_800E7384: the slot holding a tile, or a free one it is sent to be read into, or NULL if there is none or it
// isn't the game's own and too many are on their way.
static OverlayBgEntry* tile_slot(u16 tile, s32 games) {
    OverlayBgEntry* free_slot = NULL;
    u16 bank = 0;
    s32 in_flight = 0;
    s32 i;

    if (D_8010345C_shared_board != 0) {
        for (i = 0; i < D_80103464_shared_board[D_80103460_shared_board].unk_00 - 1; i++) {
            if (tile == D_80103464_shared_board[D_80103460_shared_board].unk_04[i]) {
                bank = D_8010345C_shared_board;
            }
        }
    }

    for (i = 0; i < SLOTS; i++) {
        OverlayBgEntry* s = slot(i);
        if (s->unk2 == -1) {
            free_slot = s;
        } else if ((s->unk2 == tile) && (s->unk1 == bank)) {
            s->unk0 = 1;
            return s;
        }
        in_flight += (s->compressedData != NULL);
    }

    if ((free_slot != NULL) && (games || (in_flight < EXTRA_IN_FLIGHT))) {
        u16 index = tile + 1;
        free_slot->unk0 = 1;
        free_slot->unk2 = tile;
        free_slot->unk1 = bank;
        free_slot->romAddr = D_80102DB8_shared_board + D_80101244_shared_board[index];
        free_slot->compressedSize = D_80101244_shared_board[index + 1] - D_80101244_shared_board[index];
        free_slot->compressedData = HuMemMemoryAllocPerm(free_slot->compressedSize);
        osSendMesg(&D_80103468_shared_board, free_slot, OS_MESG_NOBLOCK);
        return free_slot;
    }
    return NULL;
}

// Where the picture's left and top edges are on the map at a zoom of 1, as func_800E76EC draws: the scroll, less the
// part of a pixel the 3D's viewport (D_801049F8, in quarter pixels) is past a whole one.
static f32 left_edge(void) {
    f32 quarters = D_801049F8_shared_board[0] / 4.0f;
    return (f32)((D_80102DC6_shared_board + D_80102DC2_shared_board) - 160) - (quarters - (f32)(s32)quarters);
}

static f32 top_edge(void) {
    f32 quarters = D_801049F8_shared_board[1] / 4.0f;
    return (f32)((D_80102DC8_shared_board + D_80102DC4_shared_board) - 120) - (quarters - (f32)(s32)quarters);
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

RECOMP_PATCH void func_800E7514_shared_board_000FB134(void) {
    f32 zoom = D_80101250_shared_board;
    f32 left = left_edge();
    f32 top = top_edge();
    // The map under the 4:3 picture: a screen x is the map's (x - left) * zoom, or mirrored, (320 - (x - left)) * zoom.
    f32 low_x = (D_80102DC0_shared_board & 0x10) ? (left + 320.0f - 320.0f / zoom) : left;
    f32 high_x = low_x + 320.0f / zoom;
    f32 low_y = top;
    f32 high_y = top + 240.0f / zoom;
    f32 area[4];
    s32 pass, i, j;

    // From the map under the wide picture at this frame's view and the last one's: COLUMNS x ROWS from there cover
    // both at a zoom of 1 or more.
    board_back_area(left, top, zoom, (D_80102DC0_shared_board & 0x10) != 0, WIDE_SIDE, area);
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
                if ((column < 0) || (row < 0) || (column >= D_80101248_shared_board->unk8) ||
                    (row >= D_80101248_shared_board->unkC)) {
                    grid[j][i] = NULL;
                } else {
                    grid[j][i] = tile_slot(tile_index(column, row), games);
                }
            }
        }
    }
}

RECOMP_PATCH void func_800EA364_shared_board(s16 arg0) {
    s32 i;
    u32 offset;

    switch (arg0) {
        case 0:
            func_800E9AF0_shared_board(NULL, 0);
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

// The other bank's tile in place of this one, as the game finds it, or NULL.
static OverlayBgEntry* other_bank(OverlayBgEntry* entry) {
    s32 i;

    for (i = 0; (i < (D_80103464_shared_board->unk_00 - 1)) && (i < GAME_SLOTS); i++) {
        if (entry->unk2 == D_80103140_shared_board[i].unk2) {
            break;
        }
    }
    if ((i != (D_80103464_shared_board->unk_00 - 1)) && (i != GAME_SLOTS)) {
        return &D_80103140_shared_board[i];
    }
    return NULL;
}

// The pixels of the tile at (j, i) in the grid drawn first (the other bank's once it has faded in), or with faded the
// other bank's faded in over it; NULL past the grid or where there is none.
static u16* tile_pixels(s32 j, s32 i, s32 faded) {
    OverlayBgEntry* entry;
    OverlayBgEntry* other;

    if ((j >= COLUMNS) || (i >= ROWS) || (grid[j][i] == NULL)) {
        return NULL;
    }
    entry = grid[j][i];
    other = (D_80101268_shared_board != 0) ? other_bank(entry) : NULL;
    if (faded) {
        return (other != NULL) ? other->pixels : NULL;
    }
    return ((other != NULL) && (D_8010126C_shared_board == 0xFF)) ? other->pixels : entry->pixels;
}

// @recomp func_800E76EC's 64x32 and 64x16 parts, each loaded alone and filtered, met in a seam at a high resolution:
// a tile is loaded with its neighbours' edge texels (board_back.c says why).
static void draw_tile(Gfx** gfx, s32 j, s32 i, s32 faded) {
    board_back_tile(gfx, (first_column + j) * TILE_WIDTH, (first_row + i) * TILE_HEIGHT, tile_pixels(j, i, faded),
                    tile_pixels(j + 1, i, faded), tile_pixels(j, i + 1, faded), tile_pixels(j + 1, i + 1, faded));
}

RECOMP_PATCH void func_800E86CC_shared_board_000FC2EC(Gfx** game_gfx, s32 arg1, u8 arg2) {
    s16 saved;
    s32 i, j;
    Gfx** gfx;

    if (arg2 || !(D_80102DC0_shared_board & 4)) {
        return;
    }
    func_800E7018_shared_board_000FAC38();
    if (!(D_80102DC0_shared_board & 1)) {
        return;
    }
    func_800E71A8_shared_board_000FADC8();
    saved = func_8004D6AC_main(0xC8, 0, 0);
    func_800E7514_shared_board_000FB134();
    func_8004D6E8_main(saved);

    // @recomp In a list of the tiles' own, black behind them, to the window's edges.
    gfx = board_back_begin(game_gfx);
    gSPDisplayList((*gfx)++, D_80101270_shared_board);
    gDPSetEnvColor((*gfx)++, 0xFF, 0xFF, 0xFF, D_8010124C_shared_board);
    if (D_8010124C_shared_board < 0xFF) {
        if (D_80102DC0_shared_board & 8) {
            gDPSetCombineLERP((*gfx)++, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, 0, ENVIRONMENT, ENVIRONMENT, TEXEL0,
                              ENVIRONMENT, 0, 0, 0, 0, ENVIRONMENT);
        } else {
            gDPSetCombineLERP((*gfx)++, 0, 0, 0, TEXEL0, 0, 0, 0, ENVIRONMENT, 0, 0, 0, TEXEL0, 0, 0, 0, ENVIRONMENT);
        }
        gDPSetRenderMode((*gfx)++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    } else {
        if (D_80102DC0_shared_board & 8) {
            gDPSetCombineLERP((*gfx)++, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, 0, TEXEL0, ENVIRONMENT, TEXEL0,
                              ENVIRONMENT, 0, 0, 0, 0, TEXEL0);
        }
        gDPSetRenderMode((*gfx)++, G_RM_AA_SUB_SURF, G_RM_AA_SUB_SURF2);
    }

    // @recomp The tiles at their places on the map, moved by the scroll, zoomed and mirrored as the game draws them.
    board_back_view(gfx, left_edge(), top_edge(), D_80101250_shared_board, (D_80102DC0_shared_board & 0x10) != 0,
                    D_80101248_shared_board->unk8 * TILE_WIDTH);
    for (i = 0; i < ROWS; i++) {
        for (j = 0; j < COLUMNS; j++) {
            if (grid[j][i] == NULL) {
                continue;
            }
            draw_tile(gfx, j, i, FALSE);
            if ((tile_pixels(j, i, TRUE) != NULL) && (D_8010126C_shared_board > 0) && (D_8010126C_shared_board < 0xFF)) {
                gDPSetEnvColor((*gfx)++, 0xFF, 0xFF, 0xFF, D_8010126C_shared_board);
                gDPSetCombineLERP((*gfx)++, 0, 0, 0, TEXEL0, 0, 0, 0, ENVIRONMENT, 0, 0, 0, TEXEL0, 0, 0, 0,
                                  ENVIRONMENT);
                gDPSetRenderMode((*gfx)++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
                draw_tile(gfx, j, i, TRUE);
                gDPSetEnvColor((*gfx)++, 0xFF, 0xFF, 0xFF, D_8010124C_shared_board);
                gDPSetCombineMode((*gfx)++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                gDPSetRenderMode((*gfx)++, G_RM_AA_SUB_SURF, G_RM_AA_SUB_SURF2);
            }
        }
    }
    board_back_end(gfx);
    func_800E6FDC_shared_board_000FABFC();
}
