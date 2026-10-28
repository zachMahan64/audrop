#pragma once

#include "sync/beat_grid.hpp"

namespace audrop::sync {

struct SyncHit {
    bool judged = false;  // false when there is no usable beat grid
    int tier = -1;        // index into tuning::kSyncTiers, -1 = off-beat
    double offset_ms = 0; // signed: negative = early
    double beat_length = 0;
};

// Judge time `t` (seconds, track time) against the nearest beat.
// With `subdivide`, off-beat eighth notes count as targets too.
SyncHit judge(const BeatGrid& grid, double t, bool subdivide);

struct BeatPhase {
    bool valid = false;
    long index = 0;       // index of the beat at or before t
    double phase = 0;     // 0..1 progress towards the next beat
    double beat_length = 0;
};

BeatPhase beat_phase(const BeatGrid& grid, double t);

// Window half-width in milliseconds for `tier` at the given beat length.
double tier_window_ms(int tier, double beat_length);

}  // namespace audrop::sync
