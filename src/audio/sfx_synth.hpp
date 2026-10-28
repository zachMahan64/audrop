#pragma once

#include <vector>

namespace audrop::audio {

enum class SfxId { Lock, HardDrop, Clear, ClearBig, OnBeat, Count };

inline constexpr int kSfxSampleRate = 44100;

// Renders take `take` (0..tuning::kSfxTakes-1) of a sound effect as mono float samples at
// kSfxSampleRate, normalized to its peak level from config/tuning.hpp. Take 0 uses the tuned
// parameters as-is; later takes randomize them. Deterministic for a given id and take.
std::vector<float> render_sfx(SfxId id, int take = 0);

}  // namespace audrop::audio
