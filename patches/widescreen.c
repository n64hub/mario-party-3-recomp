// What a picture wider than 4:3 needs besides RT64's Aspect Ratio Expand, which widens the 3D: a camera's view drawn to
// the window's edges. Mario Party's patches/widescreen.c, with Mario Party 3's names.
//
// Each camera has a scissor (src/camera.c's func_800127C4_main sets it before the camera's view is drawn, from the
// rectangle CameraScissorSet gave it), and RT64 keeps a scissor that stops short of 4:3's edges as far in from the
// window's, so a board, whose camera keeps the console's overscan border (16, 12, 304, 228: the shared board code's
// D_80101770_shared_board), drew in a box with black at its sides. A camera whose scissor is the whole screen or that
// border is drawn to the window's edges (gEXSetScissor, from the left and right): from top to bottom for the border,
// and between the game's own top and bottom while it animates them, as a board's wipe between turns closes it to a band
// across the middle. A screen split between players (a minigame's halves and quarters) keeps the game's.

#include "patches.h"
#include "rt64_extended_gbi.h"
#include "game/camera.h"

#define BORDER_LEFT 16
#define BORDER_TOP 12
#define BORDER_RIGHT 304
#define BORDER_BOTTOM 228

RECOMP_PATCH void func_800127C4_main(s16 camIndex, Gfx** dispList) {
    HuCamera* camera = &gCameraList[camIndex];
    s32 x0 = camera->screenLeft;
    s32 y0 = camera->screenTop;
    s32 x1 = camera->screenRight;
    s32 y1 = camera->screenBottom;

    // @recomp The whole screen, or the border, to the window's edges.
    if (((x0 == 0) && (x1 == (s32)SCREEN_WIDTH)) || ((x0 == BORDER_LEFT) && (x1 == BORDER_RIGHT))) {
        if ((y0 == BORDER_TOP) && (y1 == BORDER_BOTTOM)) {
            y0 = 0;
            y1 = SCREEN_HEIGHT;
        }
        gEXSetScissor((*dispList)++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, y0, 0, y1);
        return;
    }

    gDPSetScissor((*dispList)++, G_SC_NON_INTERLACE, camera->screenLeft, camera->screenTop, camera->screenRight,
                  camera->screenBottom);
}
