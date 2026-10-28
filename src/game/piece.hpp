#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace audrop::game {

enum class PieceType : std::uint8_t { I, O, T, S, Z, J, L };
inline constexpr int kPieceTypeCount = 7;

// 0 = spawn, 1 = R (clockwise), 2 = 180, 3 = L (counter-clockwise)
using Rotation = int;

struct Vec2i {
    int x = 0;
    int y = 0;
    friend constexpr bool operator==(Vec2i, Vec2i) = default;
};

// Mino offsets within the piece's bounding box. y grows downwards (row 0 = top of box).
using Minos = std::array<Vec2i, 4>;

const Minos& piece_minos(PieceType type, Rotation rot);
int piece_box_size(PieceType type);
std::string_view piece_name(PieceType type);

// SRS wall kicks for a rotation from `from` to `to`, in board coordinates (y down).
// The first entry is always {0, 0}.
std::span<const Vec2i> srs_kicks(PieceType type, Rotation from, Rotation to);

constexpr Rotation rotate_cw(Rotation r) { return (r + 1) & 3; }
constexpr Rotation rotate_ccw(Rotation r) { return (r + 3) & 3; }
constexpr Rotation rotate_180(Rotation r) { return (r + 2) & 3; }

// Board cell value for a locked piece. 0 means empty.
constexpr std::uint8_t cell_value(PieceType t) { return static_cast<std::uint8_t>(t) + 1; }
constexpr PieceType cell_piece(std::uint8_t v) { return static_cast<PieceType>(v - 1); }

}  // namespace audrop::game
