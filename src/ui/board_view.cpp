#include "ui/board_view.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace audrop::ui {

using game::Board;

Vector2 BoardLayout::cell_pos(int x, int y) const {
    return {board.x + static_cast<float>(x) * cell, board.y + static_cast<float>(y - Board::kFirstVisibleRow) * cell};
}

Rectangle BoardLayout::cell_rect(int x, int y) const {
    const Vector2 p = cell_pos(x, y);
    return {p.x, p.y, cell, cell};
}

BoardLayout compute_layout(float width, float height) {
    BoardLayout L;
    const float rows = static_cast<float>(tuning::kVisibleRows);
    // Board (10) + two side columns (6 each) + gaps (1 each) = 24 cells wide.
    L.cell = std::floor(std::min(height * 0.8f / (rows + 1.5f), width * 0.94f / 24.0f));
    const float bw = L.cell * Board::kWidth, bh = L.cell * rows;
    L.board = {std::floor((width - bw) / 2), std::floor((height - bh) / 2 - L.cell * 0.5f), bw, bh};
    L.left = {L.board.x - L.cell * 7, L.board.y, L.cell * 6, bh};
    L.right = {L.board.x + bw + L.cell, L.board.y, L.cell * 6, bh};
    L.lane = {L.board.x, L.board.y + bh + L.cell * 0.45f, bw, L.cell * 0.9f};
    return L;
}

void draw_block(Rectangle r, Color c, float alpha) {
    const float inset = std::max(1.0f, r.width * 0.04f);
    const Rectangle in{r.x + inset * 0.5f, r.y + inset * 0.5f, r.width - inset, r.height - inset};
    DrawRectangleRec(in, with_alpha(c, 0.72f * alpha));
    DrawRectangleGradientV(static_cast<int>(in.x), static_cast<int>(in.y), static_cast<int>(in.width),
                           static_cast<int>(in.height * 0.55f), with_alpha(WHITE, 0.28f * alpha), with_alpha(WHITE, 0.0f));
    DrawRectangleLinesEx(in, std::max(1.0f, r.width * 0.07f), with_alpha(lerp(c, WHITE, 0.55f), 0.95f * alpha));
}

void draw_piece_icon(const Theme& th, game::PieceType type, Vector2 center, float cell, float alpha) {
    const auto& m = game::piece_minos(type, 0);
    float minx = 9, maxx = -9, miny = 9, maxy = -9;
    for (auto v : m) {
        minx = std::min(minx, static_cast<float>(v.x));
        maxx = std::max(maxx, static_cast<float>(v.x));
        miny = std::min(miny, static_cast<float>(v.y));
        maxy = std::max(maxy, static_cast<float>(v.y));
    }
    const float ox = center.x - (maxx - minx + 1) * cell / 2 - minx * cell;
    const float oy = center.y - (maxy - miny + 1) * cell / 2 - miny * cell;
    const Color c = piece_color(type);
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto v : m)
        draw_glow(th, {ox + (static_cast<float>(v.x) + 0.5f) * cell, oy + (static_cast<float>(v.y) + 0.5f) * cell},
                  cell * 1.2f, with_alpha(c, 0.18f * alpha));
    EndBlendMode();
    for (auto v : m)
        draw_block({ox + static_cast<float>(v.x) * cell, oy + static_cast<float>(v.y) * cell, cell, cell}, c, alpha);
}

