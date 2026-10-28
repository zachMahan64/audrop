// Title, mode select and high score screens.
#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "app/app.hpp"

namespace audrop {

using namespace ui;

namespace {

std::string best_label(game::Mode mode, const std::vector<persist::ScoreEntry>& table) {
    if (table.empty()) return "-";
    const auto& b = table.front();
    if (mode == game::Mode::Sprint) return format_time(b.stats.time);
    return format_int(b.stats.score);
}

}  // namespace

// --- title ------------------------------------------------------------------

void App::tick_title() {
    const std::vector<MenuItem> items{
        {"PLAY"},
        {"PLAYLIST", std::to_string(player_->size()) + " tracks"},
        {"HIGH SCORES"},
        {"SETTINGS"},
        {"QUIT"},
    };
    const auto r = menu_.update(items, menu_area(0.3f, 0.5f, 5, row_height()), row_height());
    if (r.action != MenuAction::Activate) return;
    switch (r.index) {
        case 0: go(Screen::ModeSelect); break;
        case 1: go(Screen::Playlist); break;
        case 2: go(Screen::HighScores); break;
        case 3: go(Screen::Settings); break;
        case 4: quit_ = true; break;
        default: break;
    }
}

void App::draw_title() {
    const float w = static_cast<float>(screen_w_), h = static_cast<float>(screen_h_);
    const float size = h * 0.16f * (1.0f + 0.03f * effects_.beat_pulse());
    glow_text_centered(theme_, "AUDROP", {w / 2, h * 0.3f}, size, theme_.text, 0.8f + 0.6f * effects_.beat_pulse(),
                       FontWeight::Bold, size * 0.18f);
    text_centered(theme_, "DROP  TO  THE  BEAT", {w / 2, h * 0.3f + size * 0.62f}, size * 0.16f, theme_.text_dim,
                  FontWeight::Regular, size * 0.04f);

    const std::vector<MenuItem> items{
        {"PLAY"},
        {"PLAYLIST", std::to_string(player_->size()) + " tracks"},
        {"HIGH SCORES"},
        {"SETTINGS"},
        {"QUIT"},
    };
    const Rectangle area = menu_area(0.3f, 0.5f, 5, row_height());
    draw_panel(area, row_height() * 0.4f);
    menu_.draw(theme_, items, area, row_height());
    draw_now_playing();
}

// --- mode select --------------------------------------------------------------

std::vector<MenuItem> App::mode_items() const {
    std::vector<MenuItem> items;
    for (auto m : game::kAllModes) {
        MenuItem it{std::string(game::mode_name(m)), "BEST  " + best_label(m, scores_.top(m))};
        it.hint = std::string(game::mode_description(m));
        items.push_back(std::move(it));
    }
    items.push_back({"BACK"});
    return items;
}

void App::tick_mode_select() {
    const auto items = mode_items();
    const auto r = menu_.update(items, menu_area(0.42f, 0.32f, static_cast<int>(items.size()), row_height() * 1.2f),
                                row_height() * 1.2f);
    if (r.action == MenuAction::Back || (r.action == MenuAction::Activate && r.index == 4)) {
        go(Screen::Title);
        return;
    }
    if (r.action == MenuAction::Activate && r.index >= 0 && r.index < 4) start_game(game::kAllModes[static_cast<std::size_t>(r.index)]);
}

void App::draw_mode_select() {
    draw_header("SELECT MODE");
    const auto items = mode_items();
    const Rectangle area = menu_area(0.42f, 0.32f, static_cast<int>(items.size()), row_height() * 1.2f);
    draw_panel({area.x, area.y, area.width, area.height + row_height()}, row_height() * 0.4f);
    menu_.draw(theme_, items, area, row_height() * 1.2f);
    if (player_->size() == 0) {
        const float w = static_cast<float>(screen_w_), h = static_cast<float>(screen_h_);
        text_centered(theme_, "No music loaded - you can still play, but beat sync needs a track.", {w / 2, h * 0.78f},
                      h * 0.024f, theme_.accent2);
    }
    draw_now_playing();
}

// --- high scores ----------------------------------------------------------------

void App::tick_high_scores() {
    if (ui_left()) hs_mode_ = (hs_mode_ + 3) % 4;
    if (ui_right()) hs_mode_ = (hs_mode_ + 1) % 4;
    if (ui_back() || ui_accept()) go(Screen::Title);
}

void App::draw_high_scores() {
    const auto mode = game::kAllModes[static_cast<std::size_t>(hs_mode_)];
    draw_header("HIGH SCORES");
    const float w = static_cast<float>(screen_w_), h = static_cast<float>(screen_h_);
    const float size = std::clamp(h * 0.026f, 14.0f, 30.0f);

    draw_panel({w * 0.15f, h * 0.2f, w * 0.7f, h * 0.68f}, 0);

    // Mode tabs.
    float tx = w * 0.2f;
    const float tab_w = w * 0.6f / 4;
    for (int i = 0; i < 4; ++i) {
        const bool sel = i == hs_mode_;
        const Vector2 c{tx + tab_w * (static_cast<float>(i) + 0.5f), h * 0.24f};
        text_centered(theme_, game::mode_name(game::kAllModes[static_cast<std::size_t>(i)]), c, size * 1.1f,
                      sel ? theme_.text : theme_.text_faint, FontWeight::Bold, 3);
        if (sel) DrawRectangleRec({c.x - tab_w * 0.3f, c.y + size * 0.8f, tab_w * 0.6f, 2}, theme_.accent);
    }

    // Table.
    const float x0 = w * 0.18f, x1 = w * 0.82f;
    float y = h * 0.31f;
    const bool sprint = mode == game::Mode::Sprint;
    const float cols[] = {x0, x0 + (x1 - x0) * 0.08f, x0 + (x1 - x0) * 0.32f, x0 + (x1 - x0) * 0.46f,
                          x0 + (x1 - x0) * 0.58f, x0 + (x1 - x0) * 0.72f};
    const char* heads[] = {"#", sprint ? "TIME" : "SCORE", "LINES", "PPS", "SYNC", "DATE"};
    for (int i = 0; i < 6; ++i) label(theme_, heads[i], {cols[i], y}, size * 0.7f, theme_.text_faint);
    y += size * 1.5f;
    const auto& table = scores_.top(mode);
    if (table.empty()) text(theme_, "No runs yet.", {cols[1], y}, size, theme_.text_dim);
    for (std::size_t i = 0; i < table.size(); ++i) {
        const auto& e = table[i];
        const Color c = i == 0 ? theme_.accent : theme_.text;
        char pps[16];
        std::snprintf(pps, sizeof pps, "%.2f", e.stats.pps());
        const int judged = e.stats.sync_hits() + e.stats.sync_misses;
        const std::string sync = judged ? std::to_string(e.stats.sync_hits() * 100 / judged) + "%" : "-";
        text(theme_, std::to_string(i + 1), {cols[0], y}, size, theme_.text_dim);
        text(theme_, sprint ? format_time(e.stats.time) : format_int(e.stats.score), {cols[1], y}, size, c, FontWeight::Bold);
        text(theme_, std::to_string(e.stats.lines), {cols[2], y}, size, theme_.text);
        text(theme_, pps, {cols[3], y}, size, theme_.text);
        text(theme_, sync, {cols[4], y}, size, theme_.text);
        text(theme_, e.date.substr(0, 10), {cols[5], y}, size, theme_.text_dim);
        y += size * 1.35f;
    }

    // Lifetime.
    const auto& L = scores_.lifetime();
    y = h * 0.8f;
    label(theme_, "LIFETIME", {x0, y}, size * 0.7f, theme_.text_faint);
    y += size * 1.3f;
    std::int64_t hits = 0;
    for (auto v : L.sync_tiers) hits += v;
    char line[256];
    std::snprintf(line, sizeof line, "%d games    %s played    %s lines    %s tetrises    %s T-spins    %s on-beat clears    best sync streak %d",
                  L.games, format_time(L.play_time, false).c_str(), format_int(L.lines).c_str(), format_int(L.tetrises).c_str(),
                  format_int(L.tspins).c_str(), format_int(hits).c_str(), L.best_sync_streak);
    text(theme_, line, {x0, y}, size * 0.85f, theme_.text_dim);
    text_centered(theme_, "<  >  CHANGE MODE      ESC  BACK", {w / 2, h * 0.93f}, size * 0.75f, theme_.text_faint, FontWeight::Regular, 2);
}

}  // namespace audrop
