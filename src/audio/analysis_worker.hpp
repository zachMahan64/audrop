#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <thread>

#include "sync/beat_grid.hpp"

namespace audrop::audio {

// Runs beat tracking on a background thread and caches results on disk (keyed by file hash).
class AnalysisWorker {
public:
    AnalysisWorker();
    ~AnalysisWorker();
    AnalysisWorker(const AnalysisWorker&) = delete;
    AnalysisWorker& operator=(const AnalysisWorker&) = delete;

    enum class State { Missing, Pending, Ready, Failed };
    struct Result {
        State state = State::Missing;
        sync::BeatGrid raw;
        sync::BeatAdjust adjust;
        std::uint64_t hash = 0;
    };

    // Queues `path` (no-op if already known). `urgent` jumps the queue.
    void request(const std::filesystem::path& path, bool urgent = false);
    Result get(const std::filesystem::path& path) const;
    // Stores a player correction for a track and persists it.
    void set_adjust(const std::filesystem::path& path, const sync::BeatAdjust& adjust);
    // Bumps whenever any result changes, so callers can cheaply poll for updates.
    std::uint64_t revision() const { return revision_.load(); }

private:
    void run();

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::filesystem::path> queue_;
    std::map<std::filesystem::path, Result> results_;
    std::atomic<std::uint64_t> revision_{0};
    std::atomic<bool> quit_{false};
    std::thread thread_;
};

}  // namespace audrop::audio
