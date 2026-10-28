#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include "sync/beat_grid.hpp"

namespace audrop::persist {

struct CachedBeats {
    sync::BeatGrid grid;      // as tracked
    sync::BeatAdjust adjust;  // player corrections
};

// Content hash of an audio file (FNV-1a 64 over the bytes plus the size).
std::optional<std::uint64_t> hash_file(const std::filesystem::path& path);

std::optional<CachedBeats> load_beats(std::uint64_t hash);
void save_beats(std::uint64_t hash, const CachedBeats& beats);

}  // namespace audrop::persist
