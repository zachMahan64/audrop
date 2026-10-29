cmake_minimum_required(VERSION 3.25)
project(audrop VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AUDROP_WITH_PROJECTM "Use libprojectM for the background visualizer" ON)
option(AUDROP_WITH_NFD "Use nativefiledialog-extended for file pickers" ON)
option(AUDROP_FETCH_PRESETS "Download the projectM preset + texture packs" ON)
option(AUDROP_BUILD_TESTS "Build unit tests" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# Some dependencies declare ancient cmake_minimum_required versions.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
# raylib (policy 3.5) is the first to include CheckSourceCompiles, whose functions keep the policies
# they were defined under. Without CMP0067, projectM's std::filesystem check ignores
# CMAKE_CXX_STANDARD, fails on Apple clang (default C++14), and falls back to Boost.
set(CMAKE_POLICY_DEFAULT_CMP0067 NEW)
set(FETCHCONTENT_QUIET OFF)
include(FetchContent)

# --- raylib ---------------------------------------------------------------
set(BUILD_SHARED_LIBS OFF)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
# Lets us poll input at ~1 kHz independently of rendering (see app/app.cpp).
set(SUPPORT_CUSTOM_FRAME_CONTROL ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_FLAC ON CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)

# --- nlohmann/json --------------------------------------------------------
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(json)

# --- libprojectM (LGPL-2.1, linked as a shared library) --------------------
if(AUDROP_WITH_PROJECTM)
    set(BUILD_SHARED_LIBS ON)
    set(ENABLE_PLAYLIST ON CACHE BOOL "" FORCE)
    set(ENABLE_SYSTEM_PROJECTM_EVAL OFF CACHE BOOL "" FORCE)
    set(ENABLE_SDL_UI OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(projectm
        GIT_REPOSITORY https://github.com/projectM-visualizer/projectm.git
        GIT_TAG v4.1.8
        GIT_SHALLOW TRUE
        GIT_SUBMODULES vendor/projectm-eval)
    FetchContent_MakeAvailable(projectm)
    set(BUILD_SHARED_LIBS OFF)
endif()

if(AUDROP_FETCH_PRESETS)
    # SOURCE_SUBDIR points at a directory without a CMakeLists.txt, so these are only downloaded.
    FetchContent_Declare(presets
        URL https://github.com/projectM-visualizer/presets-cream-of-the-crop/archive/0180df21f5e0bd39b9060cc5de420ed2f1f9e509.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR none)
    FetchContent_Declare(preset_textures
        URL https://github.com/projectM-visualizer/presets-milkdrop-texture-pack/archive/6368812f27bc747b517218fbf89d21d59afce4d9.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR none)
    FetchContent_MakeAvailable(presets preset_textures)
endif()

# --- nativefiledialog-extended --------------------------------------------
set(AUDROP_HAS_NFD OFF)
if(AUDROP_WITH_NFD)
    set(nfd_portal_ok ON)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(PkgConfig)
        if(PkgConfig_FOUND)
            pkg_check_modules(DBUS QUIET dbus-1)
        endif()
        if(NOT DBUS_FOUND)
            message(WARNING "dbus-1 development files not found (Fedora: dbus-devel, Debian: libdbus-1-dev). "
                            "Building without nativefiledialog; falling back to zenity/kdialog.")
            set(nfd_portal_ok OFF)
        endif()
        set(NFD_PORTAL ON CACHE BOOL "" FORCE)
    endif()
    if(nfd_portal_ok)
        set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
        set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(nfd
            GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
            GIT_TAG v1.2.1
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(nfd)
        set(AUDROP_HAS_NFD ON)
    endif()
endif()

# --- game core (no raylib; unit-testable) ---------------------------------
add_library(audrop_core STATIC
    src/game/piece.cpp
    src/game/board.cpp
    src/game/game.cpp
    src/game/scoring.cpp
    src/game/finesse.cpp
    src/sync/beat_grid.cpp
    src/sync/judge.cpp
    src/audio/beat_tracker.cpp
    src/audio/tags.cpp
    src/audio/sfx_synth.cpp
    src/fx/jelly.cpp
)
target_include_directories(audrop_core PUBLIC src)
target_compile_features(audrop_core PUBLIC cxx_std_23)

if(MSVC)
    target_compile_options(audrop_core PRIVATE /W4)
else()
    target_compile_options(audrop_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- game executable ------------------------------------------------------
add_executable(audrop
    src/main.cpp
    src/app/app.cpp
    src/app/loop_clock.cpp
    src/app/screens_menu.cpp
    src/app/screens_game.cpp
    src/app/screens_playlist.cpp
    src/app/screens_results.cpp
    src/app/screens_settings.cpp
    src/app/screens_themes.cpp
    src/app/screens_calibration.cpp
    src/app/debug_hooks.cpp
    src/audio/music_player.cpp
    src/audio/sfx.cpp
    src/audio/analysis_worker.cpp
    src/audio/spectrum.cpp
    src/viz/visualizer.cpp
    src/viz/shader_visualizer.cpp
    src/viz/projectm_visualizer.cpp
    src/viz/compositor.cpp
    src/fx/particles.cpp
    src/fx/effects.cpp
    src/fx/bubbles.cpp
    src/ui/theme.cpp
    src/ui/themes.cpp
    src/ui/widgets.cpp
    src/ui/board_view.cpp
    src/ui/hud.cpp
    src/persist/paths.cpp
    src/persist/settings.cpp
    src/persist/scores.cpp
    src/persist/beat_cache.cpp
    src/platform/gl_state.cpp
    src/platform/file_dialog.cpp
)
target_link_libraries(audrop PRIVATE audrop_core raylib nlohmann_json::nlohmann_json)
target_compile_features(audrop PRIVATE cxx_std_23)
if(NOT MSVC)
    target_compile_options(audrop PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

target_compile_definitions(audrop PRIVATE
    AUDROP_VERSION="${PROJECT_VERSION}"
    AUDROP_SOURCE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(AUDROP_WITH_PROJECTM)
    target_link_libraries(audrop PRIVATE libprojectM::projectM libprojectM::playlist)
    target_compile_definitions(audrop PRIVATE AUDROP_HAS_PROJECTM=1)
endif()
if(AUDROP_FETCH_PRESETS)
    target_compile_definitions(audrop PRIVATE
        AUDROP_PRESET_DIR="${presets_SOURCE_DIR}"
        AUDROP_PRESET_TEXTURE_DIR="${preset_textures_SOURCE_DIR}/textures")
endif()
if(AUDROP_HAS_NFD)
    target_link_libraries(audrop PRIVATE nfd)
    target_compile_definitions(audrop PRIVATE AUDROP_HAS_NFD=1)
endif()

find_package(Threads REQUIRED)
target_link_libraries(audrop PRIVATE Threads::Threads)

# --- tests ----------------------------------------------------------------
if(AUDROP_BUILD_TESTS)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(doctest)
    enable_testing()
    add_executable(audrop_tests
        tests/main.cpp
        tests/test_srs.cpp
        tests/test_game.cpp
        tests/test_scoring.cpp
        tests/test_sync.cpp
        tests/test_beat_tracker.cpp
        tests/test_tags.cpp
        tests/test_sfx.cpp
        tests/test_jelly.cpp
    )
    target_link_libraries(audrop_tests PRIVATE audrop_core doctest::doctest)
    add_test(NAME audrop_tests COMMAND audrop_tests)
endif()
