# Mario Party 3: Recompiled: the recompiled game (RecompiledFuncs/, from tools/recompile.sh), the patches over it
# (patches/, PatchesLib), and RecompFrontend's launcher and menus on RT64. The desktop program, and with Emscripten
# (emcmake cmake; tools/build_web.sh) the page, MarioParty3Recompiled.html, through lib/N64Recomp-browser/frontend
# (docs/web.md).
cmake_minimum_required(VERSION 3.20)

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version")
endif()

project(MarioParty3Recompiled LANGUAGES C CXX)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything /W4")
endif()

# Opt out of constexpr mutex constructor on windows to prevent vcruntime mismatch crashes
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
endif()

if (APPLE)
    enable_language(OBJC OBJCXX)
endif()

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

if (EMSCRIPTEN)
    # Threads are pthreads (SharedArrayBuffer), and the runtime ends a game thread by throwing. -msimd128 is the web's
    # -march=nehalem: wasm SIMD, which every browser with WebGPU has, for hlslpp's wasm backend and the
    # auto-vectorizer. -mtail-call is wasm's tail calls: with it the recompiled code's jumps to other functions are
    # return_calls (N64Recomp-web's RECOMP_TAIL_CALL), not calls that grow the stack (N64Recomp-browser's README, "What
    # the build needs").
    add_compile_options(-pthread -fwasm-exceptions -msimd128 -mtail-call)
    # With -msimd128 xxHash would take its NEON path through Emscripten's arm_neon.h, which it includes inside its
    # extern "C", where the C++ headers that reaches don't compile. XXH_HAS_INCLUDE is used for nothing else.
    add_compile_options("-DXXH_HAS_INCLUDE(x)=0")
    add_link_options(-pthread -fwasm-exceptions -mtail-call)
    # ccache when it is on PATH, hitting across checkouts (its header says how).
    include(${CMAKE_SOURCE_DIR}/lib/N64Recomp-browser/frontend/build_speed.cmake)
    # The shaders the page specializes at build time and builds before the game needs them, in sections: the boot's
    # before the first frame, the rest as the game first draws them (src/rt64_shaders.txt says how it was recorded).
    if (EXISTS ${CMAKE_SOURCE_DIR}/src/rt64_shaders.txt)
        set(RT64_PREBUILT_SHADER_LIST ${CMAKE_SOURCE_DIR}/src/rt64_shaders.txt)
    endif()
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(RT64_STATIC TRUE)
set(RT64_SDL_WINDOW_VULKAN TRUE)
add_compile_definitions(HLSL_CPU)

if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # SDL 2.30's SDL_endian.h defines _m_prefetch, which clang declares as a builtin in prfchwintrin.h
    add_compile_definitions(__PRFCHWINTRIN_H)
endif()

# lib/rt64, lib/N64ModernRuntime and lib/RecompFrontend are upstream's, at the commits N64Recomp-browser's web forks
# are built on, with its patches/ applied, if tools/build_n64recomp.sh hasn't already. The desktop build takes them too:
# the code the patched N64Recomp writes uses the patched recomp.h (RECOMP_TAIL_CALL), and the web support in them is
# behind __EMSCRIPTEN__.
include(${CMAKE_SOURCE_DIR}/lib/N64Recomp-browser/patches/apply.cmake)
n64recomp_browser_apply_patches(${CMAKE_SOURCE_DIR}/lib)

# The engine draws a frame in several RSP tasks (one a camera, the board's sprites and text in a later one), which RT64
# interpolates as one game frame with RT64_GAME_FRAME_WORKLOADS, on with Emscripten and here on the desktop too: each
# task interpolated as a frame of its own drew frames between the game's from the first tasks alone, without the HUD or
# a text box, which flickered.
set(RT64_GAME_FRAME_WORKLOADS ON)
# The VI thread queues a VI's screen update after it swaps in the framebuffer the game gave it, as with Emscripten:
# queued before, a frame was shown a VI late, after the next frame's first tasks, and RT64_GAME_FRAME_WORKLOADS ended
# most game frames at a wait instead, showing one still drawing in parts (Mario Party 1's next scene inside a star wipe).
set(ULTRAMODERN_SCREEN_UPDATE_AFTER_VI ON)
# A transform that collapses what it draws (a zero determinant) is never paired with one that doesn't between game
# frames, as with Emscripten: an object the game hides by scaling it to 0 or with a matrix of zeros was interpolated
# from its last matrix to none in the frames between (Mario Party 2's star across the whole view, a flat light grey).
set(RT64_SKIP_COLLAPSED_MATCHES ON)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/rt64 ${CMAKE_BINARY_DIR}/rt64)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime)

