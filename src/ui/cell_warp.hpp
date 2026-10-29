#pragma once

namespace audrop::ui {

// How one block is drawn out of shape: scaled by sx/sy about its centre, then moved by
// (dx, dy) cells. Identity by default.
struct CellWarp {
    float sx = 1, sy = 1, dx = 0, dy = 0;
};

// Supplies per-block warps (the jelly animation); cells are in board coordinates.
class CellWarper {
public:
    virtual ~CellWarper() = default;
    virtual CellWarp locked(int x, int y) const = 0;
    virtual CellWarp active(int x, int y) const = 0;
};

}  // namespace audrop::ui
