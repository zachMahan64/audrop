#pragma once

#include <raylib.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "audio/ring_buffer.hpp"

namespace audrop::audio {

struct Track {
    std::filesystem::path path;
    std::string title;
};

bool is_supported_audio(const std::filesystem::path& p);
// Expands directories recursively, keeps supported files, sorted per directory.
std::vector<std::filesystem::path> collect_audio(const std::vector<std::filesystem::path>& inputs);
Track make_track(const std::filesystem::path& p);

// Streams the playlist on a dedicated pump thread so playback never stalls when the main
// loop blocks (file dialogs, window drags). Also taps decoded PCM for the visualizer and
// provides a smoothed playback clock.
class MusicPlayer {
public:
    MusicPlayer();
    ~MusicPlayer();
    MusicPlayer(const MusicPlayer&) = delete;
    MusicPlayer& operator=(const MusicPlayer&) = delete;

    // --- playlist ---
    void add_tracks(const std::vector<Track>& tracks);
    void remove_track(std::size_t index);
    void move_track(std::size_t index, int delta);
    void clear();
    std::vector<Track> tracks() const;
    std::size_t size() const;

    void set_shuffle(bool on);
    bool shuffle() const;
    void set_repeat(bool on);
    bool repeat() const;

    // --- transport ---
    void play_index(std::size_t index, double start_seconds = 0);
    void next();
    void previous();
    void set_paused(bool paused);
    bool paused() const;
    void set_volume(float v);
    // Plays an in-memory file (e.g. the calibration click track) on loop, suspending the playlist.
    void play_override(std::vector<unsigned char> data, const char* file_type, std::string title);
    void end_override();

    struct NowPlaying {
        bool valid = false;
        bool override = false;
        std::size_t index = 0;
        Track track;
        double length = 0;
        std::uint64_t generation = 0;  // bumps whenever a new track starts
    };
    NowPlaying now_playing() const;
    std::optional<Track> upcoming() const;

    // --- clock & PCM (main thread) ---
    void update_clock(double wall_now);
    double time() const { return clock_; }
    std::uint64_t generation() const { return generation_.load(); }
    void drain_pcm(std::vector<float>& out) { pcm_.drain(out); }

private:
    void pump();
    bool load_locked(std::size_t index, double start_seconds);
    bool start_from_locked();
    void unload_locked();
    void rebuild_order_locked(std::optional<std::size_t> first);
    std::optional<std::size_t> order_pos_of_locked(std::size_t index) const;
    static void processor(void* buffer, unsigned int frames);

    mutable std::mutex mutex_;
    std::vector<Track> tracks_;
    std::vector<std::size_t> order_;
    std::size_t order_pos_ = 0;
    bool shuffle_ = false;
    bool repeat_ = true;
    bool paused_ = false;
    float volume_ = 0.8f;
    std::mt19937 rng_{std::random_device{}()};

    Music music_{};
    bool loaded_ = false;
    bool override_ = false;
    std::vector<unsigned char> override_data_;
    std::string override_title_;
    std::optional<std::size_t> resume_index_;
    double resume_time_ = 0;
    std::size_t current_index_ = 0;
    int consecutive_failures_ = 0;

    std::atomic<std::uint64_t> generation_{0};
    std::atomic<double> raw_time_{0};
    std::atomic<double> raw_stamp_{0};
    std::atomic<bool> playing_{false};

    // Main-thread clock state.
    double clock_ = 0;
    double last_wall_ = 0;
    double last_raw_ = -1;
    std::uint64_t clock_generation_ = 0;

    SpscRing pcm_{1 << 16};
    std::atomic<bool> quit_{false};
    std::thread thread_;
};

}  // namespace audrop::audio
