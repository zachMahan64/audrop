#pragma once

#include <raylib.h>

#include <random>
#include <vector>

#include "ui/theme.hpp"

namespace audrop::fx {

// Ambient glassy bubbles drifting up over the background (themes with `bubbles`).
class Bubbles {
public:
    // `beat` (0..1) briefly swells the bubbles.
    void update(float dt, float width, float height, float beat);
    void draw(const ui::Theme& th) const;

private:
    struct Bubble {
        float x, y;
        float radius;
        float speed;   // px / s upwards
        float phase;   // sideways sway offset
        float sway;    // sideways sway amplitude in px
    };
    Bubble spawn(bool anywhere);

    std::vector<Bubble> items_;
    std::mt19937 rng_{77};
    float width_ = 0, height_ = 0;
    float time_ = 0;
    float beat_ = 0;
};

}  // namespace audrop::fx
