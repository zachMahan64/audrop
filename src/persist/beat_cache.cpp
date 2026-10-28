#include "persist/beat_cache.hpp"

#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>
#include <vector>

#include "persist/paths.hpp"

namespace audrop::persist {
namespace {

// Bump when the beat tracker changes enough that cached grids should be recomputed.
constexpr int kTrackerVersion = 3;

std::filesystem::path cache_path(std::uint64_t hash) {
    char name[32];
    std::snprintf(name, sizeof name, "%016llx.json", static_cast<unsigned long long>(hash));
    return data_dir() / "beats" / name;
}

}  // namespace

std::optional<std::uint64_t> hash_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::uint64_t h = 1469598103934665603ull;
    std::uint64_t size = 0;
    std::vector<char> buf(1 << 16);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const auto n = static_cast<std::size_t>(in.gcount());
        for (std::size_t i = 0; i < n; ++i) {
            h ^= static_cast<unsigned char>(buf[i]);
            h *= 1099511628211ull;
        }
        size += n;
    }
    h ^= size;
    h *= 1099511628211ull;
    return h;
}

std::optional<CachedBeats> load_beats(std::uint64_t hash) {
    auto text = read_file(cache_path(hash));
    if (!text) return std::nullopt;
    const auto j = nlohmann::json::parse(*text, nullptr, false);
    if (j.is_discarded() || j.value("version", 0) != kTrackerVersion) return std::nullopt;
    try {
        CachedBeats c;
        c.grid.bpm = j.at("bpm").get<double>();
        c.grid.confidence = j.at("confidence").get<float>();
        c.grid.beats = j.at("beats").get<std::vector<double>>();
        c.adjust.offset_ms = j.value("offset_ms", 0.0);
        c.adjust.tempo_shift = j.value("tempo_shift", 0);
        return c;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

void save_beats(std::uint64_t hash, const CachedBeats& c) {
    const nlohmann::json j{
        {"version", kTrackerVersion},
        {"bpm", c.grid.bpm},
        {"confidence", c.grid.confidence},
        {"offset_ms", c.adjust.offset_ms},
        {"tempo_shift", c.adjust.tempo_shift},
        {"beats", c.grid.beats},
    };
    write_file_atomic(cache_path(hash), j.dump());
}

}  // namespace audrop::persist
