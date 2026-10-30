// What a picture wider than 4:3 needs besides RT64's Aspect Ratio Expand, which widens the 3D: a camera's view drawn to
// the window's edges, its 3D moving with the board under it. The engine's patches/camera_view.c says how; here are
// Mario Party 3's two functions that start a camera's view, src/camera.c's func_80012640_main (its viewport and
// projection) and func_800127C4_main (its scissor). A board pans its 3D by moving its camera's viewport
// (src/overlays/ovl_80_shared_board/FA250.c, beside MBBackOffsetSet).

#include "patches.h"
#include "game/camera.h"
#include "../lib/mario-party-engine/patches/camera_view.h"
#include "../lib/mario-party-engine/patches/board_back.h"

extern s16 D_800D418E_main;

RECOMP_PATCH void func_80012640_main(s16 camIndex, Gfx** dispList) {
    HuCamera* camera = &gCameraList[camIndex];
    HuCamMtxs* camMtx = &camera->mtxs[D_800D418E_main];
    Vp* viewport = &camera->viewports[D_800D418E_main];

    viewport->vp.vscale[0] = (s16)camera->screenScale.x;
    viewport->vp.vscale[1] = (s16)camera->screenScale.y;
    viewport->vp.vscale[2] = (s16)camera->screenScale.z;
    viewport->vp.vtrans[0] = (s16)camera->screenPos.x;
    viewport->vp.vtrans[1] = (s16)camera->screenPos.y;
    viewport->vp.vtrans[2] = (s16)camera->screenPos.z;

    // @recomp A widened camera's viewport in the middle, what it was moved by in its projection.
    camera_view_projection(dispList, camIndex, viewport, camera->perspNorm, &camMtx->perspMtx,
                           camera_view_widened(camera->screenLeft, camera->screenRight));
    gSPMatrix((*dispList)++, OS_PHYSICAL_TO_K0(&camMtx->lookAtMtx), G_MTX_LOAD);
}

RECOMP_PATCH void func_800127C4_main(s16 camIndex, Gfx** dispList) {
    HuCamera* camera = &gCameraList[camIndex];

    // @recomp The whole screen, or the border (in the board's sideways wipe between turns too), to the window's edges.
    camera_view_scissor(dispList, camera->screenLeft, camera->screenTop, camera->screenRight, camera->screenBottom);
}

