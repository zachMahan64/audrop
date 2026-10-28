#pragma once

#include "game/game.hpp"

namespace audrop::persist {

struct Settings {
    // Handling
    double das_ms = tuning::kDefaultDasMs;
    double arr_ms = tuning::kDefaultArrMs;
    double soft_drop_factor = tuning::kDefaultSoftDropFactor;
    bool allow_180 = false;

    // Audio
    float music_volume = 0.8f;
    bool sfx_enabled = true;
    float sfx_volume = 0.8f;

    // Sync
    double calibration_ms = 0;  // subtracted from the audio clock when judging
    bool subdivide_beats = false;

    // Visuals
    float viz_brightness = 0.7f;
    float viz_resolution = 0.75f;
    float board_opacity = 0.6f;
    float preset_seconds = 30.0f;
    bool reduce_flashing = false;
    bool screen_shake = true;
    bool show_beat_lane = true;

    // Display
    bool vsync = false;
    bool fullscreen = false;

    game::Handling handling() const { return {das_ms, arr_ms, soft_drop_factor, allow_180}; }
};

Settings load_settings();
void save_settings(const Settings& s);

}  // namespace audrop::persist
