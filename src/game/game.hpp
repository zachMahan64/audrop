#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <random>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "game/board.hpp"
#include "game/piece.hpp"
#include "game/scoring.hpp"
#include "game/stats.hpp"
#include "sync/judge.hpp"

namespace audrop::game {

enum class Mode { Marathon, Sprint, Ultra, Endless };
inline constexpr std::array kAllModes{Mode::Marathon, Mode::Sprint, Mode::Ultra, Mode::Endless};
std::string_view mode_name(Mode m);
std::string_view mode_key(Mode m);  // stable identifier for save files
std::string_view mode_description(Mode m);

enum class Button { Left, Right, SoftDrop, HardDrop, RotateCW, RotateCCW, Rotate180, Hold, Count };

struct Handling {
    double das_ms = tuning::kDefaultDasMs;
    double arr_ms = tuning::kDefaultArrMs;
    double soft_drop_factor = tuning::kDefaultSoftDropFactor;
    bool allow_180 = false;
};

struct ActivePiece {
    PieceType type = PieceType::T;
    Rotation rot = 0;
    int x = 0;
    int y = 0;
};

// --- events emitted for presentation (effects, HUD, sounds) ---------------
struct PieceLockedEvent {
    PieceType type;
    std::array<Vec2i, 4> cells;  // board coordinates
    bool hard_drop;
    int drop_distance;           // rows travelled by the hard drop
};

struct LineClearEvent {
    std::vector<int> rows;            // board rows, pre-removal
    std::vector<Board::Row> contents; // cell values of those rows before removal
    int lines = 0;
    TSpin tspin = TSpin::None;
    bool b2b = false;
    int b2b_chain = 0;
    int combo = 0;
    bool all_clear = false;
    std::int64_t score = 0;
    sync::SyncHit sync;
    PieceType piece = PieceType::T;
};

struct TSpinNoLinesEvent {
    TSpin tspin;
};
struct HoldEvent {};
struct LevelUpEvent {
    int level;
};
struct GameEndEvent {
    bool completed;  // reached the mode goal (vs topped out)
};

using GameEvent = std::variant<PieceLockedEvent, LineClearEvent, TSpinNoLinesEvent, HoldEvent, LevelUpEvent, GameEndEvent>;

class Game {
public:
    Game(Mode mode, Handling handling, std::uint64_t seed);

    void press(Button b);
    void release(Button b);
    void update(double dt);

    // Called at the instant a line clear happens; returns the beat judgment.
    void set_sync_judge(std::function<sync::SyncHit()> judge) { judge_ = std::move(judge); }

    std::vector<GameEvent> take_events() { return std::exchange(events_, {}); }

    const Board& board() const { return board_; }
    const std::optional<ActivePiece>& active() const { return active_; }
    std::optional<PieceType> hold() const { return hold_; }
    bool hold_available() const { return !hold_used_; }
    std::vector<PieceType> next_queue(int count) const;
    int ghost_y() const;  // landing y for the active piece
    const RunStats& stats() const { return stats_; }
    Mode mode() const { return mode_; }
    bool over() const { return over_; }
    bool completed() const { return completed_; }
    int combo() const { return combo_; }
    int b2b_chain() const { return b2b_chain_; }
    double lock_progress() const;  // 0..1 while grounded
    double time_remaining() const;  // Ultra only
    int lines_remaining() const;    // Sprint / Marathon
    std::uint64_t seed() const { return seed_; }

private:
    void refill_queue();
    PieceType pop_next();
    bool spawn(PieceType type);
    bool try_move(int dx, int dy);
    bool try_rotate(Rotation to);
    void on_successful_shift();
    bool grounded() const;
    void hard_drop();
    void lock_piece(bool hard, int drop_distance);
    TSpin detect_tspin() const;
    int level_for_lines(int lines) const;
    void finish(bool completed);
    void step_horizontal(double dt);
    void step_gravity(double dt);

    Mode mode_;
    Handling handling_;
    std::uint64_t seed_;
    std::mt19937_64 rng_;
    Board board_;
    std::deque<PieceType> queue_;
    std::optional<ActivePiece> active_;
    std::optional<PieceType> hold_;
    bool hold_used_ = false;

    // Input state.
    std::array<bool, static_cast<std::size_t>(Button::Count)> held_{};
    int das_dir_ = 0;
    double das_timer_ = 0;
    double arr_timer_ = 0;

    // Piece state.
    double gravity_timer_ = 0;
    double lock_timer_ = 0;
    int lock_resets_ = 0;
    int lowest_y_ = 0;
    bool last_was_rotation_ = false;
    int last_kick_index_ = 0;
    bool soft_dropped_ = false;
    int piece_inputs_ = 0;  // moves + rotations for finesse

    // Run state.
    RunStats stats_;
    int combo_ = -1;
    int b2b_chain_ = 0;  // consecutive difficult clears
    bool over_ = false;
    bool completed_ = false;
    std::function<sync::SyncHit()> judge_;
    std::vector<GameEvent> events_;
};

}  // namespace audrop::game