// The scenes the game draws at 4:3 whatever the window's width, by their overlay (OvlLoad, overlays.c, calls
// widescreen_overlay as each loads): the engine keeps black at the window's sides while one plays (board_back.h's
// board_back_scene_4x3), its src/wipe.cpp drawing it over whatever the scene's 3D reaches past 4:3 at the end of each
// frame (us.toml's hook at func_8001B0B4_main's gDPFullSync), and the camera's view and the HUD stay inside 4:3.
// - 0x06, Hide and Sneak: the forest and the path past the stage's curtains are a 2D picture of the screen's size, and
//   so is the tray the players' buttons sit in. Stretched over a wide window, the path's hiding places moved out from
//   the stump, the rock and the house standing on them, and the tray from its buttons.
// - 0x07, Ridiculous Relay: a 2D course of the screen's size, and the Bullet Bills and the hang glider past it.
// - 0x08, Thwomp Pull: two halves side by side, the pole between them, and the Thwomps past 4:3.
// - 0x0B, Eatsa Pizza: the plate, the table and the cutlery around the 3D pizza are a 2D picture of the screen's size.
//   Stretched over a wide window, the plate was nearly three times the pizza's width.
// - 0x0D, Pump Pump Away: a 2D sky of the screen's size, and the stage under it past 4:3.
// - 0x0E, Hyper Hydrants: two halves side by side, and the hoses' spray past 4:3.
// - 0x0F, Picking Panic: a team's half above the other's, and past 4:3 the tree's branches and the players' lines.
// - 0x10, Cosmic Coaster: two tracks side by side, and the rocks and the Bowser panels past 4:3.
// - 0x12, Etch 'n' Catch: the notebook is a 2D picture of the screen's size, and the lines the players draw are drawn
//   into it. Stretched over a wide window, each line showed further from the middle than the player drawing it.
// - 0x13, Log Jam: its ground and the logs on it a 2D picture of the screen's size, under a sky drawn across the
//   window.
// - 0x14, Slot Synch: a stage of the screen's size, and the slot machines' reels past it.
// - 0x16, Toadstool Titan: a room of the screen's size, and past 4:3 the crowd of Marios coming into it.
// - 0x17, Aces High: four quarters inside 4:3, each a player's sky, its bank of cloud cut off at 4:3's edge, and past
//   them its clear's blue, the black lines between the quarters running on to the window's edges.
// - 0x1D, Messy Memory: the room, its shelves and their posts, is a 2D picture of the screen's size, and the things
//   on the shelves are 3D. Stretched over a wide window, the posts stood far out past the things between them.
// - 0x1E, Picture Imperfect: a picture of the screen's size, and past it the stage, the players cut in two at 4:3's
//   edge and their stars.
// - 0x25, Awful Tower: four quarters, each a player's tower, and the ground under them past 4:3.
// - 0x27, Pipe Cleaners: the wall behind the pipes, with the A and B pads by each pipe's opening, is a 2D picture of
//   the screen's size. Stretched over a wide window, the pads moved out from the 3D openings they belong to.
// - 0x28, Snowball Summit: its ice past 4:3 drawn in stripes, under a sky drawn across the window.
// - 0x2A, Stacked Deck: the carpet the 3D cards lie on and the toy blocks at its corners are a 2D picture of the
//   screen's size. Stretched over a wide window, the carpet went on far past the cards.
// - 0x2B, Three Door Monty: the castle's wall and its three doors are a 2D picture of the screen's size. Stretched
//   over a wide window, the Z and A doors stood out past where the players run to them.
// - 0x30, Eye Sore: four views inside 4:3, and the line between the top two and the bottom two ran on past them over
//   the black.
// - 0x31, Vine With Me: a team's view above the other's, and past 4:3 the window's sides white with its leaves.
// - 0x32, Popgun Pickoff: its sky and hills are a 2D picture of the screen's size, and a darker box behind the castle
//   is drawn at 4:3. Stretched over a wide window, the box showed in the sky, its sides at 4:3's.
// - 0x33, End of the Line: two views side by side inside 4:3, then a finish shot across the window with a black
//   column at 4:3's left edge, where the screen's first column is.
// - 0x34, Bowser Toss: a player's half above the other's, and the wall between them past 4:3.
// - 0x36, Motor Rooter: two views side by side inside 4:3, and its clear white past them.
// - 0x38, Crowd Cover: the desk, the paper and the paints are a 2D picture of the screen's size, the players at the
//   paper's corners. Stretched over a wide window, the paper went on far past them.
// - 0x39, Tick Tock Hop: the clock's face is a 2D picture of the screen's size, and its hand and the players on it
//   are 3D. Stretched over a wide window, the 3 and the 9 moved out past the hand's reach.
// - 0x3A, Fowl Play: two halves side by side, and past 4:3 a strip of their sky and hills over black.
// - 0x3B, Winner's Wheel: a black stage of the screen's size, and past 4:3 rows of Piranha Plants and a player seen
//   close, cut off at its edge.
// - 0x42, Game Guy's Roulette, 0x43, Game Guy's Lucky 7, and 0x45, Game Guy's Sweet Surprise: the room, with the
//   roulette's stand, Lucky 7's numbered blocks or Sweet Surprise's table, is a 2D picture of the screen's size, and
//   Game Guy, the roulette's wheel and the Chain Chomps are 3D. Stretched over a wide window, what they stand on
//   moved out from under them.
// - 0x4F, a board's results on the star stage: its sky is a 2D picture of the screen's size, and the stage's 3D past
//   4:3, confetti-covered, was drawn beside it over the black.
static const u8 overlays_4x3[] = { 0x06, 0x07, 0x08, 0x0B, 0x0D, 0x0E, 0x0F, 0x10, 0x12, 0x13, 0x14, 0x16, 0x17, 0x1D,
                                   0x1E, 0x25, 0x27, 0x28, 0x2A, 0x2B, 0x30, 0x31, 0x32, 0x33, 0x34, 0x36, 0x38, 0x39,
                                   0x3A, 0x3B, 0x42, 0x43, 0x45, 0x4F };

void widescreen_overlay(s32 overlay) {
    s32 picture_4x3 = FALSE;
    u32 i;

    for (i = 0; i < sizeof(overlays_4x3); i++) {
        if (overlays_4x3[i] == overlay) {
            picture_4x3 = TRUE;
        }
    }
    board_back_scene_4x3(picture_4x3);
}
