#pragma once

#include <atomic>
#include <span>

#include "sync/beat_grid.hpp"

namespace audrop::audio {

// Offline beat tracker: spectral-flux onset envelope, autocorrelation tempo estimate with a
// log-normal tempo prior, then dynamic-programming beat placement (Ellis 2007), followed by
// sub-frame peak refinement and local regularisation.
//
// `mono` is the whole track mixed to mono. 22050 Hz is plenty; any rate works.
// `cancel` may be set from another thread to abort early (returns an empty grid).
sync::BeatGrid track_beats(std::span<const float> mono, int sample_rate, const std::atomic<bool>* cancel = nullptr);

}  // namespace audrop::audio
