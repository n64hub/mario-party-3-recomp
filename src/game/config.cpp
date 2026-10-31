// The settings: RecompFrontend's General, Controls, Graphics, Sound and Mods tabs, with its default bindings. Its own
// Graphics options carry what a recomp needs: Resolution and MS Anti-Aliasing (high resolution), Framerate
// (interpolation to the display's rate) and Aspect Ratio and HUD Placement (widescreen). The General tab ends with the
// board's options, Board Movement and Map Scrolling, which lib/mario-party-engine's src/board_options.cpp words and
// us.toml hooks.

#include <filesystem>

#include "recompui/config.h"
#include "util/file.h"
#include "../../lib/mario-party-engine/src/board_options.h"
#include "marioparty3.h"

void marioparty::init_config() {
    std::filesystem::path app_dir = recompui::file::get_app_folder_path();
    if (!app_dir.empty()) {
        std::filesystem::create_directories(app_dir);
    }

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = true;
    general_options.has_mouse_sensitivity = true;
    recomp::config::Config& general = recompui::config::create_general_tab(general_options);
    mp_board_options::add(general);
    recompui::config::create_controls_tab();
    recompui::config::create_graphics_tab();
    recompui::config::create_sound_tab();
    recompui::config::create_mods_tab();
    recompui::config::finalize();
}
