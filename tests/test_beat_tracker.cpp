#include <doctest/doctest.h>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

#include "audio/beat_tracker.hpp"

using namespace audrop;

namespace {

constexpr int kSr = 22050;

// Short decaying noise bursts (kick/snare-like) at the given times, plus quieter off-beat hats.
std::vector<float> click_track(double bpm, double seconds, double first_beat, bool hats) {
    std::vector<float> out(static_cast<std::size_t>(seconds * kSr), 0.0f);
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    auto burst = [&](double t, float amp, double decay) {
        const auto start = static_cast<std::size_t>(t * kSr);
        for (std::size_t i = 0; i < static_cast<std::size_t>(0.08 * kSr) && start + i < out.size(); ++i) {
            const double env = std::exp(-static_cast<double>(i) / (decay * kSr));
            const double tone = std::sin(2 * std::numbers::pi * 60.0 * static_cast<double>(i) / kSr);
            out[start + i] += amp * static_cast<float>(env * (0.6 * tone + 0.4 * noise(rng)));
        }
    };
    const double period = 60.0 / bpm;
    for (double t = first_beat; t < seconds; t += period) {
        burst(t, 0.8f, 0.02);
        if (hats) burst(t + period / 2, 0.2f, 0.005);
    }
    return out;
}

void check_track(double bpm, bool hats) {
    const double first = 0.37;
    auto audio = click_track(bpm, 30.0, first, hats);
    auto grid = audio::track_beats(audio, kSr);
    REQUIRE(grid.beats.size() > 10);
    CHECK(grid.bpm == doctest::Approx(bpm).epsilon(0.02));
    CHECK(grid.confidence > 0.3f);

    // Every detected beat should sit within a few ms of a true beat.
    const double period = 60.0 / bpm;
    double worst = 0, sum = 0;
    for (double b : grid.beats) {
        const double k = std::round((b - first) / period);
        const double err = b - (first + k * period);
        worst = std::max(worst, std::abs(err));
        sum += err;
    }
    const double mean_ms = sum / static_cast<double>(grid.beats.size()) * 1000.0;
    MESSAGE("bpm " << bpm << " mean error " << mean_ms << " ms, worst " << worst * 1000.0 << " ms");
    CHECK(std::abs(mean_ms) < 5.0);
    CHECK(worst < 0.012);
}

}  // namespace

TEST_CASE("beat tracker finds steady tempos") {
    check_track(120.0, false);
    check_track(128.0, true);
    check_track(92.0, true);
    check_track(174.0, false);
}

TEST_CASE("beat tracker reports low confidence on noise") {
    std::vector<float> audio(static_cast<std::size_t>(20 * kSr));
    std::mt19937 rng(3);
    std::normal_distribution<float> n(0.0f, 0.1f);
    for (auto& s : audio) s = n(rng);
    auto grid = audio::track_beats(audio, kSr);
    CHECK_FALSE(grid.usable());
}
