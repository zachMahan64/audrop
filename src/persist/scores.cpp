#include "persist/scores.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <nlohmann/json.hpp>

#include "persist/paths.hpp"

namespace audrop::game {

void to_json(nlohmann::json& j, const RunStats& s) {
    j = nlohmann::json{
        {"score", s.score},
        {"lines", s.lines},
        {"level", s.level},
        {"time", s.time},
        {"pieces", s.pieces},
        {"keys", s.keys},
        {"holds", s.holds},
        {"finesse_faults", s.finesse_faults},
        {"clears", s.clears},
        {"tspins", s.tspins},
        {"tspin_minis", s.tspin_minis},
        {"all_clears", s.all_clears},
        {"max_combo", s.max_combo},
        {"max_b2b", s.max_b2b},
        {"sync_tiers", s.sync_tiers},
        {"sync_misses", s.sync_misses},
        {"sync_unjudged", s.sync_unjudged},
        {"max_sync_streak", s.max_sync_streak},
        {"sync_offset_sum", s.sync_offset_sum},
        {"sync_offset_sq_sum", s.sync_offset_sq_sum},
        {"sync_offset_count", s.sync_offset_count},
    };
}

void from_json(const nlohmann::json& j, RunStats& s) {
    auto get = [&](const char* k, auto& v) {
        if (auto it = j.find(k); it != j.end()) it->get_to(v);
    };
    get("score", s.score);
    get("lines", s.lines);
    get("level", s.level);
    get("time", s.time);
    get("pieces", s.pieces);
    get("keys", s.keys);
    get("holds", s.holds);
    get("finesse_faults", s.finesse_faults);
    get("clears", s.clears);
    get("tspins", s.tspins);
    get("tspin_minis", s.tspin_minis);
    get("all_clears", s.all_clears);
    get("max_combo", s.max_combo);
    get("max_b2b", s.max_b2b);
    get("sync_tiers", s.sync_tiers);
    get("sync_misses", s.sync_misses);
    get("sync_unjudged", s.sync_unjudged);
    get("max_sync_streak", s.max_sync_streak);
    get("sync_offset_sum", s.sync_offset_sum);
    get("sync_offset_sq_sum", s.sync_offset_sq_sum);
    get("sync_offset_count", s.sync_offset_count);
}

}  // namespace audrop::game

