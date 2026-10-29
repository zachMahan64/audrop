#pragma once

#include <raylib.h>

#include <array>
#include <cstddef>
#include <random>

#include "audio/sfx_synth.hpp"
#include "config/tuning.hpp"

namespace audrop::audio {

// Gameplay sound effects. Plays straight to the mixer, separate from the music stream, so
// the visualizer and beat analysis never hear them. Needs an initialized audio device.
class Sfx {
public:
    Sfx();
    ~Sfx();
    Sfx(const Sfx&) = delete;
    Sfx& operator=(const Sfx&) = delete;

    void set_enabled(bool on) { enabled_ = on; }
    void set_volume(float v) { volume_ = v; }
    // Sounds are synthesized the first time their style is selected.
    void set_style(SfxStyle s);
    // `gain` (0..1) scales this one playback on top of the volume setting. Picks a random
    // take (never the same one twice in a row) and jitters its pitch and gain.
    void play(SfxId id, float gain = 1.0f);

private:
    struct Take {
        std::array<Sound, tuning::kSfxVoices> voices{};  // [0] owns the sample data, the rest are aliases
        std::size_t next = 0;
    };
    struct Slot {
        std::array<Take, tuning::kSfxTakes> takes{};
        int last = -1;
    };
    using Slots = std::array<Slot, static_cast<std::size_t>(SfxId::Count)>;
    std::array<Slots, static_cast<std::size_t>(SfxStyle::Count)> styles_{};
    std::array<bool, static_cast<std::size_t>(SfxStyle::Count)> loaded_{};
    SfxStyle style_ = SfxStyle::Modern;
    std::mt19937 rng_{std::random_device{}()};
    bool enabled_ = true;
    float volume_ = 0.8f;
};

}  // namespace audrop::audio