void draw_board(const Theme& th, viz::Compositor& comp, const game::Game& g, const BoardLayout& L, const BoardStyle& st) {
    const Rectangle& b = L.board;
    const float cell = L.cell;

    // Frosted glass well.
    comp.draw_frosted(b, st.panel_darkness);

    // Grid.
    const Color grid = with_alpha(th.line, 0.05f + 0.04f * st.beat_pulse);
    for (int x = 1; x < Board::kWidth; ++x)
        DrawLineEx({b.x + static_cast<float>(x) * cell, b.y}, {b.x + static_cast<float>(x) * cell, b.y + b.height}, 1, grid);
    for (int y = 1; y < tuning::kVisibleRows; ++y)
        DrawLineEx({b.x, b.y + static_cast<float>(y) * cell}, {b.x + b.width, b.y + static_cast<float>(y) * cell}, 1, grid);

    // Well edges (open top), glowing on the beat.
    const float edge_a = 0.45f + 0.45f * st.beat_pulse + 0.4f * st.clear_glow;
    const Color edge = with_alpha(lerp(th.line, st.pulse_color, std::min(1.0f, st.beat_pulse + st.clear_glow)), std::min(1.0f, edge_a));
    const float thick = std::max(2.0f, cell * 0.06f);
    DrawLineEx({b.x - thick / 2, b.y}, {b.x - thick / 2, b.y + b.height + thick / 2}, thick, edge);
    DrawLineEx({b.x + b.width + thick / 2, b.y}, {b.x + b.width + thick / 2, b.y + b.height + thick / 2}, thick, edge);
    DrawLineEx({b.x - thick, b.y + b.height + thick / 2}, {b.x + b.width + thick, b.y + b.height + thick / 2}, thick, edge);
    if (st.beat_pulse + st.clear_glow > 0.01f) {
        BeginBlendMode(BLEND_ADDITIVE);
        const Color gc = with_alpha(st.pulse_color, 0.12f * st.beat_pulse + 0.25f * st.clear_glow);
        DrawRectangleGradientH(static_cast<int>(b.x - cell * 0.6f), static_cast<int>(b.y), static_cast<int>(cell * 0.6f),
                               static_cast<int>(b.height), BLANK, gc);
        DrawRectangleGradientH(static_cast<int>(b.x + b.width), static_cast<int>(b.y), static_cast<int>(cell * 0.6f),
                               static_cast<int>(b.height), gc, BLANK);
        EndBlendMode();
    }

    // Collect cells to draw: locked, ghost, active.
    struct Cell {
        Rectangle r;
        Color c;
        float alpha;
    };
    std::vector<Cell> cells;
    const auto& board = g.board();
    const int first_row = Board::kFirstVisibleRow - 2;
    for (int y = first_row; y < Board::kHeight; ++y)
        for (int x = 0; x < Board::kWidth; ++x)
            if (auto v = board.at(x, y))
                cells.push_back({L.cell_rect(x, y), piece_color(game::cell_piece(v)), y < Board::kFirstVisibleRow ? 0.5f : 0.92f});

    if (const auto& a = g.active()) {
        const Color c = piece_color(a->type);
        const int gy = g.ghost_y();
        if (gy != a->y) {
            for (auto m : game::piece_minos(a->type, a->rot)) {
                const Rectangle r = L.cell_rect(a->x + m.x, gy + m.y);
                if (a->y + m.y < first_row) continue;
                DrawRectangleRec(r, with_alpha(c, 0.10f));
                DrawRectangleLinesEx({r.x + 2, r.y + 2, r.width - 4, r.height - 4}, std::max(1.0f, cell * 0.05f), with_alpha(c, 0.55f));
            }
        }
        // Dim slightly as lock delay runs out.
        const float lock = static_cast<float>(g.lock_progress());
        for (auto m : game::piece_minos(a->type, a->rot)) {
            if (a->y + m.y < first_row) continue;
            cells.push_back({L.cell_rect(a->x + m.x, a->y + m.y), lerp(c, WHITE, 0.15f), 1.0f - 0.25f * lock});
        }
    }

    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& c : cells)
        draw_glow(th, {c.r.x + c.r.width / 2, c.r.y + c.r.height / 2}, cell * 1.15f, with_alpha(c.c, 0.13f * c.alpha));
    EndBlendMode();
    for (const auto& c : cells) draw_block(c.r, c.c, c.alpha);
}

}  // namespace audrop::ui
