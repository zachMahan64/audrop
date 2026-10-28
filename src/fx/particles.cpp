#include "fx/particles.hpp"

#include <algorithm>
#include <cmath>

#include "config/tuning.hpp"

namespace audrop::fx {

void Particles::spawn(const Particle& p) {
    if (items_.size() >= static_cast<std::size_t>(tuning::kMaxParticles)) return;
    items_.push_back(p);
}

void Particles::update(float dt) {
    for (auto& p : items_) {
        p.life -= dt;
        const float damp = std::exp(-p.drag * dt);
        p.vel.x *= damp;
        p.vel.y = p.vel.y * damp + p.gravity * dt;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
    }
    std::erase_if(items_, [](const Particle& p) { return p.life <= 0; });
}

void Particles::draw(const ui::Theme& th, float unit, Vector2 origin) const {
    if (th.pixel_art) {
        const auto snap = [unit](float v) { return std::round(v / unit) * unit; };
        for (const auto& p : items_) {
            const float t = std::clamp(p.life / p.max_life, 0.0f, 1.0f);
            const float side = std::max(1.0f, std::round(p.size * (0.4f + 0.6f * t) * 0.6f / unit)) * unit;
            DrawRectangleRec({origin.x + snap(p.pos.x - origin.x - side / 2), origin.y + snap(p.pos.y - origin.y - side / 2), side, side},
                             p.color);
        }
        return;
    }
    ui::begin_glow(th);
    for (const auto& p : items_) {
        const float t = std::clamp(p.life / p.max_life, 0.0f, 1.0f);
        const float r = p.size * (0.4f + 0.6f * t);
        ui::draw_glow(th, p.pos, r, ui::with_alpha(p.color, t));
    }
    ui::end_glow();
}

}  // namespace audrop::fx
