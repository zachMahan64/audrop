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

}  // namespace

TEST_CASE("every sound effect renders clean, normalized audio") {
    const float peaks[] = {tuning::kSfxLockPeak, tuning::kSfxHardDropPeak, tuning::kSfxClearPeak, tuning::kSfxClearBigPeak,
                           tuning::kSfxOnBeatPeak};
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i) {
        for (int take = 0; take < tuning::kSfxTakes; ++take) {
            CAPTURE(i);
            CAPTURE(take);
            const auto s = render_sfx(static_cast<SfxId>(i), take);
            REQUIRE(s.size() > static_cast<std::size_t>(kSfxSampleRate / 20));
            CHECK(std::ranges::all_of(s, [](float v) { return std::isfinite(v); }));
            CHECK(peak(s) == doctest::Approx(peaks[i]).epsilon(0.001));
            CHECK(s.back() == 0.0f);  // fades out, so playback never ends on a click
        }
    }
}

TEST_CASE("hard drops are more pronounced than normal locks") {
    const auto lock = render_sfx(SfxId::Lock);
    const auto drop = render_sfx(SfxId::HardDrop);
    CHECK(peak(drop) > peak(lock));
    CHECK(drop.size() > lock.size());
    CHECK(render_sfx(SfxId::ClearBig).size() > render_sfx(SfxId::Clear).size());
}

TEST_CASE("sound effects are deterministic per take, and takes differ") {
    CHECK(render_sfx(SfxId::Clear, 2) == render_sfx(SfxId::Clear, 2));
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i) {
        CAPTURE(i);
        for (int a = 0; a < tuning::kSfxTakes; ++a)
            for (int b = a + 1; b < tuning::kSfxTakes; ++b)
                CHECK(render_sfx(static_cast<SfxId>(i), a) != render_sfx(static_cast<SfxId>(i), b));
    }
}
