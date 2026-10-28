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

BoardLayout compute_layout(float width, float height);

struct BoardStyle {
    float panel_opacity = 0.6f;  // frosted well tint strength
    float beat_pulse = 0;   // 0..1, decays after each beat
    float clear_glow = 0;   // 0..1, after big clears
    Color pulse_color = WHITE;
};

void draw_block(const Theme& th, Rectangle r, Color c, float alpha);
void draw_piece_icon(const Theme& th, game::PieceType type, Vector2 center, float cell, float alpha);
void draw_board(const Theme& th, viz::Compositor& comp, const game::Game& g, const BoardLayout& L, const BoardStyle& st);

}  // namespace audrop::ui
