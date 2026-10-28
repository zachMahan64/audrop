// In-game screen: input mapping, pause menu, HUD and effects.
#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <variant>

#include "app/app.hpp"
#include "sync/judge.hpp"
#include "ui/hud.hpp"

namespace audrop {

using namespace ui;

namespace {

struct Binding {
    game::Button button;
    std::array<int, 3> keys;
    std::array<int, 2> pads;
};

// KEY_NULL / 0 entries are unused slots.
constexpr Binding kBindings[] = {
    {game::Button::Left, {KEY_LEFT, 0, 0}, {GAMEPAD_BUTTON_LEFT_FACE_LEFT, 0}},
    {game::Button::Right, {KEY_RIGHT, 0, 0}, {GAMEPAD_BUTTON_LEFT_FACE_RIGHT, 0}},
    {game::Button::SoftDrop, {KEY_DOWN, 0, 0}, {GAMEPAD_BUTTON_LEFT_FACE_DOWN, 0}},
    {game::Button::HardDrop, {KEY_UP, KEY_SPACE, 0}, {GAMEPAD_BUTTON_LEFT_FACE_UP, 0}},
    {game::Button::RotateCW, {KEY_X, 0, 0}, {GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, 0}},
    {game::Button::RotateCCW, {KEY_Z, KEY_LEFT_CONTROL, KEY_RIGHT_CONTROL}, {GAMEPAD_BUTTON_RIGHT_FACE_DOWN, 0}},
    {game::Button::Rotate180, {KEY_A, 0, 0}, {GAMEPAD_BUTTON_RIGHT_FACE_UP, 0}},
    {game::Button::Hold, {KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, KEY_C}, {GAMEPAD_BUTTON_LEFT_TRIGGER_1, GAMEPAD_BUTTON_RIGHT_TRIGGER_1}},
};

bool binding_down(const Binding& b) {
    for (int k : b.keys)
        if (k && IsKeyDown(k)) return true;
    if (IsGamepadAvailable(0))
        for (int p : b.pads)
            if (p && IsGamepadButtonDown(0, p)) return true;
    return false;
}

bool binding_pressed(const Binding& b) {
    for (int k : b.keys)
        if (k && IsKeyPressed(k)) return true;
    if (IsGamepadAvailable(0))
        for (int p : b.pads)
            if (p && IsGamepadButtonPressed(0, p)) return true;
    return false;
}

bool pause_pressed() {
    return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F1) || IsKeyPressed(KEY_P) ||
           (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT));
}

constexpr double kCountdown = 1.2;
constexpr double kEndDelay = 1.8;

enum PauseRow { kResume, kRestart, kTrack, kOffset, kTempo, kVisual, kBanVisual, kSettings, kQuit };

}  // namespace

void App::start_game(game::Mode mode) {
    mode_ = mode;
    const auto seed = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    game_ = std::make_unique<game::Game>(mode, settings_.handling(), seed);
    game_->set_sync_judge([this] { return sync::judge(grid_ ? *grid_ : sync::BeatGrid{}, sync_now_, settings_.subdivide_beats); });
    effects_.reset();
    held_ = {};
    paused_ = false;
    ending_ = false;
    end_timer_ = 0;
    countdown_ = kCountdown;
    run_tracks_.clear();
    if (!track_title_.empty() && !track_override_) run_tracks_.push_back(track_title_);
    player_->set_paused(false);
    pause_menu_.select(0);
    go(Screen::Playing);
}

void App::release_all_buttons() {
    for (std::size_t i = 0; i < held_.size(); ++i)
        if (held_[i]) game_->release(static_cast<game::Button>(i));
    held_ = {};
}

void App::set_paused(bool paused) {
    paused_ = paused;
    player_->set_paused(paused);
    if (paused) {
        release_all_buttons();
        pause_menu_.select(0);
    }
}

std::vector<MenuItem> App::pause_items() const {
    char offset[32];
    std::snprintf(offset, sizeof offset, "%+.0f ms", adjust_.offset_ms);
    const char* tempo = adjust_.tempo_shift < 0 ? "HALF" : adjust_.tempo_shift > 0 ? "DOUBLE" : "AS DETECTED";
    const bool has_grid = beat_state_ == audio::AnalysisWorker::State::Ready && !track_override_;
    return {
        {"RESUME"},
        {"RESTART"},
        {"TRACK", track_title_.empty() ? "-" : track_title_, true, player_->size() > 0, "Skip to the previous / next track"},
        {"BEAT OFFSET", offset, true, has_grid, "Shift this track's beat grid if it feels early or late"},
        {"TEMPO", tempo, true, has_grid, "Fix tracks detected at half or double speed"},
        {"NEXT VISUAL", viz_->current_preset(), false, viz_->supports_presets()},
        {"BAN THIS VISUAL", "", false, viz_->supports_presets(), "Never show this projectM preset again"},
        {"SETTINGS"},
        {"QUIT TO MENU"},
    };
}

