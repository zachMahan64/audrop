#pragma once

#include <vector>

namespace audrop::sync {

struct BeatGrid {
    std::vector<double> beats;  // seconds, ascending
    double bpm = 0;
    float confidence = 0;       // 0..1

    bool usable() const;
};

// Per-track manual corrections the player can make when the tracker gets it wrong.
struct BeatAdjust {
    double offset_ms = 0;
    int tempo_shift = 0;  // -1 = half tempo, 0 = as tracked, +1 = double tempo

    bool operator==(const BeatAdjust&) const = default;
};

BeatGrid apply_adjust(const BeatGrid& raw, const BeatAdjust& adjust);

}  // namespace audrop::sync
