#pragma once

#include <cstdint>

namespace audrop::game {

enum class TSpin { None, Mini, Full };

struct ClearScoreInput {
    int lines = 0;
    TSpin tspin = TSpin::None;
    int level = 1;
    bool b2b_active = false;  // a difficult clear happened previously and the chain is unbroken
    int combo = -1;           // combo counter *after* this clear (0 = first clear in a row)
    bool all_clear = false;
    double sync_multiplier = 1.0;
};

struct ClearScore {
    std::int64_t action = 0;     // line clear / T-spin points after B2B
    std::int64_t combo = 0;
    std::int64_t all_clear = 0;
    std::int64_t total = 0;      // including the sync multiplier
    bool difficult = false;      // tetris or T-spin with lines
    bool b2b_applied = false;
};

ClearScore score_clear(const ClearScoreInput& in);

double gravity_seconds_per_row(int level);

}  // namespace audrop::game
