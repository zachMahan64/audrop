#include "sync/judge.hpp"

#include <algorithm>
#include <cmath>

#include "config/tuning.hpp"

namespace audrop::sync {

double tier_window_ms(int tier, double beat_length) {
    const auto& t = tuning::kSyncTiers[static_cast<std::size_t>(tier)];
    return std::max(t.beat_fraction * beat_length * 1000.0, t.min_ms);
}

BeatPhase beat_phase(const BeatGrid& grid, double t) {
    BeatPhase p;
    const auto& b = grid.beats;
    if (b.size() < 2) return p;

    auto it = std::upper_bound(b.begin(), b.end(), t);
    std::size_t i;
    if (it == b.begin()) {
        // Before the first beat: extrapolate backwards with the first interval.
        const double len = b[1] - b[0];
        const double k = std::floor((t - b[0]) / len);
        p.valid = true;
        p.beat_length = len;
        p.index = static_cast<long>(k);
        p.phase = (t - (b[0] + k * len)) / len;
        return p;
    }
    i = static_cast<std::size_t>(it - b.begin()) - 1;
    if (i + 1 >= b.size()) {
        // After the last beat: extrapolate with the last interval.
        const double len = b[i] - b[i - 1];
        const double k = std::floor((t - b[i]) / len);
        p.valid = true;
        p.beat_length = len;
        p.index = static_cast<long>(i) + static_cast<long>(k);
        p.phase = (t - (b[i] + k * len)) / len;
        return p;
    }
    p.valid = true;
    p.index = static_cast<long>(i);
    p.beat_length = b[i + 1] - b[i];
    p.phase = (t - b[i]) / p.beat_length;
    return p;
}

SyncHit judge(const BeatGrid& grid, double t, bool subdivide) {
    SyncHit hit;
    if (!grid.usable()) return hit;

    const BeatPhase p = beat_phase(grid, t);
    if (!p.valid || p.beat_length <= 0) return hit;

    // Distance to the nearest target, as a signed fraction of the beat.
    double frac = p.phase;  // 0..1 between beat i and i+1
    double target_spacing = subdivide ? 0.5 : 1.0;
    double nearest = std::round(frac / target_spacing) * target_spacing;
    double signed_frac = frac - nearest;  // negative = early (before target)

    hit.judged = true;
    hit.beat_length = p.beat_length;
    hit.offset_ms = signed_frac * p.beat_length * 1000.0;

    const double abs_ms = std::abs(hit.offset_ms);
    for (int tier = tuning::kSyncTierCount - 1; tier >= 0; --tier) {
        if (abs_ms <= tier_window_ms(tier, p.beat_length)) {
            hit.tier = tier;
            break;
        }
    }
    return hit;
}

}  // namespace audrop::sync