if (EMSCRIPTEN)
    add_subdirectory(${CMAKE_SOURCE_DIR}/lib/N64Recomp-browser/frontend ${CMAKE_BINARY_DIR}/web_frontend)
    # fmt 11.1 (N64Recomp's) leans on <cstdlib> arriving through other headers, which Emscripten's libc++ doesn't do.
    target_compile_options(fmt PRIVATE -include cstdlib)
endif()

target_include_directories(rt64 PRIVATE ${CMAKE_BINARY_DIR}/rt64/src)

# RecompiledFuncs: the game's code, from N64Recomp us.toml, and its audio microcode, from RSPRecomp n_aspMain.us.toml
# (tools/recompile.sh)
add_library(RecompiledFuncs STATIC)

target_compile_options(RecompiledFuncs PRIVATE
    -fno-strict-aliasing
    -Wno-unused-variable
    -Wno-implicit-function-declaration
)
# The microcode's vector unit (librecomp's rsp_vu_impl.hpp) uses SSE4.1 on x86-64, as the program does.
if (CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    target_compile_options(RecompiledFuncs PRIVATE -march=nehalem)
endif()
# A call to an undeclared function links to a trap in WebAssembly, so it is an error there (N64Recomp-web's funcs.h
# declares every function the recompiled code calls).
if (EMSCRIPTEN)
    target_compile_options(RecompiledFuncs PRIVATE -Werror=implicit-function-declaration)
endif()

target_include_directories(RecompiledFuncs PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
)

file(GLOB FUNC_C_SOURCES ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.c)
file(GLOB FUNC_CXX_SOURCES ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.cpp)
target_sources(RecompiledFuncs PRIVATE ${FUNC_C_SOURCES} ${FUNC_CXX_SOURCES})

# PatchesLib: the patches (patches/), compiled for MIPS by patches/Makefile and recompiled by N64Recomp patches.toml
# (the runtime's own N64Recomp, target N64RecompCLI). Needs a clang with a MIPS backend and ld.lld
# (PATCHES_C_COMPILER, PATCHES_LD). MP_PATCHES=OFF is for CI only,
# which stops before linking: the game needs the patches to load its overlays and run its processes.
option(MP_PATCHES "Build the patches library (the game needs it)" ON)
set(PATCHES_C_COMPILER clang CACHE STRING "clang with a MIPS backend, for patches/")
set(PATCHES_LD ld.lld CACHE STRING "ld.lld, for patches/")

if (MP_PATCHES)
    add_library(PatchesLib STATIC)

    target_compile_options(PatchesLib PRIVATE
        -fno-strict-aliasing
        -Wno-unused-variable
        -Wno-implicit-function-declaration
    )

    target_include_directories(PatchesLib PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include
        ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include
        ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
    )

    target_sources(PatchesLib PRIVATE
        ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches.c
        ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c
    )

    # make is incremental, so this runs every build and relinks only when a patch source changed.
    add_custom_target(PatchesBin
        COMMAND ${CMAKE_COMMAND} -E env CC=${PATCHES_C_COMPILER} LD=${PATCHES_LD} make
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/patches
        BYPRODUCTS ${CMAKE_SOURCE_DIR}/patches/patches.elf
    )

    # The recompiler runs where the build does: for the page that is tools/recompile.sh's native N64Recomp, not the
    # runtime's target, which Emscripten would build for the browser.
    if (EMSCRIPTEN)
        set(MP_N64RECOMP ${CMAKE_SOURCE_DIR}/build/n64recomp/N64Recomp)
        # The patches call the game's functions: undeclared, a call links to a trap in WebAssembly.
        target_compile_options(PatchesLib PRIVATE -Werror=implicit-function-declaration
            "SHELL:-include ${CMAKE_SOURCE_DIR}/RecompiledFuncs/funcs.h")
    else()
        set(MP_N64RECOMP N64RecompCLI)
    endif()

    add_custom_command(OUTPUT
            ${CMAKE_SOURCE_DIR}/patches/patches.bin
            ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches.c
            ${CMAKE_SOURCE_DIR}/RecompiledPatches/recomp_overlays.inl
            ${CMAKE_SOURCE_DIR}/RecompiledPatches/funcs.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/RecompiledPatches
        COMMAND ${MP_N64RECOMP} patches.toml
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS ${CMAKE_SOURCE_DIR}/patches/patches.elf ${CMAKE_SOURCE_DIR}/patches.toml
    )

    add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.h
        COMMAND file_to_c ${CMAKE_SOURCE_DIR}/patches/patches.bin marioparty_patches_bin
            ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.h
        DEPENDS ${CMAKE_SOURCE_DIR}/patches/patches.bin
    )
