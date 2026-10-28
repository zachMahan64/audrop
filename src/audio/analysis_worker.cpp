#include "audio/analysis_worker.hpp"

#include <raylib.h>

#include "audio/beat_tracker.hpp"
#include "persist/beat_cache.hpp"
#include "persist/paths.hpp"

namespace audrop::audio {
namespace {

constexpr int kAnalysisRate = 22050;

std::optional<sync::BeatGrid> analyze(const std::filesystem::path& path, const std::atomic<bool>& cancel) {
    const std::string p = persist::path_to_utf8(path);
    Wave wave = LoadWave(p.c_str());
    if (!IsWaveValid(wave)) return std::nullopt;
    WaveFormat(&wave, kAnalysisRate, 32, 1);
    float* samples = LoadWaveSamples(wave);
    const std::size_t count = wave.frameCount;
    UnloadWave(wave);
    if (!samples) return std::nullopt;
    auto grid = track_beats({samples, count}, kAnalysisRate, &cancel);
    UnloadWaveSamples(samples);
    return grid;
}

}  // namespace

AnalysisWorker::AnalysisWorker() : thread_([this] { run(); }) {}

AnalysisWorker::~AnalysisWorker() {
    quit_ = true;
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void AnalysisWorker::request(const std::filesystem::path& path, bool urgent) {
    {
        std::lock_guard lock(mutex_);
        auto it = results_.find(path);
        if (it != results_.end() && it->second.state != State::Pending) return;
        if (it == results_.end()) results_[path].state = State::Pending;
        std::erase(queue_, path);
        if (urgent) queue_.push_front(path);
        else queue_.push_back(path);
    }
    cv_.notify_one();
}

AnalysisWorker::Result AnalysisWorker::get(const std::filesystem::path& path) const {
    std::lock_guard lock(mutex_);
    auto it = results_.find(path);
    return it == results_.end() ? Result{} : it->second;
}

void AnalysisWorker::set_adjust(const std::filesystem::path& path, const sync::BeatAdjust& adjust) {
    persist::CachedBeats cached;
    std::uint64_t hash = 0;
    {
        std::lock_guard lock(mutex_);
        auto it = results_.find(path);
        if (it == results_.end() || it->second.state != State::Ready) return;
        it->second.adjust = adjust;
        cached = {it->second.raw, adjust};
        hash = it->second.hash;
    }
    ++revision_;
    persist::save_beats(hash, cached);
}

void AnalysisWorker::run() {
    while (!quit_) {
        std::filesystem::path path;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return quit_ || !queue_.empty(); });
            if (quit_) return;
            path = queue_.front();
            queue_.pop_front();
        }

        Result result;
        result.state = State::Failed;
        if (auto hash = persist::hash_file(path)) {
            result.hash = *hash;
            if (auto cached = persist::load_beats(*hash)) {
                result.raw = std::move(cached->grid);
                result.adjust = cached->adjust;
                result.state = State::Ready;
            } else if (auto grid = analyze(path, quit_)) {
                if (quit_) return;
                result.raw = std::move(*grid);
                result.state = State::Ready;
                persist::save_beats(*hash, {result.raw, {}});
                TraceLog(LOG_INFO, "audrop: analysed %s: %.1f BPM, confidence %.2f, %zu beats",
                         persist::path_to_utf8(path.filename()).c_str(), result.raw.bpm, result.raw.confidence,
                         result.raw.beats.size());
            }
        }
        {
            std::lock_guard lock(mutex_);
            results_[path] = std::move(result);
        }
        ++revision_;
    }
}

}  // namespace audrop::audio
