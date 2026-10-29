#include "fx/jelly.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "config/tuning.hpp"

namespace audrop::fx {

namespace {

constexpr float kOmega = 2 * std::numbers::pi_v<float> * tuning::kJellyHz;

// Impacts are dropped once their wobble has died down everywhere they reach.
constexpr float kImpactLife = tuning::kJellyDecay * 6 + tuning::kJellyRippleDelay * 8;

}  // namespace

void Jelly::reset() {
    impacts_.clear();
    lag_ = pop_ = {};
    last_.reset();
    last_key_ = -1;
}

void Jelly::on_lock(const game::PieceLockedEvent& e) {
    float strength = tuning::kJellyLockSquash;
    if (e.hard_drop && e.drop_distance > 0) {
        const float depth = std::min(1.0f, static_cast<float>(e.drop_distance) / tuning::kVisibleRows);
        strength += (tuning::kJellySquash - tuning::kJellyLockSquash) * depth;
    }
    impacts_.push_back({{e.cells.begin(), e.cells.end()}, strength});
}

void Jelly::on_clear(const game::LineClearEvent& e) {
    if (e.rows.empty()) return;
    // Rows above the cleared ones move down; keep earlier impacts on the blocks they hit.
    for (auto& im : impacts_)
        for (auto& c : im.cells)
            c.y += static_cast<int>(std::ranges::count_if(e.rows, [&](int r) { return r > c.y; }));
    // Whatever fell lands on the lowest cleared row.
    Impact im{{}, tuning::kJellyClearSquash};
    const int row = std::ranges::max(e.rows);
    for (int x = 0; x < game::Board::kWidth; ++x) im.cells.push_back({x, row});
    impacts_.push_back(std::move(im));
}

void Jelly::Spring::step(float dt) {
    constexpr float w = 2 * std::numbers::pi_v<float> * tuning::kJellySpringHz;
    // Semi-implicit Euler in small steps stays stable at any frame rate.
    for (float left = std::min(dt, 0.1f); left > 0; left -= 1.0f / 240) {
        const float h = std::min(left, 1.0f / 240);
        v += (-w * w * x - 2 * tuning::kJellySpringDamping * w * v) * h;
        x += v * h;
    }
}

void Jelly::track(const game::Game& g) {
    const auto& a = g.active();
    if (!a) {
        last_.reset();
        return;
    }
    const int key = g.stats().pieces * 4096 + g.stats().holds;
    if (last_ && key == last_key_) {
        if (a->rot != last_->rot) {
            pop_.v += tuning::kJellyRotatePop;
        } else if (a->x != last_->x) {
            lag_.x = std::clamp(lag_.x - static_cast<float>(a->x - last_->x) * tuning::kJellyMoveLag, -tuning::kJellyMaxLag,
                                tuning::kJellyMaxLag);
        }
    } else {
        lag_ = pop_ = {};
    }
    last_ = *a;
    last_key_ = key;
    center_x_ = center_y_ = 0;
    for (auto m : game::piece_minos(a->type, a->rot)) {
        center_x_ += static_cast<float>(a->x + m.x) + 0.5f;
        center_y_ += static_cast<float>(a->y + m.y) + 0.5f;
    }
    center_x_ /= 4;
    center_y_ /= 4;
}

void Jelly::update(float dt) {
    for (auto& im : impacts_) im.age += dt;
    std::erase_if(impacts_, [](const Impact& im) { return im.age > kImpactLife; });
    lag_.step(dt);
    pop_.step(dt);
}

float Jelly::squash(int x, int y) const {
    float q = 0;
    for (const auto& im : impacts_) {
        float d2 = 1e9f;
        for (auto c : im.cells) {
            const float dx = static_cast<float>(c.x - x), dy = static_cast<float>(c.y - y);
            d2 = std::min(d2, dx * dx + dy * dy);
        }
        const float d = std::sqrt(d2);
        const float t = im.age - d * tuning::kJellyRippleDelay;
        if (t <= 0) continue;
        q += im.strength * std::exp(-d / tuning::kJellyReach) * std::exp(-t / tuning::kJellyDecay) * std::sin(kOmega * t);
    }
    return q;
}

ui::CellWarp Jelly::locked(int x, int y) const {
    if (impacts_.empty()) return {};
    const float q = squash(x, y);
    // Each block rests on the (squashed) blocks below it, and squashes onto its own bottom edge.
    float below = 0;
    for (int yy = y + 1; yy < game::Board::kHeight; ++yy) below += squash(x, yy);
    return {1 + q * tuning::kJellyWiden, 1 - q, 0, below + q / 2};
}

ui::CellWarp Jelly::active(int x, int y) const {
    // Scale about the piece's centre, then trail sideways, stretched along the motion.
    const float s = 1 + pop_.x, stretch = std::abs(lag_.x) * tuning::kJellyLagStretch;
    const float cx = static_cast<float>(x) + 0.5f, cy = static_cast<float>(y) + 0.5f;
    return {s * (1 + stretch), s * (1 - stretch * 0.5f), (cx - center_x_) * (s - 1) * (1 + stretch) + lag_.x,
            (cy - center_y_) * (s - 1)};
}

}  // namespace audrop::fx
