// Developer hooks, all opt-in via environment variables:
//   AUDROP_DEBUG_MODE=marathon|sprint|ultra|endless  start that mode right away
//   AUDROP_DEBUG_BOT=1                               a simple bot plays, dropping on the beat
//   AUDROP_DEBUG_BOT=drop                            ...or just hard-drops everything (quick game over)
//   AUDROP_DEBUG_SCREEN=playlist|settings|scores|calibration  open that screen at startup
//   AUDROP_DEBUG_SHOT=name.png AUDROP_DEBUG_SHOT_AT=8 save a screenshot after N seconds and quit
//   AUDROP_DEBUG_SHOT_CLEAR=3                        ...or 0.12 s after the Nth line clear instead
#include <raylib.h>

#include <cstdlib>
#include <string_view>

#include "app/app.hpp"
#include "sync/judge.hpp"

namespace audrop {

namespace {

const char* env(const char* name) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : nullptr;
}

// Classic placement heuristic: lines good; height, holes and bumpiness bad.
double evaluate(const game::Board& board, int lines) {
    std::array<int, game::Board::kWidth> heights{};
    int holes = 0;
    for (int x = 0; x < game::Board::kWidth; ++x) {
        bool seen = false;
        for (int y = 0; y < game::Board::kHeight; ++y) {
            if (board.at(x, y)) {
                if (!seen) heights[static_cast<std::size_t>(x)] = game::Board::kHeight - y;
                seen = true;
            } else if (seen) {
                ++holes;
            }
        }
    }
    int agg = 0, bump = 0;
    for (std::size_t x = 0; x < heights.size(); ++x) {
        agg += heights[x];
        if (x + 1 < heights.size()) bump += std::abs(heights[x] - heights[x + 1]);
    }
    return 0.76 * lines - 0.51 * agg - 0.36 * holes * 2 - 0.18 * bump;
}

}  // namespace

void App::init_debug_hooks() {
    if (const char* shot = env("AUDROP_DEBUG_SHOT")) {
        debug_shot_ = shot;
        debug_shot_at_ = env("AUDROP_DEBUG_SHOT_AT") ? std::atof(env("AUDROP_DEBUG_SHOT_AT")) : 6.0;
        if (const char* n = env("AUDROP_DEBUG_SHOT_CLEAR")) {
            debug_shot_clear_ = std::atoi(n);
            debug_shot_at_ = 1e9;
        }
    }
    if (const char* m = env("AUDROP_DEBUG_MODE")) {
        for (auto mode : game::kAllModes)
            if (game::mode_key(mode) == std::string_view(m)) debug_mode_ = mode;
    }
    debug_bot_ = env("AUDROP_DEBUG_BOT") != nullptr;
    debug_bot_drop_only_ = debug_bot_ && std::string_view(env("AUDROP_DEBUG_BOT")) == "drop";
    if (const char* sc = env("AUDROP_DEBUG_SCREEN")) {
        const std::string_view v(sc);
        if (v == "playlist") go(Screen::Playlist);
        if (v == "settings") go(Screen::Settings);
        if (v == "scores") go(Screen::HighScores);
        if (v == "modes") go(Screen::ModeSelect);
        if (v == "calibration") start_calibration();
    }
}

void App::debug_after_present() {
    if (debug_shot_.empty() || wall_ < debug_shot_at_) return;
    TakeScreenshot(debug_shot_.c_str());
    debug_shot_.clear();
    quit_ = true;
}

void App::debug_on_clear() {
    if (debug_shot_clear_ > 0 && ++debug_clears_ == debug_shot_clear_) debug_shot_at_ = wall_ + 0.12;
}

void App::debug_bot_step() {
    if (!game_ || !game_->active()) return;
    if (debug_bot_drop_only_) {
        game_->press(game::Button::HardDrop);
        game_->release(game::Button::HardDrop);
        return;
    }
    const auto& a = *game_->active();
    if (debug_bot_piece_ != game_->stats().pieces) {
        debug_bot_piece_ = game_->stats().pieces;
        double best = -1e9;
        for (int rot = 0; rot < 4; ++rot) {
            for (int x = -3; x < game::Board::kWidth; ++x) {
                game::Board b = game_->board();
                if (!b.fits(a.type, rot, x, a.y)) continue;
                int y = a.y;
                while (b.fits(a.type, rot, x, y + 1)) ++y;
                b.place(a.type, rot, x, y);
                const int lines = static_cast<int>(b.clear_full_rows().size());
                const double score = evaluate(b, lines);
                if (score > best) {
                    best = score;
                    debug_bot_rot_ = rot;
                    debug_bot_x_ = x;
                }
            }
        }
    }
    if (wall_ < debug_bot_next_) return;
    debug_bot_next_ = wall_ + 0.03;
    auto tap = [&](game::Button b) {
        game_->press(b);
        game_->release(b);
    };
    if (a.rot != debug_bot_rot_) {
        tap(game::Button::RotateCW);
    } else if (a.x != debug_bot_x_) {
        tap(a.x < debug_bot_x_ ? game::Button::Right : game::Button::Left);
    } else {
        // Drop on the beat when we know where it is.
        if (const auto* g = usable_grid()) {
            const auto p = sync::beat_phase(*g, sync_now_);
            if (p.valid && p.phase > 0.01 && p.phase < 0.985) return;
        }
        tap(game::Button::HardDrop);
    }
}

}  // namespace audrop
