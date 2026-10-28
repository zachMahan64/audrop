#pragma once

#include <string>

#include "game/game.hpp"
#include "sync/beat_grid.hpp"
#include "ui/board_view.hpp"
#include "ui/theme.hpp"

namespace audrop::ui {

struct HudInfo {
    const game::Game* game = nullptr;
    const sync::BeatGrid* grid = nullptr;  // nullptr while analysing / unusable
    std::string beat_status;               // shown in the lane when there is no grid
    double sync_time = 0;                  // calibrated track time
    bool subdivide = false;
    bool show_lane = true;
    float beat_pulse = 0;
    std::string best;                      // personal best for the mode, preformatted
};

void draw_hud(const Theme& th, const BoardLayout& L, const HudInfo& info);
void draw_beat_lane(const Theme& th, Rectangle lane, const sync::BeatGrid* grid, double t, bool subdivide,
                    const std::string& status);

}  // namespace audrop::ui