endif()

add_executable(MarioParty3Recompiled)

target_sources(MarioParty3Recompiled PRIVATE
    ${CMAKE_SOURCE_DIR}/src/main/main.cpp
    ${CMAKE_SOURCE_DIR}/src/main/register_overlays.cpp
    ${CMAKE_SOURCE_DIR}/src/game/config.cpp
    ${CMAKE_SOURCE_DIR}/src/game/libultra.cpp
    ${CMAKE_SOURCE_DIR}/lib/mario-party-engine/src/process_threads.cpp
    ${CMAKE_SOURCE_DIR}/lib/mario-party-engine/src/hud.cpp
    ${CMAKE_SOURCE_DIR}/lib/mario-party-engine/src/wipe.cpp
    ${CMAKE_SOURCE_DIR}/lib/mario-party-engine/src/aspect_ratio.cpp
    ${CMAKE_SOURCE_DIR}/src/game/recomp_api.cpp
    ${CMAKE_SOURCE_DIR}/src/game/survey.cpp
)
if (MP_PATCHES)
    target_sources(MarioParty3Recompiled PRIVATE ${CMAKE_SOURCE_DIR}/src/main/register_patches.cpp)
    # register_patches.cpp includes the patches' section table, which the command above writes in the middle of a
    # build: without this edge an incremental build after a patch moved kept the old table.
    set_source_files_properties(${CMAKE_SOURCE_DIR}/src/main/register_patches.cpp PROPERTIES
        OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/RecompiledPatches/recomp_overlays.inl)
endif()

target_include_directories(MarioParty3Recompiled PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
    ${CMAKE_SOURCE_DIR}/lib/rt64/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/thirdparty
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/thirdparty/sse2neon
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/hlslpp/include
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/inc
    ${CMAKE_SOURCE_DIR}/lib/rt64/src
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/rhi
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/render
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompinput/include
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompui/include
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompui/src
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/nativefiledialog-extended/src/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

# The launcher's thumbnail: text only (icons/512.png), no art from the game.
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.h
    COMMAND file_to_c ${CMAKE_SOURCE_DIR}/icons/512.png icon_bytes ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.h
    DEPENDS ${CMAKE_SOURCE_DIR}/icons/512.png
)

target_sources(MarioParty3Recompiled PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c)

if (CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    target_compile_options(MarioParty3Recompiled PRIVATE
        -march=nehalem
        -fno-strict-aliasing
        -fms-extensions
    )
else()
    target_compile_options(MarioParty3Recompiled PRIVATE
        -fno-strict-aliasing
        -fms-extensions
    )
endif()

if (MSVC)
    # Disable identical code folding, since this breaks mod function patching as multiple functions can get merged into one.
    target_link_options(MarioParty3Recompiled PRIVATE /OPT:NOICF)
endif()

if (WIN32)
    include(FetchContent)

    if (DEFINED ENV{SDL2_VERSION})
        set(SDL2_VERSION $ENV{SDL2_VERSION})
    else()
        set(SDL2_VERSION "2.30.3")
    endif()

    FetchContent_Declare(
        sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-VC.zip
    )
    FetchContent_MakeAvailable(sdl2)
    target_include_directories(MarioParty3Recompiled PRIVATE
        ${sdl2_SOURCE_DIR}/include
    )
    target_link_directories(MarioParty3Recompiled PRIVATE
        ${sdl2_SOURCE_DIR}/lib/x64
    )

    add_custom_command(TARGET MarioParty3Recompiled POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxil.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxcompiler.dll"
            $<TARGET_FILE_DIR:MarioParty3Recompiled>)

    # A dev build (Debug, RelWithDebInfo) opens a console beside the window for its log; Release, which
    # tools/release.sh builds for players, is a windowed program only.
    set_target_properties(
        MarioParty3Recompiled
        PROPERTIES
            LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
            LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
            LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE"
            LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )

    target_sources(MarioParty3Recompiled PRIVATE ${CMAKE_SOURCE_DIR}/icons/app.rc)
    target_link_libraries(MarioParty3Recompiled PRIVATE SDL2 Winmm.lib)
    add_compile_definitions(NOMINMAX)
endif()

if (APPLE)
    find_package(SDL2 REQUIRED)
    target_include_directories(MarioParty3Recompiled PRIVATE ${SDL2_INCLUDE_DIRS})

    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)

    target_link_libraries(MarioParty3Recompiled PRIVATE ${CMAKE_DL_LIBS} Threads::Threads SDL2::SDL2)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(SDL2 REQUIRED)
    add_compile_definitions("PLUME_SDL_VULKAN_ENABLED")
    add_compile_definitions("RT64_SDL_WINDOW_VULKAN")
    target_include_directories(MarioParty3Recompiled PRIVATE ${SDL2_INCLUDE_DIRS})

    find_package(Freetype REQUIRED)
    include_directories(${FREETYPE_LIBRARIES})
    target_link_libraries(MarioParty3Recompiled PRIVATE ${FREETYPE_LIBRARIES} SDL2::SDL2)

    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)

    target_link_libraries(MarioParty3Recompiled PRIVATE "-latomic -static-libstdc++" ${CMAKE_DL_LIBS} Threads::Threads)
