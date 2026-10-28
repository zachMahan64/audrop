#pragma once

#include <raylib.h>

#include <vector>

#include "ui/theme.hpp"

namespace audrop::fx {

struct Particle {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float life;
    float max_life;
    float size;
    float drag;     // velocity damping per second
    float gravity;  // px / s^2
};

class Particles {
public:
    void spawn(const Particle& p);
    void update(float dt);
    // Pixel-art themes draw solid squares snapped to a `unit` grid anchored at `origin`.
    void draw(const ui::Theme& th, float unit, Vector2 origin) const;
    void clear() { items_.clear(); }
    std::size_t size() const { return items_.size(); }

private:
    std::vector<Particle> items_;
};

}  // namespace audrop::fx
