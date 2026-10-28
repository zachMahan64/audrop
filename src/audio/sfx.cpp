#include "audio/sfx.hpp"

#include <algorithm>
#include <vector>

namespace audrop::audio {

Sfx::Sfx() {
    for (std::size_t st = 0; st < styles_.size(); ++st) {
        auto& slots = styles_[st];
        for (std::size_t i = 0; i < slots.size(); ++i) {
            for (int t = 0; t < tuning::kSfxTakes; ++t) {
                std::vector<float> samples = render_sfx(static_cast<SfxId>(i), t, static_cast<SfxStyle>(st));
                Wave wave{};
                wave.frameCount = static_cast<unsigned int>(samples.size());
                wave.sampleRate = kSfxSampleRate;
                wave.sampleSize = 32;
                wave.channels = 1;
                wave.data = samples.data();
                auto& take = slots[i].takes[static_cast<std::size_t>(t)];
                take.voices[0] = LoadSoundFromWave(wave);  // copies the samples
                for (std::size_t v = 1; v < take.voices.size(); ++v) take.voices[v] = LoadSoundAlias(take.voices[0]);
            }
        }
    }
}

Sfx::~Sfx() {
    for (auto& slots : styles_) {
        for (auto& slot : slots) {
            for (auto& take : slot.takes) {
                for (std::size_t v = 1; v < take.voices.size(); ++v) UnloadSoundAlias(take.voices[v]);
                UnloadSound(take.voices[0]);
            }
        }
    }
}

void Sfx::play(SfxId id, float gain) {
    if (!enabled_ || volume_ <= 0) return;
    auto& slot = styles_[static_cast<std::size_t>(style_)][static_cast<std::size_t>(id)];
    int t = std::uniform_int_distribution(0, tuning::kSfxTakes - 1)(rng_);
    if (tuning::kSfxTakes > 1 && t == slot.last)  // shift to one of the other takes
        t = (t + 1 + std::uniform_int_distribution(0, tuning::kSfxTakes - 2)(rng_)) % tuning::kSfxTakes;
    slot.last = t;
    auto& take = slot.takes[static_cast<std::size_t>(t)];
    // Round-robin so rapid repeats overlap instead of cutting each other off.
    Sound& s = take.voices[take.next];
    take.next = (take.next + 1) % take.voices.size();
    auto jitter = [&](float amount) { return std::uniform_real_distribution(1.0f - amount, 1.0f + amount)(rng_); };
    SetSoundVolume(s, std::min(1.0f, volume_ * std::clamp(gain, 0.0f, 1.0f) * jitter(tuning::kSfxPlayGain)));
    SetSoundPitch(s, jitter(tuning::kSfxPlayPitch));
    PlaySound(s);
}

}  // namespace audrop::audio
