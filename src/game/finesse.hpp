#pragma once

#include "game/piece.hpp"

namespace audrop::game {

// Minimum number of inputs (taps, DAS charges, rotations) needed to bring a freshly spawned
// piece to the given final position on an empty board. Positions with the same footprint
// (e.g. S rotated 0 vs 2) are treated as equivalent.
int finesse_min_inputs(PieceType type, Rotation final_rot, int final_x, bool allow_180);

}  // namespace audrop::game
