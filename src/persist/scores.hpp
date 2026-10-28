#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include "game/game.hpp"

namespace audrop::persist {

inline constexpr int kScoresPerMode = 10;

struct ScoreEntry {
    std::string mode;
    bool completed = false;
    std::string date;  // ISO-8601 local time
    std::vector<std::string> tracks;
    game::RunStats stats;
};

struct LifetimeStats {
    int games = 0;
    double play_time = 0;
    std::int64_t lines = 0;
    std::int64_t pieces = 0;
    std::int64_t tetrises = 0;
    std::int64_t tspins = 0;
    std::int64_t all_clears = 0;
    std::array<std::int64_t, tuning::kSyncTierCount> sync_tiers{};
    std::int64_t sync_misses = 0;
    int best_sync_streak = 0;
    int best_combo = 0;
};

class ScoreBook {
public:
    void load();
    void save() const;

    // Records a finished run. Returns its 0-based rank in the mode's table, or -1.
    int submit(const ScoreEntry& entry);
    const std::vector<ScoreEntry>& top(game::Mode mode) const;
    const LifetimeStats& lifetime() const { return lifetime_; }

    // Sprint ranks completed runs by time; everything else by score.
    static bool better(game::Mode mode, const ScoreEntry& a, const ScoreEntry& b);

private:
    std::map<std::string, std::vector<ScoreEntry>, std::less<>> tables_;
    LifetimeStats lifetime_;
};

std::string now_iso8601();

}  // namespace audrop::persist
