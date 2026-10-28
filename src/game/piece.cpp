#include "game/piece.hpp"

#include <string_view>

namespace audrop::game {
namespace {

struct ShapeDef {
    int size;
    std::string_view rows;  // spawn orientation, `size` rows of `size` chars, '#' = mino
};

constexpr std::array<ShapeDef, kPieceTypeCount> kShapes{{
    {4, "....####........"},  // I
    {4, ".##..##........."},  // O (centred in a 4-wide box like the guideline)
    {3, ".#.###..."},         // T
    {3, ".####...."},         // S
    {3, "##..##..."},         // Z
    {3, "#..###..."},         // J
    {3, "..####..."},         // L
}};

constexpr Minos spawn_minos(const ShapeDef& def) {
    Minos out{};
    int n = 0;
    for (int y = 0; y < def.size; ++y)
        for (int x = 0; x < def.size; ++x)
            if (def.rows[static_cast<std::size_t>(y * def.size + x)] == '#') out[static_cast<std::size_t>(n++)] = {x, y};
    return out;
}

// Rotating an NxN box clockwise maps (x, y) -> (N-1-y, x).
constexpr Minos rotated_cw(const Minos& m, int size) {
    Minos out{};
    for (std::size_t i = 0; i < m.size(); ++i) out[i] = {size - 1 - m[i].y, m[i].x};
    return out;
}

using RotationTable = std::array<std::array<Minos, 4>, kPieceTypeCount>;

constexpr RotationTable build_rotations() {
    RotationTable table{};
    for (std::size_t t = 0; t < kShapes.size(); ++t) {
        Minos m = spawn_minos(kShapes[t]);
        for (std::size_t r = 0; r < 4; ++r) {
            table[t][r] = m;
            // The O piece does not rotate in SRS.
            if (t != static_cast<std::size_t>(PieceType::O)) m = rotated_cw(m, kShapes[t].size);
        }
    }
    return table;
}

constexpr RotationTable kRotations = build_rotations();

// SRS kick tables as published, with +y = up. Converted to y-down on lookup.
using KickRow = std::array<Vec2i, 5>;

// Index: [from][to] for the eight 90-degree transitions.
constexpr KickRow kJlstzKicks[4][4] = {
    // from 0
    {{}, {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}}, {}, {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}}},
    // from R
    {{{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}}, {}, {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}}, {}},
    // from 2
    {{}, {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}}, {}, {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}}},
    // from L
    {{{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}}, {}, {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}}, {}},
};

constexpr KickRow kIKicks[4][4] = {
    // from 0
    {{}, {{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}}, {}, {{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}}},
    // from R
    {{{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}}, {}, {{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}}, {}},
    // from 2
    {{}, {{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}}, {}, {{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}}},
    // from L
    {{{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}}, {}, {{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}}, {}},
};

// 180-degree kicks (SRS+ style), indexed by `from`.
using Kick180Row = std::array<Vec2i, 6>;
constexpr Kick180Row k180Kicks[4] = {
    {{{0, 0}, {0, 1}, {1, 1}, {-1, 1}, {1, 0}, {-1, 0}}},     // 0 -> 2
    {{{0, 0}, {1, 0}, {1, 2}, {1, 1}, {0, 2}, {0, 1}}},       // R -> L
    {{{0, 0}, {0, -1}, {-1, -1}, {1, -1}, {-1, 0}, {1, 0}}},  // 2 -> 0
    {{{0, 0}, {-1, 0}, {-1, 2}, {-1, 1}, {0, 2}, {0, 1}}},    // L -> R
};

template <std::size_t N>
constexpr std::array<Vec2i, N> flip_y(const std::array<Vec2i, N>& in) {
    std::array<Vec2i, N> out{};
    for (std::size_t i = 0; i < N; ++i) out[i] = {in[i].x, -in[i].y};
    return out;
}

template <std::size_t N>
using FlippedTable = std::array<std::array<std::array<Vec2i, N>, 4>, 4>;

template <std::size_t N>
constexpr FlippedTable<N> flip_table(const std::array<Vec2i, N> (&in)[4][4]) {
    FlippedTable<N> out{};
    for (std::size_t a = 0; a < 4; ++a)
        for (std::size_t b = 0; b < 4; ++b) out[a][b] = flip_y(in[a][b]);
    return out;
}

constexpr auto kJlstzDown = flip_table(kJlstzKicks);
constexpr auto kIDown = flip_table(kIKicks);
constexpr std::array<Kick180Row, 4> k180Down{flip_y(k180Kicks[0]), flip_y(k180Kicks[1]), flip_y(k180Kicks[2]),
                                             flip_y(k180Kicks[3])};

constexpr Vec2i kNoKick[1] = {{0, 0}};

}  // namespace

const Minos& piece_minos(PieceType type, Rotation rot) {
    return kRotations[static_cast<std::size_t>(type)][static_cast<std::size_t>(rot & 3)];
}

int piece_box_size(PieceType type) { return kShapes[static_cast<std::size_t>(type)].size; }

std::string_view piece_name(PieceType type) {
    constexpr std::string_view names[] = {"I", "O", "T", "S", "Z", "J", "L"};
    return names[static_cast<std::size_t>(type)];
}

std::span<const Vec2i> srs_kicks(PieceType type, Rotation from, Rotation to) {
    from &= 3;
    to &= 3;
    if (type == PieceType::O || from == to) return kNoKick;
    if (((from + 2) & 3) == to) return k180Down[static_cast<std::size_t>(from)];
    const auto& table = type == PieceType::I ? kIDown : kJlstzDown;
    return table[static_cast<std::size_t>(from)][static_cast<std::size_t>(to)];
}

}  // namespace audrop::game
