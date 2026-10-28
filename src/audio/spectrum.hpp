#pragma once

#include <array>
#include <span>
#include <vector>

#include "audio/fft.hpp"

namespace audrop::audio {

// Lightweight live analysis of the playing audio for UI pulses and the shader visualizer.
class Spectrum {
public:
    static constexpr std::size_t kBands = 32;

    Spectrum();
    void push(std::span<const float> stereo);
    void update(double dt);

    float bass() const { return bass_; }
    float mid() const { return mid_; }
    float treble() const { return treble_; }
    float level() const { return level_; }
    float bass_hit() const { return bass_hit_; }  // transient, decays quickly
    const std::array<float, kBands>& bands() const { return bands_; }

private:
    RealFft fft_;
    std::vector<float> history_;  // mono
    std::vector<float> mag_;
    std::array<float, kBands> bands_{};
    std::array<float, kBands> peaks_{};
    float bass_ = 0, mid_ = 0, treble_ = 0, level_ = 0, bass_hit_ = 0;
    float bass_avg_ = 0;
};

}  // namespace audrop::audio
