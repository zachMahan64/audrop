#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "config/tuning.hpp"
#include "game/piece.hpp"

namespace audrop::game {

// The playfield. Row 0 is the top of the hidden buffer; rows
// [kHeight - kVisibleRows, kHeight) are visible. y grows downwards.
class Board {
public:
    static constexpr int kWidth = tuning::kBoardWidth;
    static constexpr int kHeight = tuning::kBoardHeight;
    static constexpr int kFirstVisibleRow = kHeight - tuning::kVisibleRows;

    using Row = std::array<std::uint8_t, kWidth>;

    std::uint8_t at(int x, int y) const { return rows_[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]; }
    void set(int x, int y, std::uint8_t v) { rows_[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = v; }

    // Out-of-bounds cells (walls, floor) count as occupied; above the top counts as empty.
    bool occupied(int x, int y) const;
    bool fits(PieceType type, Rotation rot, int px, int py) const;
    void place(PieceType type, Rotation rot, int px, int py);

    // Removes full rows and returns their indices (top to bottom, pre-removal coordinates).
    std::vector<int> clear_full_rows();
    bool row_full(int y) const;
    bool empty() const;
    void reset() { rows_ = {}; }

private:
    std::array<Row, kHeight> rows_{};
};

}  // namespace audrop::game
