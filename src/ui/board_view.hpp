#pragma once

#include <raylib.h>

#include "game/game.hpp"
#include "ui/theme.hpp"
#include "viz/compositor.hpp"

namespace audrop::ui {

struct BoardLayout {
    Rectangle board{};  // visible 10 x 20 well
    float cell = 0;
    Rectangle left{};   // hold + mode info column
    Rectangle right{};  // next queue + score column
    Rectangle lane{};   // beat lane under the board

    Vector2 cell_pos(int x, int y) const;  // top-left of board cell (y in board rows)
    Rectangle cell_rect(int x, int y) const;
};

// With `snap`, the cell size is rounded down to a multiple of 8 so pixel-art blocks get whole
// fat pixels (cell / 8).
BoardLayout compute_layout(float width, float height, bool snap = false);

struct BoardStyle {
    float panel_opacity = 0.6f;  // frosted well tint strength
    float beat_pulse = 0;   // 0..1, decays after each beat
    float clear_glow = 0;   // 0..1, after big clears
    Color pulse_color = WHITE;
};

void draw_block(const Theme& th, Rectangle r, Color c, float alpha, game::PieceType type);
// Panel background in the theme's style: frosted (or translucent) fill plus gloss or a chunky
// border. `unit` is the border's pixel size (one fat pixel for pixel-art themes).
void draw_panel_bg(const Theme& th, viz::Compositor& comp, Rectangle r, float opacity, float unit);
// Chunky two-tone frame just outside `r` (boxed themes), its inner edge in `light`.
void draw_box(const Theme& th, Rectangle r, float unit, Color light);
void draw_piece_icon(const Theme& th, game::PieceType type, Vector2 center, float cell, float alpha);
void draw_board(const Theme& th, viz::Compositor& comp, const game::Game& g, const BoardLayout& L, const BoardStyle& st);

}  // namespace audrop::ui