void App::handle_pause_menu() {
    const auto items = pause_items();
    const float rh = row_height() * 0.9f;
    const Rectangle area = menu_area(0.4f, 0.3f, static_cast<int>(items.size()), rh);
    const auto r = pause_menu_.update(items, area, rh);
    if (r.action == MenuAction::None) {
        if (pause_pressed() && !IsKeyPressed(KEY_ESCAPE)) set_paused(false);
        return;
    }
    if (r.action == MenuAction::Back) {
        set_paused(false);
        return;
    }
    const int dir = r.action == MenuAction::Left ? -1 : 1;
    auto save_adjust = [&] {
        analysis_->set_adjust(track_path_, adjust_);
        grid_ = sync::apply_adjust(raw_grid_, adjust_);
    };
    switch (r.index) {
        case kResume: set_paused(false); break;
        case kRestart: start_game(mode_); break;
        case kTrack:
            player_->set_paused(false);
            if (dir < 0) player_->previous();
            else player_->next();
            player_->set_paused(true);
            break;
        case kOffset:
            adjust_.offset_ms += 5.0 * dir;
            save_adjust();
            break;
        case kTempo:
            adjust_.tempo_shift = std::clamp(adjust_.tempo_shift + dir, -1, 1);
            save_adjust();
            break;
        case kVisual: viz_->next_preset(false); break;
        case kBanVisual: viz_->ban_current(); break;
        case kSettings: go(Screen::Settings); break;
        case kQuit:
            game_.reset();
            paused_ = false;
            player_->set_paused(false);
            go(Screen::Title);
            break;
        default: break;
    }
}

void App::tick_game(double dt) {
    if (!game_) {
        go(Screen::Title);
        return;
    }
    if (paused_) {
        handle_pause_menu();
        return;
    }
    if (ending_) {
        end_timer_ -= dt;
        if (end_timer_ <= 0) finish_game();
        return;
    }
    if (pause_pressed() || !IsWindowFocused()) {
        set_paused(true);
        return;
    }
    if (countdown_ > 0) {
        countdown_ -= dt;
        return;
    }

    if (debug_bot_) debug_bot_step();
    for (const auto& b : kBindings) {
        const auto i = static_cast<std::size_t>(b.button);
        const bool down = binding_down(b);
        const bool pressed = binding_pressed(b);
        if (pressed && !held_[i]) {
            game_->press(b.button);
            held_[i] = true;
        }
        if (held_[i] && !down) {
            game_->release(b.button);
            held_[i] = false;
        }
    }
    game_->update(dt);

    const auto events = game_->take_events();
    play_event_sfx(events);
    for (const auto& ev : events) {
        std::visit(
            [&](const auto& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, game::PieceLockedEvent>) {
                    effects_.on_lock(e, layout_);
                } else if constexpr (std::is_same_v<T, game::LineClearEvent>) {
                    effects_.on_clear(e, layout_);
                    debug_on_clear();
                    if (effects_.take_preset_cut()) viz_->next_preset(true);
                } else if constexpr (std::is_same_v<T, game::TSpinNoLinesEvent>) {
                    effects_.on_tspin_no_lines(e.tspin);
                } else if constexpr (std::is_same_v<T, game::LevelUpEvent>) {
                    effects_.on_level_up(e.level);
                } else if constexpr (std::is_same_v<T, game::GameEndEvent>) {
                    ending_ = true;
                    end_timer_ = kEndDelay;
                    release_all_buttons();
                    const char* msg = !e.completed ? "GAME OVER" : mode_ == game::Mode::Ultra ? "TIME UP" : "COMPLETE";
                    effects_.popup(msg, e.completed ? theme_.popup_complete : theme_.popup_game_over);
                }
            },
            ev);
    }
}

