#include <doctest/doctest.h>

#include "game/scoring.hpp"

using namespace audrop::game;

TEST_CASE("basic line clears scale with level") {
    CHECK(score_clear({1, TSpin::None, 1, false, 0, false, 1.0}).total == 100);
    CHECK(score_clear({4, TSpin::None, 3, false, 0, false, 1.0}).total == 2400);
}

TEST_CASE("back-to-back applies to difficult clears only") {
    auto tetris = score_clear({4, TSpin::None, 1, true, 0, false, 1.0});
    CHECK(tetris.b2b_applied);
    CHECK(tetris.total == 1200);

    auto single = score_clear({1, TSpin::None, 1, true, 0, false, 1.0});
    CHECK_FALSE(single.b2b_applied);
    CHECK_FALSE(single.difficult);
    CHECK(single.total == 100);

    auto tsd = score_clear({2, TSpin::Full, 1, true, 0, false, 1.0});
    CHECK(tsd.difficult);
    CHECK(tsd.total == 1800);
}

TEST_CASE("t-spins") {
    CHECK(score_clear({0, TSpin::Full, 1, false, -1, false, 1.0}).total == 400);
    CHECK(score_clear({0, TSpin::Mini, 1, false, -1, false, 1.0}).total == 100);
    CHECK(score_clear({3, TSpin::Full, 2, false, 0, false, 1.0}).total == 3200);
    CHECK(score_clear({1, TSpin::Mini, 1, false, 0, false, 1.0}).total == 200);
}

TEST_CASE("combo and all clear") {
    auto c = score_clear({1, TSpin::None, 2, false, 3, false, 1.0});
    CHECK(c.combo == 300);
    CHECK(c.total == 500);

    auto pc = score_clear({4, TSpin::None, 1, false, 0, true, 1.0});
    CHECK(pc.all_clear == 2000);
    CHECK(pc.total == 2800);

    auto b2b_pc = score_clear({4, TSpin::None, 1, true, 0, true, 1.0});
    CHECK(b2b_pc.all_clear == 3200);
}

TEST_CASE("sync multiplier only applies to clears with lines") {
    CHECK(score_clear({4, TSpin::None, 1, false, 0, false, 2.0}).total == 1600);
    CHECK(score_clear({0, TSpin::Full, 1, false, -1, false, 2.0}).total == 400);
}

TEST_CASE("gravity curve") {
    CHECK(gravity_seconds_per_row(1) == doctest::Approx(1.0));
    CHECK(gravity_seconds_per_row(2) == doctest::Approx(0.793));
    CHECK(gravity_seconds_per_row(15) < 0.01);
}
