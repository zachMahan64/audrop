#include "audio/spectrum.hpp"

#include <algorithm>
#include <cmath>

namespace audrop::audio {
namespace {

constexpr std::size_t kFftSize = 1024;
constexpr float kAssumedRate = 48000.0f;  // raylib mixes at the device rate; 44.1k vs 48k barely matters here

float bin_of(float hz) { return hz / kAssumedRate * static_cast<float>(kFftSize); }

}  // namespace

Spectrum::Spectrum() : fft_(kFftSize), history_(kFftSize, 0.0f), mag_(kFftSize / 2 + 1) {}

void Spectrum::push(std::span<const float> stereo) {
    const std::size_t frames = stereo.size() / 2;
    if (frames == 0) return;
    const std::size_t take = std::min(frames, kFftSize);
    // Shift history left and append the newest mono frames.
    std::move(history_.begin() + static_cast<std::ptrdiff_t>(take), history_.end(), history_.begin());
    const std::size_t src0 = frames - take;
    for (std::size_t i = 0; i < take; ++i)
        history_[kFftSize - take + i] = 0.5f * (stereo[(src0 + i) * 2] + stereo[(src0 + i) * 2 + 1]);
}

void Spectrum::update(double dt) {
    fft_.magnitudes(history_, mag_);
    const float norm = 4.0f / static_cast<float>(kFftSize);
    auto band_energy = [&](float lo_hz, float hi_hz) {
        const auto lo = static_cast<std::size_t>(std::max(1.0f, bin_of(lo_hz)));
        const auto hi = std::max(lo + 1, static_cast<std::size_t>(bin_of(hi_hz)));
        float acc = 0;
        for (std::size_t k = lo; k < hi && k < mag_.size(); ++k) acc += mag_[k] * norm;
        return acc / static_cast<float>(hi - lo);
    };

    const float fdt = static_cast<float>(dt);
    auto smooth = [&](float& v, float target, float attack, float release) {
        const float rate = target > v ? attack : release;
        v += (target - v) * std::min(1.0f, rate * fdt);
    };

    // Log-spaced bands, auto-gained per band so quiet tracks still move the visuals.
    for (std::size_t b = 0; b < kBands; ++b) {
        const float t0 = static_cast<float>(b) / kBands, t1 = static_cast<float>(b + 1) / kBands;
        const float lo = 40.0f * std::pow(400.0f, t0), hi = 40.0f * std::pow(400.0f, t1);
        const float e = std::log1p(20.0f * band_energy(lo, hi));
        peaks_[b] = std::max(peaks_[b] * (1.0f - 0.25f * fdt), e);
        smooth(bands_[b], e / std::max(peaks_[b], 0.05f), 30.0f, 8.0f);
    }

    const float bass = std::log1p(20.0f * band_energy(30, 150));
    const float mid = std::log1p(20.0f * band_energy(300, 2000));
    const float treble = std::log1p(20.0f * band_energy(4000, 12000));
    smooth(bass_, bass, 25.0f, 6.0f);
    smooth(mid_, mid, 20.0f, 5.0f);
    smooth(treble_, treble, 20.0f, 5.0f);
    smooth(level_, (bass + mid + treble) / 3.0f, 15.0f, 3.0f);

    smooth(bass_avg_, bass, 2.0f, 2.0f);
    const float hit = std::max(0.0f, bass - bass_avg_ * 1.25f);
    bass_hit_ = std::max(bass_hit_ * (1.0f - std::min(1.0f, 10.0f * fdt)), std::min(1.0f, hit * 3.0f));
}

}  // namespace audrop::audio
