#include <doctest/doctest.h>

#include "config/tuning.hpp"
#include "sync/judge.hpp"

using namespace audrop;

namespace {

sync::BeatGrid grid_120() {
    sync::BeatGrid g;
    g.bpm = 120;
    g.confidence = 1.0f;
    for (int i = 0; i < 100; ++i) g.beats.push_back(1.0 + i * 0.5);
    return g;
}

}  // namespace

TEST_CASE("tiers by offset at 120 bpm") {
    auto g = grid_120();
    auto perfect = sync::judge(g, 2.005, false);
    CHECK(perfect.judged);
    CHECK(perfect.tier == tuning::kSyncTierCount - 1);
    CHECK(perfect.offset_ms == doctest::Approx(5.0));

    // 1/16 of a 500ms beat = 31.25ms => GOOD for 30ms early
    auto good = sync::judge(g, 2.0 - 0.030, false);
    CHECK(good.tier == 1);
    CHECK(good.offset_ms < 0);

    // 1/8 beat = 62.5ms; 100ms off is a miss
    auto miss = sync::judge(g, 2.1, false);
    CHECK(miss.judged);
    CHECK(miss.tier == -1);
}

TEST_CASE("ms floors keep fast tempos fair") {
    sync::BeatGrid g;
    g.confidence = 1.0f;
    for (int i = 0; i < 100; ++i) g.beats.push_back(i * (60.0 / 200.0));  // 200 BPM
    // 1/48 of 300ms is 6.25ms, but the PERFECT floor is 10ms.
    auto hit = sync::judge(g, g.beats[10] + 0.009, false);
    CHECK(hit.tier == tuning::kSyncTierCount - 1);
}

TEST_CASE("subdivision targets") {
    auto g = grid_120();
    CHECK(sync::judge(g, 2.25, false).tier == -1);
    CHECK(sync::judge(g, 2.25, true).tier == tuning::kSyncTierCount - 1);
}

TEST_CASE("unusable grids are not judged") {
    sync::BeatGrid g = grid_120();
    g.confidence = 0.0f;
    CHECK_FALSE(sync::judge(g, 2.0, false).judged);
}

TEST_CASE("beat adjustments") {
    auto g = grid_120();
    auto half = sync::apply_adjust(g, {0, -1});
    CHECK(half.beats.size() == 50);
    CHECK(half.bpm == doctest::Approx(60));
    auto dbl = sync::apply_adjust(g, {10, 1});
    CHECK(dbl.beats.size() == 199);
    CHECK(dbl.beats[1] == doctest::Approx(1.26));
}

TEST_CASE("beat phase extrapolates beyond the grid") {
    auto g = grid_120();
    auto p = sync::beat_phase(g, 0.25);
    CHECK(p.valid);
    CHECK(p.phase == doctest::Approx(0.5));
    auto q = sync::beat_phase(g, g.beats.back() + 0.75);
    CHECK(q.phase == doctest::Approx(0.5));
}
