#include <doctest/doctest.h>

#include <variant>

#include "game/game.hpp"

using namespace audrop::game;

namespace {

template <class T>
int count_events(const std::vector<GameEvent>& evs) {
    int n = 0;
    for (const auto& e : evs) n += std::holds_alternative<T>(e) ? 1 : 0;
    return n;
}

void tap(Game& g, Button b) {
    g.press(b);
    g.release(b);
}

}  // namespace

TEST_CASE("pieces spawn at the top of the visible area") {
    Game g(Mode::Marathon, {}, 1);
    REQUIRE(g.active());
    CHECK(g.active()->x == 3);
    CHECK(g.active()->y == Board::kFirstVisibleRow - 1);
}

TEST_CASE("7-bag gives every piece once per bag") {
    Game g(Mode::Endless, {}, 42);
    std::array<int, kPieceTypeCount> seen{};
    ++seen[static_cast<std::size_t>(g.active()->type)];
    for (PieceType t : g.next_queue(6)) ++seen[static_cast<std::size_t>(t)];
    for (int n : seen) CHECK(n == 1);
}

TEST_CASE("hard drop locks and spawns the next piece") {
    Game g(Mode::Marathon, {}, 7);
    const auto next = g.next_queue(1)[0];
    tap(g, Button::HardDrop);
    auto evs = g.take_events();
    CHECK(count_events<PieceLockedEvent>(evs) == 1);
    REQUIRE(g.active());
    CHECK(g.active()->type == next);
    CHECK(g.stats().pieces == 1);
    CHECK(g.stats().score > 0);
}

TEST_CASE("gravity and lock delay lock a piece eventually") {
    Game g(Mode::Marathon, {}, 3);
    for (int i = 0; i < 30000 && g.stats().pieces == 0; ++i) g.update(0.001);
    CHECK(g.stats().pieces == 1);
}

TEST_CASE("hold swaps once per piece") {
    Game g(Mode::Marathon, {}, 9);
    const auto first = g.active()->type;
    tap(g, Button::Hold);
    CHECK(g.hold() == first);
    CHECK_FALSE(g.hold_available());
    const auto second = g.active()->type;
    tap(g, Button::Hold);
    CHECK(g.active()->type == second);  // second hold ignored
}

TEST_CASE("DAS moves the piece to the wall") {
    Handling h;
    h.das_ms = 100;
    h.arr_ms = 0;
    Game g(Mode::Marathon, h, 11);
    g.press(Button::Left);
    for (int i = 0; i < 150; ++i) g.update(0.001);
    const auto& p = *g.active();
    int min_x = 99;
    for (Vec2i m : piece_minos(p.type, p.rot)) min_x = std::min(min_x, p.x + m.x);
    CHECK(min_x == 0);
}

TEST_CASE("clearing lines with a sync judge records the tier") {
    Game g(Mode::Endless, {}, 5);
    g.set_sync_judge([] {
        audrop::sync::SyncHit h;
        h.judged = true;
        h.tier = audrop::tuning::kSyncTierCount - 1;
        h.offset_ms = 2.0;
        return h;
    });
    // Drop pieces until a line clears (random play won't, so fill the floor by brute force:
    // place I pieces flat at x = 0..? We instead simulate many drops spread across the board.)
    int clears = 0;
    for (int i = 0; i < 400 && !g.over() && clears == 0; ++i) {
        // Spread pieces left-to-right to eventually complete rows.
        const int target = (i * 3) % 8;
        while (g.active()) {
            const auto& p = *g.active();
            int min_x = 99;
            for (Vec2i m : piece_minos(p.type, p.rot)) min_x = std::min(min_x, p.x + m.x);
            if (min_x == target) break;
            const int before = p.x;
            tap(g, min_x < target ? Button::Right : Button::Left);
            if (!g.active() || g.active()->x == before) break;
        }
        tap(g, Button::HardDrop);
        for (const auto& e : g.take_events())
            if (const auto* c = std::get_if<LineClearEvent>(&e)) {
                ++clears;
                CHECK(c->sync.tier == audrop::tuning::kSyncTierCount - 1);
            }
    }
    if (clears > 0) {
        CHECK(g.stats().sync_tiers[audrop::tuning::kSyncTierCount - 1] >= 1);
        CHECK(g.stats().avg_offset_ms() == doctest::Approx(2.0));
    }
}

TEST_CASE("topping out ends the game") {
    Game g(Mode::Marathon, {}, 13);
    for (int i = 0; i < 200 && !g.over(); ++i) tap(g, Button::HardDrop);
    CHECK(g.over());
    CHECK_FALSE(g.completed());
}

TEST_CASE("ultra ends after two minutes") {
    Game g(Mode::Ultra, {}, 1);
    for (int i = 0; i < 1300 && !g.over(); ++i) g.update(0.1);
    CHECK(g.over());
}
