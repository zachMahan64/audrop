#include "ui/hud.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "config/tuning.hpp"
#include "sync/judge.hpp"

namespace audrop::ui {
namespace {

std::string fmt(const char* f, double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

// Vertical step from a label to the value under it. Pixel fonts round small labels up, so
// they get measured spacing instead.
float below_label(const Theme& th, float size) {
    return th.pixel_art ? measure(th, "A", size).y + 2 * th.unit : size * 1.2f;
}

void stat_row(const Theme& th, float x, float right, float& y, float size, std::string_view name, const std::string& value) {
    const float label_size = size * 0.7f;
    const float need = measure(th, name, label_size, FontWeight::Regular, label_size * 0.22f).x +
                       measure(th, value, size).x + size * 0.5f;
    if (x + need > right) {
        // Too wide for one line (wide pixel fonts): value goes under the label.
        label(th, name, {x, y}, label_size, th.text_dim);
        y += below_label(th, label_size);
        text_right(th, value, {right, y}, size, th.text, FontWeight::Regular);
        y += size * 1.3f;
        return;
    }
    label(th, name, {x, y + size * 0.12f}, label_size, th.text_dim);
    text_right(th, value, {right, y - size * 0.12f}, size, th.text, FontWeight::Regular);
    y += size * 1.15f;
}

}  // namespace

void draw_beat_lane(const Theme& th, Rectangle lane, const sync::BeatGrid* grid, double t, bool subdivide,
                    const std::string& status) {
    const float cx = lane.x + lane.width / 2, cy = lane.y + lane.height / 2;
    DrawLineEx({lane.x, cy}, {lane.x + lane.width, cy}, 1, with_alpha(th.line, 0.12f));

    if (!grid) {
        text_centered(th, status, {cx, cy}, lane.height * 0.55f, th.text_faint, FontWeight::Regular, 2);
        return;
    }
    const sync::BeatPhase p = sync::beat_phase(*grid, t);
    if (!p.valid || p.beat_length <= 0) return;

    const float px_per_beat = lane.width / 4.0f;
    // Tier windows as nested bands around the centre, widest first.
    for (int tier = 0; tier < tuning::kSyncTierCount; ++tier) {
        const double ms = sync::tier_window_ms(tier, p.beat_length);
        const float half = static_cast<float>(ms / 1000.0 / p.beat_length) * px_per_beat;
        const float h = lane.height * (0.35f + 0.1f * static_cast<float>(tier) / tuning::kSyncTierCount);
        DrawRectangleRec({cx - half, cy - h / 2, half * 2, h},
                         with_alpha(th.tiers[static_cast<std::size_t>(tier)], 0.12f));
    }
    DrawLineEx({cx, lane.y}, {cx, lane.y + lane.height}, 2, with_alpha(th.text, 0.7f));

    // Beat ticks scroll towards the centre line.
    for (int j = -2; j <= 3; ++j) {
        for (int sub = 0; sub < (subdivide ? 2 : 1); ++sub) {
            const double rel = j + sub * 0.5 - p.phase;  // beats until this target
            const float x = cx + static_cast<float>(rel) * px_per_beat;
            if (x < lane.x - 2 || x > lane.x + lane.width + 2) continue;
            const bool half = sub == 1;
            const float dist = std::abs(static_cast<float>(rel));
            const float fade = 1.0f - std::min(1.0f, dist / 2.0f);
            const float near = std::max(0.0f, 1.0f - dist * 6.0f);
            const float h = lane.height * (half ? 0.4f : 0.8f);
            DrawLineEx({x, cy - h / 2}, {x, cy + h / 2}, half ? 2.0f : 3.0f + 2.0f * near,
                       with_alpha(lerp(th.accent, th.flash, near), 0.25f + 0.75f * fade));
        }
    }
}

void draw_hud(const Theme& th, const BoardLayout& L, const HudInfo& info) {
    const game::Game& g = *info.game;
    const auto& s = g.stats();
    const float cell = L.cell;
    const float big = cell * 1.35f, mid = cell * 0.85f, small = cell * 0.55f;
    // Breathing room between the panel's top edge and the HOLD / NEXT headings.
    const float top_inset = cell * 0.35f;

    // --- left column: hold + mode progress ---
    float y = L.left.y + top_inset;
    label(th, "HOLD", {L.left.x, y}, small, th.text_dim);
    y += small * 1.4f;
    DrawLineEx({L.left.x, y}, {L.left.x + L.left.width, y}, 1, with_alpha(th.line, 0.2f));
    if (auto h = g.hold())
        draw_piece_icon(th, *h, {L.left.x + L.left.width / 2, y + cell * 1.6f}, cell * 0.9f, g.hold_available() ? 1.0f : 0.35f);
    y += cell * 3.4f;

    const float lx = L.left.x, lr = L.left.x + L.left.width;
    switch (g.mode()) {
        case game::Mode::Sprint:
            label(th, "LINES LEFT", {lx, y}, small, th.text_dim);
            text(th, std::to_string(g.lines_remaining()), {lx, y + below_label(th, small)}, big * 1.3f, th.text, FontWeight::Bold);
            y += below_label(th, small) + big * 1.5f;
            break;
        case game::Mode::Ultra:
            label(th, "TIME LEFT", {lx, y}, small, th.text_dim);
            text(th, format_time(g.time_remaining(), false), {lx, y + below_label(th, small)}, big * 1.3f,
                 g.time_remaining() < 10 ? th.accent2 : th.text, FontWeight::Bold);
            y += below_label(th, small) + big * 1.5f;
            break;
        case game::Mode::Marathon:
        case game::Mode::Endless:
            label(th, "LEVEL", {lx, y}, small, th.text_dim);
            text(th, std::to_string(s.level), {lx, y + below_label(th, small)}, big * 1.3f, th.text, FontWeight::Bold);
            y += below_label(th, small) + big * 1.5f;
            break;
    }
    if (g.mode() == game::Mode::Marathon)
        stat_row(th, lx, lr, y, mid, "LINES", std::to_string(s.lines) + " / " + std::to_string(tuning::kMarathonLines));
    else
        stat_row(th, lx, lr, y, mid, "LINES", std::to_string(s.lines));
    if (g.mode() != game::Mode::Ultra) stat_row(th, lx, lr, y, mid, "TIME", format_time(s.time, g.mode() == game::Mode::Sprint));

    // --- right column: next queue + score ---
    y = L.right.y + top_inset;
    label(th, "NEXT", {L.right.x, y}, small, th.text_dim);
    y += small * 1.4f;
    DrawLineEx({L.right.x, y}, {L.right.x + L.right.width, y}, 1, with_alpha(th.line, 0.2f));
    const auto queue = g.next_queue(tuning::kNextPreviewCount);
    float qy = y + cell * 1.6f;
    for (std::size_t i = 0; i < queue.size(); ++i) {
        const float sc = i == 0 ? 0.9f : 0.62f;
        draw_piece_icon(th, queue[i], {L.right.x + L.right.width / 2, qy}, cell * sc, i == 0 ? 1.0f : 0.75f);
        qy += cell * (i == 0 ? 2.8f : 2.0f);
    }
    y = qy + cell * 0.2f;

    const float rx = L.right.x, rr = L.right.x + L.right.width;
    label(th, "SCORE", {rx, y}, small, th.text_dim);
    text(th, format_int(s.score), {rx, y + below_label(th, small)}, big, th.text, FontWeight::Bold);
    y += below_label(th, small) + big * 1.25f;
    if (!info.best.empty()) {
        label(th, "BEST " + info.best, {rx, y}, small * 0.9f, th.text_faint);
        y += small * 1.6f;
    }
    stat_row(th, rx, rr, y, mid * 0.85f, "PPS", fmt("%.2f", s.pps()));
    stat_row(th, rx, rr, y, mid * 0.85f, "SYNC", std::to_string(s.sync_streak) + "x");

    // --- beat lane ---
    if (info.show_lane) draw_beat_lane(th, L.lane, info.grid, info.sync_time, info.subdivide, info.beat_status);
}

}  // namespace audrop::ui
