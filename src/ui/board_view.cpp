#include "ui/board_view.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace audrop::ui {

using game::Board;

namespace {

// Shape of the rounded block styles (inset and corner roundness as fractions of the cell);
// ghosts reuse them so they match the pieces.
constexpr float kSoftInset = 0.07f, kSoftRound = 0.3f;
constexpr float kGelInset = 0.06f, kGelRound = 0.38f;

Rectangle inset_rect(Rectangle r, float fraction) {
    const float inset = std::max(1.0f, r.width * fraction);
    return {r.x + inset * 0.5f, r.y + inset * 0.5f, r.width - inset, r.height - inset};
}

// Landing preview for the active piece: tinted fill and piece-coloured outline, shaped like the theme's blocks.
void draw_ghost(const Theme& th, Rectangle r, Color c) {
    const Color fill = with_alpha(lerp(c, WHITE, th.ghost_whiten), th.ghost_alpha);
    const Color edge = with_alpha(c, 0.55f);
    const float thick = std::max(1.0f, r.width * 0.05f);
    if (th.blocks == BlockStyle::Glass) {
        DrawRectangleRec(r, fill);
        DrawRectangleLinesEx({r.x + 2, r.y + 2, r.width - 4, r.height - 4}, thick, edge);
        return;
    }
    const bool gel = th.blocks == BlockStyle::Gel;
    const Rectangle in = inset_rect(r, gel ? kGelInset : kSoftInset);
    const float round = gel ? kGelRound : kSoftRound;
    DrawRectangleRounded(in, round, 6, fill);
    DrawRectangleRoundedLinesEx(in, round, 6, thick, edge);
}

}  // namespace

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

void draw_block(const Theme& th, Rectangle r, Color c, float alpha) {
    switch (th.blocks) {
        case BlockStyle::Glass: {
            const float inset = std::max(1.0f, r.width * 0.04f);
            const Rectangle in{r.x + inset * 0.5f, r.y + inset * 0.5f, r.width - inset, r.height - inset};
            DrawRectangleRec(in, with_alpha(c, 0.72f * alpha));
            DrawRectangleGradientV(static_cast<int>(in.x), static_cast<int>(in.y), static_cast<int>(in.width),
                                   static_cast<int>(in.height * 0.55f), with_alpha(WHITE, 0.28f * alpha), with_alpha(WHITE, 0.0f));
            DrawRectangleLinesEx(in, std::max(1.0f, r.width * 0.07f), with_alpha(lerp(c, WHITE, 0.55f), 0.95f * alpha));
            break;
        }
        case BlockStyle::Soft: {
            // Opaque rounded tile on a soft shadow, with a pale sheen across the top.
            const Rectangle in = inset_rect(r, kSoftInset);
            const float round = kSoftRound, drop = std::max(1.0f, r.width * 0.06f);
            DrawRectangleRounded({in.x, in.y + drop, in.width, in.height}, round, 4, with_alpha(th.line, 0.16f * alpha));
            DrawRectangleRounded(in, round, 4, with_alpha(c, alpha));
            DrawRectangleRounded({in.x + in.width * 0.14f, in.y + in.height * 0.1f, in.width * 0.72f, in.height * 0.26f}, 1.0f, 4,
                                 with_alpha(WHITE, 0.4f * alpha));
            DrawRectangleRoundedLinesEx(in, round, 4, std::max(1.0f, r.width * 0.04f), with_alpha(lerp(c, th.line, 0.3f), 0.45f * alpha));
            break;
        }
        case BlockStyle::Gel: {
            // Jelly tile: saturated body, light pooling at the bottom, a specular cap on top.
            const Rectangle in = inset_rect(r, kGelInset);
            const float round = kGelRound;
            const auto ix = [](float v) { return static_cast<int>(v); };
            DrawRectangleRounded(in, round, 6, with_alpha(lerp(c, BLACK, 0.12f), alpha));
            DrawRectangleGradientV(ix(in.x + in.width * 0.12f), ix(in.y + in.height * 0.45f), ix(in.width * 0.76f),
                                   ix(in.height * 0.45f), with_alpha(c, 0.0f), with_alpha(lerp(c, WHITE, 0.55f), 0.85f * alpha));
            DrawRectangleRounded({in.x + in.width * 0.1f, in.y + in.height * 0.07f, in.width * 0.8f, in.height * 0.4f}, 0.8f, 6,
                                 with_alpha(WHITE, 0.32f * alpha));
            DrawRectangleRounded({in.x + in.width * 0.18f, in.y + in.height * 0.1f, in.width * 0.64f, in.height * 0.16f}, 1.0f, 6,
                                 with_alpha(WHITE, 0.45f * alpha));
            DrawRectangleRoundedLinesEx(in, round, 6, std::max(1.0f, r.width * 0.045f), with_alpha(lerp(c, BLACK, 0.45f), 0.75f * alpha));
            break;
        }
    }
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
    const Color c = th.piece(type);
    begin_glow(th);
    for (auto v : m)
        draw_glow(th, {ox + (static_cast<float>(v.x) + 0.5f) * cell, oy + (static_cast<float>(v.y) + 0.5f) * cell},
                  cell * 1.2f, with_alpha(c, 0.18f * alpha));
    end_glow();
    for (auto v : m)
        draw_block(th, {ox + static_cast<float>(v.x) * cell, oy + static_cast<float>(v.y) * cell, cell, cell}, c, alpha);
}

