#include "game/scoring.hpp"

#include <algorithm>
#include <cmath>

#include "config/tuning.hpp"

namespace audrop::game {

ClearScore score_clear(const ClearScoreInput& in) {
    using namespace tuning;
    ClearScore s;
    const int lines = std::clamp(in.lines, 0, 4);
    const auto idx = static_cast<std::size_t>(lines);

    double action = 0;
    if (in.tspin == TSpin::Full)
        action = kTSpinScore[std::min<std::size_t>(idx, 3)];
    else if (in.tspin == TSpin::Mini)
        action = kTSpinMiniScore[std::min<std::size_t>(idx, 2)];
    else
        action = kLineClearScore[idx];
    action *= in.level;

    s.difficult = lines == 4 || (in.tspin != TSpin::None && lines > 0);
    if (s.difficult && in.b2b_active) {
        s.b2b_applied = true;
        action *= kBackToBackMultiplier;
    }
    s.action = static_cast<std::int64_t>(std::llround(action));

    if (lines > 0 && in.combo > 0) s.combo = static_cast<std::int64_t>(kComboScore) * in.combo * in.level;

    if (lines > 0 && in.all_clear) {
        const int base = (lines == 4 && s.b2b_applied) ? kB2BTetrisAllClearScore : kAllClearScore[idx];
        s.all_clear = static_cast<std::int64_t>(base) * in.level;
    }

    // The sync bonus only applies to clears that actually removed lines.
    const double mult = lines > 0 ? in.sync_multiplier : 1.0;
    s.total = static_cast<std::int64_t>(std::llround(static_cast<double>(s.action + s.combo + s.all_clear) * mult));
    return s;
}

double gravity_seconds_per_row(int level) {
    const int l = std::max(1, level);
    return std::pow(0.8 - (l - 1) * 0.007, l - 1);
}

}  // namespace audrop::game
