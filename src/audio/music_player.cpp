#include "audio/music_player.hpp"

#include <algorithm>
#include <cctype>
#include <numeric>

#include "app/loop_clock.hpp"
#include "persist/paths.hpp"

namespace audrop::audio {
namespace fs = std::filesystem;

namespace {

SpscRing* g_ring = nullptr;

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

bool is_supported_audio(const fs::path& p) {
    const auto ext = lower(p.extension().string());
    return ext == ".mp3" || ext == ".flac" || ext == ".wav" || ext == ".ogg" || ext == ".qoa";
}

std::vector<fs::path> collect_audio(const std::vector<fs::path>& inputs) {
    std::vector<fs::path> out;
    std::error_code ec;
    for (const auto& in : inputs) {
        if (fs::is_directory(in, ec)) {
            std::vector<fs::path> found;
            for (auto it = fs::recursive_directory_iterator(in, fs::directory_options::skip_permission_denied, ec);
                 it != fs::recursive_directory_iterator(); it.increment(ec)) {
                if (ec) break;
                if (it->is_regular_file(ec) && is_supported_audio(it->path())) found.push_back(it->path());
            }
            std::ranges::sort(found);
            out.insert(out.end(), found.begin(), found.end());
        } else if (fs::is_regular_file(in, ec) && is_supported_audio(in)) {
            out.push_back(in);
        }
    }
    return out;
}

Track make_track(const fs::path& p) { return {p, persist::path_to_utf8(p.stem())}; }

MusicPlayer::MusicPlayer() {
    g_ring = &pcm_;
    thread_ = std::thread([this] { pump(); });
}

MusicPlayer::~MusicPlayer() {
    quit_ = true;
    if (thread_.joinable()) thread_.join();
    std::lock_guard lock(mutex_);
    unload_locked();
    g_ring = nullptr;
}

void MusicPlayer::processor(void* buffer, unsigned int frames) {
    // raylib hands processors interleaved stereo float frames.
    if (g_ring) g_ring->push({static_cast<const float*>(buffer), static_cast<std::size_t>(frames) * 2});
}

void MusicPlayer::unload_locked() {
    if (!loaded_) return;
    StopMusicStream(music_);
    DetachAudioStreamProcessor(music_.stream, &MusicPlayer::processor);
    UnloadMusicStream(music_);
    music_ = {};
    loaded_ = false;
    playing_ = false;
}

bool MusicPlayer::load_locked(std::size_t index, double start_seconds) {
    unload_locked();
    if (index >= tracks_.size()) return false;
    const std::string path = persist::path_to_utf8(tracks_[index].path);
    Music m = LoadMusicStream(path.c_str());
    if (!IsMusicValid(m)) {
        TraceLog(LOG_WARNING, "audrop: could not load %s", path.c_str());
        ++consecutive_failures_;
        return false;
    }
    m.looping = false;
    AttachAudioStreamProcessor(m.stream, &MusicPlayer::processor);
    SetMusicVolume(m, volume_);
    PlayMusicStream(m);
    if (start_seconds > 0) SeekMusicStream(m, static_cast<float>(start_seconds));
    if (paused_) PauseMusicStream(m);
    music_ = m;
    loaded_ = true;
    current_index_ = index;
    consecutive_failures_ = 0;
    raw_time_ = start_seconds;
    raw_stamp_ = now_seconds();
    ++generation_;
    return true;
}

bool MusicPlayer::start_from_locked() {
    // Try the track at order_pos_, then the following ones, until one loads.
    for (std::size_t attempts = 0; attempts < order_.size(); ++attempts) {
        if (load_locked(order_[order_pos_], 0)) return true;
        order_pos_ = (order_pos_ + 1) % order_.size();
    }
    return false;
}

void MusicPlayer::rebuild_order_locked(std::optional<std::size_t> first) {
    order_.resize(tracks_.size());
    std::iota(order_.begin(), order_.end(), std::size_t{0});
    order_pos_ = 0;
    if (order_.empty()) return;
    if (shuffle_) {
        std::ranges::shuffle(order_, rng_);
        if (first) {
            auto it = std::ranges::find(order_, *first);
            std::iter_swap(order_.begin(), it);
        }
    } else if (first) {
        order_pos_ = *first;
    }
}

std::optional<std::size_t> MusicPlayer::order_pos_of_locked(std::size_t index) const {
    auto it = std::ranges::find(order_, index);
    if (it == order_.end()) return std::nullopt;
    return static_cast<std::size_t>(it - order_.begin());
}

void MusicPlayer::pump() {
    while (!quit_) {
        {
            std::lock_guard lock(mutex_);
            if (loaded_ && !paused_) {
                UpdateMusicStream(music_);
                if (!IsMusicStreamPlaying(music_)) {
                    if (override_) {
                        PlayMusicStream(music_);
                    } else {
                        // Track finished: advance through the play order.
                        bool started = false;
                        std::size_t attempts = 0;
                        while (!started && attempts++ < order_.size()) {
                            if (order_pos_ + 1 >= order_.size()) {
                                if (!repeat_) break;
                                if (shuffle_) rebuild_order_locked(std::nullopt);
                                order_pos_ = 0;
                            } else {
                                ++order_pos_;
                            }
                            started = load_locked(order_[order_pos_], 0);
                        }
                        if (!started) unload_locked();
                    }
                }
                if (loaded_) {
                    const double r = GetMusicTimePlayed(music_);
                    if (r != raw_time_.load()) {
                        raw_time_ = r;
                        raw_stamp_ = now_seconds();
                    }
                }
            }
            playing_ = loaded_ && !paused_;
        }
        sleep_seconds(0.002);
    }
}

void MusicPlayer::add_tracks(const std::vector<Track>& tracks) {
    std::lock_guard lock(mutex_);
    const std::size_t base = tracks_.size();
    for (const auto& t : tracks) {
        const bool dup = std::ranges::any_of(tracks_, [&](const Track& e) { return e.path == t.path; });
        if (!dup) tracks_.push_back(t);
    }
    for (std::size_t i = base; i < tracks_.size(); ++i) {
        if (shuffle_ && order_.size() > order_pos_ + 1) {
            std::uniform_int_distribution<std::size_t> d(order_pos_ + 1, order_.size());
            order_.insert(order_.begin() + static_cast<std::ptrdiff_t>(d(rng_)), i);
        } else {
            order_.push_back(i);
        }
    }
    if (!loaded_ && !override_ && tracks_.size() > base) {
        order_pos_ = *order_pos_of_locked(base);
        start_from_locked();
    }
}

void MusicPlayer::remove_track(std::size_t index) {
    std::lock_guard lock(mutex_);
    if (index >= tracks_.size()) return;
    const bool was_current = loaded_ && !override_ && current_index_ == index;
    if (was_current) unload_locked();
    tracks_.erase(tracks_.begin() + static_cast<std::ptrdiff_t>(index));

    const auto pos = order_pos_of_locked(index);
    std::erase(order_, index);
    for (auto& o : order_)
        if (o > index) --o;
    if (pos && *pos < order_pos_) --order_pos_;
    if (current_index_ > index) --current_index_;
    if (resume_index_) {
        if (*resume_index_ == index) resume_index_.reset();
        else if (*resume_index_ > index) --*resume_index_;
    }

    if (was_current && !order_.empty()) {
        if (order_pos_ >= order_.size()) order_pos_ = 0;
        start_from_locked();
    }
}

void MusicPlayer::move_track(std::size_t index, int delta) {
    std::lock_guard lock(mutex_);
    const auto target = static_cast<std::ptrdiff_t>(index) + delta;
    if (index >= tracks_.size() || target < 0 || target >= static_cast<std::ptrdiff_t>(tracks_.size())) return;
    const auto j = static_cast<std::size_t>(target);
    std::swap(tracks_[index], tracks_[j]);
    for (auto& o : order_) {
        if (o == index) o = j;
        else if (o == j) o = index;
    }
    if (current_index_ == index) current_index_ = j;
    else if (current_index_ == j) current_index_ = index;
    if (!shuffle_) rebuild_order_locked(current_index_);
}

void MusicPlayer::clear() {
    std::lock_guard lock(mutex_);
    if (!override_) unload_locked();
    tracks_.clear();
    order_.clear();
    order_pos_ = 0;
    resume_index_.reset();
}

std::vector<Track> MusicPlayer::tracks() const {
    std::lock_guard lock(mutex_);
    return tracks_;
}

std::size_t MusicPlayer::size() const {
    std::lock_guard lock(mutex_);
    return tracks_.size();
}

void MusicPlayer::set_shuffle(bool on) {
    std::lock_guard lock(mutex_);
    if (shuffle_ == on) return;
    shuffle_ = on;
    rebuild_order_locked(loaded_ ? std::optional{current_index_} : std::nullopt);
}

bool MusicPlayer::shuffle() const {
    std::lock_guard lock(mutex_);
    return shuffle_;
}

void MusicPlayer::set_repeat(bool on) {
    std::lock_guard lock(mutex_);
    repeat_ = on;
}

bool MusicPlayer::repeat() const {
    std::lock_guard lock(mutex_);
    return repeat_;
}

void MusicPlayer::play_index(std::size_t index, double start_seconds) {
    std::lock_guard lock(mutex_);
    if (index >= tracks_.size()) return;
    if (override_) {
        resume_index_ = index;
        resume_time_ = start_seconds;
        return;
    }
    if (auto pos = order_pos_of_locked(index)) order_pos_ = *pos;
    load_locked(index, start_seconds);
}

void MusicPlayer::next() {
    std::lock_guard lock(mutex_);
    if (override_ || order_.empty()) return;
    for (std::size_t attempts = 0; attempts < order_.size(); ++attempts) {
        order_pos_ = (order_pos_ + 1) % order_.size();
        if (order_pos_ == 0 && shuffle_) rebuild_order_locked(std::nullopt);
        if (load_locked(order_[order_pos_], 0)) return;
    }
}

void MusicPlayer::previous() {
    std::lock_guard lock(mutex_);
    if (override_ || order_.empty()) return;
    if (loaded_ && GetMusicTimePlayed(music_) > 3.0f) {
        load_locked(current_index_, 0);
        return;
    }
    order_pos_ = (order_pos_ + order_.size() - 1) % order_.size();
    load_locked(order_[order_pos_], 0);
}

void MusicPlayer::set_paused(bool paused) {
    std::lock_guard lock(mutex_);
    if (paused_ == paused) return;
    paused_ = paused;
    if (!loaded_) return;
    if (paused) PauseMusicStream(music_);
    else ResumeMusicStream(music_);
    raw_stamp_ = now_seconds();
}

bool MusicPlayer::paused() const {
    std::lock_guard lock(mutex_);
    return paused_;
}

void MusicPlayer::set_volume(float v) {
    std::lock_guard lock(mutex_);
    volume_ = v;
    if (loaded_) SetMusicVolume(music_, v);
}

void MusicPlayer::play_override(std::vector<unsigned char> data, const char* file_type, std::string title) {
    std::lock_guard lock(mutex_);
    if (!override_) {
        resume_index_ = loaded_ ? std::optional{current_index_} : std::nullopt;
        resume_time_ = loaded_ ? GetMusicTimePlayed(music_) : 0.0;
    }
    unload_locked();
    override_data_ = std::move(data);
    override_title_ = std::move(title);
    Music m = LoadMusicStreamFromMemory(file_type, override_data_.data(), static_cast<int>(override_data_.size()));
    if (!IsMusicValid(m)) return;
    m.looping = true;
    AttachAudioStreamProcessor(m.stream, &MusicPlayer::processor);
    SetMusicVolume(m, volume_);
    PlayMusicStream(m);
    music_ = m;
    loaded_ = true;
    override_ = true;
    paused_ = false;
    raw_time_ = 0;
    raw_stamp_ = now_seconds();
    ++generation_;
}

void MusicPlayer::end_override() {
    std::lock_guard lock(mutex_);
    if (!override_) return;
    unload_locked();
    override_ = false;
    override_data_.clear();
    if (resume_index_ && *resume_index_ < tracks_.size()) {
        if (auto pos = order_pos_of_locked(*resume_index_)) order_pos_ = *pos;
        load_locked(*resume_index_, resume_time_);
    } else {
        ++generation_;
    }
    resume_index_.reset();
}

MusicPlayer::NowPlaying MusicPlayer::now_playing() const {
    std::lock_guard lock(mutex_);
    NowPlaying np;
    np.generation = generation_;
    if (!loaded_) return np;
    np.valid = true;
    np.override = override_;
    np.length = GetMusicTimeLength(music_);
    if (override_) {
        np.track.title = override_title_;
    } else {
        np.index = current_index_;
        np.track = tracks_[current_index_];
    }
    return np;
}

std::optional<Track> MusicPlayer::upcoming() const {
    std::lock_guard lock(mutex_);
    if (order_.empty() || override_) return std::nullopt;
    const std::size_t pos = order_pos_ + 1;
    if (pos < order_.size()) return tracks_[order_[pos]];
    if (repeat_ && !shuffle_) return tracks_[order_[0]];
    return std::nullopt;
}

void MusicPlayer::update_clock(double wall_now) {
    const std::uint64_t gen = generation_.load();
    const double raw = raw_time_.load();
    const double stamp = raw_stamp_.load();
    const double dt = wall_now - last_wall_;
    last_wall_ = wall_now;

    if (gen != clock_generation_) {
        clock_generation_ = gen;
        clock_ = raw;
        return;
    }
    if (!playing_.load()) {
        clock_ = raw;
        return;
    }
    // raylib's position only advances once per audio callback; extrapolate from the last
    // change and slew towards it so the clock is smooth and monotonic in practice.
    const double target = raw + std::clamp(wall_now - stamp, 0.0, 0.1);
    clock_ += dt;
    const double err = target - clock_;
    if (std::abs(err) > 0.05) clock_ = target;
    else clock_ += err * std::min(1.0, dt * 20.0);
}

}  // namespace audrop::audio
