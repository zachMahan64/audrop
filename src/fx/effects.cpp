#include "fx/effects.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "config/tuning.hpp"

namespace audrop::fx {

using namespace ui;

std::string clear_name(int lines, game::TSpin tspin) {
    static constexpr const char* kCounts[] = {"", "SINGLE", "DOUBLE", "TRIPLE", "QUAD"};
    if (tspin == game::TSpin::None) return kCounts[std::clamp(lines, 0, 4)];
    std::string s = tspin == game::TSpin::Mini ? "MINI T-SPIN" : "T-SPIN";
    if (lines > 0) s += std::string(" ") + kCounts[std::clamp(lines, 0, 3)];
    return s;
}

void Effects::reset() {
    particles_.clear();
    rows_.clear();
    cells_.clear();
    trails_.clear();
    waves_.clear();
    popups_.clear();
    jelly_.reset();
    trauma_ = chroma_ = flash_ = beat_pulse_ = clear_glow_ = 0;
    if (th_) flash_color_ = pulse_color_ = th_->flash;
    preset_cut_ = false;
}

void Effects::popup(std::string text, Color color) {
    Popup p;
    p.text = std::move(text);
    p.color = color;
    p.size = 0;  // resolved at draw time from the layout
    p.slot = 2;
    p.duration = 1.6f;
    std::erase_if(popups_, [](const Popup& o) { return o.slot == 2; });
    popups_.push_back(std::move(p));
}

void Effects::on_lock(const game::PieceLockedEvent& e, const BoardLayout& L) {
    jelly_.on_lock(e);
    const Color c = th_->piece(e.type);
    for (auto v : e.cells) {
        CellFlash f;
        f.r = L.cell_rect(v.x, v.y);
        f.color = lerp(c, th_->flash, 0.7f);
        f.duration = 0.18f;
        cells_.push_back(f);
    }
    if (e.hard_drop && e.drop_distance > 0) {
        // One streak per column, from where the piece started to where it landed.
        for (auto v : e.cells) {
            bool top = true;
            for (auto o : e.cells)
                if (o.x == v.x && o.y < v.y) top = false;
            if (!top) continue;
            Trail t;
            const Vector2 bottom = L.cell_pos(v.x, v.y);
            const float h = static_cast<float>(e.drop_distance) * L.cell;
            t.r = {bottom.x + L.cell * 0.15f, bottom.y - h, L.cell * 0.7f, h};
            t.color = c;
            t.duration = 0.22f;
            trails_.push_back(t);
        }
    }
}

void Effects::on_clear(const game::LineClearEvent& e, const BoardLayout& L) {
    jelly_.on_clear(e);
    const int tier = e.sync.tier;
    float tier_fx;
    Color tier_color;
    if (tier >= 0) {
        const auto& t = tuning::kSyncTiers[static_cast<std::size_t>(tier)];
        tier_fx = t.fx_intensity;
        tier_color = th_->tiers[static_cast<std::size_t>(tier)];
    } else if (e.sync.judged) {
        tier_fx = tuning::kOffBeatFx;
        tier_color = lerp(th_->off_beat, th_->flash, 0.5f);
    } else {
        // No beat grid (yet): moderate effects so the game still feels good.
        tier_fx = 0.3f;
        tier_color = lerp(th_->accent, th_->flash, 0.6f);
    }
    float size = tuning::kClearSizeFx[static_cast<std::size_t>(std::clamp(e.lines, 0, 4))];
    if (e.tspin != game::TSpin::None) size += tuning::kTSpinFxBonus;
    if (e.all_clear) size += tuning::kAllClearFxBonus;
    const float I = tier_fx * size;
    const float Ic = std::min(1.0f, I);

    // Row flashes.
    float rows_y = 0;
    for (int row : e.rows) {
        RowFlash f;
        f.y = L.cell_pos(0, row).y;
        f.height = L.cell;
        f.intensity = Ic;
        f.color = lerp(th_->flash, tier_color, 0.5f);
        f.duration = 0.25f + 0.3f * Ic;
        rows_.push_back(f);
        rows_y += f.y + L.cell / 2;
    }
    rows_y /= static_cast<float>(std::max<std::size_t>(1, e.rows.size()));
    const float cx = L.board.x + L.board.width / 2;

    // Particles from every cleared cell.
    const bool spill = I >= tuning::kFxSpill;
    const int per_cell = tuning::kParticlesPerCellBase +
                         static_cast<int>(static_cast<float>(tuning::kParticlesPerCellMax - tuning::kParticlesPerCellBase) * Ic);
    for (std::size_t r = 0; r < e.rows.size(); ++r) {
        for (int x = 0; x < game::Board::kWidth; ++x) {
            const auto v = e.contents[r][static_cast<std::size_t>(x)];
            const Color base = v ? th_->piece(game::cell_piece(v)) : th_->flash;
            const Rectangle cr = L.cell_rect(x, e.rows[r]);
            const float side = (static_cast<float>(x) - 4.5f) / 4.5f;
            for (int k = 0; k < per_cell; ++k) {
                Particle p;
                p.pos = {cr.x + rand01() * cr.width, cr.y + rand01() * cr.height};
                const float speed = (120.0f + 1000.0f * I) * (0.25f + 0.75f * rand01());
                const float ang = (rand01() - 0.5f) * std::numbers::pi_v<float> * (0.5f + 0.9f * I);
                const float dir = side == 0 ? (rand01() < 0.5f ? -1.0f : 1.0f) : (side > 0 ? 1.0f : -1.0f);
                p.vel = {dir * std::cos(ang) * speed * (0.4f + 0.6f * std::abs(side)), std::sin(ang) * speed};
                p.color = lerp(base, tier_color, 0.45f * Ic);
                p.max_life = p.life = spill ? 0.8f + rand01() * 0.9f : 0.3f + rand01() * 0.4f;
                p.size = L.cell * (0.2f + 0.35f * rand01()) * (1.0f + 0.6f * I);
                p.drag = spill ? 1.4f : 3.5f;
                p.gravity = spill ? 260.0f : 60.0f;
                particles_.spawn(p);
            }
        }
    }

    clear_glow_ = std::max(clear_glow_, Ic);
    pulse_color_ = tier_color;

    // Pixel art has no smooth distortion: no shockwave or chromatic split.
    const bool smooth = !th_->pixel_art;
    if (smooth && I >= tuning::kFxShockwave) {
        Wave w;
        w.center = {cx, rows_y};
        const float diag = std::hypot(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
        w.max_radius = diag * (0.25f + 0.5f * Ic);
        w.strength = L.cell * (0.25f + 0.8f * Ic);
        w.duration = 0.75f;
        waves_.push_back(w);
        if (waves_.size() > 4) waves_.erase(waves_.begin());
    }
    if (I >= tuning::kFxScreenShake && opts_.screen_shake) trauma_ = std::min(1.0f, trauma_ + 0.3f + 0.5f * (I - tuning::kFxScreenShake));
    if (smooth && I >= tuning::kFxChromatic) chroma_ = std::max(chroma_, opts_.reduce_flashing ? 3.0f : 4.0f + 12.0f * Ic);
    if (I >= tuning::kFxFullFlash && !opts_.reduce_flashing) {
        flash_ = std::max(flash_, 0.22f * Ic);
        flash_color_ = lerp(th_->flash, tier_color, 0.5f);
    }
    if (tier == tuning::kSyncTierCount - 1 && !opts_.reduce_flashing &&
        (e.lines == 4 || e.tspin != game::TSpin::None || e.all_clear))
        preset_cut_ = true;

    // Judgment popup (on the board).
    if (e.sync.judged) {
        Popup j;
        char ms[32];
        std::snprintf(ms, sizeof ms, "%+.0f ms", e.sync.offset_ms);
        j.sub = ms;
        j.slot = 0;
        if (tier >= 0) {
            j.text = std::string(tuning::kSyncTiers[static_cast<std::size_t>(tier)].name);
            j.color = tier_color;
            j.size = 1.0f + 0.7f * tier_fx;
            j.duration = 0.9f;
        } else {
            j.text = "OFF BEAT";
            j.color = th_->off_beat;
            j.size = 0.75f;
            j.duration = 0.6f;
        }
        std::erase_if(popups_, [](const Popup& o) { return o.slot == 0; });
        popups_.push_back(std::move(j));
    }

    // Clear-type popup (left column).
    Popup c;
    c.text = clear_name(e.lines, e.tspin);
    std::string sub;
    if (e.b2b) sub += "B2B x" + std::to_string(e.b2b_chain) + "  ";
    if (e.combo > 0) sub += std::to_string(e.combo) + " COMBO  ";
    sub += "+" + format_int(e.score);
    c.sub = sub;
    c.color = e.lines == 4 || e.tspin != game::TSpin::None ? th_->popup_big : th_->popup_clear;
    c.size = e.lines == 4 || e.tspin != game::TSpin::None ? 1.0f : 0.8f;
    c.slot = 1;
    c.duration = 1.4f;
    std::erase_if(popups_, [](const Popup& o) { return o.slot == 1; });
    popups_.push_back(std::move(c));

    if (e.all_clear) popup("ALL CLEAR", th_->popup_all_clear);
}

void Effects::on_tspin_no_lines(game::TSpin t) {
    Popup c;
    c.text = clear_name(0, t);
    c.color = th_->popup_tspin;
    c.size = 0.8f;
    c.slot = 1;
    c.duration = 1.0f;
    std::erase_if(popups_, [](const Popup& o) { return o.slot == 1; });
    popups_.push_back(std::move(c));
}

void Effects::on_level_up(int level) { popup("LEVEL " + std::to_string(level), th_->popup_level); }

void Effects::on_beat(float strength) { beat_pulse_ = std::max(beat_pulse_, strength); }

void Effects::update(float dt) {
    time_ += dt;
    particles_.update(dt);
    jelly_.update(dt);
    auto age = [dt](auto& list) {
        for (auto& x : list) x.age += dt;
        std::erase_if(list, [](const auto& x) { return x.age >= x.duration; });
    };
    age(rows_);
    age(cells_);
    age(trails_);
    age(waves_);
    age(popups_);
    trauma_ = std::max(0.0f, trauma_ - 1.4f * dt);
    chroma_ *= std::exp(-6.0f * dt);
    flash_ *= std::exp(-7.0f * dt);
    beat_pulse_ *= std::exp(-6.0f * dt);
    clear_glow_ *= std::exp(-2.5f * dt);
}

namespace {

// Pixel effects blink rather than fade: on for one 50 ms frame, off for the next.
bool blink_on(float age) { return static_cast<int>(age / 0.05f) % 2 == 0; }

}  // namespace

void Effects::draw_board_layer_pixel(const BoardLayout& L) const {
    const float u = L.cell / 8.0f;
    for (const auto& t : trails_) {
        // Solid bands that thin out towards the top of the drop, in a few hard steps.
        const float left = t.r.x - L.cell * 0.15f;
        const int rows = std::max(1, static_cast<int>(std::lround(t.r.height / L.cell)));
        const float fade = std::ceil((1.0f - t.t()) * 3.0f) / 3.0f;
        for (int k = 0; k < rows; ++k) {
            const float strength = std::ceil(static_cast<float>(rows - k) / static_cast<float>(rows) * 4.0f) / 4.0f;
            DrawRectangleRec({left + u, t.r.y + t.r.height - static_cast<float>(k + 1) * L.cell, 5 * u, L.cell},
                             with_alpha(t.color, 0.4f * strength * fade));
        }
    }
    for (const auto& c : cells_)
        if (c.t() < 0.5f) DrawRectangleRec({c.r.x, c.r.y, 7 * u, 7 * u}, with_alpha(c.color, 0.8f));
    for (const auto& r : rows_) {
        // Hard flash: solid for the first half, then blinking between the flash and judgment colours.
        if (r.t() >= 0.5f && !blink_on(r.age)) continue;
        const Color c = r.t() < 0.5f && blink_on(r.age) ? th_->flash : r.color;
        DrawRectangleRec({L.board.x, r.y, L.board.width, r.height}, with_alpha(c, 0.85f));
    }
}

void Effects::draw_board_layer(const Theme& th, const BoardLayout& L) const {
    if (th.pixel_art) return draw_board_layer_pixel(L);
    begin_glow(th);
    for (const auto& t : trails_) {
        const float a = 1.0f - t.t();
        DrawRectangleGradientV(static_cast<int>(t.r.x), static_cast<int>(t.r.y), static_cast<int>(t.r.width),
                               static_cast<int>(t.r.height), BLANK, with_alpha(t.color, 0.45f * a));
    }
    for (const auto& c : cells_) DrawRectangleRec(c.r, with_alpha(c.color, 0.55f * (1.0f - c.t())));
    for (const auto& r : rows_) {
        const float t = r.t();
        const float a = (1.0f - t) * (1.0f - t);
        const float grow = 1.0f + r.intensity * 0.8f * t;
        const float h = r.height * (1.0f + 1.2f * r.intensity * t);
        const float w = L.board.width * grow;
        const Rectangle rect{L.board.x + (L.board.width - w) / 2, r.y + r.height / 2 - h / 2, w, h};
        DrawRectangleRec(rect, with_alpha(r.color, 0.65f * a));
        draw_glow(th, {rect.x + rect.width / 2, rect.y + rect.height / 2}, w * 0.6f, with_alpha(r.color, 0.35f * a * (0.3f + r.intensity)));
    }
    end_glow();
}

void Effects::draw_overlay(const Theme& th, const BoardLayout& L) const {
    const float u = L.cell / 8.0f;
    particles_.draw(th, u, {L.board.x, L.board.y});
    const float cell = L.cell;
    for (const auto& p : popups_) {
        const float t = p.t();
        float alpha, pop, rise = 20.0f * t;
        if (th.pixel_art) {
            // Appear at once, blink out at the end, rise in whole pixels.
            if (t > 0.7f && !blink_on(p.age)) continue;
            alpha = 1.0f;
            pop = p.age < 0.1f ? 1.25f : 1.0f;
            rise = std::round(rise / u) * u;
        } else {
            const float in = std::min(1.0f, p.age / 0.08f);
            const float out = t > 0.7f ? 1.0f - (t - 0.7f) / 0.3f : 1.0f;
            alpha = in * out;
            pop = 1.0f + 0.35f * std::max(0.0f, 1.0f - p.age / 0.12f);
        }
        Vector2 at;
        float size;
        switch (p.slot) {
            case 0:
                at = {L.board.x + L.board.width / 2, L.board.y + L.board.height * 0.3f - rise};
                size = cell * 1.25f * p.size * pop;
                break;
            case 1:
                at = {L.left.x + L.left.width / 2, L.board.y + L.board.height * 0.66f};
                size = cell * 1.05f * p.size * pop;
                break;
            default:
                at = {L.board.x + L.board.width / 2, L.board.y + L.board.height * 0.5f};
                size = cell * 1.5f * pop;
                break;
        }
        glow_text_centered(th, p.text, at, size, with_alpha(p.color, alpha), alpha, FontWeight::Bold, size * 0.06f);
        if (!p.sub.empty())
            text_centered(th, p.sub, {at.x, at.y + size * 0.75f}, cell * 0.6f, with_alpha(th.text_dim, alpha),
                          FontWeight::Regular, 1.5f);
    }
}

viz::PostParams Effects::post() const {
    viz::PostParams p;
    p.chroma = chroma_;
    p.flash = with_alpha(flash_color_, std::min(1.0f, flash_));
    for (const auto& w : waves_) {
        if (p.wave_count >= 4) break;
        const float t = w.t();
        const float ease = 1.0f - (1.0f - t) * (1.0f - t);
        p.waves[static_cast<std::size_t>(p.wave_count++)] = {w.center, w.max_radius * ease, w.strength * (1.0f - t)};
    }
    return p;
}

Vector2 Effects::shake() const {
    if (trauma_ <= 0) return {0, 0};
    const float amt = 16.0f * trauma_ * trauma_;
    return {amt * std::sin(time_ * 53.0f) * std::cos(time_ * 21.0f), amt * std::cos(time_ * 47.0f) * std::sin(time_ * 17.0f)};
}

}  // namespace audrop::fx