namespace audrop::persist {
namespace {

using nlohmann::json;

std::filesystem::path scores_path() { return data_dir() / "scores.json"; }

json entry_to_json(const ScoreEntry& e) {
    return json{{"mode", e.mode}, {"completed", e.completed}, {"date", e.date}, {"tracks", e.tracks}, {"stats", e.stats}};
}

ScoreEntry entry_from_json(const json& j) {
    ScoreEntry e;
    e.mode = j.value("mode", "");
    e.completed = j.value("completed", false);
    e.date = j.value("date", "");
    if (auto it = j.find("tracks"); it != j.end()) it->get_to(e.tracks);
    if (auto it = j.find("stats"); it != j.end()) it->get_to(e.stats);
    return e;
}

std::optional<game::Mode> mode_from_key(std::string_view key) {
    for (auto m : game::kAllModes)
        if (game::mode_key(m) == key) return m;
    return std::nullopt;
}

}  // namespace

std::string now_iso8601() {
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

bool ScoreBook::better(game::Mode mode, const ScoreEntry& a, const ScoreEntry& b) {
    if (mode == game::Mode::Sprint) {
        if (a.completed != b.completed) return a.completed;
        if (a.completed) return a.stats.time < b.stats.time;
        return a.stats.lines > b.stats.lines;
    }
    return a.stats.score > b.stats.score;
}

void ScoreBook::load() {
    tables_.clear();
    lifetime_ = {};
    auto text = read_file(scores_path());
    if (!text) return;
    const json j = json::parse(*text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return;

    try {
        if (auto it = j.find("modes"); it != j.end() && it->is_object()) {
            for (const auto& [key, list] : it->items()) {
                auto& table = tables_[key];
                for (const auto& e : list) table.push_back(entry_from_json(e));
            }
        }
        if (auto it = j.find("lifetime"); it != j.end()) {
            const json& l = *it;
            lifetime_.games = l.value("games", 0);
            lifetime_.play_time = l.value("play_time", 0.0);
            lifetime_.lines = l.value("lines", std::int64_t{0});
            lifetime_.pieces = l.value("pieces", std::int64_t{0});
            lifetime_.tetrises = l.value("tetrises", std::int64_t{0});
            lifetime_.tspins = l.value("tspins", std::int64_t{0});
            lifetime_.all_clears = l.value("all_clears", std::int64_t{0});
            if (auto t = l.find("sync_tiers"); t != l.end()) t->get_to(lifetime_.sync_tiers);
            lifetime_.sync_misses = l.value("sync_misses", std::int64_t{0});
            lifetime_.best_sync_streak = l.value("best_sync_streak", 0);
            lifetime_.best_combo = l.value("best_combo", 0);
        }
    } catch (const json::exception&) {
        // Corrupt file: keep whatever loaded so far.
    }
}

void ScoreBook::save() const {
    json modes = json::object();
    for (const auto& [key, list] : tables_) {
        json arr = json::array();
        for (const auto& e : list) arr.push_back(entry_to_json(e));
        modes[key] = std::move(arr);
    }
    const json j{
        {"version", 1},
        {"modes", modes},
        {"lifetime",
         {
             {"games", lifetime_.games},
             {"play_time", lifetime_.play_time},
             {"lines", lifetime_.lines},
             {"pieces", lifetime_.pieces},
             {"tetrises", lifetime_.tetrises},
             {"tspins", lifetime_.tspins},
             {"all_clears", lifetime_.all_clears},
             {"sync_tiers", lifetime_.sync_tiers},
             {"sync_misses", lifetime_.sync_misses},
             {"best_sync_streak", lifetime_.best_sync_streak},
             {"best_combo", lifetime_.best_combo},
         }},
    };
    write_file_atomic(scores_path(), j.dump(2));
}

int ScoreBook::submit(const ScoreEntry& entry) {
    const auto& s = entry.stats;
    ++lifetime_.games;
    lifetime_.play_time += s.time;
    lifetime_.lines += s.lines;
    lifetime_.pieces += s.pieces;
    lifetime_.tetrises += s.clears[4];
    lifetime_.tspins += s.total_tspins();
    lifetime_.all_clears += s.all_clears;
    for (std::size_t i = 0; i < lifetime_.sync_tiers.size(); ++i) lifetime_.sync_tiers[i] += s.sync_tiers[i];
    lifetime_.sync_misses += s.sync_misses;
    lifetime_.best_sync_streak = std::max(lifetime_.best_sync_streak, s.max_sync_streak);
    lifetime_.best_combo = std::max(lifetime_.best_combo, s.max_combo);

    const auto mode = mode_from_key(entry.mode);
    if (!mode) return -1;
    // Unfinished sprints and empty runs don't belong on the board.
    if (*mode == game::Mode::Sprint && !entry.completed) return -1;
    if (s.score <= 0 && s.lines <= 0) return -1;

    auto& table = tables_[entry.mode];
    auto pos = std::ranges::find_if(table, [&](const ScoreEntry& e) { return better(*mode, entry, e); });
    const auto rank = static_cast<int>(pos - table.begin());
    if (rank >= kScoresPerMode) return -1;
    table.insert(pos, entry);
    if (table.size() > static_cast<std::size_t>(kScoresPerMode)) table.resize(kScoresPerMode);
    return rank;
}

const std::vector<ScoreEntry>& ScoreBook::top(game::Mode mode) const {
    static const std::vector<ScoreEntry> empty;
    auto it = tables_.find(game::mode_key(mode));
    return it == tables_.end() ? empty : it->second;
}

}  // namespace audrop::persist
