#include <doctest/doctest.h>

#include <cmath>

#include "config/tuning.hpp"
#include "fx/jelly.hpp"

using namespace audrop;
using game::Board;

namespace {

game::PieceLockedEvent floor_lock(bool hard, int distance) {
    const int y = Board::kHeight - 1;
    return {game::PieceType::I, {{{3, y}, {4, y}, {5, y}, {6, y}}}, hard, distance};
}

void run(fx::Jelly& j, float seconds) {
    for (float t = 0; t < seconds; t += 1.0f / 60) j.update(1.0f / 60);
}

bool identity(const ui::CellWarp& w) { return w.sx == 1 && w.sy == 1 && w.dx == 0 && w.dy == 0; }

}  // namespace

TEST_CASE("a landed piece squashes, then settles back to its normal shape") {
    fx::Jelly j;
    const int y = Board::kHeight - 1;
    CHECK(identity(j.locked(4, y)));
    j.on_lock(floor_lock(true, 18));
    j.update(0.04f);  // near the first squash peak
    const auto w = j.locked(4, y);
    CHECK(w.sy < 0.95f);
    CHECK(w.sx > 1.0f);
    CHECK(w.dy > 0.0f);  // squashes onto its bottom edge
    // Further blocks wobble less.
    CHECK(1 - j.locked(9, y).sy < 1 - w.sy);
    run(j, 2.0f);
    CHECK(identity(j.locked(4, y)));
}

TEST_CASE("long hard drops land harder than soft locks") {
    fx::Jelly soft, hard;
    soft.on_lock(floor_lock(false, 0));
    hard.on_lock(floor_lock(true, 18));
    soft.update(0.04f);
    hard.update(0.04f);
    const int y = Board::kHeight - 1;
    CHECK(hard.locked(4, y).sy < soft.locked(4, y).sy);
}

TEST_CASE("the falling piece trails sideways moves and pops on rotation") {
    game::Game g(game::Mode::Marathon, {}, 1);
    fx::Jelly j;
    j.track(g);
    const auto& a = *g.active();
    CHECK(identity(j.active(a.x + 1, a.y + 1)));

    g.press(game::Button::Right);
    g.release(game::Button::Right);
    j.track(g);
    CHECK(j.active(a.x + 1, a.y + 1).dx < 0);  // lags behind the move to the right
    run(j, 2.0f);
    CHECK(std::abs(j.active(a.x + 1, a.y + 1).dx) < 0.001f);

    g.press(game::Button::RotateCW);
    g.release(game::Button::RotateCW);
    j.track(g);
    j.update(0.03f);
    CHECK(j.active(a.x + 1, a.y + 1).sx > 1.0f);
}
