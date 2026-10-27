#ifndef __MARIOPARTY3_H__
#define __MARIOPARTY3_H__

#include <string_view>

namespace marioparty {
    inline constexpr std::u8string_view program_id = u8"MarioParty3Recompiled";
    inline constexpr std::string_view program_name = "Mario Party 3: Recompiled";

    // The game's code sections (register_overlays.cpp) and the patches over it (register_patches.cpp).
    void register_overlays();
    void register_patches();

    // The settings (config.cpp): RecompFrontend's tabs.
    void init_config();
}

#endif
