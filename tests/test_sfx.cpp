#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include "audio/sfx_synth.hpp"
#include "config/tuning.hpp"

using namespace audrop::audio;
namespace tuning = audrop::tuning;

namespace {

float peak(const std::vector<float>& s) {
    float m = 0;
    for (const float v : s) m = std::max(m, std::abs(v));
    return m;
}

constexpr SfxStyle kStyles[] = {SfxStyle::Modern, SfxStyle::Chip, SfxStyle::Aero, SfxStyle::Soft};

}  // namespace

TEST_CASE("every sound effect renders clean, normalized audio") {
    const float peaks[][5] = {
        {tuning::kSfxLockPeak, tuning::kSfxHardDropPeak, tuning::kSfxClearPeak, tuning::kSfxClearBigPeak, tuning::kSfxOnBeatPeak},
        {tuning::kChipLockPeak, tuning::kChipHardDropPeak, tuning::kChipClearPeak, tuning::kChipClearBigPeak,
         tuning::kChipOnBeatPeak},
        {tuning::kAeroLockPeak, tuning::kAeroHardDropPeak, tuning::kAeroClearPeak, tuning::kAeroClearBigPeak,
         tuning::kAeroOnBeatPeak},
        {tuning::kSoftLockPeak, tuning::kSoftHardDropPeak, tuning::kSoftClearPeak, tuning::kSoftClearBigPeak,
         tuning::kSoftOnBeatPeak},
    };
    static_assert(std::size(peaks) == static_cast<std::size_t>(SfxStyle::Count));
    for (const auto style : kStyles)
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i) {
        for (int take = 0; take < tuning::kSfxTakes; ++take) {
            CAPTURE(static_cast<int>(style));
            CAPTURE(i);
            CAPTURE(take);
            const auto s = render_sfx(static_cast<SfxId>(i), take, style);
            REQUIRE(s.size() > static_cast<std::size_t>(kSfxSampleRate / 20));
            CHECK(std::ranges::all_of(s, [](float v) { return std::isfinite(v); }));
            CHECK(peak(s) == doctest::Approx(peaks[static_cast<int>(style)][i]).epsilon(0.001));
            CHECK(s.back() == 0.0f);  // fades out, so playback never ends on a click
        }
    }
}

TEST_CASE("hard drops are more pronounced than normal locks") {
    for (const auto style : kStyles) {
        CAPTURE(static_cast<int>(style));
        const auto lock = render_sfx(SfxId::Lock, 0, style);
        const auto drop = render_sfx(SfxId::HardDrop, 0, style);
        CHECK(peak(drop) > peak(lock));
        CHECK(drop.size() > lock.size());
        CHECK(render_sfx(SfxId::ClearBig, 0, style).size() > render_sfx(SfxId::Clear, 0, style).size());
    }
}

TEST_CASE("each style has its own sounds") {
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i)
        for (std::size_t a = 0; a < std::size(kStyles); ++a)
            for (std::size_t b = a + 1; b < std::size(kStyles); ++b) {
                CAPTURE(i);
                CAPTURE(a);
                CAPTURE(b);
                CHECK(render_sfx(static_cast<SfxId>(i), 0, kStyles[a]) != render_sfx(static_cast<SfxId>(i), 0, kStyles[b]));
            }
}

TEST_CASE("sound effects are deterministic per take, and takes differ") {
    CHECK(render_sfx(SfxId::Clear, 2) == render_sfx(SfxId::Clear, 2));
    for (const auto style : kStyles) CHECK(render_sfx(SfxId::Clear, 2, style) == render_sfx(SfxId::Clear, 2, style));
    for (const auto style : kStyles)
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i) {
        CAPTURE(static_cast<int>(style));
        CAPTURE(i);
        for (int a = 0; a < tuning::kSfxTakes; ++a)
            for (int b = a + 1; b < tuning::kSfxTakes; ++b)
                CHECK(render_sfx(static_cast<SfxId>(i), a, style) != render_sfx(static_cast<SfxId>(i), b, style));
    }
}
