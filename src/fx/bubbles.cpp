#include "fx/bubbles.hpp"

#include <algorithm>
#include <cmath>

namespace audrop::fx {
namespace {

constexpr int kCount = 26;

}  // namespace

Bubbles::Bubble Bubbles::spawn(bool anywhere) {
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    const float size = u(rng_) * u(rng_);  // mostly small, a few large
    Bubble b;
    b.radius = height_ * (0.01f + 0.045f * size);
    b.x = u(rng_) * width_;
    b.y = anywhere ? u(rng_) * height_ : height_ + b.radius;
    b.speed = height_ * (0.02f + 0.05f * size) * (0.8f + 0.4f * u(rng_));
    b.phase = u(rng_) * 6.283f;
    b.sway = b.radius * (0.5f + 1.5f * u(rng_));
    return b;
}

void Bubbles::update(float dt, float width, float height, float beat) {
    if (std::abs(width - width_) > 1 || std::abs(height - height_) > 1) {
        width_ = width;
        height_ = height;
        items_.clear();
        for (int i = 0; i < kCount; ++i) items_.push_back(spawn(true));
    }
    time_ += dt;
    beat_ = beat;
    for (auto& b : items_) {
        b.y -= b.speed * dt;
        if (b.y < -b.radius * 2) b = spawn(false);
    }
}

void Bubbles::draw(const ui::Theme& th) const {
    using ui::with_alpha;
    for (const auto& b : items_) {
        const float r = b.radius * (1.0f + 0.06f * beat_);
        const float x = b.x + std::sin(time_ * 0.7f + b.phase) * b.sway;
        const Vector2 c{x, b.y};
        DrawCircleGradient(static_cast<int>(x), static_cast<int>(b.y), r, Color{255, 255, 255, 10}, with_alpha(th.accent, 0.22f));
        DrawRing(c, r * 0.93f, r, 0, 360, 48, with_alpha(WHITE, 0.35f));
        DrawEllipse(static_cast<int>(x - r * 0.32f), static_cast<int>(b.y - r * 0.42f), r * 0.3f, r * 0.15f, with_alpha(WHITE, 0.6f));
        DrawCircleV({x + r * 0.42f, b.y + r * 0.4f}, r * 0.07f, with_alpha(WHITE, 0.4f));
    }
}

}  // namespace audrop::fx
