#include "sync/beat_grid.hpp"

#include "config/tuning.hpp"

namespace audrop::sync {

bool BeatGrid::usable() const { return beats.size() >= 8 && confidence >= tuning::kMinBeatConfidence; }

BeatGrid apply_adjust(const BeatGrid& raw, const BeatAdjust& adjust) {
    BeatGrid out;
    out.confidence = raw.confidence;
    out.bpm = raw.bpm;
    const double offset = adjust.offset_ms / 1000.0;

    if (adjust.tempo_shift < 0) {
        out.bpm = raw.bpm / 2.0;
        for (std::size_t i = 0; i < raw.beats.size(); i += 2) out.beats.push_back(raw.beats[i] + offset);
    } else if (adjust.tempo_shift > 0) {
        out.bpm = raw.bpm * 2.0;
        for (std::size_t i = 0; i < raw.beats.size(); ++i) {
            out.beats.push_back(raw.beats[i] + offset);
            if (i + 1 < raw.beats.size()) out.beats.push_back((raw.beats[i] + raw.beats[i + 1]) / 2.0 + offset);
        }
    } else {
        out.beats.reserve(raw.beats.size());
        for (double b : raw.beats) out.beats.push_back(b + offset);
    }
    return out;
}

}  // namespace audrop::sync