void App::play_event_sfx(const std::vector<game::GameEvent>& events) {
    // A clear brings its own impact, so the lock that caused it stays silent.
    const bool clearing = std::ranges::any_of(events, [](const auto& ev) { return std::holds_alternative<game::LineClearEvent>(ev); });
    for (const auto& ev : events) {
        if (const auto* lock = std::get_if<game::PieceLockedEvent>(&ev); lock && !clearing) {
            if (lock->hard_drop) {
                const float depth = std::min(1.0f, static_cast<float>(lock->drop_distance) / tuning::kSfxHardDropFullRows);
                sfx_->play(audio::SfxId::HardDrop, tuning::kSfxHardDropMinGain + (1 - tuning::kSfxHardDropMinGain) * depth);
            } else {
                sfx_->play(audio::SfxId::Lock);
            }
        } else if (const auto* clear = std::get_if<game::LineClearEvent>(&ev)) {
            const bool big = clear->lines >= 4 || clear->tspin == game::TSpin::Full || clear->all_clear;
            sfx_->play(big ? audio::SfxId::ClearBig : audio::SfxId::Clear);
            if (clear->sync.tier >= 0) {
                const float intensity = tuning::kSyncTiers[static_cast<std::size_t>(clear->sync.tier)].fx_intensity;
                sfx_->play(audio::SfxId::OnBeat, tuning::kSfxOnBeatMinGain + (1 - tuning::kSfxOnBeatMinGain) * intensity);
            }
        }
    }
}

void App::draw_game() {
    if (!game_) return;
    Camera2D cam{};
    cam.zoom = 1;
    cam.offset = effects_.shake();
    BeginMode2D(cam);

    // Frosted side panels and lane strip keep the HUD legible over any preset.
    const float pad = layout_.cell * 0.45f;
    const float side_dark = settings_.board_opacity * 0.75f;
    const Color side = with_alpha(theme_.frost, side_dark);
    comp_->draw_frosted({layout_.left.x - pad, layout_.board.y, layout_.left.width + pad * 2, layout_.board.height}, side, theme_.panel_gloss);
    comp_->draw_frosted({layout_.right.x - pad, layout_.board.y, layout_.right.width + pad * 2, layout_.board.height}, side, theme_.panel_gloss);
    if (settings_.show_beat_lane)
        comp_->draw_frosted({layout_.lane.x, layout_.lane.y, layout_.lane.width, layout_.lane.height}, side, theme_.panel_gloss);

    BoardStyle style;
    style.panel_opacity = settings_.board_opacity;
    style.beat_pulse = effects_.beat_pulse();
    style.clear_glow = effects_.clear_glow();
    style.pulse_color = effects_.pulse_color();
    draw_board(theme_, *comp_, *game_, layout_, style);
    effects_.draw_board_layer(theme_, layout_);

    HudInfo info;
    info.game = game_.get();
    info.grid = usable_grid();
    info.beat_status = beat_status();
    info.sync_time = sync_now_;
    info.subdivide = settings_.subdivide_beats;
    info.show_lane = settings_.show_beat_lane;
    info.beat_pulse = effects_.beat_pulse();
    if (const auto& top = scores_.top(mode_); !top.empty())
        info.best = mode_ == game::Mode::Sprint ? format_time(top.front().stats.time) : format_int(top.front().stats.score);
    draw_hud(theme_, layout_, info);
    effects_.draw_overlay(theme_, layout_);
    EndMode2D();

    const float w = static_cast<float>(screen_w_), h = static_cast<float>(screen_h_);
    const float cell = layout_.cell;

    // Now playing, bottom-left under the hold panel, kept clear of the beat lane.
    if (!track_title_.empty()) {
        const float size = std::clamp(h * 0.022f, 13.0f, 24.0f);
        draw_track_info(size, layout_.lane.x - size * 2);
    }

    if (countdown_ > 0 && !paused_) {
        const char* msg = countdown_ > kCountdown * 0.35 ? "READY" : "GO";
        glow_text_centered(theme_, msg, {layout_.board.x + layout_.board.width / 2, layout_.board.y + layout_.board.height * 0.45f},
                           cell * 1.6f, theme_.text, 1.0f, FontWeight::Bold, cell * 0.15f);
    }

    if (paused_) {
        DrawRectangle(0, 0, screen_w_, screen_h_, with_alpha(theme_.panel, 0.72f));
        glow_text_centered(theme_, "PAUSED", {w / 2, h * 0.2f}, h * 0.08f, theme_.text, 0.6f, FontWeight::Bold, h * 0.01f);
        const auto items = pause_items();
        const float rh = row_height() * 0.9f;
        pause_menu_.draw(theme_, items, menu_area(0.4f, 0.3f, static_cast<int>(items.size()), rh), rh);
    }
}

void App::finish_game() {
    if (!game_) return;
    persist::ScoreEntry e;
    e.mode = std::string(game::mode_key(mode_));
    e.completed = game_->completed();
    e.date = persist::now_iso8601();
    e.tracks = run_tracks_;
    e.stats = game_->stats();
    last_rank_ = scores_.submit(e);
    scores_.save();
    last_entry_ = std::move(e);
    ending_ = false;
    go(Screen::Results);
}

}  // namespace audrop
