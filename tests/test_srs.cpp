#include <doctest/doctest.h>

#include <algorithm>

#include "game/board.hpp"
#include "game/finesse.hpp"
#include "game/piece.hpp"

using namespace audrop::game;

namespace {

bool contains(const Minos& m, Vec2i v) { return std::ranges::find(m, v) != m.end(); }

}  // namespace

TEST_CASE("spawn shapes match the guideline") {
    const Minos& t = piece_minos(PieceType::T, 0);
    CHECK(contains(t, {1, 0}));
    CHECK(contains(t, {0, 1}));
    CHECK(contains(t, {1, 1}));
    CHECK(contains(t, {2, 1}));

    const Minos& i = piece_minos(PieceType::I, 0);
    for (int x = 0; x < 4; ++x) CHECK(contains(i, {x, 1}));
}

TEST_CASE("rotations cycle back to spawn") {
    for (int t = 0; t < kPieceTypeCount; ++t) {
        auto type = static_cast<PieceType>(t);
        CHECK(piece_minos(type, 4) == piece_minos(type, 0));
    }
}

TEST_CASE("I piece rotates into column 2 of its box when rotated clockwise") {
    const Minos& r = piece_minos(PieceType::I, 1);
    for (int y = 0; y < 4; ++y) CHECK(contains(r, {2, y}));
}

TEST_CASE("T rotation clockwise points right") {
    const Minos& r = piece_minos(PieceType::T, 1);
    CHECK(contains(r, {1, 0}));
    CHECK(contains(r, {1, 1}));
    CHECK(contains(r, {2, 1}));
    CHECK(contains(r, {1, 2}));
}

TEST_CASE("kick tables convert to y-down") {
    // JLSTZ 0->R second test is (-1, 0), third is (-1, +1) upwards => y = -1 in board space.
    auto k = srs_kicks(PieceType::T, 0, 1);
    REQUIRE(k.size() == 5);
    CHECK(k[0] == Vec2i{0, 0});
    CHECK(k[1] == Vec2i{-1, 0});
    CHECK(k[2] == Vec2i{-1, -1});
    CHECK(k[3] == Vec2i{0, 2});
    CHECK(k[4] == Vec2i{-1, 2});

    auto ik = srs_kicks(PieceType::I, 0, 1);
    CHECK(ik[1] == Vec2i{-2, 0});
    CHECK(ik[4] == Vec2i{1, -2});

    CHECK(srs_kicks(PieceType::O, 0, 1).size() == 1);
}

TEST_CASE("board clears full rows and compacts") {
    Board b;
    for (int x = 0; x < Board::kWidth; ++x) b.set(x, Board::kHeight - 1, 1);
    b.set(0, Board::kHeight - 2, 3);
    auto rows = b.clear_full_rows();
    REQUIRE(rows.size() == 1);
    CHECK(rows[0] == Board::kHeight - 1);
    CHECK(b.at(0, Board::kHeight - 1) == 3);
    CHECK(b.at(0, Board::kHeight - 2) == 0);
}

TEST_CASE("finesse minimum inputs") {
    // Spawn position needs no inputs.
    CHECK(finesse_min_inputs(PieceType::T, 0, 3, false) == 0);
    // Left wall: one DAS.
    CHECK(finesse_min_inputs(PieceType::T, 0, 0, false) == 1);
    // One tap right.
    CHECK(finesse_min_inputs(PieceType::T, 0, 4, false) == 1);
    // T flat-side-up (rot 2) at spawn x: two rotations without 180, one with.
    CHECK(finesse_min_inputs(PieceType::T, 2, 3, false) == 2);
    CHECK(finesse_min_inputs(PieceType::T, 2, 3, true) == 1);
    // O piece rotations are all equivalent.
    CHECK(finesse_min_inputs(PieceType::O, 2, 3, false) == 0);
    // Vertical I against the left wall: rotate + DAS.
    CHECK(finesse_min_inputs(PieceType::I, 3, -1, false) == 2);
}
