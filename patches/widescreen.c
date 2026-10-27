// What a picture wider than 4:3 needs besides RT64's Aspect Ratio Expand, which widens the 3D: a camera's view drawn to
// the window's edges, its 3D moving with the board under it. The engine's patches/camera_view.c says how; here are
// Mario Party 3's two functions that start a camera's view, src/camera.c's func_80012640_main (its viewport and
// projection) and func_800127C4_main (its scissor). A board pans its 3D by moving its camera's viewport
// (src/overlays/ovl_80_shared_board/FA250.c, beside MBBackOffsetSet).

#include "patches.h"
#include "game/camera.h"
#include "../lib/mario-party-engine/patches/camera_view.h"

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

    // @recomp The whole screen, or the border, to the window's edges.
    camera_view_scissor(dispList, camera->screenLeft, camera->screenTop, camera->screenRight, camera->screenBottom);
}
