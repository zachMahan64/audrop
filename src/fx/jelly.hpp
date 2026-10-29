#pragma once

#include <optional>
#include <vector>

#include "game/game.hpp"
#include "ui/cell_warp.hpp"

namespace audrop::fx {

// Squash-and-stretch for the jelly themes. Landed pieces squish into the stack and wobble back,
// the impact ripples into nearby blocks (each block rests on the squashed ones below it), and
// the falling piece trails behind sideways moves and pops on rotation. Only changes how blocks
// are drawn; tuning lives in config/tuning.hpp.
class Jelly : public ui::CellWarper {
public:
    void reset();
    void on_lock(const game::PieceLockedEvent& e);
    void on_clear(const game::LineClearEvent& e);
    // Watches the falling piece for moves and rotations; call once per frame before drawing.
    void track(const game::Game& g);
    void update(float dt);

    ui::CellWarp locked(int x, int y) const override;
    ui::CellWarp active(int x, int y) const override;

private:
    struct Impact {
        std::vector<game::Vec2i> cells;
        float strength;
        float age = 0;
    };
    // Damped spring pulled towards 0.
    struct Spring {
        float x = 0, v = 0;
        void step(float dt);
    };

    float squash(int x, int y) const;  // > 0 flattened, < 0 stretched tall

    std::vector<Impact> impacts_;
    Spring lag_;  // sideways offset of the falling piece (cells)
    Spring pop_;  // its scale - 1
    std::optional<game::ActivePiece> last_;
    int last_key_ = -1;  // changes when a new piece spawns or one is held
    float center_x_ = 0, center_y_ = 0;  // falling piece's centre in board cells
};

}  // namespace audrop::fx
