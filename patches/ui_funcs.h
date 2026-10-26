// RecompFrontend includes this from the game's patches/ (recompui/src/api/ui_api_events.cpp), for the native that
// runs mods' UI callbacks.
#ifndef __UI_FUNCS_INTERNAL_H__
#define __UI_FUNCS_INTERNAL_H__

#include "patch_helpers.h"
#include "recompui_event_structs.h"

DECLARE_FUNC(void, recomp_run_ui_callbacks);

#endif
