#include "game/finesse.hpp"

#include <algorithm>
#include <array>
#include <queue>

#include "game/board.hpp"

namespace audrop::game {
namespace {

constexpr int kProbeY = 20;  // any height well clear of the floor works on an empty board
constexpr int kSpawnX = 3;

using Footprint = std::array<Vec2i, 4>;

Footprint footprint(PieceType type, Rotation rot, int x) {
    const Minos& m = piece_minos(type, rot);
    int min_y = m[0].y;
    for (Vec2i v : m) min_y = std::min(min_y, v.y);
    Footprint f{};
    for (std::size_t i = 0; i < 4; ++i) f[i] = {x + m[i].x, m[i].y - min_y};
    std::ranges::sort(f, [](Vec2i a, Vec2i b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
    return f;
}

}  // namespace

int finesse_min_inputs(PieceType type, Rotation final_rot, int final_x, bool allow_180) {
    static const Board empty_board;
    const Footprint target = footprint(type, final_rot, final_x);

    // State: x in [-3, 12] (box origin can go negative), rotation 0..3.
    constexpr int kXOff = 4, kXRange = 18;
    std::array<int, kXRange * 4> dist;
    dist.fill(-1);
    auto key = [](int x, Rotation r) { return (x + kXOff) * 4 + r; };

    std::queue<std::pair<int, Rotation>> q;
    dist[static_cast<std::size_t>(key(kSpawnX, 0))] = 0;
    q.push({kSpawnX, 0});

    while (!q.empty()) {
        auto [x, r] = q.front();
        q.pop();
        const int d = dist[static_cast<std::size_t>(key(x, r))];
        if (footprint(type, r, x) == target) return d;

        auto visit = [&](int nx, Rotation nr) {
            if (nx + kXOff < 0 || nx + kXOff >= kXRange) return;
            auto& slot = dist[static_cast<std::size_t>(key(nx, nr))];
            if (slot >= 0) return;
            slot = d + 1;
            q.push({nx, nr});
        };

        for (int dir : {-1, 1}) {
            if (empty_board.fits(type, r, x + dir, kProbeY)) visit(x + dir, r);
            int wall = x;
            while (empty_board.fits(type, r, wall + dir, kProbeY)) wall += dir;
            if (wall != x) visit(wall, r);
        }

        auto try_rotate = [&](Rotation to) {
            for (Vec2i k : srs_kicks(type, r, to)) {
                if (empty_board.fits(type, to, x + k.x, kProbeY + k.y)) {
                    visit(x + k.x, to);
                    return;
                }
            }
        };
        try_rotate(rotate_cw(r));
        try_rotate(rotate_ccw(r));
        if (allow_180) try_rotate(rotate_180(r));
    }
    return 0;  // unreachable on an empty board
}

}  // namespace audrop::game
