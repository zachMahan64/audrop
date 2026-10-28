#include "game/board.hpp"

#include <algorithm>

namespace audrop::game {

bool Board::occupied(int x, int y) const {
    if (x < 0 || x >= kWidth || y >= kHeight) return true;
    if (y < 0) return false;
    return at(x, y) != 0;
}

bool Board::fits(PieceType type, Rotation rot, int px, int py) const {
    for (Vec2i m : piece_minos(type, rot))
        if (occupied(px + m.x, py + m.y)) return false;
    return true;
}

void Board::place(PieceType type, Rotation rot, int px, int py) {
    for (Vec2i m : piece_minos(type, rot)) {
        const int x = px + m.x, y = py + m.y;
        if (x >= 0 && x < kWidth && y >= 0 && y < kHeight) set(x, y, cell_value(type));
    }
}

bool Board::row_full(int y) const {
    const Row& r = rows_[static_cast<std::size_t>(y)];
    return std::ranges::all_of(r, [](std::uint8_t c) { return c != 0; });
}

bool Board::empty() const {
    return std::ranges::all_of(rows_, [](const Row& r) { return std::ranges::all_of(r, [](auto c) { return c == 0; }); });
}

std::vector<int> Board::clear_full_rows() {
    std::vector<int> cleared;
    for (int y = 0; y < kHeight; ++y)
        if (row_full(y)) cleared.push_back(y);
    if (cleared.empty()) return cleared;

    // Compact non-full rows downward.
    int write = kHeight - 1;
    for (int read = kHeight - 1; read >= 0; --read) {
        if (row_full(read)) continue;
        if (write != read) rows_[static_cast<std::size_t>(write)] = rows_[static_cast<std::size_t>(read)];
        --write;
    }
    for (; write >= 0; --write) rows_[static_cast<std::size_t>(write)] = Row{};
    return cleared;
}

}  // namespace audrop::game