void draw_board(const Theme& th, viz::Compositor& comp, const game::Game& g, const BoardLayout& L, const BoardStyle& st) {
    const Rectangle& b = L.board;
    const float cell = L.cell;

    // Frosted glass well.
    comp.draw_frosted(b, with_alpha(th.frost, st.panel_opacity), th.panel_gloss);

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
        begin_glow(th);
        const Color gc = with_alpha(st.pulse_color, 0.12f * st.beat_pulse + 0.25f * st.clear_glow);
        DrawRectangleGradientH(static_cast<int>(b.x - cell * 0.6f), static_cast<int>(b.y), static_cast<int>(cell * 0.6f),
                               static_cast<int>(b.height), BLANK, gc);
        DrawRectangleGradientH(static_cast<int>(b.x + b.width), static_cast<int>(b.y), static_cast<int>(cell * 0.6f),
                               static_cast<int>(b.height), gc, BLANK);
        end_glow();
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
                cells.push_back({L.cell_rect(x, y), th.piece(game::cell_piece(v)), y < Board::kFirstVisibleRow ? 0.5f : 0.92f});

    if (const auto& a = g.active()) {
        const Color c = th.piece(a->type);
        const int gy = g.ghost_y();
        if (gy != a->y) {
            for (auto m : game::piece_minos(a->type, a->rot)) {
                const Rectangle r = L.cell_rect(a->x + m.x, gy + m.y);
                if (a->y + m.y < first_row) continue;
                draw_ghost(th, r, c);
            }
        }
        // Dim slightly as lock delay runs out.
        const float lock = static_cast<float>(g.lock_progress());
        for (auto m : game::piece_minos(a->type, a->rot)) {
            if (a->y + m.y < first_row) continue;
            cells.push_back({L.cell_rect(a->x + m.x, a->y + m.y), lerp(c, WHITE, 0.15f), 1.0f - 0.25f * lock});
        }
    }

    begin_glow(th);
    for (const auto& c : cells)
        draw_glow(th, {c.r.x + c.r.width / 2, c.r.y + c.r.height / 2}, cell * 1.15f, with_alpha(c.c, 0.13f * c.alpha));
    end_glow();
    for (const auto& c : cells) draw_block(th, c.r, c.c, c.alpha);
}

}  // namespace audrop::ui
