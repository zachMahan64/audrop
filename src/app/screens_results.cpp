// End-of-run statistics screen.
#include <raylib.h>

#include <algorithm>
#include <cstdio>

#include "app/app.hpp"
#include "config/tuning.hpp"

namespace audrop {

using namespace ui;

namespace {

std::string f2(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", v);
    return buf;
}

const std::vector<MenuItem>& result_items() {
    static const std::vector<MenuItem> items{{"RETRY"}, {"CHANGE MODE"}, {"MAIN MENU"}};
    return items;
}

}  // namespace

void App::tick_results() {
    const float rh = row_height();
    const auto r = menu_.update(result_items(), menu_area(0.24f, 0.8f, 3, rh * 0.8f), rh * 0.8f);
    if (r.action == MenuAction::Back) {
        go(Screen::Title);
        return;
    }
    if (r.action != MenuAction::Activate) return;
    if (r.index == 0) start_game(mode_);
    if (r.index == 1) go(Screen::ModeSelect);
    if (r.index == 2) go(Screen::Title);
}

void App::draw_results() {
    const auto& s = last_entry_.stats;
    const float w = static_cast<float>(screen_w_), h = static_cast<float>(screen_h_);
    const char* title = !last_entry_.completed ? "GAME OVER" : mode_ == game::Mode::Ultra ? "TIME UP" : "COMPLETE";
    const std::string sub = std::string(game::mode_name(mode_));
    draw_header(title, sub.c_str());

    // Headline number.
    const bool sprint = mode_ == game::Mode::Sprint;
    const std::string headline = sprint && last_entry_.completed ? format_time(s.time) : format_int(s.score);
    glow_text_centered(theme_, headline, {w / 2, h * 0.255f}, h * 0.09f, theme_.text, 0.5f, FontWeight::Bold, 4);
    if (last_rank_ >= 0) {
        const std::string rank = last_rank_ == 0 ? "NEW PERSONAL BEST" : "#" + std::to_string(last_rank_ + 1) + " ON THE BOARD";
        text_centered(theme_, rank, {w / 2, h * 0.315f}, h * 0.028f, theme_.accent, FontWeight::Bold, 4);
    }

    // Three columns of stats.
    const float size = std::clamp(h * 0.024f, 13.0f, 28.0f);
    const float col_w = w * 0.24f;
    const float x0 = w / 2 - col_w * 1.5f - size * 2;
    // Starts low enough that the panel's padding (and border) clears the rank line above.
    draw_panel({x0, h * 0.39f, col_w * 3 + size * 4, h * 0.54f}, size * 1.2f);
    auto column = [&](float x, const char* heading, const std::vector<std::pair<std::string, std::string>>& rows) {
        float y = h * 0.41f;
        label(theme_, heading, {x, y}, size * 0.75f, theme_.accent);
        y += size * 1.6f;
        for (const auto& [k, v] : rows) {
            text(theme_, k, {x, y}, size, theme_.text_dim);
            text_right(theme_, v, {x + col_w, y}, size, theme_.text, FontWeight::Bold);
            y += size * 1.3f;
        }
    };

    column(x0, "RUN",
           {{"Score", format_int(s.score)},
            {"Lines", std::to_string(s.lines)},
            {"Level", std::to_string(s.level)},
            {"Time", format_time(s.time)},
            {"Pieces", std::to_string(s.pieces)},
            {"Pieces / sec", f2(s.pps())},
            {"Keys / piece", f2(s.kpp())},
            {"Finesse faults", std::to_string(s.finesse_faults)},
            {"Holds", std::to_string(s.holds)}});

    column(x0 + col_w + size * 2, "CLEARS",
           {{"Singles", std::to_string(s.clears[1])},
            {"Doubles", std::to_string(s.clears[2])},
            {"Triples", std::to_string(s.clears[3])},
            {"Quads", std::to_string(s.clears[4])},
            {"T-spin S/D/T",
             std::to_string(s.tspins[1]) + " / " + std::to_string(s.tspins[2]) + " / " + std::to_string(s.tspins[3])},
            {"T-spin (no lines)", std::to_string(s.tspins[0])},
            {"Mini T-spins", std::to_string(s.tspin_minis[0] + s.tspin_minis[1] + s.tspin_minis[2])},
            {"All clears", std::to_string(s.all_clears)},
            {"Max combo / B2B", std::to_string(s.max_combo) + " / " + std::to_string(s.max_b2b)}});

    std::vector<std::pair<std::string, std::string>> sync_rows;
    for (int t = tuning::kSyncTierCount - 1; t >= 0; --t) {
        std::string name(tuning::kSyncTiers[static_cast<std::size_t>(t)].name);
        if (name.size() > 2)  // "PERFECT" -> "Perfect", but keep "OK"
            for (std::size_t i = 1; i < name.size(); ++i) name[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
        sync_rows.emplace_back(name, std::to_string(s.sync_tiers[static_cast<std::size_t>(t)]));
    }
    sync_rows.emplace_back("Off beat", std::to_string(s.sync_misses));
    sync_rows.emplace_back("Best sync streak", std::to_string(s.max_sync_streak));
    char off[48];
    std::snprintf(off, sizeof off, "%+.1f ms  (+/- %.1f)", s.avg_offset_ms(), s.offset_stddev_ms());
    sync_rows.emplace_back("Avg offset", s.sync_offset_count ? off : "-");
    column(x0 + (col_w + size * 2) * 2, "BEAT SYNC", sync_rows);

    if (s.sync_offset_count >= 8 && std::abs(s.avg_offset_ms()) > 15)
        text_centered(theme_,
                      s.avg_offset_ms() > 0 ? "You tend to clear late - try audio calibration in Settings"
                                            : "You tend to clear early - try audio calibration in Settings",
                      {w / 2, h * 0.765f}, size * 0.85f, theme_.text_faint);

    const float rh = row_height();
    menu_.draw(theme_, result_items(), menu_area(0.24f, 0.8f, 3, rh * 0.8f), rh * 0.8f);
}

}  // namespace audrop
