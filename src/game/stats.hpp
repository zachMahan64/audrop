#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "config/tuning.hpp"

namespace audrop::game {

struct RunStats {
    std::int64_t score = 0;
    int lines = 0;
    int level = 1;
    double time = 0;  // seconds of active play
    int pieces = 0;
    int keys = 0;
    int holds = 0;
    int finesse_faults = 0;

    std::array<int, 5> clears{};        // index = lines cleared (1..4); [4] = tetrises
    std::array<int, 4> tspins{};        // full T-spins by lines (0..3)
    std::array<int, 3> tspin_minis{};   // mini T-spins by lines (0..2)
    int all_clears = 0;
    int max_combo = 0;
    int max_b2b = 0;                    // longest back-to-back chain (bonus applications)

    std::array<int, tuning::kSyncTierCount> sync_tiers{};
    int sync_misses = 0;       // judged clears outside every window
    int sync_unjudged = 0;     // clears with no usable beat grid
    int sync_streak = 0;
    int max_sync_streak = 0;
    double sync_offset_sum = 0;
    double sync_offset_sq_sum = 0;
    int sync_offset_count = 0;

    double pps() const { return time > 0 ? pieces / time : 0; }
    double kpp() const { return pieces > 0 ? static_cast<double>(keys) / pieces : 0; }
    double avg_offset_ms() const { return sync_offset_count ? sync_offset_sum / sync_offset_count : 0; }
    double offset_stddev_ms() const {
        if (sync_offset_count < 2) return 0;
        const double m = avg_offset_ms();
        return std::sqrt(std::max(0.0, sync_offset_sq_sum / sync_offset_count - m * m));
    }
    int sync_hits() const {
        int n = 0;
        for (int v : sync_tiers) n += v;
        return n;
    }
    int total_tspins() const {
        int n = 0;
        for (int v : tspins) n += v;
        for (int v : tspin_minis) n += v;
        return n;
    }
};

}  // namespace audrop::game