endif()

# dxc, for recompui's shaders
set(DXC_COMMON_OPTS "-I${PROJECT_SOURCE_DIR}/src")
set(DXC_DXIL_OPTS "-Wno-ignored-attributes")
set(DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
set(DXC_PS_OPTS "${DXC_COMMON_OPTS}" "-E" "PSMain" "-T ps_6_0" "-D DYNAMIC_RENDER_PARAMS")
set(DXC_VS_OPTS "${DXC_COMMON_OPTS}" "-E" "VSMain" "-T vs_6_0" "-D DYNAMIC_RENDER_PARAMS" "-fvk-invert-y")
set(DXC_CS_OPTS "${DXC_COMMON_OPTS}" "-E" "CSMain" "-T cs_6_0")
set(DXC_GS_OPTS "${DXC_COMMON_OPTS}" "-E" "GSMain" "-T gs_6_0")
set(DXC_RT_OPTS "${DXC_COMMON_OPTS}" "-D" "RT_SHADER" "-T" "lib_6_3" "-fspv-target-env=vulkan1.1spirv1.4" "-fspv-extension=SPV_KHR_ray_tracing" "-fspv-extension=SPV_EXT_descriptor_indexing")

if (CMAKE_HOST_WIN32)
    set(DXC "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxc.exe")
elseif (CMAKE_HOST_APPLE)
    # Apple's binary is universal, so it'll work on both x86_64 and arm64
    set(DXC "DYLD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/arm64/dxc-macos")
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/x64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxc-linux")
else()
    set(DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/arm64/dxc-linux")
endif()

set(RECOMP_FRONTEND_N64MODERNRUNTIME_PATH ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime)
set(RECOMP_FRONTEND_RT64_PATH ${CMAKE_SOURCE_DIR}/lib/rt64)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/RecompFrontend)

# Prints what the process scheduler's threads cost every 5 seconds (lib/mario-party-engine/src/process_threads.cpp).
option(MP_PROCESS_STATS "Print the process scheduler's counts and costs" OFF)
if (MP_PROCESS_STATS)
    # Only its two files, so turning it on rebuilds them and not the game.
    set_source_files_properties(${CMAKE_SOURCE_DIR}/lib/mario-party-engine/src/process_threads.cpp
        ${CMAKE_SOURCE_DIR}/src/game/recomp_api.cpp
        PROPERTIES COMPILE_DEFINITIONS MP_PROCESS_STATS)
endif()

if (MP_PATCHES)
    target_link_libraries(MarioParty3Recompiled PRIVATE PatchesLib)
endif()
target_link_libraries(MarioParty3Recompiled PRIVATE
    RecompiledFuncs
    recompui
    recompinput
    librecomp
    ultramodern
    rt64
)

if (EMSCRIPTEN)
    # The page (lib/N64Recomp-browser/frontend), with the menus' fonts and icons and the controller mappings in its .data
    # file. No ROM: the player picks theirs in the launcher, and it stays in the browser.
    n64recomp_browser_frontend_page(MarioParty3Recompiled)
    target_link_options(MarioParty3Recompiled PRIVATE
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/assets@/assets"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/recompcontrollerdb.txt@/recompcontrollerdb.txt")
else()
    target_link_libraries(MarioParty3Recompiled PRIVATE nfd)
endif()

set_property(TARGET MarioParty3Recompiled PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
