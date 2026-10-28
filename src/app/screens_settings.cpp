// Settings screen.
#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "app/app.hpp"

namespace audrop {

using namespace ui;

namespace {

enum Row {
    kDas,
    kArr,
    kSoftDrop,
    k180,
    kVolume,
    kVizBrightness,
    kVizResolution,
    kPanel,
    kPresetTime,
    kLane,
    kSubdivide,
    kOffset,
    kCalibrate,
    kReduceFlash,
    kShake,
    kVsync,
    kFullscreen,
    kBack,
};

std::string pct(float v) { return std::to_string(static_cast<int>(std::lround(v * 100))) + "%"; }
std::string ms(double v) { return std::to_string(static_cast<int>(std::lround(v))) + " ms"; }
const char* on_off(bool b) { return b ? "ON" : "OFF"; }

constexpr double kSoftDropSteps[] = {5, 10, 20, 30, 40};
constexpr float kResolutionSteps[] = {0.5f, 0.75f, 1.0f};
constexpr float kPresetSteps[] = {10, 15, 20, 30, 45, 60, 90, 120};

template <class T, std::size_t N>
T step_in(const T (&steps)[N], T current, int dir) {
    std::size_t i = 0;
    for (std::size_t k = 0; k < N; ++k)
        if (std::abs(static_cast<double>(steps[k] - current)) < std::abs(static_cast<double>(steps[i] - current))) i = k;
    const auto next = static_cast<std::ptrdiff_t>(i) + dir;
    return steps[std::clamp<std::ptrdiff_t>(next, 0, static_cast<std::ptrdiff_t>(N) - 1)];
}

}  // namespace

std::vector<MenuItem> App::settings_items() const {
    const auto& s = settings_;
    char offset[32];
    std::snprintf(offset, sizeof offset, "%+.0f ms", s.calibration_ms);
    return {
        {"DAS", ms(s.das_ms), true, true, "Delay before a held direction auto-repeats"},
        {"ARR", ms(s.arr_ms), true, true, "Auto-repeat interval (0 = instant)"},
        {"SOFT DROP SPEED", s.soft_drop_factor >= tuning::kInstantSoftDropFactor ? "INSTANT" : std::to_string(static_cast<int>(s.soft_drop_factor)) + "x", true},
        {"180 ROTATION (A)", on_off(s.allow_180), true},
        {"MUSIC VOLUME", pct(s.music_volume), true},
        {"VISUALIZER BRIGHTNESS", pct(s.viz_brightness), true},
        {"VISUALIZER RESOLUTION", pct(s.viz_resolution), true, true, "Lower is faster on weak GPUs"},
        {"BOARD PANEL DARKNESS", pct(s.board_opacity), true},
        {"VISUAL CHANGE EVERY", std::to_string(static_cast<int>(s.preset_seconds)) + " s", true},
        {"BEAT LANE", on_off(s.show_beat_lane), true},
        {"SYNC ON 8TH NOTES", on_off(s.subdivide_beats), true, true, "Off-beat eighth notes also count for sync bonuses"},
        {"AUDIO OFFSET", offset, true, true, "Positive if you hear the music later than the game expects"},
        {"CALIBRATE AUDIO OFFSET...", "", false, true, "Tap along to a click track to measure your offset"},
        {"REDUCE FLASHING", on_off(s.reduce_flashing), true, true, "Disables full-screen flashes and preset hard cuts"},
        {"SCREEN SHAKE", on_off(s.screen_shake), true},
        {"VSYNC", on_off(s.vsync), true, true, "Off gives the most precise input timing"},
        {"FULLSCREEN", on_off(s.fullscreen), true, true, "F11 toggles at any time"},
        {"BACK"},
    };
}

void App::adjust_setting(int index, int dir) {
    auto& s = settings_;
    switch (index) {
        case kDas: s.das_ms = std::clamp(s.das_ms + 5.0 * dir, 30.0, 400.0); break;
        case kArr: s.arr_ms = std::clamp(s.arr_ms + 5.0 * dir, 0.0, 150.0); break;
        case kSoftDrop: s.soft_drop_factor = step_in(kSoftDropSteps, s.soft_drop_factor, dir); break;
        case k180: s.allow_180 = !s.allow_180; break;
        case kVolume: s.music_volume = std::clamp(s.music_volume + 0.05f * static_cast<float>(dir), 0.0f, 1.0f); break;
        case kVizBrightness: s.viz_brightness = std::clamp(s.viz_brightness + 0.05f * static_cast<float>(dir), 0.1f, 1.3f); break;
        case kVizResolution: s.viz_resolution = step_in(kResolutionSteps, s.viz_resolution, dir); break;
        case kPanel: s.board_opacity = std::clamp(s.board_opacity + 0.05f * static_cast<float>(dir), 0.0f, 0.95f); break;
        case kPresetTime: s.preset_seconds = step_in(kPresetSteps, s.preset_seconds, dir); break;
        case kLane: s.show_beat_lane = !s.show_beat_lane; break;
        case kSubdivide: s.subdivide_beats = !s.subdivide_beats; break;
        case kOffset: s.calibration_ms = std::clamp(s.calibration_ms + 1.0 * dir, -300.0, 300.0); break;
        case kReduceFlash: s.reduce_flashing = !s.reduce_flashing; break;
        case kShake: s.screen_shake = !s.screen_shake; break;
        case kVsync: s.vsync = !s.vsync; break;
        case kFullscreen:
            s.fullscreen = !s.fullscreen;
            ToggleBorderlessWindowed();
            break;
        default: return;
    }
    apply_settings();
    persist::save_settings(s);
}

void App::tick_settings() {
    const auto items = settings_items();
    const float rh = row_height() * 0.72f;
    const int rows = std::max(6, static_cast<int>(static_cast<float>(screen_h_) * 0.62f / rh));
    menu_.set_scroll_rows(rows);
    const auto r = menu_.update(items, menu_area(0.5f, 0.2f, std::min(rows, static_cast<int>(items.size())), rh), rh);
    auto back = [&] { go(game_ && paused_ ? Screen::Playing : Screen::Title); };
    switch (r.action) {
        case MenuAction::Back: back(); break;
        case MenuAction::Left: adjust_setting(r.index, -1); break;
        case MenuAction::Right: adjust_setting(r.index, 1); break;
        case MenuAction::Activate:
            if (r.index == kCalibrate) start_calibration();
            if (r.index == kBack) back();
            break;
        case MenuAction::None: break;
    }
}

void App::draw_settings() {
    draw_header("SETTINGS");
    const auto items = settings_items();
    const float rh = row_height() * 0.72f;
    const int rows = std::max(6, static_cast<int>(static_cast<float>(screen_h_) * 0.62f / rh));
    const Rectangle area = menu_area(0.5f, 0.2f, std::min(rows, static_cast<int>(items.size())), rh);
    draw_panel({area.x, area.y, area.width, area.height + rh}, rh * 0.5f);
    menu_.draw(theme_, items, area, rh);
    const float w = static_cast<float>(screen_w_), h = static_cast<float>(screen_h_);
    text_centered(theme_,
                  "CONTROLS:  LEFT/RIGHT move   DOWN soft drop   UP/SPACE hard drop   X rotate   Z/CTRL rotate left   "
                  "A 180   SHIFT/C hold   ESC pause",
                  {w / 2, h * 0.95f}, std::clamp(h * 0.018f, 11.0f, 20.0f), theme_.text_faint, FontWeight::Regular, 1);
}

}  // namespace audrop
