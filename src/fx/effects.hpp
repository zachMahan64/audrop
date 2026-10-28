#pragma once

#include <random>
#include <string>
#include <vector>

#include "fx/particles.hpp"
#include "game/game.hpp"
#include "ui/board_view.hpp"
#include "ui/theme.hpp"
#include "viz/compositor.hpp"

namespace audrop::fx {

struct FxOptions {
    bool reduce_flashing = false;
    bool screen_shake = true;
};

// All game-feel effects: line-clear bursts that scale with beat accuracy, lock flashes,
// hard-drop trails, judgment/clear popups, screen shake and post-processing parameters.
class Effects {
public:
    void reset();
    void set_options(const FxOptions& o) { opts_ = o; }

    void on_lock(const game::PieceLockedEvent& e, const ui::BoardLayout& L);
    void on_clear(const game::LineClearEvent& e, const ui::BoardLayout& L);
    void on_tspin_no_lines(game::TSpin t);
    void on_level_up(int level);
    void on_beat(float strength);
    void popup(std::string text, Color color);

    void update(float dt);
    void draw_board_layer(const ui::Theme& th, const ui::BoardLayout& L) const;
    void draw_overlay(const ui::Theme& th, const ui::BoardLayout& L) const;

    viz::PostParams post() const;
    Vector2 shake() const;
    float beat_pulse() const { return beat_pulse_; }
    float clear_glow() const { return clear_glow_; }
    Color pulse_color() const { return pulse_color_; }
    // True once after a clear grand enough to hard-cut the visualizer preset.
    bool take_preset_cut() { return std::exchange(preset_cut_, false); }

private:
    struct Timed {
        float age = 0;
        float duration = 1;
        float t() const { return age / duration; }
    };
    struct RowFlash : Timed {
        float y, height, intensity;
        Color color;
    };
    struct CellFlash : Timed {
        Rectangle r;
        Color color;
    };
    struct Trail : Timed {
        Rectangle r;
        Color color;
    };
    struct Wave : Timed {
        Vector2 center;
        float max_radius, strength;
    };
    struct Popup : Timed {
        std::string text;
        std::string sub;
        Color color;
        float size;
        int slot;  // 0 = judgment (board), 1 = clear text (left column), 2 = centre banner
    };

    float rand01() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_); }

    FxOptions opts_;
    std::mt19937 rng_{1234};
    Particles particles_;
    std::vector<RowFlash> rows_;
    std::vector<CellFlash> cells_;
    std::vector<Trail> trails_;
    std::vector<Wave> waves_;
    std::vector<Popup> popups_;
    float trauma_ = 0;
    float chroma_ = 0;
    float flash_ = 0;
    Color flash_color_ = WHITE;
    float beat_pulse_ = 0;
    float clear_glow_ = 0;
    Color pulse_color_ = WHITE;
    float time_ = 0;
    bool preset_cut_ = false;
};

std::string clear_name(int lines, game::TSpin tspin);

}  // namespace audrop::fx
