#include "game/game.hpp"

#include <algorithm>

#include "game/finesse.hpp"

namespace audrop::game {

std::string_view mode_name(Mode m) {
    switch (m) {
        case Mode::Marathon: return "MARATHON";
        case Mode::Sprint: return "SPRINT";
        case Mode::Ultra: return "ULTRA";
        case Mode::Endless: return "ENDLESS";
    }
    return "?";
}

std::string_view mode_key(Mode m) {
    switch (m) {
        case Mode::Marathon: return "marathon";
        case Mode::Sprint: return "sprint";
        case Mode::Ultra: return "ultra";
        case Mode::Endless: return "endless";
    }
    return "unknown";
}

std::string_view mode_description(Mode m) {
    switch (m) {
        case Mode::Marathon: return "150 lines across 15 levels";
        case Mode::Sprint: return "Clear 40 lines as fast as you can";
        case Mode::Ultra: return "Score as much as you can in 2 minutes";
        case Mode::Endless: return "No goal. Ride the playlist";
    }
    return "";
}

Game::Game(Mode mode, Handling handling, std::uint64_t seed)
    : mode_(mode), handling_(handling), seed_(seed), rng_(seed) {
    stats_.level = 1;
    refill_queue();
    spawn(pop_next());
}

void Game::refill_queue() {
    while (queue_.size() < 14) {
        std::array<PieceType, kPieceTypeCount> bag{PieceType::I, PieceType::O, PieceType::T, PieceType::S,
                                                   PieceType::Z, PieceType::J, PieceType::L};
        std::ranges::shuffle(bag, rng_);
        queue_.insert(queue_.end(), bag.begin(), bag.end());
    }
}

PieceType Game::pop_next() {
    refill_queue();
    PieceType t = queue_.front();
    queue_.pop_front();
    return t;
}

std::vector<PieceType> Game::next_queue(int count) const {
    const auto n = std::min<std::size_t>(static_cast<std::size_t>(count), queue_.size());
    return {queue_.begin(), queue_.begin() + static_cast<std::ptrdiff_t>(n)};
}

bool Game::spawn(PieceType type) {
    ActivePiece p{type, 0, 3, Board::kFirstVisibleRow - 2};
    if (!board_.fits(p.type, p.rot, p.x, p.y)) {
        finish(false);  // block out
        return false;
    }
    active_ = p;
    // Guideline: the piece drops one row immediately if it can.
    if (board_.fits(p.type, p.rot, p.x, p.y + 1)) active_->y++;

    gravity_timer_ = 0;
    lock_timer_ = 0;
    lock_resets_ = 0;
    lowest_y_ = active_->y;
    last_was_rotation_ = false;
    last_kick_index_ = 0;
    soft_dropped_ = false;
    piece_inputs_ = 0;
    return true;
}

bool Game::grounded() const {
    return active_ && !board_.fits(active_->type, active_->rot, active_->x, active_->y + 1);
}

void Game::on_successful_shift() {
    if (lock_resets_ >= tuning::kMaxLockResets) return;
    lock_timer_ = 0;
    if (grounded()) ++lock_resets_;
}

bool Game::try_move(int dx, int dy) {
    if (!active_) return false;
    ActivePiece& p = *active_;
    if (!board_.fits(p.type, p.rot, p.x + dx, p.y + dy)) return false;
    p.x += dx;
    p.y += dy;
    if (dy > 0 && p.y > lowest_y_) {
        lowest_y_ = p.y;
        lock_resets_ = 0;
        lock_timer_ = 0;
    }
    if (dx != 0) {
        last_was_rotation_ = false;
        on_successful_shift();
    }
    return true;
}

bool Game::try_rotate(Rotation to) {
    if (!active_) return false;
    ActivePiece& p = *active_;
    const auto kicks = srs_kicks(p.type, p.rot, to);
    for (std::size_t i = 0; i < kicks.size(); ++i) {
        const Vec2i k = kicks[i];
        if (!board_.fits(p.type, to, p.x + k.x, p.y + k.y)) continue;
        p.x += k.x;
        p.y += k.y;
        p.rot = to;
        last_was_rotation_ = true;
        last_kick_index_ = static_cast<int>(i);
        if (p.y > lowest_y_) {
            lowest_y_ = p.y;
            lock_resets_ = 0;
            lock_timer_ = 0;
        } else {
            on_successful_shift();
        }
        return true;
    }
    return false;
}

void Game::press(Button b) {
    const auto bi = static_cast<std::size_t>(b);
    if (over_ || held_[bi]) return;
    held_[bi] = true;
    ++stats_.keys;
    if (!active_) return;

    switch (b) {
        case Button::Left:
        case Button::Right: {
            das_dir_ = b == Button::Left ? -1 : 1;
            das_timer_ = 0;
            arr_timer_ = 0;
            ++piece_inputs_;
            try_move(das_dir_, 0);
            break;
        }
        case Button::SoftDrop: break;  // handled by step_gravity
        case Button::HardDrop: hard_drop(); break;
        case Button::RotateCW:
            ++piece_inputs_;
            try_rotate(rotate_cw(active_->rot));
            break;
        case Button::RotateCCW:
            ++piece_inputs_;
            try_rotate(rotate_ccw(active_->rot));
            break;
        case Button::Rotate180:
            if (!handling_.allow_180) break;
            ++piece_inputs_;
            try_rotate(rotate_180(active_->rot));
            break;
        case Button::Hold: {
            if (hold_used_) break;
            const PieceType current = active_->type;
            ++stats_.holds;
            PieceType next = hold_ ? *hold_ : pop_next();
            hold_ = current;
            if (spawn(next)) hold_used_ = true;
            events_.emplace_back(HoldEvent{});
            break;
        }
        case Button::Count: break;
    }
}

void Game::release(Button b) {
    held_[static_cast<std::size_t>(b)] = false;
    auto release_dir = [&](int dir, Button other) {
        if (das_dir_ != dir) return;
        if (held_[static_cast<std::size_t>(other)]) {
            das_dir_ = -dir;
            das_timer_ = 0;
            arr_timer_ = 0;
        } else {
            das_dir_ = 0;
        }
    };
    if (b == Button::Left) release_dir(-1, Button::Right);
    if (b == Button::Right) release_dir(1, Button::Left);
}

void Game::step_horizontal(double dt) {
    if (das_dir_ == 0 || !active_) return;
    const double dt_ms = dt * 1000.0;
    const double before = das_timer_;
    das_timer_ += dt_ms;
    if (das_timer_ < handling_.das_ms) return;

    if (handling_.arr_ms <= 0) {
        while (try_move(das_dir_, 0)) {
        }
        return;
    }
    if (before < handling_.das_ms) {
        try_move(das_dir_, 0);
        arr_timer_ = das_timer_ - handling_.das_ms;
    } else {
        arr_timer_ += dt_ms;
    }
    while (arr_timer_ >= handling_.arr_ms) {
        if (!try_move(das_dir_, 0)) {
            arr_timer_ = 0;
            break;
        }
        arr_timer_ -= handling_.arr_ms;
    }
}

void Game::step_gravity(double dt) {
    if (!active_) return;
    double interval = gravity_seconds_per_row(stats_.level);
    const bool soft = held_[static_cast<std::size_t>(Button::SoftDrop)];
    if (soft) {
        if (handling_.soft_drop_factor >= tuning::kInstantSoftDropFactor) {
            int n = 0;
            while (try_move(0, 1)) ++n;
            if (n > 0) {
                stats_.score += n * tuning::kSoftDropPerCell;
                soft_dropped_ = true;
            }
            gravity_timer_ = 0;
            return;
        }
        interval /= handling_.soft_drop_factor;
    }

    gravity_timer_ += dt;
    while (gravity_timer_ >= interval) {
        gravity_timer_ -= interval;
        if (!try_move(0, 1)) {
            gravity_timer_ = 0;
            break;
        }
        if (soft) {
            stats_.score += tuning::kSoftDropPerCell;
            soft_dropped_ = true;
        }
    }
}

void Game::update(double dt) {
    if (over_) return;
    stats_.time += dt;
    if (mode_ == Mode::Ultra && stats_.time >= tuning::kUltraSeconds) {
        stats_.time = tuning::kUltraSeconds;
        finish(true);
        return;
    }
    if (!active_) return;

    step_horizontal(dt);
    step_gravity(dt);
    if (!active_) return;

    if (grounded()) {
        if (lock_resets_ >= tuning::kMaxLockResets) {
            lock_piece(false, 0);
            return;
        }
        lock_timer_ += dt;
        if (lock_timer_ >= tuning::kLockDelay) lock_piece(false, 0);
    } else {
        lock_timer_ = 0;
    }
}

void Game::hard_drop() {
    int n = 0;
    while (try_move(0, 1)) ++n;
    stats_.score += n * tuning::kHardDropPerCell;
    lock_piece(true, n);
}

TSpin Game::detect_tspin() const {
    if (!active_ || active_->type != PieceType::T || !last_was_rotation_) return TSpin::None;
    const ActivePiece& p = *active_;
    // Corners of the T's 3x3 box: 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right.
    const std::array<bool, 4> corner{
        board_.occupied(p.x, p.y),
        board_.occupied(p.x + 2, p.y),
        board_.occupied(p.x, p.y + 2),
        board_.occupied(p.x + 2, p.y + 2),
    };
    const int count = static_cast<int>(std::ranges::count(corner, true));
    if (count < 3) return TSpin::None;

    // The two corners on the side the T is pointing towards.
    constexpr std::array<std::array<int, 2>, 4> kFront{{{0, 1}, {1, 3}, {2, 3}, {0, 2}}};
    const auto& front = kFront[static_cast<std::size_t>(p.rot)];
    if (corner[static_cast<std::size_t>(front[0])] && corner[static_cast<std::size_t>(front[1])]) return TSpin::Full;
    // The fifth SRS kick test always upgrades to a full T-spin.
    if (last_kick_index_ == 4) return TSpin::Full;
    return TSpin::Mini;
}

int Game::level_for_lines(int lines) const {
    switch (mode_) {
        case Mode::Marathon: return std::min(tuning::kMarathonMaxLevel, 1 + lines / tuning::kLinesPerLevel);
        case Mode::Endless: return std::min(tuning::kEndlessMaxLevel, 1 + lines / tuning::kLinesPerLevel);
        case Mode::Sprint:
        case Mode::Ultra: return 1;
    }
    return 1;
}

void Game::lock_piece(bool hard, int drop_distance) {
    if (!active_) return;
    const ActivePiece p = *active_;
    const TSpin tspin = detect_tspin();

    std::array<Vec2i, 4> cells{};
    bool all_above = true;
    const Minos& minos = piece_minos(p.type, p.rot);
    for (std::size_t i = 0; i < 4; ++i) {
        cells[i] = {p.x + minos[i].x, p.y + minos[i].y};
        if (cells[i].y >= Board::kFirstVisibleRow) all_above = false;
    }
    board_.place(p.type, p.rot, p.x, p.y);
    active_.reset();
    ++stats_.pieces;
    events_.emplace_back(PieceLockedEvent{p.type, cells, hard, drop_distance});

    if (!soft_dropped_ && tspin == TSpin::None &&
        piece_inputs_ > finesse_min_inputs(p.type, p.rot, p.x, handling_.allow_180))
        ++stats_.finesse_faults;

    LineClearEvent clear;
    for (int y = 0; y < Board::kHeight; ++y) {
        if (!board_.row_full(y)) continue;
        clear.rows.push_back(y);
        Board::Row row{};
        for (int x = 0; x < Board::kWidth; ++x) row[static_cast<std::size_t>(x)] = board_.at(x, y);
        clear.contents.push_back(row);
    }
    board_.clear_full_rows();
    const int lines = static_cast<int>(clear.rows.size());

    if (lines == 0) {
        combo_ = -1;
        if (tspin != TSpin::None) {
            const ClearScore sc = score_clear({0, tspin, stats_.level, false, -1, false, 1.0});
            stats_.score += sc.total;
            if (tspin == TSpin::Full)
                ++stats_.tspins[0];
            else
                ++stats_.tspin_minis[0];
            events_.emplace_back(TSpinNoLinesEvent{tspin});
        }
    } else {
        ++combo_;
        const sync::SyncHit hit = judge_ ? judge_() : sync::SyncHit{};
        const double mult = hit.tier >= 0 ? tuning::kSyncTiers[static_cast<std::size_t>(hit.tier)].multiplier : 1.0;
        const bool all_clear = board_.empty();
        const ClearScore sc = score_clear({lines, tspin, stats_.level, b2b_chain_ > 0, combo_, all_clear, mult});

        b2b_chain_ = sc.difficult ? b2b_chain_ + 1 : 0;

        stats_.score += sc.total;
        stats_.lines += lines;
        ++stats_.clears[static_cast<std::size_t>(lines)];
        if (tspin == TSpin::Full) ++stats_.tspins[static_cast<std::size_t>(std::min(lines, 3))];
        if (tspin == TSpin::Mini) ++stats_.tspin_minis[static_cast<std::size_t>(std::min(lines, 2))];
        if (all_clear) ++stats_.all_clears;
        stats_.max_combo = std::max(stats_.max_combo, combo_);
        stats_.max_b2b = std::max(stats_.max_b2b, b2b_chain_ - 1);

        if (!hit.judged) {
            ++stats_.sync_unjudged;
        } else if (hit.tier >= 0) {
            ++stats_.sync_tiers[static_cast<std::size_t>(hit.tier)];
            ++stats_.sync_streak;
            stats_.max_sync_streak = std::max(stats_.max_sync_streak, stats_.sync_streak);
            stats_.sync_offset_sum += hit.offset_ms;
            stats_.sync_offset_sq_sum += hit.offset_ms * hit.offset_ms;
            ++stats_.sync_offset_count;
        } else {
            ++stats_.sync_misses;
            stats_.sync_streak = 0;
        }

        clear.lines = lines;
        clear.tspin = tspin;
        clear.b2b = sc.b2b_applied;
        clear.b2b_chain = std::max(0, b2b_chain_ - 1);
        clear.combo = combo_;
        clear.all_clear = all_clear;
        clear.score = sc.total;
        clear.sync = hit;
        clear.piece = p.type;
        events_.emplace_back(std::move(clear));

        const int new_level = level_for_lines(stats_.lines);
        if (new_level > stats_.level) {
            stats_.level = new_level;
            events_.emplace_back(LevelUpEvent{new_level});
        }
        if ((mode_ == Mode::Sprint && stats_.lines >= tuning::kSprintLines) ||
            (mode_ == Mode::Marathon && stats_.lines >= tuning::kMarathonLines)) {
            finish(true);
            return;
        }
    }

    if (lines == 0 && all_above) {
        finish(false);  // lock out
        return;
    }
    hold_used_ = false;
    spawn(pop_next());
}

void Game::finish(bool completed) {
    if (over_) return;
    over_ = true;
    completed_ = completed;
    active_.reset();
    events_.emplace_back(GameEndEvent{completed});
}

int Game::ghost_y() const {
    if (!active_) return 0;
    int y = active_->y;
    while (board_.fits(active_->type, active_->rot, active_->x, y + 1)) ++y;
    return y;
}

double Game::lock_progress() const {
    return grounded() ? std::clamp(lock_timer_ / tuning::kLockDelay, 0.0, 1.0) : 0.0;
}

double Game::time_remaining() const {
    return mode_ == Mode::Ultra ? std::max(0.0, tuning::kUltraSeconds - stats_.time) : 0.0;
}

int Game::lines_remaining() const {
    if (mode_ == Mode::Sprint) return std::max(0, tuning::kSprintLines - stats_.lines);
    if (mode_ == Mode::Marathon) return std::max(0, tuning::kMarathonLines - stats_.lines);
    return 0;
}

}  // namespace audrop::game
